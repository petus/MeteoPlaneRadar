// =============================================================================
//  MeteoPlaneRadar
//  Live aircraft radar (adsb.fi) + CHMU precipitation meteoradar on a round
//  touchscreen.
// =============================================================================
//
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
//           Ondra OK1CDJ / apps.ok1cdj.com
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
//    - swipe left/right             = change the range
//    - short tap                    = aircraft detail / selection
//    - long press LEFT half         = previous screen
//    - long press RIGHT half        = next screen
//    - hold BOOT at startup (~3 s)  = factory reset
//
//  Map orientation (Settings -> "Nahore"):
//    You set WHICH DIRECTION IS AT THE TOP of the aircraft radar - simply the
//    way you are looking out of the window. Pick "V" and everything east of you
//    is drawn at the top, north ends up on the left, exactly as in reality.
//    "+" walks the compass clockwise (S -> SV -> V -> JV ...), MAP_ROT_STEP_DEG
//    (45 deg) per press, and the value survives a restart. The compass marks
//    S/V/J/Z around the rim and the small compass next to the buttons show
//    where north currently is. The weather screen is deliberately NOT turned -
//    a precipitation map is read north-up.
//
//  Serial log: 115200 Bd over the connector marked "USB" (the ESP32-S3 native
//  USB). The other USB-C connector on the board will not show anything.
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
// Inspired
// https://github.com/MatixYo/ESP32-Plane-Radar
// https://github.com/Selbyl/ESP32-S30Touch-LCD-2.1_Plane-Radar
// https://github.com/mylms/ESP-MeteoRadar/tree/main 
//
//  Licence: MIT (see the LICENSE file).
//
//  Version: see src/Version.h (FW_VERSION)
//  Change history: see CHANGELOG.md in the repository root.
//
// =============================================================================

#include <Arduino_GFX_Library.h>
#include <Wire.h>
#include <time.h>   // tzset() - see the note above setup()
#include "esp_heap_caps.h"
#include "esp_system.h"   // esp_reset_reason()

#include "TCA9554.h"
#include "Display_ST7701.h"
#include "Canvas16.h"
#include "Touch_CST820.h"
#include "Settings.h"
#include "Version.h"
#include "Config.h"
#include "UI.h"
#include "WiFiPortal.h"
#include "GeoIP.h"
#include "ADSB.h"
#include "ScreenPlanes.h"
#include "APRS.h"
#include "ScreenAPRS.h"
#include "CHMU.h"
#include "ScreenWeather.h"
#include "ScreenSettings.h"
#include "OTA.h"
#include "Watchdog.h"

// Board pins, time zone, ranges and limits all live in Config.h.

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
  UI_TextCentered("Drzte pro reset...", LCD_HEIGHT / 2, C_WHITE, 2);
  gfx->flush();
  unsigned long start = millis();
  while (digitalRead(BOOT_PIN) == LOW) {
    if (millis() - start >= 3000) {
      UI_TextCentered("Mazu nastaveni", LCD_HEIGHT / 2 + 30, C_RED, 2);
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
// 0 = aircraft radar, 1 = meteoradar, 2 = APRS station, 3 = settings.
enum { SCREEN_PLANES = 0, SCREEN_METEO = 1, SCREEN_APRS = 2, SCREEN_SETTINGS = 3,
       SCREEN_COUNT = 4 };
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
    case SCREEN_APRS:     ScreenAPRS_Draw();     break;
    case SCREEN_SETTINGS: ScreenSettings_Draw(); break;
  }
  drawScreenDots();
  gfx->flush();    // hand the framebuffer over; Canvas16 switches to the other
}

static void enterActive() {
  switch (s_screen) {
    case SCREEN_PLANES:   ScreenPlanes_Enter();   break;
    case SCREEN_METEO:    ScreenWeather_Enter();  break;
    case SCREEN_APRS:     ScreenAPRS_Enter();     break;
    case SCREEN_SETTINGS: ScreenSettings_Enter(); break;
  }
  drawActive();
}

// Long press switches screens by direction: dir -1 = previous, +1 = next.
// Wraps around so both sides always do something (the dots at the top show
// where you are).
static void switchScreen(int dir) {
  s_screen = (s_screen + dir + SCREEN_COUNT) % SCREEN_COUNT;
  Settings_SetScreen(s_screen);           // remember across restarts (debounced)
  Serial.printf("Screen: %d\n", s_screen);
  enterActive();
}

static bool activeTick() {
  switch (s_screen) {
    case SCREEN_PLANES:   return ScreenPlanes_Tick();
    case SCREEN_METEO:    return ScreenWeather_Tick();
    case SCREEN_APRS:     return ScreenAPRS_Tick();
    case SCREEN_SETTINGS: return ScreenSettings_Tick();
  }
  return false;
}

// Swipe -> range on the radar screens; the meteoradar and planes both use it.
static void activeChangeRange(int dir) {
  switch (s_screen) {
    case SCREEN_PLANES: ScreenPlanes_ChangeRange(dir);  break;
    case SCREEN_METEO:  ScreenWeather_ChangeRange(dir); break;
    case SCREEN_APRS:   ScreenAPRS_ChangeRange(dir);    break;
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

static const char* resetReasonText() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON:   return "zapnuti napajeni";
    case ESP_RST_EXT:       return "externi reset (tlacitko)";
    case ESP_RST_SW:        return "softwarovy restart (napr. po OTA)";
    case ESP_RST_PANIC:     return "PANIC - vyjimka v programu";
    case ESP_RST_INT_WDT:   return "WATCHDOG (preruseni)";
    case ESP_RST_TASK_WDT:  return "WATCHDOG (zaseknuta smycka)";
    case ESP_RST_WDT:       return "WATCHDOG (jiny)";
    case ESP_RST_DEEPSLEEP: return "probuzeni z deep sleep";
    case ESP_RST_BROWNOUT:  return "BROWNOUT - podpeti napajeni";
    case ESP_RST_SDIO:      return "SDIO";
    default:                return "neznamy";
  }
}

// The weather screen needs no clock (its HH:MM comes from each frame's own name
// in CHMU.cpp), but the APRS screen does: it shows how old a station's last
// position is, and "we have current data" is the whole point. So SNTP is started
// once WiFi is up (see setup()). It is non-blocking - time() simply becomes
// valid a few seconds later, and every HTTPS connection still uses setInsecure()
// so no certificate validity is checked.
//
// applyTimezone() runs first and independently: CHMU.cpp converts each frame's
// UTC timestamp with localtime_r(), and without TZ in the environment that would
// quietly hand back UTC - the labels would be an hour or two out. This has to
// hold even before (or without) an NTP sync, so the TZ is set here explicitly;
// configTzTime() later sets the same TZ again and adds the servers.
static void applyTimezone() {
  setenv("TZ", TZ_INFO, 1);
  tzset();
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.printf("\n=== MeteoPlaneRadar v%s ===\n", FW_VERSION);
  // Why the board came up. Invaluable in a bug report: a panic or a watchdog
  // reset means something crashed, a brownout points at the power supply.
  Serial.printf("Duvod restartu: %s\n", resetReasonText());
  Serial.printf("Volna pamet: %u B\n", (unsigned)ESP.getFreeHeap());

  // ST7701 cold-start fix. On a raw power-on the RGB panel occasionally comes up
  // split into two halves; a warm reset always brings it up cleanly (the LCD
  // supply is held by the TCA9554 across a reset, so the panel stays powered).
  // Rather than chase a fragile init-timing race, reboot ONCE on a cold power-on
  // so the panel is always initialised on that reliable warm path. This runs
  // before the display is touched (no split is ever shown), costs a fraction of
  // a second, and cannot loop: the reboot's reset reason is no longer POWERON.
  if (esp_reset_reason() == ESP_RST_POWERON) {
    Serial.println("Studeny start - reboot pro spolehlivy nabeh displeje");
    Serial.flush();
    esp_restart();
  }

  Settings_Begin();
  applyTimezone();   // needed for the weather frame labels, not for a clock

  Wire.begin(I2C_SDA, I2C_SCL, 400000);
  delay(50);
  TCA9554_Init();
  TCA9554_SetPin(EXIO_LCD_PWR, false);
  delay(10);

  Backlight_Init();
  // Backlight stays off for now. The panel powers up with random memory
  // content, so lighting it before the first frame is drawn shows a white
  // flash. Set_Backlight() comes right after the first flush() below.
  if (!ST7701_Init()) {
    // Nothing can be drawn from here on, so the serial log is the only way out.
    // Keep restarting rather than sitting on a black screen forever: a transient
    // failure then fixes itself, and a permanent one at least says why.
    Serial.println("Displej se nepodarilo inicializovat, restartuji za 5 s");
    delay(5000);
    ESP.restart();
  }

  // Single PSRAM canvas (anti-flicker). All drawing goes here; flush() pushes
  // the whole frame to the panel in one draw_bitmap.
  Canvas16* canvas = new Canvas16(LCD_WIDTH, LCD_HEIGHT);
  // Prefer drawing straight into the panel's second framebuffer (no copy on
  // flush). Fall back to our own buffer if the driver does not expose them.
  uint16_t* fb1 = LCD_FrameBuffer(1);
  if (fb1) {
    canvas->useExternalBuffer(fb1, 1);
    Serial.println("Displej: dvojity framebuffer, kresleni bez kopirovani");
  } else if (!canvas->begin()) {
    Serial.println("FATAL: canvas alloc failed (check OPI PSRAM)");
    while (true) delay(1000);
  }
  gfx = canvas;
  gfx->setTextWrap(false);
  gfx->fillScreen(C_BLACK);
  gfx->flush();
  Set_Backlight(Settings_Backlight());   // now there is something to look at

  if (!Touch_Init()) {
    // Not fatal - the radar keeps running, it just cannot be operated. Worth
    // knowing about, because from the outside it looks like a frozen board.
    Serial.println("VAROVANI: dotyk se neinicializoval, ovladani nebude fungovat");
  }

  checkBootReset();

  ADSB_SetPollFn(netPoll);
  APRS_SetPollFn(netPoll);
  CHMU_SetPollFn(netPoll);

  WiFi_ConnectOrPortal();

  // Start SNTP (TZ was already set by applyTimezone). Non-blocking: the network
  // stack is up now, time() turns valid once a server answers. Used by the APRS
  // screen to show how fresh a station's last position is.
  configTzTime(TZ_INFO, "pool.ntp.org", "time.google.com", "time.cloudflare.com");

  if (WiFi_IsConnected()) {
    GeoIP_DetectIfNeeded();   // fill in the location by IP if the user did not set one
  }

  s_screen = Settings_Screen();                        // restore the last screen
  if (s_screen >= SCREEN_COUNT) s_screen = SCREEN_PLANES;
  enterActive();                                       // Enter() also restores the range

  Watchdog_Begin();   // hardware watchdog for 24/7 operation
  Serial.println("Setup done");
}

// --- Display watchdog -------------------------------------------------------
// The task watchdog cannot see this failure: the sketch is fine, it is the RGB
// peripheral that stops scanning (black screen, backlight still on, only a
// power cycle helps). So watch the frame counter fed by the VSYNC interrupt.
// First try to repair - the most likely cause is the I/O expander having lost
// the display's reset or power line - and if the panel still does not come
// back, reboot instead of leaving a black brick on the shelf.
static void displayWatchdog() {
#if DISPLAY_WD
  static uint32_t lastCount = 0;
  static unsigned long lastMove = 0;
  static bool repaired = false;
  static bool armed = false;        // see below

  uint32_t now = LCD_VsyncCount();
  unsigned long ms = millis();
  if (now != lastCount) {           // panel is scanning - all good
    lastCount = now;
    lastMove = ms;
    repaired = false;
    // Arm only once the counter has demonstrably been moving. If a future board
    // or driver version never delivered the VSYNC event at all, an always-armed
    // watchdog would reboot a perfectly healthy device every few seconds - far
    // worse than the fault it is meant to catch.
    if (!armed && now > 100) armed = true;
    return;
  }
  if (!armed) return;               // never seen a frame - do not judge
  if (lastMove == 0) { lastMove = ms; return; }

  unsigned long stalled = ms - lastMove;
  if (!repaired && stalled >= DISPLAY_WD_DEAD_MS) {
    repaired = true;
    Serial.printf("DISPLEJ: zadny snimek %lu ms, zkousim opravu\n", stalled);
    TCA9554_Verify();                       // expander back to known state
    TCA9554_SetPin(EXIO_LCD_PWR, false);    // power on (active low)
    TCA9554_SetPin(EXIO_LCD_RST, true);     // release reset
    Set_Backlight(Settings_Backlight());
  } else if (stalled >= DISPLAY_WD_REBOOT_MS) {
    Serial.printf("DISPLEJ: panel neobnoven po %lu ms, restartuji\n", stalled);
    Serial.flush();
    ESP.restart();
  }
#endif
}

void loop() {
  // --- Touch: swipe (range) vs short tap (detail) vs long press (switch) ---
  //
  // The gesture does NOT end on the first empty sample. The CST820 skips a
  // sample now and then, and Touch_Read() also throws away corrupted ones - so
  // an empty sample in the middle of a drag is normal, not a lifted finger.
  // Ending the gesture there would split one swipe into several short taps,
  // each of which would be taken for a tap on the map. Wait for
  // TOUCH_RELEASE_MS of continuous silence instead.
  static bool touching = false;
  static int  startX = 0, startY = 0;
  static int  lastX = 0, lastY = 0;
  static unsigned long startMs = 0;
  static unsigned long lastSeenMs = 0;   // last sample with a finger on it

  TouchData t;
  Touch_Read(&t);

  if (t.points > 0) {
    if (!touching) {                 // touch begins
      touching = true;
      startX = t.x; startY = t.y; startMs = millis();
    }
    lastX = t.x; lastY = t.y;        // last valid position
    lastSeenMs = millis();
  } else if (touching && millis() - lastSeenMs >= TOUCH_RELEASE_MS) {
    touching = false;                // finger really is up -> evaluate
    int dx = lastX - startX;
    int dy = lastY - startY;
    unsigned long dur = lastSeenMs - startMs;   // to the last real sample
    bool smallMove = (abs(dx) < 60 && abs(dy) < 60);

#if TOUCH_DEBUG
    // Log every finished gesture. Together with the SEL: lines from ScreenPlanes
    // this shows whether a detail panel closed because of a (possibly spurious)
    // touch, or because the aircraft dropped out of the data.
    Serial.printf("TOUCH: start=(%d,%d) konec=(%d,%d) dx=%d dy=%d %lums -> %s\n",
                  startX, startY, lastX, lastY, dx, dy, (unsigned long)dur,
                  (abs(dx) >= 70 && abs(dy) <= 90 && dur <= 700) ? "swipe"
                    : (smallMove && dur >= 500) ? "dlouhy stisk"
                    : (smallMove) ? "klepnuti" : "ignorovano");
#endif

    if (abs(dx) >= 70 && abs(dy) <= 90 && dur <= 700) {
      // Horizontal swipe -> change the range. If a detail is open, a swipe just
      // closes it (so you cannot accidentally re-scale behind the panel).
      if (activeModalOpen()) { ScreenPlanes_CloseDetail(); drawActive(); }
      else                   { activeChangeRange(dx < 0 ? +1 : -1); drawActive(); }
    } else if (smallMove && dur >= 500) {
      // Long press -> switch screen by side: left half = previous, right half =
      // next (or close an open detail first).
      if (activeModalOpen()) { ScreenPlanes_CloseDetail(); drawActive(); }
      else switchScreen(lastX < LCD_WIDTH / 2 ? -1 : +1);
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
    enterActive();                         // redraw once it returns
  }

  // Firmware update over WiFi (ElegantOTA in AP mode). OTA_Run() is blocking and
  // ends by rebooting, so nothing after it runs on a normal update.
  if (ScreenSettings_WantsOTA()) {
    ScreenSettings_ClearOTA();
    Watchdog_Suspend();
    OTA_Run();
    Watchdog_Resume();   // reached only if OTA_Run() ever returns without reboot
    enterActive();
  }

  // Redrawing is decoupled from reading the touch and capped at ~12 FPS.
  static unsigned long lastDraw = 0;
  bool wantDraw = activeTick();
  if (wantDraw && millis() - lastDraw >= 80) {
    drawActive();
    lastDraw = millis();
  }

  // Cheap insurance for the display: one I2C read every few seconds that checks
  // the expander still holds LCD power and reset where we left them, and puts
  // them back if not. A single corrupted write there would otherwise blank the
  // panel permanently without the sketch ever noticing.
  static unsigned long lastExio = 0;
  if (millis() - lastExio >= EXPANDER_CHECK_MS) {
    lastExio = millis();
    TCA9554_Verify();
  }

  displayWatchdog();
  Settings_Tick();   // debounced persist of range/screen to NVS
  Watchdog_Feed();
  delay(5);
}