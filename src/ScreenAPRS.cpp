// =============================================================================
//  MeteoPlaneRadar
//  Screen 3: one configured APRS station on the map.
//
//  Reuses the aircraft radar's map machinery: the same equirectangular
//  projection (with the "Nahore" rotation), the same European borders/cities
//  underlay, range rings and compass. The station is drawn as a green diamond
//  with its callsign; if it sits outside the current range an arrow at the rim
//  points towards it. Swipe left/right changes the range exactly like the radar.
//
//  Project: MeteoPlaneRadar - live aircraft radar on a round touchscreen
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
//           Ondra OK1CDJ / apps.ok1cdj.com
//  Web:     https://chiptron.cz
//  Board:   Waveshare ESP32-S3-Touch-LCD-2.1 (round 480x480 display, ST7701)
// =============================================================================
#include "ScreenAPRS.h"
#include "APRS.h"
#include "Settings.h"
#include "Config.h"
#include "EuBorder.h"
#include "UI.h"
#include "Display_ST7701.h"

#include <WiFi.h>
#include <math.h>
#include <string.h>
#include <time.h>

// Round panel - map centred on the middle of the screen (same as ScreenPlanes).
#define R_CX (LCD_WIDTH / 2)
#define R_CY (LCD_HEIGHT / 2)
#define R_RADIUS 230
#define DEG2RAD 0.0174532925f

static const float RANGES_KM[] = APRS_RANGES_KM;
static const int   RANGE_COUNT = sizeof(RANGES_KM) / sizeof(RANGES_KM[0]);
static int s_rangeIdx = 2;   // 100 km by default (stations sit farther out)

static float currentRange() { return RANGES_KM[s_rangeIdx]; }

static unsigned long s_nextFetch = 0;
static bool          s_dataOk = false;

// --- Map orientation ---------------------------------------------------------
// Identical to ScreenPlanes: the user picks which compass bearing is at the TOP
// of the screen and the PROJECTION is rotated (never the display). Kept local so
// ScreenPlanes stays untouched; a future refactor could share one MapView.
static float s_rotSin = 0.0f, s_rotCos = 1.0f;
static uint16_t s_topDeg = 0;

static void refreshRotation() {
  uint16_t deg = Settings_TopBearing();
  if (deg == s_topDeg) return;
  s_topDeg = deg;
  float r = (float)deg * DEG2RAD;
  s_rotSin = sinf(r);
  s_rotCos = cosf(r);
}

static void project(float lat, float lon, double clat, double clon,
                    float rangeKm, int* sx, int* sy) {
  float latr = clat * DEG2RAD;
  float dxKm = (lon - clon) * 111.0f * cosf(latr);
  float dyKm = (lat - clat) * 111.0f;
  float rx = dxKm * s_rotCos - dyKm * s_rotSin;
  float ry = dxKm * s_rotSin + dyKm * s_rotCos;
  float scale = (float)R_RADIUS / rangeKm;   // px per km
  *sx = R_CX + (int)(rx * scale);
  *sy = R_CY - (int)(ry * scale);
}

// The map is centred on the STATION itself (set each frame before drawing); the
// range is a radius around it. The user's own location plays no part here.
static double s_ctrLat = 0, s_ctrLon = 0;

// ProjectFn wrapper for the border/city drawing (station centre + range).
static void cityProject(float lat, float lon, int* sx, int* sy) {
  project(lat, lon, s_ctrLat, s_ctrLon, currentRange(), sx, sy);
}

static bool configured() {
  const char* c = Settings_AprsCall();
  return c && c[0];
}

// Station marker: a filled diamond with a white centre dot (clearly different
// from the winged aircraft arrow on the radar screen).
static void drawStation(int x, int y, uint16_t col) {
  gfx->fillTriangle(x, y - 9, x - 8, y, x + 8, y, col);
  gfx->fillTriangle(x, y + 9, x - 8, y, x + 8, y, col);
  gfx->fillCircle(x, y, 2, C_WHITE);
}

// Human-readable age of the station's last position, plus a traffic-light
// colour so a glance says whether the data is current. Needs a synced clock
// (SNTP is started in setup); until time() is valid it reads "cas: ?".
//   green  <= 15 min   yellow <= 1 h   red older
static uint16_t ageText(long lastUtc, char* buf, size_t n) {
  time_t now = time(nullptr);
  if (now < 1600000000L || lastUtc <= 0) { snprintf(buf, n, "cas: ?"); return C_GRAY; }
  long age = (long)now - lastUtc;
  if (age < 0) age = 0;
  if      (age < 90)     snprintf(buf, n, "pred %ld s",   age);
  else if (age < 5400)   snprintf(buf, n, "pred %ld min", (age + 30) / 60);
  else if (age < 172800) snprintf(buf, n, "pred %ld h",   (age + 1800) / 3600);
  else                   snprintf(buf, n, "pred %ld d",   age / 86400);
  if (age <= 900)  return C_GREEN;
  if (age <= 3600) return C_YELLOW;
  return C_RED;
}

void ScreenAPRS_Enter() {
  s_rangeIdx = Settings_AprsRange();
  if (s_rangeIdx >= RANGE_COUNT) s_rangeIdx = 2;   // guard against a stale value
  s_nextFetch = 0;
}

bool ScreenAPRS_Tick() {
  if (WiFi.status() != WL_CONNECTED) return false;
  if (!configured()) return false;                 // nothing to fetch yet

  if (millis() >= s_nextFetch) {
    s_dataOk = APRS_Fetch(Settings_AprsCall(), Settings_AprsKey());
    // Normal cadence; after a transport failure back off to double the interval.
    s_nextFetch = millis() + (s_dataOk ? APRS_PERIOD_MS : APRS_PERIOD_MS * 2);
    return true;   // new data (or a fresh status) -> redraw
  }
  return false;
}

// Swipe: change the range and re-fetch immediately at the new radius.
void ScreenAPRS_ChangeRange(int dir) {
  s_rangeIdx = (s_rangeIdx + dir + RANGE_COUNT) % RANGE_COUNT;
  Settings_SetAprsRange(s_rangeIdx);   // remember across restarts (debounced)
  Serial.printf("APRS range: %.0f km\n", currentRange());
  s_nextFetch = 0;
}

void ScreenAPRS_Draw() {
  gfx->fillScreen(C_BLACK);
  refreshRotation();
  float range = currentRange();

  // Title: the configured callsign (or a generic label).
  UI_TextCentered(configured() ? Settings_AprsCall() : "APRS", 48, C_CYAN, 2);

  const bool haveFix = configured() && APRS_HasFix() && APRS_Get()->hasPos;

  if (!configured()) {
    UI_TextCentered("Nastav volaci znak", R_CY - 12, C_YELLOW, 2);
    UI_TextCentered("ve WiFi portalu (Nastaveni)", R_CY + 14, C_GRAY, 1);
  } else if (!haveFix) {
    // No position yet - nothing to centre the map on.
    if (WiFi.status() != WL_CONNECTED) {
      UI_TextCentered("Cekam na WiFi", R_CY, C_YELLOW, 2);
    } else {
      const char* s = APRS_Status();
      UI_TextCentered((s && s[0]) ? s : "Cekam na data...", R_CY, C_YELLOW, 2);
    }
  } else {
    const AprsStation* st = APRS_Get();

    // --- Map centred on the STATION; the range is a radius around it ---
    s_ctrLat = st->lat;
    s_ctrLon = st->lon;
    const double clat = st->lat;
    const double clon = st->lon;
    const float  marginKm = range * 1.2f;
    const float  dLat = marginKm / 111.0f;
    const float  dLon = marginKm / (111.0f * cosf(clat * DEG2RAD));
    const float  lat0 = clat - dLat, lat1 = clat + dLat;
    const float  lon0 = clon - dLon, lon1 = clon + dLon;

    EuBorder_Draw(cityProject, C_GRAY, lat0, lat1, lon0, lon1);
    {
      int rad = LCD_WIDTH / 2 - 4;
      bool showFull = (range <= 25.0f);
      uint8_t maxTier = (range <= 25.0f) ? 3 : (range <= 50.0f ? 2 : 1);
      EuBorder_DrawCities(cityProject, R_CX, R_CY, rad, C_DKGRAY, C_GRAY,
                          showFull, maxTier, lat0, lat1, lon0, lon1);
    }

    // Range rings.
    gfx->drawCircle(R_CX, R_CY, LCD_WIDTH / 2 - 2, C_DKGRAY);
    gfx->drawCircle(R_CX, R_CY, LCD_WIDTH / 4, C_DKGRAY);

    // Compass marks (rotated with the map, like ScreenPlanes).
    {
      const int   cr = 205;
      const char* lbl[4] = { "S", "V", "J", "Z" };
      const int   brg[4] = { 0, 90, 180, 270 };
      gfx->setTextSize(1);
      gfx->setTextColor(C_GRAY);
      for (int i = 0; i < 4; i++) {
        float a = (brg[i] - (int)s_topDeg) * DEG2RAD;
        int cxp = R_CX + (int)(cr * sinf(a)) - 3;
        int cyp = R_CY - (int)(cr * cosf(a)) - 4;
        gfx->setCursor(cxp, cyp);
        gfx->print(lbl[i]);
      }
    }

    // The station sits at the centre of its own map.
    drawStation(R_CX, R_CY, C_GREEN);

    // Freshness of the last position (colour-coded) - the point of the screen.
    char age[24];
    uint16_t ageCol = ageText(st->lastTimeUtc, age, sizeof(age));
    UI_TextCentered(age, LCD_HEIGHT - 104, ageCol, 2);
    // Comment, and speed/course when the station is moving.
    if (st->comment[0])
      UI_TextCentered(st->comment, LCD_HEIGHT - 126, C_GRAY, 1);
    if (st->speedKmh > 0.0f) {
      char info[40];
      snprintf(info, sizeof(info), "%.0f km/h  %.0f deg", st->speedKmh, st->course);
      UI_TextCentered(info, LCD_HEIGHT - 148, C_GRAY, 1);
    }
  }

  // Range readout + selector dots (same layout as ScreenPlanes).
  char rbuf[16];
  snprintf(rbuf, sizeof(rbuf), "%.0f km", range);
  UI_TextCentered(rbuf, LCD_HEIGHT - 76, C_YELLOW, 2);

  int dotGap = 24;
  int totalW = (RANGE_COUNT - 1) * dotGap;
  int startX = R_CX - totalW / 2;
  int dotY = LCD_HEIGHT - 50;
  for (int i = 0; i < RANGE_COUNT; i++) {
    int x = startX + i * dotGap;
    if (i == s_rangeIdx) gfx->fillCircle(x, dotY, 5, C_YELLOW);
    else                 gfx->drawCircle(x, dotY, 5, C_GRAY);
  }
}
