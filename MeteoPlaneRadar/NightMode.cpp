// =============================================================================
//  MeteoPlaneRadar
//  Day / night brightness and the display switched off at night. See NightMode.h.
//
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
// =============================================================================
#include "NightMode.h"
#include "Settings.h"
#include "Forecast.h"
#include "Outside.h"
#include "Display_ST7701.h"
#include "Config.h"
#include <time.h>

static bool s_applied = false;
static bool s_lastState = false;

// 0.9.2
static bool          s_inDark = false;      // inside the dark window
static unsigned long s_wakeUntil = 0;       // lit by a touch until (0 = not)
static bool          s_alert = false;       // an alert has the screen
static bool          s_boost = false;       // print alert at full brightness
static bool          s_off = false;         // what the backlight shows now

// Inside a window of minutes of the day; one that crosses midnight works too.
static bool inWindow(int m, int from, int to) {
  if (from == to) return false;
  if (from < to) return m >= from && m < to;
  return m >= from || m < to;
}

static bool woken() {
  return s_wakeUntil && (long)(millis() - s_wakeUntil) < 0;
}

static bool wantOff() {
  if (!s_inDark) return false;
  if (woken()) return false;
  if (s_alert && Settings_DarkAlerts()) return false;
  return true;
}

void NightMode_Apply() {
  s_off = wantOff();
  if (s_off)        Set_Backlight(0);
  else if (s_boost) Set_Backlight(Settings_BrightDay());
  else              Set_Backlight(Settings_Backlight());
  s_applied = true;
}

void NightMode_Toggle() {
  if (Settings_NightAuto()) return;
  Settings_SetNight(!Settings_IsNight());
  NightMode_Apply();
}

bool NightMode_ScreenOff()    { return s_off; }
bool NightMode_InDarkWindow() { return s_inDark; }

void NightMode_Wake(uint32_t ms) {
  if (!s_inDark) return;
  s_wakeUntil = millis() + ms;
  if (s_wakeUntil == 0) s_wakeUntil = 1;
  if (s_off) NightMode_Apply();
}

void NightMode_SetAlert(bool on) {
  if (on == s_alert) return;
  s_alert = on;
  if (s_off != wantOff()) NightMode_Apply();
}

void NightMode_SetBoost(bool on) {
  if (on == s_boost) return;
  s_boost = on;
  NightMode_Apply();
}

// Is it night by the sun? false + *known=false when the sun times are not in yet.
static bool nightBySun(time_t now, bool* known) {
  time_t rise = 0, set = 0;
  if (!Forecast_SunTimes(&rise, &set)) { *known = false; return false; }
  *known = true;
  const long off = (long)Settings_NightOffsetMin() * 60L;
  const time_t nightEnds   = rise + off;     // dawn, shifted later by the offset
  const time_t nightStarts = set  - off;     // dusk, shifted earlier
  // Note on the day boundary: the sun times we hold are for the day of the last
  // fetch. Just after midnight they are yesterday's, so both events are already
  // in the past and the test still says "night" - which is correct. The next
  // refresh (half hourly) replaces them with today's.
  return (now < nightEnds) || (now >= nightStarts);
}

void NightMode_Tick() {
  // Push the initial brightness once, even before anything else is known.
  if (!s_applied) NightMode_Apply();
  if (!Outside_TimeValid()) return;          // no clock yet - nothing to compare

  const time_t now = time(nullptr);
  struct tm lt; localtime_r(&now, &lt);
  const int mins = lt.tm_hour * 60 + lt.tm_min;

  // --- Day / night level ---
  if (Settings_NightAuto()) {
    bool known = true, night;
    if (Settings_NightSource() == NIGHT_SRC_CLOCK)
      night = inWindow(mins, Settings_NightFromMin(), Settings_NightToMin());
    else
      night = nightBySun(now, &known);
    if (known && (night != s_lastState || night != Settings_IsNight())) {
      s_lastState = night;
      Settings_SetNight(night);
      NightMode_Apply();
      Serial.printf("Rezim: %s (jas %u%%)\n", night ? "nocni" : "denni",
                    (unsigned)Settings_Backlight());
    }
  }

  // --- Display off ---
  const bool dark = Settings_DarkOn() &&
                    inWindow(mins, Settings_DarkFromMin(), Settings_DarkToMin());
  if (dark != s_inDark) {
    s_inDark = dark;
    s_wakeUntil = 0;
    Serial.printf("Displej: %s\n", dark ? "vypnuto na noc" : "zapnuto");
  }
  if (s_wakeUntil && !woken()) s_wakeUntil = 0;
  if (s_off != wantOff()) NightMode_Apply();
}
