// =============================================================================
//  MeteoPlaneRadar
//  Persisting settings to NVS (location, brightness, units).
//
//  Project: MeteoPlaneRadar - live aircraft radar on a round touchscreen
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
//           Ondra OK1CDJ / apps.ok1cdj.com
//  Web:     https://chiptron.cz
//  Board:   Waveshare ESP32-S3-Touch-LCD-2.1 (round 480x480 display, ST7701)
// =============================================================================
#include "Settings.h"
#include <Preferences.h>
#include <string.h>   // strncpy for the APRS callsign / API key

static Preferences prefs;
static const char* NS = "planeradar";

static double  s_lat = DEFAULT_LAT;
static double  s_lon = DEFAULT_LON;
static bool    s_hasLoc = false;
static uint8_t s_bl = 80;
static bool    s_metric = false;   // aviation units by default

// UI state (range per screen + active screen) with a debounced NVS write.
static uint8_t s_rngP = 1;              // plane range index (25 km default)
static uint8_t s_rngM = 1;              // meteo range index (50 km default)
static uint8_t s_rngA = 2;              // APRS range index (100 km default)
static uint8_t s_scr  = 0;              // active screen (0 = planes)
static uint16_t s_top = 0;              // bearing shown at the top (0 = north up)
static uint8_t s_autoMin = 0;           // auto screen switch: 0 = off, 1..9 minutes
static bool          s_uiDirty   = false;
static unsigned long s_uiDirtyAt = 0;

// APRS station identity + aprs.fi API key (written immediately, not debounced).
static char s_aprsCall[16] = "";
static char s_aprsKey[24]  = "";

void Settings_Begin() {
  if (prefs.begin(NS, true)) {
    s_lat    = prefs.getDouble("lat", DEFAULT_LAT);
    s_lon    = prefs.getDouble("lon", DEFAULT_LON);
    s_hasLoc = prefs.getBool("hasLoc", false);
    s_bl     = prefs.getUChar("bl", 80);
    s_metric = prefs.getBool("metric", false);
    s_rngP   = prefs.getUChar("rngP", 1);
    s_rngM   = prefs.getUChar("rngM", 1);
    s_rngA   = prefs.getUChar("rngA", 2);
    s_scr    = prefs.getUChar("scr", 0);
    s_top    = prefs.getUShort("topb", 0);   // new key: old "rot" meant something else
    s_autoMin = prefs.getUChar("autoMin", 0);
    prefs.getString("aprsCall", s_aprsCall, sizeof(s_aprsCall));
    prefs.getString("aprsKey",  s_aprsKey,  sizeof(s_aprsKey));
    prefs.end();
  }
}

double Settings_Lat() { return s_lat; }
double Settings_Lon() { return s_lon; }
bool   Settings_HasLocation() { return s_hasLoc; }

void Settings_SetLocation(double lat, double lon) {
  s_lat = lat; s_lon = lon; s_hasLoc = true;
  if (prefs.begin(NS, false)) {
    prefs.putDouble("lat", lat);
    prefs.putDouble("lon", lon);
    prefs.putBool("hasLoc", true);
    prefs.end();
  }
}

uint8_t Settings_Backlight() { return s_bl; }

// Brightness goes through the same debounce as the rest of the UI state.
// Dragging the slider across the screen used to write flash on every single
// touch sample - dozens of erase/write cycles for one adjustment.
void Settings_SetBacklight(uint8_t pct) {
  if (pct == s_bl) return;
  s_bl = pct;
  s_uiDirty = true; s_uiDirtyAt = millis();
}

bool Settings_MetricUnits() { return s_metric; }

void Settings_SetMetricUnits(bool metric) {
  s_metric = metric;
  if (prefs.begin(NS, false)) { prefs.putBool("metric", metric); prefs.end(); }
}

uint8_t Settings_PlaneRange() { return s_rngP; }
void    Settings_SetPlaneRange(uint8_t idx) {
  if (idx != s_rngP) { s_rngP = idx; s_uiDirty = true; s_uiDirtyAt = millis(); }
}
uint8_t Settings_MeteoRange() { return s_rngM; }
void    Settings_SetMeteoRange(uint8_t idx) {
  if (idx != s_rngM) { s_rngM = idx; s_uiDirty = true; s_uiDirtyAt = millis(); }
}
uint8_t Settings_AprsRange() { return s_rngA; }
void    Settings_SetAprsRange(uint8_t idx) {
  if (idx != s_rngA) { s_rngA = idx; s_uiDirty = true; s_uiDirtyAt = millis(); }
}

const char* Settings_AprsCall() { return s_aprsCall; }
void Settings_SetAprsCall(const char* call) {
  if (!call) return;
  strncpy(s_aprsCall, call, sizeof(s_aprsCall) - 1);
  s_aprsCall[sizeof(s_aprsCall) - 1] = '\0';
  if (prefs.begin(NS, false)) { prefs.putString("aprsCall", s_aprsCall); prefs.end(); }
}

const char* Settings_AprsKey() { return s_aprsKey; }
void Settings_SetAprsKey(const char* key) {
  if (!key) return;
  strncpy(s_aprsKey, key, sizeof(s_aprsKey) - 1);
  s_aprsKey[sizeof(s_aprsKey) - 1] = '\0';
  if (prefs.begin(NS, false)) { prefs.putString("aprsKey", s_aprsKey); prefs.end(); }
}
uint16_t Settings_TopBearing() { return s_top; }
void     Settings_SetTopBearing(uint16_t deg) {
  deg %= 360;
  if (deg != s_top) { s_top = deg; s_uiDirty = true; s_uiDirtyAt = millis(); }
}

uint8_t Settings_Screen() { return s_scr; }
void    Settings_SetScreen(uint8_t idx) {
  if (idx != s_scr) { s_scr = idx; s_uiDirty = true; s_uiDirtyAt = millis(); }
}

uint8_t Settings_AutoSwitchMin() { return s_autoMin; }
void    Settings_SetAutoSwitchMin(uint8_t m) {
  if (m > 9) m = 9;
  if (m != s_autoMin) { s_autoMin = m; s_uiDirty = true; s_uiDirtyAt = millis(); }
}

// Debounced flush: write only after the UI has been idle for a moment, so a
// burst of swipes (or a drag across the brightness slider) results in a single
// NVS write instead of one per step.
void Settings_Tick() {
  if (!s_uiDirty) return;
  if (millis() - s_uiDirtyAt < 2000) return;
  if (prefs.begin(NS, false)) {
    prefs.putUChar("rngP", s_rngP);
    prefs.putUChar("rngM", s_rngM);
    prefs.putUChar("rngA", s_rngA);
    prefs.putUChar("scr",  s_scr);
    prefs.putUShort("topb", s_top);
    prefs.putUChar("bl",   s_bl);
    prefs.putUChar("autoMin", s_autoMin);
    prefs.end();
  }
  s_uiDirty = false;
}

void Settings_ClearAll() {
  if (prefs.begin(NS, false)) { prefs.clear(); prefs.end(); }
  s_lat = DEFAULT_LAT; s_lon = DEFAULT_LON; s_hasLoc = false; s_bl = 80;
  s_metric = false;
  s_rngP = 1; s_rngM = 1; s_rngA = 2; s_scr = 0; s_top = 0; s_autoMin = 0;
  s_uiDirty = false;
  s_aprsCall[0] = '\0'; s_aprsKey[0] = '\0';
}
