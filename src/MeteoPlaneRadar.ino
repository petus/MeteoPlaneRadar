// =============================================================================
//  MeteoPlaneRadar
//  Live aircraft radar (adsb.fi) + CHMU precipitation meteoradar on a round
//  touchscreen.
// =============================================================================
//
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
//  Web:     https://chiptron.cz
//  Board:   Waveshare ESP32-S3-Touch-LCD-2.1
//           - ESP32-S3R8 (8 MB PSRAM, 16 MB flash)
//           - round 480x480 display, ST7701 controller (RGB interface)
//           - CST820 capacitive touch (I2C)
//           - TCA9554 I/O expander (LCD reset / CS / power control)
//
//  Screens (cycled by a LONG press):
//    1) Aircraft radar - adsb.fi, tap an aircraft for details
//    2) Meteoradar     - CHMU precipitation composite, animated
//    3) Settings       - brightness, WiFi, location
//
//  Controls (both radar screens):
//    - swipe left/right            = change the range
//    - short tap                   = aircraft detail / selection
//    - long press                  = switch screen (Planes -> Meteo -> Settings)
//    - hold BOOT at startup (~3 s)  = factory reset
//
//  Anti-flicker (verified, ported from the SatRadar project):
//    - single off-screen canvas in PSRAM (Canvas16), pushed to the panel with
//      ONE draw_bitmap per frame instead of pixel-by-pixel (Display_ST7701),
//    - RGB pixel clock 8 MHz + num_fbs=1 with bounce buffers (no double_fb),
//    so the PSRAM bus is never hit by simultaneous read (RGB DMA) and a stream
//    of tiny writes, which was what made pixels flicker.
//
//  Libraries used (Arduino IDE, ESP32 core 3.x):
//    - GFX Library for Arduino (moononournation) - drawing
//    - PNGdec (bitbank2)                          - decoding the CHMU frames
//    - ArduinoJson (bblanchon)                    - parsing the ADS-B data
//    - WiFiManager (tzapu)                        - WiFi configuration portal
//    - QRCode (ricmoo)                            - QR code (bundled)
//    - Preferences, Wire, HTTPClient, esp_lcd     - part of the ESP32 core
//
//  Data sources (attribution required, personal non-commercial use only):
//    - Aircraft: adsb.fi, https://adsb.fi
//    - Precip.:  Cesky hydrometeorologicky ustav (CHMU), https://opendata.chmi.cz
//    - Location: ip-api.com (automatic detection by IP)
//
//  Licence: MIT (see the LICENSE file).
// =============================================================================

#include <Arduino_GFX_Library.h>
#include <Wire.h>
#include <time.h>
#include "esp_heap_caps.h"

#include "TCA9554.h"
#include "Display_ST7701.h"
#include "Canvas16.h"
#include "Touch_CST820.h"
#include "Settings.h"
#include "UI.h"
#include "WiFiPortal.h"
#include "GeoIP.h"
#include "ADSB.h"
#include "ScreenPlanes.h"
#include "CHMU.h"
#include "ScreenWeather.h"
#include "ScreenSettings.h"
#include "Watchdog.h"

#define I2C_SDA 15
#define I2C_SCL 7
#define TZ_INFO "CET-1CEST,M3.5.0,M10.5.0/3"
#define BOOT_PIN 0

// gfx = single off-screen canvas in PSRAM. Everything is drawn here, then
// flush() pushes the whole frame to the panel in one shot -> no flicker.
// (Typed as Arduino_GFX* so the shared UI code needs no changes; the object is
//  a Canvas16, whose virtual flush()/writers do the PSRAM-safe work.)
Arduino_GFX* gfx = nullptr;

// yield() during long network transfers, and feed the watchdog so multi-second
// CHMU downloads (6 frames) never trip it.
static void netPoll() { yield(); Watchdog_Feed(); }

static void checkBootReset() {
  pinMode(BOOT_PIN, INPUT_PULLUP);
  if (digitalRead(BOOT_PIN) != LOW) return;
  gfx->fillScreen(C_BLACK);
  UI_TextCentered("Hold to reset...", LCD_HEIGHT / 2, C_WHITE, 2);
  gfx->flush();
  unsigned long start = millis();
  while (digitalRead(BOOT_PIN) == LOW) {
    if (millis() - start >= 3000) {
      UI_TextCentered("Erasing settings", LCD_HEIGHT / 2 + 30, C_RED, 2);
      gfx->flush();
      Settings_ClearAll();
      WiFi_Reset();
      delay(800);
      ESP.restart();
    }
    delay(20);
  }
}

// --- Screen manager ---
// 0 = aircraft radar, 1 = meteoradar, 2 = settings.
enum { SCREEN_PLANES = 0, SCREEN_METEO = 1, SCREEN_SETTINGS = 2, SCREEN_COUNT = 3 };
static int s_screen = SCREEN_PLANES;

// Screen indicator - dots near the top edge (inside the circle).
static void drawScreenDots() {
  int gap = 20;
  int cx = LCD_WIDTH / 2;
  int y = 18;
  int startX = cx - (SCREEN_COUNT - 1) * gap / 2;
  for (int i = 0; i < SCREEN_COUNT; i++) {
    int x = startX + i * gap;
    if (i == s_screen) gfx->fillCircle(x, y, 4, C_WHITE);
    else               gfx->drawCircle(x, y, 4, C_GRAY);
  }
}

static void drawActive() {
  switch (s_screen) {
    case SCREEN_PLANES:   ScreenPlanes_Draw();   break;
    case SCREEN_METEO:    ScreenWeather_Draw();  break;
    case SCREEN_SETTINGS: ScreenSettings_Draw(); break;
  }
  drawScreenDots();
  gfx->flush();    // push the whole frame to the panel at once
}

static void enterActive() {
  switch (s_screen) {
    case SCREEN_PLANES:   ScreenPlanes_Enter();   break;
    case SCREEN_METEO:    ScreenWeather_Enter();  break;
    case SCREEN_SETTINGS: ScreenSettings_Enter(); break;
  }
  drawActive();
}

// Long press cycles through the screens.
static void cycleScreen() {
  s_screen = (s_screen + 1) % SCREEN_COUNT;
  Serial.printf("Screen: %d\n", s_screen);
  enterActive();
}

static bool activeTick() {
  switch (s_screen) {
    case SCREEN_PLANES:   return ScreenPlanes_Tick();
    case SCREEN_METEO:    return ScreenWeather_Tick();
    case SCREEN_SETTINGS: return ScreenSettings_Tick();
  }
  return false;
}

// Swipe -> range on the radar screens; the meteoradar and planes both use it.
static void activeChangeRange(int dir) {
  switch (s_screen) {
    case SCREEN_PLANES: ScreenPlanes_ChangeRange(dir);  break;
    case SCREEN_METEO:  ScreenWeather_ChangeRange(dir); break;
    default: break;   // settings has no range
  }
}

static bool activeTap(int x, int y) {
  switch (s_screen) {
    case SCREEN_PLANES:   return ScreenPlanes_HandleTap(x, y);
    case SCREEN_SETTINGS: return ScreenSettings_HandleTap(x, y);
    default: return false;   // meteoradar has no tap targets
  }
}

// Is a modal (the aircraft detail) open? Then swipe/long-press are captured to
// close it rather than change range / switch screen.
static bool activeModalOpen() {
  return (s_screen == SCREEN_PLANES) && ScreenPlanes_DetailOpen();
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n=== MeteoPlaneRadar ===");

  Settings_Begin();

  Wire.begin(I2C_SDA, I2C_SCL, 400000);
  delay(50);
  TCA9554_Init();
  TCA9554_SetPin(EXIO_LCD_PWR, false);
  delay(10);

  Backlight_Init();
  Set_Backlight(Settings_Backlight());
  ST7701_Init();

  // Single PSRAM canvas (anti-flicker). All drawing goes here; flush() pushes
  // the whole frame to the panel in one draw_bitmap.
  Canvas16* canvas = new Canvas16(LCD_WIDTH, LCD_HEIGHT);
  if (!canvas->begin()) {
    Serial.println("FATAL: canvas alloc failed (check OPI PSRAM)");
    while (true) delay(1000);
  }
  gfx = canvas;
  gfx->setTextWrap(false);
  gfx->fillScreen(C_BLACK);
  gfx->flush();

  Touch_Init();

  checkBootReset();

  ADSB_SetPollFn(netPoll);
  CHMU_SetPollFn(netPoll);

  WiFi_ConnectOrPortal();

  if (WiFi_IsConnected()) {
    configTzTime(TZ_INFO, "pool.ntp.org");
    GeoIP_DetectIfNeeded();   // fill in the location by IP if the user did not set one
  }

  s_screen = SCREEN_PLANES;
  ScreenPlanes_Enter();
  drawActive();

  Watchdog_Begin();   // hardware watchdog for 24/7 operation
  Serial.println("Setup done");
}

void loop() {
  // --- Touch: swipe (range) vs short tap (detail) vs long press (switch) ---
  static bool touching = false;
  static int  startX = 0, startY = 0;
  static int  lastX = 0, lastY = 0;
  static unsigned long startMs = 0;

  TouchData t;
  Touch_Read(&t);

  if (t.points > 0) {
    if (!touching) {                 // touch begins
      touching = true;
      startX = t.x; startY = t.y; startMs = millis();
    }
    lastX = t.x; lastY = t.y;        // last valid position
  } else if (touching) {             // touch ends -> evaluate the gesture
    touching = false;
    int dx = lastX - startX;
    int dy = lastY - startY;
    unsigned long dur = millis() - startMs;
    bool smallMove = (abs(dx) < 60 && abs(dy) < 60);

    if (abs(dx) >= 70 && abs(dy) <= 90 && dur <= 700) {
      // Horizontal swipe -> change the range. If a detail is open, a swipe just
      // closes it (so you cannot accidentally re-scale behind the panel).
      if (activeModalOpen()) { ScreenPlanes_CloseDetail(); drawActive(); }
      else                   { activeChangeRange(dx < 0 ? +1 : -1); drawActive(); }
    } else if (smallMove && dur >= 500) {
      // Long press -> switch screen (or close an open detail first).
      if (activeModalOpen()) { ScreenPlanes_CloseDetail(); drawActive(); }
      else                     cycleScreen();
    } else if (smallMove && dur < 500) {
      // Short tap -> aircraft detail / selection.
      if (activeTap(lastX, lastY)) drawActive();
    }
  }

  WiFi_Loop();

  // Request for the WiFi portal coming from the settings screen.
  if (ScreenSettings_WantsPortal()) {
    ScreenSettings_ClearPortal();
    Watchdog_Suspend();                    // the portal blocks - stop watching
    WiFi_StartPortal();                    // blocking - draws its own AP screen
    Watchdog_Resume();
    if (WiFi_IsConnected()) {
      configTzTime(TZ_INFO, "pool.ntp.org");
    }
    enterActive();                         // redraw once it returns
  }

  // Redrawing is decoupled from reading the touch and capped at ~12 FPS.
  static unsigned long lastDraw = 0;
  bool wantDraw = activeTick();
  if (wantDraw && millis() - lastDraw >= 80) {
    drawActive();
    lastDraw = millis();
  }

  Watchdog_Feed();
  delay(5);
}
