// =============================================================================
//  MeteoPlaneRadar
//  Aircraft-nearby watch - see PlaneWatch.h.
//
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
// =============================================================================
#include "PlaneWatch.h"
#include "ADSB.h"
#include "Settings.h"
#include "Config.h"
#include "Status.h"
#include "Geo.h"
#include <WiFi.h>
#include <string.h>

// Home Assistant alone does not need an eight-second cadence - it polls far
// less often than that itself.
#define PLANE_WATCH_HA_PERIOD_MS 30000UL

static unsigned long s_next = 15000;      // after the boot settles

struct Seen { char hex[8]; unsigned long at; };
static Seen s_seen[PLANE_ALERT_MEMORY];
static int  s_seenPos = 0;

static bool seenRecently(const char* hex) {
  const unsigned long now = millis();
  for (const Seen& s : s_seen)
    if (s.hex[0] && strcmp(s.hex, hex) == 0 && now - s.at < PLANE_ALERT_FORGET_MS) return true;
  return false;
}
static void remember(const char* hex) {
  Seen& s = s_seen[s_seenPos];
  strncpy(s.hex, hex, sizeof(s.hex) - 1);
  s.hex[sizeof(s.hex) - 1] = '\0';
  s.at = millis();
  s_seenPos = (s_seenPos + 1) % PLANE_ALERT_MEMORY;
}

// The radius worth asking for: the alert circle, or the window if it reaches
// further.
static float watchRadiusKm() {
  float r = (float)Settings_PlaneAlertKm();
  if (Settings_WindowOn() && Settings_WindowKm() > r) r = (float)Settings_WindowKm();
  if (r < 5.0f) r = 5.0f;
  return r;
}

// The same rule the map uses for its filters (ScreenPlanes passesFilter), so
// what is hidden there does not pop up here.
static bool passes(const Aircraft& a) {
  if (Settings_OnlyWithCallsign() && !a.callsign[0]) return false;
  const uint16_t lo = Settings_AltMinFt(), hi = Settings_AltMaxFt();
  if (lo == 0 && hi >= 60000) return true;
  if (a.altFt <= 0) return true;
  return a.altFt >= (float)lo && a.altFt <= (float)hi;
}

void PlaneWatch_Tick(bool planesShowing) {
  const bool alert = Settings_PlaneAlert();
  const bool ha = Settings_HaEnabled();
  if (!alert && !ha) return;
  if (planesShowing) return;                        // the screen fetches itself
  const unsigned long now = millis();
  if ((long)(now - s_next) < 0) return;
  if (WiFi.status() != WL_CONNECTED) { s_next = now + PLANE_WATCH_PERIOD_MS; return; }

  const float r = watchRadiusKm();
  const bool ok = ADSB_Fetch(Settings_Lat(), Settings_Lon(), r);
  const unsigned long period = alert ? PLANE_WATCH_PERIOD_MS : PLANE_WATCH_HA_PERIOD_MS;
  s_next = millis() + (ok ? period : period * 2);
  if (ok) Status_Set(ST_PLANEWATCH, "OK, %d do %.0f km", ADSB_Count(), r);
  else    Status_Set(ST_PLANEWATCH, "ceka (pauza %lu s)", ADSB_PauseLeftMs() / 1000UL);
}

int PlaneWatch_Best(float* kmOut, float* brgOut, bool* winOut) {
  if (ADSB_DataStale()) return -1;
  const Aircraft* list = ADSB_List();
  const int n = ADSB_Count();
  const bool winOn = Settings_WindowOn();
  const float winKm = (float)Settings_WindowKm();
  const float winHalf = Settings_WindowWidth() * 0.5f;
  const float winAz = (float)Settings_WindowAz();
  const float nearKm = (float)Settings_PlaneAlertKm();

  int bestWin = -1, bestNear = -1;
  float kWin = 1e9f, kNear = 1e9f, bWin = 0, bNear = 0;
  for (int i = 0; i < n; i++) {
    const Aircraft& a = list[i];
    if (a.onGround || !passes(a)) continue;
    float km, brg;
    Geo_DistBearing(Settings_Lat(), Settings_Lon(), a.lat, a.lon, &km, &brg);
    if (winOn && km <= winKm && Geo_AngleDiff(brg, winAz) <= winHalf && km < kWin) {
      bestWin = i; kWin = km; bWin = brg;
    }
    if (km <= nearKm && km < kNear) { bestNear = i; kNear = km; bNear = brg; }
  }
  if (bestWin >= 0) {
    if (kmOut) *kmOut = kWin;
    if (brgOut) *brgOut = bWin;
    if (winOut) *winOut = true;
    return bestWin;
  }
  if (bestNear >= 0) {
    if (kmOut) *kmOut = kNear;
    if (brgOut) *brgOut = bNear;
    if (winOut) *winOut = false;
    return bestNear;
  }
  return -1;
}

bool PlaneWatch_Take(char* hex, size_t hexCap, float* km, bool* inWindow) {
  if (!Settings_PlaneAlert()) return false;
  // An aircraft from data a minute old is no longer where the list says - an
  // alert would point at an empty patch of sky.
  if (ADSB_DataStale()) return false;
  const Aircraft* list = ADSB_List();
  const int n = ADSB_Count();
  const bool winOn = Settings_WindowOn();
  const float winKm = (float)Settings_WindowKm();
  const float winHalf = Settings_WindowWidth() * 0.5f;
  const float winAz = (float)Settings_WindowAz();
  const float nearKm = (float)Settings_PlaneAlertKm();

  // Same priority as PlaneWatch_Best(), but skipping what was already shown -
  // otherwise one aircraft parked in the window would block every other one.
  int best = -1; bool bestIsWin = false; float bestKm = 1e9f;
  for (int i = 0; i < n; i++) {
    const Aircraft& a = list[i];
    if (a.onGround || !a.hex[0] || !passes(a) || seenRecently(a.hex)) continue;
    float d, brg;
    Geo_DistBearing(Settings_Lat(), Settings_Lon(), a.lat, a.lon, &d, &brg);
    const bool w = winOn && d <= winKm && Geo_AngleDiff(brg, winAz) <= winHalf;
    const bool nr = d <= nearKm;
    if (!w && !nr) continue;
    // A window aircraft beats any non-window one; within the same class the
    // nearer one wins.
    if (best < 0 || (w && !bestIsWin) || (w == bestIsWin && d < bestKm)) {
      best = i; bestIsWin = w; bestKm = d;
    }
  }
  if (best < 0) return false;
  strncpy(hex, list[best].hex, hexCap - 1);
  hex[hexCap - 1] = '\0';
  if (km) *km = bestKm;
  if (inWindow) *inWindow = bestIsWin;
  remember(list[best].hex);
  return true;
}
