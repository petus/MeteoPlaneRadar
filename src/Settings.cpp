// =============================================================================
//  MeteoPlaneRadar
//  Persisting settings to NVS (location, brightness, units).
//
//  Project: MeteoPlaneRadar - live aircraft radar on a round touchscreen
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
//  Web:     https://chiptron.cz
//  Board:   Waveshare ESP32-S3-Touch-LCD-2.1 (round 480x480 display, ST7701)
// =============================================================================
#include "Settings.h"
#include <Preferences.h>

static Preferences prefs;
static const char* NS = "planeradar";

static double  s_lat = DEFAULT_LAT;
static double  s_lon = DEFAULT_LON;
static bool    s_hasLoc = false;
static uint8_t s_bl = 80;
static bool    s_metric = false;   // aviation units by default

void Settings_Begin() {
  if (prefs.begin(NS, true)) {
    s_lat    = prefs.getDouble("lat", DEFAULT_LAT);
    s_lon    = prefs.getDouble("lon", DEFAULT_LON);
    s_hasLoc = prefs.getBool("hasLoc", false);
    s_bl     = prefs.getUChar("bl", 80);
    s_metric = prefs.getBool("metric", false);
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

void Settings_SetBacklight(uint8_t pct) {
  s_bl = pct;
  if (prefs.begin(NS, false)) { prefs.putUChar("bl", pct); prefs.end(); }
}

bool Settings_MetricUnits() { return s_metric; }

void Settings_SetMetricUnits(bool metric) {
  s_metric = metric;
  if (prefs.begin(NS, false)) { prefs.putBool("metric", metric); prefs.end(); }
}

void Settings_ClearAll() {
  if (prefs.begin(NS, false)) { prefs.clear(); prefs.end(); }
  s_lat = DEFAULT_LAT; s_lon = DEFAULT_LON; s_hasLoc = false; s_bl = 80;
  s_metric = false;
}
