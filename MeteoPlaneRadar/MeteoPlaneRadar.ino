// =============================================================================
//  MeteoPlaneRadar
//  Clock, live aircraft radar (adsb.fi), precipitation radar (CHMU or
//  RainViewer) and a weather forecast (Open-Meteo) on a round touchscreen.
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
//  Screens (cycled by a LONG press; each can be switched off in the web UI):
//    1) Clock       - time, date, current temperature, seconds ring
//    2) Aircraft    - adsb.fi, tap an aircraft for details and its route
//    3) Weather     - animated precipitation, CHMU or RainViewer
//    4) Forecast    - now/+3h/+6h, today/tomorrow/after, then the weekdays
//    5) Price       - spot electricity price as a 24-hour dial (OTE)
//    6) Generation  - what the Czech grid is running on right now (ENTSO-E)
//    7) Settings    - always reachable, and shows the web address
//
//  The two energy screens are OFF on a device that is updated rather than newly
//  set up - see the note above the screen indices in Config.h.
//
//  Configuration lives in a browser. The device serves its own settings page
//  permanently at http://meteoplaneradar.local/ once it is on the home network,
//  and a captive portal on its own access point before that. Firmware updates
//  are at /update on the same server.
//
//  Controls:
//    - swipe left/right             = change the range (radar screens)
//    - short tap                    = aircraft detail / day-night toggle
//    - long press LEFT half         = previous screen
//    - long press RIGHT half        = next screen
//    - hold BOOT at startup (~3 s)  = factory reset
//
//  Serial log: 115200 Bd over the connector marked "USB" (the ESP32-S3 native
//  USB). The other USB-C connector on the board will not show anything.
//
//  Libraries (Arduino IDE, ESP32 core 3.x):
//    - GFX Library for Arduino (moononournation) - drawing
//    - PNGdec (bitbank2)                          - CHMU frames, RainViewer tiles
//    - ArduinoJson (bblanchon, v7)                - all the JSON
//    - QRCode (ricmoo)                            - QR code (bundled)
//    - WebServer, DNSServer, ESPmDNS, Preferences, Wire, HTTPClient, Update,
//      esp_lcd                                    - part of the ESP32 core
//
//  Neither WiFiManager nor ElegantOTA is used any more - see the note at the
//  top of WiFiPortal.h and the /update section of WebConfig.cpp.
//
//  Data sources (attribution required, personal non-commercial use only):
//    - Aircraft:    adsb.fi, https://adsb.fi
//    - Route:       adsb.lol, https://adsb.lol
//                   (route data by https://github.com/vradarserver/standing-data)
//    - Precip. CZ:  CHMU, https://opendata.chmi.cz
//    - Precip. EU:  RainViewer, https://www.rainviewer.com
//    - Weather:     Open-Meteo, https://open-meteo.com
//    - Price:       OTE-CR via https://spotovaelektrina.cz
//    - Generation:  ENTSO-E via https://api.energy-charts.info (Fraunhofer ISE)
//    - Location:    ip-api.com
//    - Map:         Natural Earth (public domain), GeoNames (CC BY 4.0)
//    - Clock:       the "Date" header of the responses above. No NTP client.
//
//  Licence: MIT (see LICENSE.txt). Version: Version.h. History: CHANGELOG.md.
// =============================================================================

#include "AAFont.h"   // CZT(): accents with the smooth fonts
#include <Arduino_GFX_Library.h>
#include <Wire.h>
#include <time.h>
#include "esp_heap_caps.h"
#include "esp_system.h"

#include "TCA9554.h"
#include "Board.h"
#include "Display_ST7701.h"
#include "Canvas16.h"
#include "Touch.h"
#include "Settings.h"
#include "Version.h"
#include "Config.h"
#include "Lang.h"
#include "Layout.h"
#include "Status.h"
#include "UI.h"
#include "Net.h"
#include "WiFiPortal.h"
#include "WebConfig.h"
#include "GeoIP.h"
#include "ADSB.h"
#include "ScreenPlanes.h"
#include "CHMU.h"
#include "RainViewer.h"
#include "ScreenWeather.h"
#include "ScreenClock.h"
#include "ScreenForecast.h"
#include "ScreenPrice.h"
#include "ScreenMix.h"
#include "Cards.h"
#include "ScreenCards.h"
#include "ScreenSettings.h"
#include "Forecast.h"
#include "Energy.h"
#include "NightMode.h"
#include "TimeZone.h"
#include "Watchdog.h"
#include "Outside.h"
#include "Route.h"
#include "StormWatch.h"
#include "PlaneWatch.h"
#include "ADSB.h"

// gfx = single off-screen canvas in PSRAM. Everything is drawn here, then
// flush() pushes the whole frame to the panel in one shot -> no flicker.
Arduino_GFX* gfx = nullptr;

// =============================================================================
//  Touch: capture and dispatch, deliberately separated
//
//  A fetch blocks for seconds inside activeTick(), and the touch used to be
//  read only at the top of loop() - so a tap during a download was missed, or
//  sampled so late it was mis-read. The handling is therefore split:
//
//    CAPTURE (touchPump) reads the controller and advances the gesture. It
//    draws nothing and makes no network call, so it is safe to run from
//    anywhere - including netPoll(), which the fetchers call throughout a
//    transfer. The gesture is recognised when it is made.
//
//    DISPATCH (dispatchTouch) acts on it, and only from loop(). Redrawing from
//    inside a download would re-enter the drawing code mid-frame.
//
//  The action still waits for the fetch to end, but fires the instant it does.
// =============================================================================
enum PendKind : uint8_t { PEND_NONE = 0, PEND_SWIPE, PEND_LONG, PEND_TAP };
static PendKind s_pendKind = PEND_NONE;
static int      s_pendA = 0, s_pendB = 0;

static unsigned long s_touchPauseUntil = 0;   // auto-cycling paused until this

// How long a deliberate gesture holds the cycling off. Scaled to the cycling
// interval and clamped at both ends - see the note in Config.h. Defined up here
// because dispatchTouch() below needs it.
static unsigned long autoRotatePauseMs() {
  const uint16_t secs = Settings_AutoRotateSec();
  if (secs == 0) return 0;
  unsigned long p = (unsigned long)secs * 1000UL * AUTO_ROTATE_PAUSE_FACTOR;
  if (p < AUTO_ROTATE_PAUSE_MIN_MS) p = AUTO_ROTATE_PAUSE_MIN_MS;
  if (p > AUTO_ROTATE_PAUSE_MAX_MS) p = AUTO_ROTATE_PAUSE_MAX_MS;
  return p;
}

// Gesture state. File scope rather than static locals in loop(), because
// touchPump() now runs from two places.
static bool          s_touching = false;
static int           s_startX = 0, s_startY = 0;
static int           s_lastX = 0, s_lastY = 0;
static unsigned long s_startMs = 0;
static unsigned long s_lastSeenMs = 0;
static unsigned long s_lastPump = 0;

// Read the touch and advance the gesture. Safe to call from inside a transfer.
//
// The gesture does NOT end on the first empty sample. Both controllers skip one
// now and then, and Touch_Read() also throws away corrupted ones - an empty
// sample mid-drag is normal, not a lifted finger. Ending there would split one
// swipe into several taps on the map.
static void touchPump() {
  // Throttle. Touch_Read() is cheap when nothing is happening, but with a
  // finger down it does an I2C transfer every call, and netPoll() can run
  // thousands of times a second inside a download. Every 5 ms is far finer than
  // the 60 ms of silence a release needs.
  unsigned long now = millis();
  if (now - s_lastPump < 5) return;
  s_lastPump = now;

  TouchData t;
  Touch_Read(&t);

  if (t.points > 0) {
    if (!s_touching) {
      s_touching = true;
      s_startX = t.x; s_startY = t.y; s_startMs = now;
    }
    s_lastX = t.x; s_lastY = t.y;
    s_lastSeenMs = now;
    // Deliberately NOT pausing the cycling here. Merely putting a finger on the
    // glass - to pick an aircraft - is not a request to stop; only the gestures
    // that mean "I am driving this myself" are, and those are handled in
    // dispatchTouch() once the gesture is actually recognised.
    return;
  }

  if (!s_touching || now - s_lastSeenMs < TOUCH_RELEASE_MS) return;

  s_touching = false;
  const int dx = s_lastX - s_startX;
  const int dy = s_lastY - s_startY;
  const unsigned long dur = s_lastSeenMs - s_startMs;
  const bool smallMove = (abs(dx) < 60 && abs(dy) < 60);

#if TOUCH_DEBUG
  Serial.printf("TOUCH: start=(%d,%d) konec=(%d,%d) dx=%d dy=%d %lums -> %s\n",
                s_startX, s_startY, s_lastX, s_lastY, dx, dy, (unsigned long)dur,
                (abs(dx) >= 70 && abs(dy) <= 90 && dur <= 700) ? "swipe"
                  : (smallMove && dur >= 500) ? "dlouhy stisk"
                  : (smallMove) ? "klepnuti" : "ignorovano");
#endif

  // The most recent gesture wins. If one is still queued behind a long download
  // the user has changed their mind since, and acting on the stale one would be
  // the wrong answer arriving late.
  if (abs(dx) >= 70 && abs(dy) <= 90 && dur <= 700) {
    s_pendKind = PEND_SWIPE;
    s_pendA = (dx < 0) ? +1 : -1;
  } else if (smallMove && dur >= 500) {
    s_pendKind = PEND_LONG;
    s_pendA = s_lastX;
  } else if (smallMove) {
    s_pendKind = PEND_TAP;
    s_pendA = s_lastX; s_pendB = s_lastY;
  }
}

// yield() during long network transfers, feed the watchdog so multi-second
// downloads never trip it, and keep sampling the touch so a gesture made during
// a download is still the gesture that gets acted on.
static void netPoll() { yield(); Watchdog_Feed(); touchPump(); }

static void checkBootReset() {
  pinMode(BOOT_PIN, INPUT_PULLUP);
  if (digitalRead(BOOT_PIN) != LOW) return;
  gfx->fillScreen(C_BLACK);
  UI_TextCentered(CZT("Drzte pro reset...", "Držte pro reset..."), LCD_HEIGHT / 2, C_WHITE, 2);
  gfx->flush();
  unsigned long start = millis();
  while (digitalRead(BOOT_PIN) == LOW) {
    if (millis() - start >= 3000) {
      UI_TextCentered(CZT("Mazu nastaveni", "Mažu nastavení"), LCD_HEIGHT / 2 + 30, C_RED, 2);
      gfx->flush();
      Settings_ClearAll();
      delay(800);
      ESP.restart();
    }
    delay(20);
  }
}

// =============================================================================
//  Screen manager
//
//  Five screens, four of which can be switched off. "Off" means genuinely
//  invisible: the long press skips over it, the automatic cycling skips it, and
//  it has no dot at the top - so the dots always match what a long press will
//  actually do. Settings can never be switched off, or a device with everything
//  else disabled would have no way back to the web UI.
// =============================================================================
static int s_screen = SCREEN_PLANES_I;

// --- Aircraft-nearby alert state (0.8.0) ---
// While it runs, the aircraft screen is on display for a few seconds with the
// triggering aircraft's detail open; then the screen it interrupted comes back.
static bool          s_alertOn = false;
static unsigned long s_alertUntil = 0;
static int           s_alertReturn = -1;

// What is actually on the display, for the web status page and Home Assistant.
// Settings_Screen() only follows the user's own choices (it is what is restored
// at boot), not the automatic cycling or an alert.
int  MPR_CurrentScreen() { return s_screen; }
bool MPR_AlertActive()   { return s_alertOn; }

// Reachable at all: has a dot, a long press stops on it. 0.8.0 adds two ways
// for a screen that is switched off to turn up anyway - the weather radar
// while a storm is near (when the user asked for that), and the aircraft
// screen for the length of an alert.
static bool screenVisible(int i) {
  if (Settings_ScreenEnabled((uint8_t)i)) return true;
  if (i == SCREEN_METEO_I && Settings_StormOnly() && StormWatch_Active()) return true;
  if (i == SCREEN_PLANES_I && s_alertOn) return true;
  return false;
}

// Part of the AUTOMATIC cycling. Narrower than visible: with "radar only when
// a storm is coming" the weather screen is skipped while the sky is clear even
// though a long press still reaches it, and with the aircraft alert on, the
// aircraft screen only ever appears through the alert.
static bool inRotation(int i) {
  if (i == SCREEN_SETTINGS_I) return false;
  if (i == SCREEN_METEO_I && Settings_StormOnly()) return StormWatch_Active();
  if (i == SCREEN_PLANES_I && Settings_PlaneAlert()) return false;
  return Settings_ScreenEnabled((uint8_t)i);
}

// The visible screens in order, used for the dots and for stepping.
static int visibleList(int* out) {
  int n = 0;
  for (int i = 0; i < SCREEN_N; i++) if (screenVisible(i)) out[n++] = i;
  return n;
}

// One dot per place in the stepping. Every screen has one place except the
// cards screen, which has one per installed card - to the user each card is a
// screen of its own (ScreenCards.h). The row can be switched off in the web UI
// and is off by default: with several cards it is long and tells little.
static void drawScreenDots() {
  if (!Settings_ScreenDots()) return;
  int vis[SCREEN_N];
  const int nVis = visibleList(vis);
  int n = 0;
  for (int i = 0; i < nVis; i++) n += (vis[i] == SCREEN_CARDS_I) ? ScreenCards_Count() : 1;
  if (n <= 1) return;                    // one screen - a single dot says nothing

  // The row sits near the rim, where the round panel is narrow: about 160 px
  // across. Eight dots fit at the usual spacing; more are drawn closer and
  // smaller rather than run off the glass.
  const int room = 160;
  int gap = 20, r = 4;
  if ((n - 1) * gap > room) { gap = room / (n - 1); r = (gap >= 9) ? 3 : 2; }
  const int cx = LCD_WIDTH / 2;
  const int y = LY_DOTS;
  const int startX = cx - (n - 1) * gap / 2;
  int k = 0;
  for (int i = 0; i < nVis; i++) {
    const bool cards = (vis[i] == SCREEN_CARDS_I);
    const int places = cards ? ScreenCards_Count() : 1;
    for (int j = 0; j < places; j++, k++) {
      const int x = startX + k * gap;
      const bool on = (vis[i] == s_screen) && (!cards || j == ScreenCards_Index());
      if (on) gfx->fillCircle(x, y, r, C_WHITE);
      else    gfx->drawCircle(x, y, r, C_GRAY);
    }
  }
}

static void drawActive() {
  switch (s_screen) {
    case SCREEN_CLOCK_I:    ScreenClock_Draw();    break;
    case SCREEN_PLANES_I:   ScreenPlanes_Draw();   break;
    case SCREEN_METEO_I:    ScreenWeather_Draw();  break;
    case SCREEN_FORECAST_I: ScreenForecast_Draw(); break;
    case SCREEN_PRICE_I:    ScreenPrice_Draw();    break;
    case SCREEN_MIX_I:      ScreenMix_Draw();      break;
    case SCREEN_CARDS_I:    ScreenCards_Draw();    break;
    case SCREEN_SETTINGS_I: ScreenSettings_Draw(); break;
  }
  drawScreenDots();
  gfx->flush();    // hand the framebuffer over; Canvas16 switches to the other
}

static void enterActive() {
  switch (s_screen) {
    case SCREEN_CLOCK_I:    ScreenClock_Enter();    break;
    case SCREEN_PLANES_I:   ScreenPlanes_Enter();   break;
    case SCREEN_METEO_I:    ScreenWeather_Enter();  break;
    case SCREEN_FORECAST_I: ScreenForecast_Enter(); break;
    case SCREEN_PRICE_I:    ScreenPrice_Enter();    break;
    case SCREEN_MIX_I:      ScreenMix_Enter();      break;
    case SCREEN_CARDS_I:    ScreenCards_Enter();    break;
    case SCREEN_SETTINGS_I: ScreenSettings_Enter(); break;
  }
  drawActive();
}

// dir -1 = previous, +1 = next. Walks over disabled screens; the guard stops it
// looping forever if somehow nothing is enabled at all.
// On the cards screen a step goes to the neighbouring card first; only past
// the last (or the first) one does it leave for another screen. That is what
// makes every card a screen of its own in the stepping and in the cycling.
static bool stepWithinCards(int dir) {
  if (s_screen != SCREEN_CARDS_I || !ScreenCards_Step(dir)) return false;
  Serial.printf("Screen: %d, karta %d\n", s_screen, ScreenCards_Index() + 1);
  drawActive();
  return true;
}

static void switchScreen(int dir) {
  if (stepWithinCards(dir)) return;
  int next = s_screen;
  for (int guard = 0; guard < SCREEN_N; guard++) {
    next = (next + dir + SCREEN_N) % SCREEN_N;
    if (screenVisible(next)) break;
  }
  if (next == s_screen) {
    // The cards screen is the only one there is: go round its cards.
    if (s_screen == SCREEN_CARDS_I && ScreenCards_Count() > 1) {
      ScreenCards_Select(dir > 0 ? 0 : -1);
      drawActive();
    }
    return;
  }
  // Coming from the screen before, start at the first card; coming back from
  // the screen after, at the last.
  if (next == SCREEN_CARDS_I) ScreenCards_Select(dir > 0 ? 0 : -1);
  s_screen = next;
  Settings_SetScreen((uint8_t)s_screen);
  Serial.printf("Screen: %d\n", s_screen);
  enterActive();
}

// Jump straight to a screen. Used by the web remote control; a disabled screen
// is refused, which the handler has already checked.
static void gotoScreen(int idx) {
  if (idx < 0 || idx >= SCREEN_N) return;
  if (!screenVisible(idx)) return;
  if (idx == s_screen) return;
  s_screen = idx;
  Settings_SetScreen((uint8_t)s_screen);
  Serial.printf("Screen: %d (web)\n", s_screen);
  enterActive();
}

static bool activeTick() {
  switch (s_screen) {
    case SCREEN_CLOCK_I:    return ScreenClock_Tick();
    case SCREEN_PLANES_I:   return ScreenPlanes_Tick();
    case SCREEN_METEO_I:    return ScreenWeather_Tick();
    case SCREEN_FORECAST_I: return ScreenForecast_Tick();
    case SCREEN_PRICE_I:    return ScreenPrice_Tick();
    case SCREEN_MIX_I:      return ScreenMix_Tick();
    case SCREEN_CARDS_I:    return ScreenCards_Tick();
    case SCREEN_SETTINGS_I: return ScreenSettings_Tick();
  }
  return false;
}

static void activeChangeRange(int dir) {
  switch (s_screen) {
    case SCREEN_PLANES_I: ScreenPlanes_ChangeRange(dir);  break;
    case SCREEN_METEO_I:  ScreenWeather_ChangeRange(dir); break;
    // The price screen has no range in kilometres, but a swipe there means the
    // same thing it means everywhere else - show me the other one - so it is
    // wired to the same gesture rather than inventing a second one.
    case SCREEN_PRICE_I:  ScreenPrice_ChangeRange(dir);   break;
    // And on the cards screen it steps to the next installed card.
    case SCREEN_CARDS_I:  ScreenCards_ChangeRange(dir);   break;
    default: break;   // the other screens have no range
  }
}

static bool activeTap(int x, int y) {
  switch (s_screen) {
    case SCREEN_CLOCK_I:    return ScreenClock_HandleTap(x, y);
    case SCREEN_PLANES_I:   return ScreenPlanes_HandleTap(x, y);
    case SCREEN_PRICE_I:    return ScreenPrice_HandleTap(x, y);
    case SCREEN_SETTINGS_I: return ScreenSettings_HandleTap(x, y);
    default: return false;
  }
}

// Is a modal (the aircraft detail) open? Then swipe/long-press are captured to
// close it rather than change range / switch screen.
static bool activeModalOpen() {
  return (s_screen == SCREEN_PLANES_I) && ScreenPlanes_DetailOpen();
}

// The user touched the glass (or the browser) during an aircraft alert: they
// are driving now. The alert stops counting down and nothing is switched back
// behind their back.
static void alertTakenOver() {
  if (!s_alertOn) return;
  s_alertOn = false;
  ScreenPlanes_AlertEnd();
}

// Act on a gesture captured earlier (possibly during a download). Called only
// from loop(), where nothing else is drawing.
static void dispatchTouch() {
  if (s_pendKind == PEND_NONE) return;
  const PendKind kind = s_pendKind;
  const int a = s_pendA, b = s_pendB;
  s_pendKind = PEND_NONE;

  if (s_alertOn) {
    alertTakenOver();
    // A tap during an alert only closes the panel the alert opened - it must
    // not go on to select whatever aircraft happened to be under the finger.
    if (kind == PEND_TAP) { drawActive(); return; }
  }

  switch (kind) {
    case PEND_SWIPE:
      // With the detail open a swipe just closes it, so you cannot accidentally
      // re-scale the map behind the panel.
      if (activeModalOpen()) { ScreenPlanes_CloseDetail(); drawActive(); }
      else                   { activeChangeRange(a); drawActive(); }
      s_touchPauseUntil = millis() + autoRotatePauseMs();
      break;
    case PEND_LONG:
      if (activeModalOpen()) { ScreenPlanes_CloseDetail(); drawActive(); }
      else switchScreen(a < LCD_WIDTH / 2 ? -1 : +1);
      s_touchPauseUntil = millis() + autoRotatePauseMs();
      break;
    case PEND_TAP:
      // A tap does not pause the cycling - see touchPump().
      if (activeTap(a, b)) drawActive();
      break;
    default:
      break;
  }
}

// --- Automatic cycling ------------------------------------------------------
// Paused by the gestures that mean the user is driving - a swipe or a long
// press - and held while an aircraft detail is open. A plain tap does not stop
// it; being switched away mid-sentence is annoying, but so is a device that
// stops cycling because someone brushed the glass.
static unsigned long s_lastRotate = 0;

static void autoRotateTick() {
  const uint16_t secs = Settings_AutoRotateSec();
  if (secs == 0) return;
  if (s_screen == SCREEN_SETTINGS_I) return;      // never cycle away from settings

  unsigned long now = millis();

  // The alert runs its own clock and hands back to the cycling when done.
  if (s_alertOn) { s_lastRotate = now; return; }

  // An open aircraft detail holds the cycling: the user is reading it. The
  // timer is kept fresh rather than stopped, so when the panel closes - by a
  // tap, or because the aircraft dropped out of the data - cycling resumes with
  // a full interval instead of switching away immediately.
  //
  // 0.8.4: unless the data behind the panel has gone stale. Then nobody is
  // reading anything live, and the cycling carries on - the panel is closed
  // so the screen does not come back to frozen numbers next time round.
  if (activeModalOpen()) {
    if (!ADSB_DataStale()) { s_lastRotate = now; return; }
    if (now - s_lastRotate >= (unsigned long)secs * 1000UL) ScreenPlanes_CloseDetail();
  }

  if (now < s_touchPauseUntil) return;
  if (now - s_lastRotate < (unsigned long)secs * 1000UL) return;
  s_lastRotate = now;

  // Every card gets the same interval as any other screen: on the cards
  // screen the next stop is the next card, until they have all been shown.
  if (stepWithinCards(+1)) return;

  // Step to the next screen that takes part in the cycling.
  int next = s_screen;
  bool found = false;
  for (int guard = 0; guard < SCREEN_N; guard++) {
    next = (next + 1) % SCREEN_N;
    if (inRotation(next)) { found = true; break; }
  }
  if (!found || next == s_screen) {
    // Nothing else is in the cycling: go round the cards.
    if (s_screen == SCREEN_CARDS_I && ScreenCards_Count() > 1) {
      ScreenCards_Select(0);
      drawActive();
    }
    return;
  }
  if (next == SCREEN_CARDS_I) ScreenCards_Select(0);
  s_screen = next;
  enterActive();
}

// --- Aircraft-nearby alert (0.8.0) ---------------------------------------------
static void endAlert() {
  ScreenPlanes_AlertEnd();
  s_alertOn = false;
  if (s_screen == SCREEN_PLANES_I) {
    int r = s_alertReturn;
    if (r < 0 || r >= SCREEN_N || !screenVisible(r) || r == SCREEN_PLANES_I) {
      // The screen we came from is gone (switched off meanwhile) - take the
      // first one that is still there.
      r = SCREEN_SETTINGS_I;
      for (int i = 0; i < SCREEN_N; i++) if (screenVisible(i)) { r = i; break; }
    }
    s_screen = r;
    enterActive();
  }
  // A full interval on the screen we returned to, not whatever was left of it.
  s_lastRotate = millis();
}

static void planeAlertTick() {
  if (!Settings_PlaneAlert()) {
    if (s_alertOn) endAlert();
    return;
  }
  const unsigned long now = millis();
  if (s_alertOn) {
    if ((long)(now - s_alertUntil) >= 0) endAlert();
    return;
  }
  // Never interrupt the settings screen, the aircraft screen itself, an open
  // detail, or someone who has just been swiping around.
  if (s_screen == SCREEN_PLANES_I || s_screen == SCREEN_SETTINGS_I) return;
  if (activeModalOpen()) return;
  if (now < s_touchPauseUntil) return;

  char hex[8];
  float km = 0;
  bool win = false;
  if (!PlaneWatch_Take(hex, sizeof(hex), &km, &win)) return;

  Serial.printf("Letadlo %s: %s, %.1f km\n", hex, win ? "v okne" : "nablizku", km);
  s_alertReturn = s_screen;
  s_alertOn = true;
  s_alertUntil = now + (unsigned long)Settings_PlaneAlertSec() * 1000UL;
  s_screen = SCREEN_PLANES_I;
  ScreenPlanes_Enter();
  ScreenPlanes_AlertBegin(hex, km, win);
  drawActive();
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

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.printf("\n=== MeteoPlaneRadar v%s ===\n", FW_VERSION);
  Serial.printf("Duvod restartu: %s\n", resetReasonText());
  Serial.printf("Volna pamet: %u B\n", (unsigned)ESP.getFreeHeap());

  Settings_Begin();          // also sets the interface language
  // Before the panel is started: the first boot after this feature arrived
  // formats the (so far unused) filesystem partition, and a flash write is
  // best done while there is no frame on the glass to disturb.
  Cards_Begin();
  CHMU_SetMasked(Settings_RadarSource() == RADAR_SRC_CHMU_MASKED);
  TimeZone_Begin();      // compiled-in default; the forecast may correct it
  Layout_SelfTest();         // no-op unless LAYOUT_DEBUG is on

  Wire.begin(I2C_SDA, I2C_SCL, 400000);
  delay(50);
  TCA9554_Init();
  TCA9554_SetPin(EXIO_LCD_PWR, false);
  delay(10);

  // Which of the two boards is this? Must happen here: it decides the ST7701
  // register sequence and the vertical timing, so it has to be known before
  // ST7701_Init(), and it is answered over I2C, which is up by now.
  Board_Detect();

  Backlight_Init();
  // Backlight stays off for now. The panel powers up with random memory
  // content, so lighting it before the first frame shows a white flash.
  if (!ST7701_Init()) {
    Serial.println("Displej se nepodarilo inicializovat, restartuji za 5 s");
    delay(5000);
    ESP.restart();
  }

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
  NightMode_Apply();          // now there is something to look at

  if (!Touch_Init()) {
    // Not fatal - the screens keep running, they just cannot be operated. Worth
    // knowing about, because from the outside it looks like a frozen board.
    Serial.println("VAROVANI: dotyk se neinicializoval, ovladani nebude fungovat");
  }

  checkBootReset();

  ADSB_SetPollFn(netPoll);
  CHMU_SetPollFn(netPoll);
  Net_SetPollFn(netPoll);
  RainViewer_SetPollFn(netPoll);
  Route_SetPollFn(netPoll);

  // Connects with the stored credentials, or brings up the access point and
  // leaves it up. Either way the web server is running when this returns.
  WiFi_Begin();

  if (WiFi_IsConnected()) {
    GeoIP_DetectIfNeeded();   // fill in the location by IP if none is stored
  }

  s_screen = Settings_Screen();
  if (s_screen >= SCREEN_N || !screenVisible(s_screen)) {
    // The stored screen was switched off since the last run - start on the
    // first one that is actually enabled.
    s_screen = SCREEN_SETTINGS_I;
    for (int i = 0; i < SCREEN_N; i++) if (screenVisible(i)) { s_screen = i; break; }
  }
  // In access-point mode the QR screen owns the display and must stay up until a
  // network is entered. Drawing a data screen here would paint straight over it
  // - which is what happened on a first run and after a factory reset. The
  // screen is still chosen above, so it is ready the moment WiFi comes up.
  if (!WiFi_IsAP()) enterActive();

  Watchdog_Begin();   // hardware watchdog for 24/7 operation
  Serial.println("Setup done");
}

// --- Display watchdog -------------------------------------------------------
// The task watchdog cannot see this failure: the sketch is fine, it is the RGB
// peripheral that stops scanning (black screen, backlight still on, only a
// power cycle helps). So watch the frame counter fed by the VSYNC interrupt.
static void displayWatchdog() {
#if DISPLAY_WD
  static uint32_t lastCount = 0;
  static unsigned long lastMove = 0;
  static bool repaired = false;
  static bool armed = false;

  uint32_t now = LCD_VsyncCount();
  unsigned long ms = millis();
  if (now != lastCount) {           // panel is scanning - all good
    lastCount = now;
    lastMove = ms;
    repaired = false;
    // Arm only once the counter has demonstrably been moving. If a future board
    // never delivered the VSYNC event at all, an always-armed watchdog would
    // reboot a perfectly healthy device every few seconds.
    if (!armed && now > 100) armed = true;
    return;
  }
  if (!armed) return;
  if (lastMove == 0) { lastMove = ms; return; }

  unsigned long stalled = ms - lastMove;
  if (!repaired && stalled >= DISPLAY_WD_DEAD_MS) {
    repaired = true;
    Serial.printf("DISPLEJ: zadny snimek %lu ms, zkousim opravu\n", stalled);
    TCA9554_Verify();
    TCA9554_SetPin(EXIO_LCD_PWR, false);
    TCA9554_SetPin(EXIO_LCD_RST, true);
    Set_Backlight(Settings_Backlight());
  } else if (stalled >= DISPLAY_WD_REBOOT_MS) {
    Serial.printf("DISPLEJ: panel neobnoven po %lu ms, restartuji\n", stalled);
    Serial.flush();
    ESP.restart();
  }
#endif
}

void loop() {
  // A firmware upload owns the machine: writing flash suspends the PSRAM cache
  // from under the RGB DMA, so nothing may draw, and every spare cycle should
  // go to the transfer. The watchdog is fed by the OTA progress callback.
  if (WebConfig_UpdateBusy()) {
    WebConfig_Loop();
    delay(1);
    return;
  }

  // Touch: sample and act. During a download the sampling also happens inside
  // netPoll(), so by the time we get back here the gesture is already waiting.
  touchPump();

  // --- Access point: the portal owns the display ----------------------------
  // Until a network is entered there is no data to put on the screens anyway,
  // and the QR code is the only thing standing between the user and a working
  // device. So nothing below this block may draw, and no gesture may switch
  // away from it. The web server and the WiFi state machine keep running.
  static bool s_apOwnsScreen = false;
  static uint8_t s_apLang = 0xFF;
  if (WiFi_IsAP()) {
    s_pendKind = PEND_NONE;              // a stray swipe must not take the QR away
    if (!s_apOwnsScreen || s_apLang != (uint8_t)Lang_Get()) {
      // Redrawn on a language change too: the portal has its own selector, and
      // the instructions under the QR code should follow it.
      s_apLang = (uint8_t)Lang_Get();
      s_apOwnsScreen = true;
      WiFi_DrawApScreen();
    }
    WiFi_Loop();                         // may accept credentials and leave AP mode
    WebConfig_Loop();
    if (WebConfig_WantsRestart()) {
      Serial.println("Nastaveni zmeneno, restartuji");
      Serial.flush();
      delay(400);
      ESP.restart();
    }
    // No Outside_Tick/Forecast_Tick here - there is no route to the internet and
    // every attempt would just burn the loop on timeouts.
    displayWatchdog();
    NightMode_Tick();
    Settings_Tick();
    Watchdog_Feed();
    delay(5);
    return;
  }
  if (s_apOwnsScreen) {
    // A network was accepted: take the display back and start the cycling clock
    // from now, so the first switch comes a full interval after setup ends.
    s_apOwnsScreen = false;
    s_apLang = 0xFF;
    s_lastRotate = millis();
    s_touchPauseUntil = 0;
    s_pendKind = PEND_NONE;
    enterActive();
  }

  dispatchTouch();

  // The same actions, asked for from the web instead of the glass. They are
  // carried out here rather than in the request handler for the same reason a
  // gesture is - drawing must not happen from anywhere else.
  {
    const int scr = WebConfig_TakeScreen();
    if (scr >= 0) {
      alertTakenOver();
      if (activeModalOpen()) ScreenPlanes_CloseDetail();
      gotoScreen(scr);
      s_touchPauseUntil = millis() + autoRotatePauseMs();
    }
    const int step = WebConfig_TakeScreenStep();
    if (step) {
      alertTakenOver();
      if (activeModalOpen()) ScreenPlanes_CloseDetail();
      switchScreen(step);
      s_touchPauseUntil = millis() + autoRotatePauseMs();
    }
    const int rng = WebConfig_TakeRangeStep();
    if (rng) {
      alertTakenOver();
      activeChangeRange(rng);
      drawActive();
      s_touchPauseUntil = millis() + autoRotatePauseMs();
    }
  }

  WiFi_Loop();
  WebConfig_Loop();

  // The user asked to forget the network from the settings screen.
  if (ScreenSettings_WantsWifiReset()) {
    ScreenSettings_ClearWifiReset();
    WiFi_Reset();               // drops to the access point and redraws
    return;
  }

  // Some settings only take effect from a clean start (which screens exist,
  // which radar source, where we are). The web handler asks for it; doing it
  // here means the browser has already had its reply.
  if (WebConfig_WantsRestart()) {
    Serial.println("Nastaveni zmeneno, restartuji");
    Serial.flush();
    delay(400);
    ESP.restart();
  }

  // Redrawing is decoupled from reading the touch and capped at ~12 FPS.
  static unsigned long lastDraw = 0;
  bool wantDraw = activeTick();
  // The dots switch in the web UI takes effect at once, not at the next redraw
  // a quiet screen (a card that refreshes hourly) would get round to.
  static bool dotsShown = Settings_ScreenDots();
  if (dotsShown != Settings_ScreenDots()) { dotsShown = Settings_ScreenDots(); wantDraw = true; }
  if (wantDraw && millis() - lastDraw >= 80) {
    drawActive();
    lastDraw = millis();
  }

  autoRotateTick();
  planeAlertTick();

  // Cheap insurance for the display: one I2C read every few seconds that checks
  // the expander still holds LCD power and reset where we left them.
  static unsigned long lastExio = 0;
  if (millis() - lastExio >= EXPANDER_CHECK_MS) {
    lastExio = millis();
    TCA9554_Verify();
  }

  displayWatchdog();
  Outside_Tick();     // outside temperature and the clock safety net
  Forecast_Tick();    // forecast, sun times and air quality
  Energy_Tick();      // spot price and generation mix (only if those screens are on)
  Cards_Tick();       // user-installed JSON cards (only if that screen is on)
  StormWatch_Tick();  // newest radar frame near us (only when something wants it)
  PlaneWatch_Tick(s_screen == SCREEN_PLANES_I);   // small poll for the alert / HA
  NightMode_Tick();   // day/night brightness
  Route_Tick();       // pending "where is it flying from/to" lookup
  Settings_Tick();    // debounced persist of UI state to NVS
  Watchdog_Feed();
  delay(5);
}
