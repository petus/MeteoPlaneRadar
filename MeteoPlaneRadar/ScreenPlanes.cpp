// =============================================================================
//  MeteoPlaneRadar
//  Screen 1: aircraft radar (adsb.fi) + aircraft detail.
//
//  Project: MeteoPlaneRadar - live aircraft radar on a round touchscreen
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
//  Web:     https://chiptron.cz
//  Board:   Waveshare ESP32-S3-Touch-LCD-2.1 (round 480x480 display, ST7701)
// =============================================================================
#include "AAFont.h"   // CZT(): accents with the smooth fonts
#include "ScreenPlanes.h"
#include "ADSB.h"
#include "Settings.h"
#include "Config.h"
#include "Route.h"
#include "EuBorder.h"
#include "UI.h"
#include "Layout.h"
#include "Lang.h"
#include "Status.h"
#include "Display_ST7701.h"
#include "PlaneIcon.h"
#include "Geo.h"

#include <WiFi.h>
#include <math.h>
#include <string.h>   // strncpy / strcmp for the hex-based selection

// Round panel - radar centred on the middle of the screen.
// R_RADIUS = 230 px maps the selected range onto the radar circle.
#define R_CX (LCD_WIDTH / 2)
#define R_CY (LCD_HEIGHT / 2)
#define R_RADIUS 230

// Ranges (radius in km).
static const float RANGES_KM[] = PLANE_RANGES_KM;
static const int   RANGE_COUNT = sizeof(RANGES_KM) / sizeof(RANGES_KM[0]);
static int s_rangeIdx = 1;   // 25 km by default

static float currentRange() { return RANGES_KM[s_rangeIdx]; }

// Base poll interval by range. Larger areas return more data and are less
// time-critical, so they are polled less often - keeps us a light user of the
// free adsb.fi API.
static unsigned long basePeriodMs() {
  float r = currentRange();
  if (r <= ADSB_NEAR_KM) return ADSB_PERIOD_NEAR_MS;
  if (r <= ADSB_MID_KM)  return ADSB_PERIOD_MID_MS;
  return ADSB_PERIOD_FAR_MS;
}

static unsigned long s_nextFetch = 0;
static bool  s_dataOk = false;
static String s_status = "...";

// Aircraft detail - the selected aircraft is remembered by its ICAO hex address,
// NOT by its index in the list.
//
// The list is rebuilt from scratch on every fetch (every 5 s) and adsb.fi does
// not guarantee a stable order: one aircraft leaving the area shifts everything
// after it. Holding an index across a fetch therefore silently repoints the
// detail at a different aircraft. Keying on the hex means the panel either
// follows the aircraft you actually picked, or closes when it genuinely leaves.
//
// Empty string = nothing selected / detail closed.
static char s_selectedHex[8] = "";

// Grace period for the detail panel. adsb.fi sometimes omits an aircraft from a
// single poll and sends it again in the next one; closing the panel on the
// first miss looks like it closes by itself. We therefore keep the last known
// data and only give up after DETAIL_GRACE_POLLS consecutive misses.
static int      s_selMiss = 0;          // consecutive polls without the aircraft
static Aircraft s_selCache;             // last known data of the selected one
static bool     s_selCacheOk = false;

// Screen positions from the last draw, for tap-to-select. Parallel to the
// aircraft list, so s_planeHex[i] records which aircraft the point belongs to
// and the tap resolves to an identity rather than to a slot number.
static int  s_planeX[ADSB_MAX];
static int  s_planeY[ADSB_MAX];
static char s_planeHex[ADSB_MAX][8];
static int  s_planeN = 0;

bool ScreenPlanes_DetailOpen() { return s_selectedHex[0] != '\0'; }

// reason = short text for the serial log, so we can tell WHY the panel closed
// (user tap vs. the aircraft disappearing from the data).
static void selectNone(const char* reason) {
#if TOUCH_DEBUG
  if (s_selectedHex[0]) Serial.printf("SEL: zavreno (%s) hex=%s\n", reason, s_selectedHex);
#else
  (void)reason;
#endif
  s_selectedHex[0] = '\0';
  s_selMiss = 0;
  s_selCacheOk = false;
  Route_Clear();
}
static void selectHex(const char* hex) {
  strncpy(s_selectedHex, hex, sizeof(s_selectedHex) - 1);
  s_selectedHex[sizeof(s_selectedHex) - 1] = '\0';
  s_selMiss = 0;
  s_selCacheOk = false;
#if TOUCH_DEBUG
  Serial.printf("SEL: vybrano hex=%s\n", s_selectedHex);
#endif
}

// Convert an aircraft's lat/lon into screen coordinates (north up).
// --- Map orientation -------------------------------------------------------
// The user picks WHICH COMPASS BEARING IS AT THE TOP of the screen, i.e. the
// direction they are looking out of the window. Everything then follows:
//
//     screen angle of bearing b  =  b - topBearing      (0 = up, clockwise)
//
// So looking east (top = 90) puts north on the left, which is exactly where it
// is in reality. Setting an "amount to turn by" instead used to need the
// opposite value (360 - bearing) and felt like the map was mirrored.
//
// We do NOT rotate the display (the canvas only supports rotation 0 and turning
// the framebuffer would break touch mapping) - the PROJECTION is rotated.
// Borders and cities go through the same project(), so they follow along for
// free; only the aircraft icons need their heading corrected separately.
static float s_rotSin = 0.0f, s_rotCos = 1.0f;   // cached sin/cos of the angle
static uint16_t s_topDeg = 0;                    // bearing shown at the top

static void refreshRotation() {
  uint16_t deg = Settings_TopBearing();
  if (deg == s_topDeg) return;
  s_topDeg = deg;
  float r = (float)deg * 0.0174532925f;
  s_rotSin = sinf(r);
  s_rotCos = cosf(r);
}

static void project(float lat, float lon, double clat, double clon,
                    float rangeKm, int* sx, int* sy) {
  float latr = clat * 0.0174532925f;
  float dxKm = (lon - clon) * 111.0f * cosf(latr);
  float dyKm = (lat - clat) * 111.0f;
  // Rotate the local east/north vector so that the chosen bearing ends up at
  // the top: with top = 90 (looking east) a target due east appears up.
  float rx = dxKm * s_rotCos - dyKm * s_rotSin;
  float ry = dxKm * s_rotSin + dyKm * s_rotCos;
  float scale = (float)R_RADIUS / rangeKm;   // px per km
  *sx = R_CX + (int)(rx * scale);
  *sy = R_CY - (int)(ry * scale);
}

// Wrapper matching the ProjectFn signature (used to draw the map via EuBorder).
// Uses the current user location and the selected range.
static void cityProject(float lat, float lon, int* sx, int* sy) {
  project(lat, lon, Settings_Lat(), Settings_Lon(), currentRange(), sx, sy);
}

// --- Altitude colour bands ---
// The colour of an aircraft encodes its barometric altitude, so a glance tells
// you whether something is on approach overhead or just transiting at cruise.
// The bands are contiguous - every altitude falls into exactly one.
//
//   < 2 km   red     approach/departure, helicopters, light aircraft
//   2-6 km   orange  climb/descent, regional traffic
//   6-10 km  yellow  lower cruise levels
//   >= 10 km blue    long-haul cruise
//
// An aircraft reporting no altitude at all (altFt == 0 and not on the ground)
// is drawn grey rather than being forced into the "low" band, which would
// otherwise paint every unknown target an alarming red.
//
// 0.8.0: both the three boundaries and the four colours are settings now. The
// defaults are the values above, so the radar looks exactly as it did until
// someone changes them.
static uint16_t altColor(float altFt, bool known) {
  if (!known) return C_GRAY;
  const float m = altFt * 0.3048f;    // feet -> metres
  for (uint8_t i = 0; i < 3; i++)
    if (m < (float)Settings_AltBandM(i)) return Settings_AltColor(i);
  return Settings_AltColor(3);
}

// Aircraft icon. The shape now follows the airframe - see PlaneIcon.h. The
// original winged arrow is still there for anything that cannot be classified.
static void drawPlane(int x, int y, float trackDeg, bool hasTrack, uint8_t kind, uint16_t col) {
  PlaneIcon_Draw(x, y, trackDeg, hasTrack, kind, col);
}

// --- Filters ----------------------------------------------------------------
// Set from the web UI. The point is not to hide traffic but to make a busy sky
// readable: over an approach path the interesting aircraft is the one at 2000
// ft, and at home under an airway it is the opposite.
//
// The filter applies to DRAWING only. The aircraft stays in the list, so the
// count, the emergency scan and a watched callsign all still see it - hiding an
// emergency because of an altitude filter would be a nasty surprise.
static bool passesFilter(const Aircraft& a) {
  if (Settings_OnlyWithCallsign() && !a.callsign[0]) return false;
  const uint16_t lo = Settings_AltMinFt(), hi = Settings_AltMaxFt();
  if (lo == 0 && hi >= 60000) return true;          // filter off
  if (a.altFt <= 0) return true;                    // unknown altitude - keep it
  if (a.altFt < (float)lo || a.altFt > (float)hi) return false;
  return true;
}

// Is this the aircraft the user asked to be told about? Matched against both
// the callsign and the ICAO address, because people quote whichever they have.
static bool isWatched(const Aircraft& a) {
  const char* w = Settings_WatchCallsign();
  if (!w || !*w) return false;
  if (a.callsign[0] && strcasecmp(a.callsign, w) == 0) return true;
  if (a.hex[0] && strcasecmp(a.hex, w) == 0) return true;
  return false;
}

void ScreenPlanes_Enter() {
  s_rangeIdx = Settings_PlaneRange();
  if (s_rangeIdx >= RANGE_COUNT) s_rangeIdx = 1;   // guard against a stale value
  s_nextFetch = 0;
}

// --- Aircraft-nearby alert (0.8.0) -------------------------------------------
// The screen is brought up for a few seconds with the detail of the aircraft
// that triggered it already open. The range is picked to fit that aircraft and
// is NOT remembered - when the alert ends the user's own range comes back.
static bool s_alert = false;
static bool s_alertWindow = false;
static float s_alertKm = 0;

void ScreenPlanes_AlertBegin(const char* hex, float distKm, bool inWindow) {
  s_alert = true;
  s_alertWindow = inWindow;
  s_alertKm = distKm;
  // Smallest range that shows the aircraft with a little room to spare.
  int idx = RANGE_COUNT - 1;
  for (int i = 0; i < RANGE_COUNT; i++) {
    if (RANGES_KM[i] >= distKm * 1.15f) { idx = i; break; }
  }
  s_rangeIdx = idx;
  s_nextFetch = 0;
  if (hex && *hex) selectHex(hex);
}

void ScreenPlanes_AlertEnd() {
  if (!s_alert) return;
  s_alert = false;
  selectNone("konec upozorneni");
  s_rangeIdx = Settings_PlaneRange();
  if (s_rangeIdx >= RANGE_COUNT) s_rangeIdx = 1;
  s_nextFetch = 0;
}

bool ScreenPlanes_AlertActive() { return s_alert; }

bool ScreenPlanes_Tick() {
  if (WiFi.status() != WL_CONNECTED) { s_status = T(S_WIFI_WAIT); return false; }

  // Odpoved na trasu prijde nezavisle na stahovani letadel, obvykle do vteriny
  // po klepnuti. Bez tohohle by se dokreslila az s pristim pollem, takze u
  // stredniho dosahu az o deset sekund pozdeji, a panel by mezitim zbytecne
  // ukazoval "zjistuji trasu". Ctenim priznak zhasne, takze to prekresli jednou.
  bool routeChanged = Route_TakeChanged();

  if (millis() >= s_nextFetch) {
    s_status = T(S_DOWNLOADING);
    s_dataOk = ADSB_Fetch(Settings_Lat(), Settings_Lon(), currentRange());
    s_status = s_dataOk ? T(S_OK) : T(S_ERROR);
    // Normal cadence depends on range; after a failure back off to double the
    // interval rather than hammering the API at the normal rate.
    unsigned long period = basePeriodMs();
    s_nextFetch = millis() + (s_dataOk ? period : period * 2);

    // Selection bookkeeping happens HERE (once per poll), not while drawing -
    // drawing runs many times per second and would burn the grace period in an
    // instant. The selection is an ICAO hex, so a reordered list cannot move it
    // onto a different aircraft.
    if (s_dataOk && ScreenPlanes_DetailOpen()) {
      if (ADSB_FindByHex(s_selectedHex) >= 0) {
        s_selMiss = 0;                       // still there
      } else if (++s_selMiss > DETAIL_GRACE_POLLS) {
        selectNone("letadlo zmizelo z dat");
      }
#if TOUCH_DEBUG
      else Serial.printf("SEL: chybi v datech (%d/%d) hex=%s\n",
                         s_selMiss, DETAIL_GRACE_POLLS, s_selectedHex);
#endif
    }
    return true;   // new data -> redraw
  }
  return routeChanged;   // jinak kreslit jen kdyz dorazila trasa
}

// Short tap: with the detail open any tap closes it (the units toggle that used
// to sit at the bottom of the panel now lives on the settings screen).
// Otherwise select the nearest aircraft.
bool ScreenPlanes_HandleTap(int x, int y) {
  if (ScreenPlanes_DetailOpen()) {
    selectNone("tap mimo panel");
    return true;
  }
  // Find the aircraft nearest the tap (within 30 px), then remember *which
  // aircraft* it was, not where it happened to sit in the list.
  int best = -1;
  long bestD = 30L * 30L;
  for (int i = 0; i < s_planeN; i++) {
    long dx = s_planeX[i] - x, dy = s_planeY[i] - y;
    long d = dx * dx + dy * dy;
    if (d < bestD) { bestD = d; best = i; }
  }
  if (best >= 0 && s_planeHex[best][0]) {
    selectHex(s_planeHex[best]);
    // The callsign is only known from the data, so the lookup is kicked off
    // where the aircraft is drawn (below) - here we just have the hex.
    return true;
  }
  return false;
}

// Swipe: change the range (both directions). Re-fetch immediately at the new
// radius. Ignored while the detail panel is open (close it first).
void ScreenPlanes_ChangeRange(int dir) {
  if (ScreenPlanes_DetailOpen()) return;
  s_alert = false;                       // the user has taken over
  s_rangeIdx = (s_rangeIdx + dir + RANGE_COUNT) % RANGE_COUNT;
  Settings_SetPlaneRange(s_rangeIdx);   // remember across restarts (debounced)
  Serial.printf("ADSB range: %.0f km\n", currentRange());
  s_nextFetch = 0;
}

void ScreenPlanes_RangeText(char* out, size_t cap) {
  if (!out || !cap) return;
  snprintf(out, cap, "%.0f %s", currentRange(), T(S_KM));
}

// Close the detail panel (called on the long-press screen switch).
void ScreenPlanes_CloseDetail() { selectNone("dlouhy stisk / prepnuti obrazovky"); }

void ScreenPlanes_Draw() {
  gfx->fillScreen(C_BLACK);
  Layout_Begin();
  refreshRotation();           // pick up a changed rotation setting
  float range = currentRange();

  // Reserve the chrome BEFORE the map is drawn. City labels and callsigns are
  // placed by data rather than by design, so they have to lose the argument -
  // and they can only lose it if the space is already taken when they ask.
  Layout_ReserveBand(LY_DOTS - 6, 12);        // screen selector dots
  Layout_ReserveBand(LY_STATUS - 3, 22);      // clock + outside temperature
  // The alert banner is taller than the count it replaces, and the legend has
  // its boundary numbers under the colour bar - both bands are sized for the
  // taller variant so a city label can never creep into either.
  Layout_ReserveBand(LY_SUB - 6, 22);         // aircraft count / alert banner
  Layout_ReserveBand(LY_LEGEND - 2, 24);      // altitude legend + numbers
  Layout_ReserveBand(LY_RANGE - 2, 20);       // range readout
  Layout_ReserveBand(LY_RANGE_DOTS - 6, 12);  // range dots

  // --- Map underlay ---
  // The map data spans the whole of Europe, so work out which slice of it can
  // actually be on screen and hand that window to EuBorder, which throws away
  // everything else before drawing. The radar circle has a radius of `range`
  // km, so the visible span is `range` in every direction; a 20% margin keeps
  // lines that only clip the edge of the view from being dropped.
  const double clat = Settings_Lat();
  const double clon = Settings_Lon();
  const float  marginKm = range * 1.2f;
  const float  dLat = marginKm / 111.0f;
  const float  dLon = marginKm / (111.0f * cosf(clat * 0.0174532925f));
  const float  lat0 = clat - dLat, lat1 = clat + dLat;
  const float  lon0 = clon - dLon, lon1 = clon + dLon;

  EuBorder_Draw(cityProject, C_GRAY, lat0, lat1, lon0, lon1);

  // Cities as an underlay (below the aircraft, so it is clear where they are).
  // Fewer of them survive as the range grows, otherwise a dense region such as
  // the Ruhr would bury the traffic under a wall of labels.
  {
    int rad = LCD_WIDTH / 2 - 4;
    bool showFull = (range <= 25.0f);    // full names only at close range
    uint8_t maxTier;
    // Tier 3 includes the Czech district towns, which is what makes the map
    // recognisable close to home; further out only the bigger places survive,
    // otherwise the labels would bury the traffic.
    if (range <= 50.0f) maxTier = 3;
    else                maxTier = 2;
    EuBorder_DrawCities(cityProject, R_CX, R_CY, rad, C_DKGRAY, C_GRAY,
                        showFull, maxTier, lat0, lat1, lon0, lon1);
  }

  // Range rings and centre marker (2 circles, white cross).
  gfx->drawCircle(R_CX, R_CY, LCD_WIDTH / 2 - 2, C_DKGRAY);
  gfx->drawCircle(R_CX, R_CY, LCD_WIDTH / 4, C_DKGRAY);
  gfx->drawFastHLine(R_CX - 8, R_CY, 16, C_WHITE);
  gfx->drawFastVLine(R_CX, R_CY - 8, 16, C_WHITE);

  // --- Window view (0.8.0) ---
  // The sector visible from the window, so the alert's idea of "in view" can
  // be checked against the map. Two dim rays and a dotted arc at the distance
  // set as the limit (or the edge of the radar, if that is closer).
  if (Settings_WindowOn()) {
    const float half = Settings_WindowWidth() * 0.5f;
    const float a0 = (float)Settings_WindowAz() - half;
    const float a1 = (float)Settings_WindowAz() + half;
    float rr = (float)R_RADIUS * (float)Settings_WindowKm() / range;
    if (rr > R_RADIUS) rr = R_RADIUS;
    const uint16_t wc = 0x0339;                    // dim teal
    for (int e = 0; e < 2; e++) {
      const float b = ((e ? a1 : a0) - (float)s_topDeg) * 0.0174532925f;
      gfx->drawLine(R_CX, R_CY, R_CX + (int)(rr * sinf(b)), R_CY - (int)(rr * cosf(b)), wc);
    }
    for (float d = a0; d <= a1; d += 3.0f) {
      const float b = (d - (float)s_topDeg) * 0.0174532925f;
      gfx->drawPixel(R_CX + (int)(rr * sinf(b)), R_CY - (int)(rr * cosf(b)), wc);
    }
  }

  // Aircraft.
  int shown = 0;
  const Aircraft* list = ADSB_List();
  int n = ADSB_Count();
  // 0.8.4: when adsb.fi has not answered for a while, the last aircraft are
  // shown grey, and after ADSB_HIDE_MS not at all - see Config.h.
  const bool stale = ADSB_DataStale();
  if (ADSB_DataHidden()) n = 0;
  s_planeN = n;

  // Resolve the selection once per frame. If the aircraft has left the area it
  // is simply not in the new data, so the detail closes by itself rather than
  // silently latching onto whoever now sits at that index.
  // Do NOT close the panel here - that is decided once per poll in Tick(), so
  // the grace period actually lasts. While the aircraft is missing we keep
  // drawing the last known values from the cache.
  int selIdx = ADSB_FindByHex(s_selectedHex);

  const char* alertCode = nullptr;    // worst emergency seen this frame
  bool watchedSeen = false;

  for (int i = 0; i < n; i++) {
    s_planeX[i] = -9999; s_planeY[i] = -9999;   // default: off-screen
    s_planeHex[i][0] = '\0';
    if (list[i].onGround) continue;

    // Scan for emergencies and the watched aircraft before the filter, so
    // neither can be hidden by an altitude setting.
    const char* em = Settings_SquawkAlert() ? ADSB_EmergencyCode(list[i]) : nullptr;
    const bool watched = isWatched(list[i]);
    if (em && !alertCode) alertCode = em;
    if (watched) watchedSeen = true;

    if (!em && !watched && !passesFilter(list[i])) continue;
    int sx, sy;
    project(list[i].lat, list[i].lon, Settings_Lat(), Settings_Lon(), range, &sx, &sy);
    int dx = sx - R_CX, dy = sy - R_CY;
    if (dx * dx + dy * dy > R_RADIUS * R_RADIUS) continue;   // outside the circle
    // Store position *and* identity, so a tap resolves to an aircraft and not
    // to a list slot that may mean something else by the time it is used.
    s_planeX[i] = sx; s_planeY[i] = sy;
    strncpy(s_planeHex[i], list[i].hex, sizeof(s_planeHex[i]) - 1);
    s_planeHex[i][sizeof(s_planeHex[i]) - 1] = '\0';
    // Highlight the selected aircraft with a ring. It is white, not cyan: the
    // icon colour now carries the altitude, and cyan sat too close to the blue
    // of the cruise band to read as "selected".
    if (i == selIdx) gfx->drawCircle(sx, sy, 16, C_WHITE);
    // An emergency gets a red ring, a watched aircraft a green one. Both are
    // drawn wider than the selection ring so they read at a glance.
    if (em)      { gfx->drawCircle(sx, sy, 20, C_RED);   gfx->drawCircle(sx, sy, 21, C_RED); }
    else if (watched) { gfx->drawCircle(sx, sy, 20, C_GREEN); gfx->drawCircle(sx, sy, 21, C_GREEN); }
    // Altitude is only meaningful when the aircraft actually reports it.
    bool altKnown = (list[i].altFt > 0.0f);
    // The map is turned, so the icon heading has to be corrected the same way -
    // otherwise the aircraft would point the wrong direction.
    float screenTrack = list[i].track - (float)s_topDeg;
    while (screenTrack < 0.0f) screenTrack += 360.0f;
    // Stale data is drawn grey: the positions are where the aircraft WERE.
    drawPlane(sx, sy, screenTrack, list[i].hasTrack, list[i].icon,
              stale ? C_GRAY : altColor(list[i].altFt, altKnown));
    // Label under the icon: the callsign, or the ICAO address when the aircraft
    // broadcasts none. The fallback lives HERE and nowhere else - Aircraft::
    // callsign is deliberately left empty in that case, because it is what gets
    // sent to the route API and a hex address there reads as a flight number
    // (see copyCallsign in ADSB.cpp). Drawing it is harmless; asking about it
    // is not.
    const char* label = list[i].callsign[0] ? list[i].callsign : list[i].hex;
    if (label[0]) {
      // Centred below the icon (radius ~14 px). It has to claim its space: two
      // aircraft close together used to print their labels straight through
      // each other, and near the rim one could land on the legend or the range
      // readout.
      int tw = Layout_TextW(label, 2);
      int tx = sx - tw / 2, ty = sy + 22;
      if (Layout_Claim(tx - 2, ty - 2, tw + 4, 20)) {
        gfx->setTextSize(2);
        gfx->setTextColor(em ? C_RED : (watched ? C_GREEN : C_WHITE));
        gfx->setCursor(tx, ty);
        gfx->print(label);
      }
    }
    shown++;
  }

  // Top of the screen, under the screen-selector dots at y=18:
  //   y 30..45  clock + outside temperature (size 2, the same as the range)
  //   y 52..59  aircraft count (size 1 - it is a secondary number)
  //   y 74      altitude legend
  UI_DrawStatusLine(LY_STATUS);

  // The line under the clock: normally the aircraft count, but an emergency
  // squawk takes it over. There is exactly one line for this, so the two can
  // never overlap - the alert simply outranks the count.
  char sub[40];
  if (alertCode) {
    const char* what = (strcmp(alertCode, SQUAWK_HIJACK) == 0) ? T(S_HIJACK)
                     : (strcmp(alertCode, SQUAWK_RADIO)  == 0) ? T(S_RADIO_FAIL)
                                                               : T(S_EMERGENCY);
    snprintf(sub, sizeof(sub), "%s  %s", alertCode, what);
    int tw = Layout_TextW(sub, 2);
    gfx->fillRect(LCD_WIDTH / 2 - tw / 2 - 6, LY_SUB - 4, tw + 12, 20, C_RED);
    UI_TextCentered(sub, LY_SUB - 1, C_WHITE, 2);
  } else if (WiFi.status() == WL_CONNECTED && stale && ADSB_DataAgeMs() != 0xFFFFFFFFUL) {
    // How old the grey aircraft are, and how long until the next attempt.
    const unsigned long ageMin = ADSB_DataAgeMs() / 60000UL;
    const unsigned long pause = ADSB_PauseLeftMs() / 1000UL;
    if (ADSB_DataHidden()) snprintf(sub, sizeof(sub), "%s", T(S_NO_PLANE_DATA));
    else                   snprintf(sub, sizeof(sub), "%s: %lu %s", T(S_OLD_PLANES), ageMin ? ageMin : 1, T(S_MIN));
    if (pause) {
      const size_t l = strlen(sub);
      snprintf(sub + l, sizeof(sub) - l, " (%s %lu s)", T(S_PAUSE), pause);
    }
    UI_TextCentered(sub, LY_SUB, C_YELLOW, 1);
  } else if (WiFi.status() != WL_CONNECTED || !s_dataOk) {
    UI_TextCentered(s_status.c_str(), LY_SUB, C_YELLOW, 1);
  } else {
    snprintf(sub, sizeof(sub), "%s: %d%s", T(S_AIRCRAFT), shown,
             watchedSeen ? " *" : "");
    UI_TextCentered(sub, LY_SUB, watchedSeen ? C_GREEN : C_CYAN, 1);
  }

  // --- Altitude legend ---
  // The icon colour means nothing without a key. Four swatches in a continuous
  // bar, with the band boundaries (2 / 6 / 10 km) marked underneath. It sits
  // below the aircraft count, where the round panel still has room - the bottom
  // of the screen is already taken by the range readout.
  {
    uint16_t cols[4];
    char bounds[3][8];                              // km, at the band edges
    for (int i = 0; i < 4; i++) cols[i] = Settings_AltColor(i);
    for (int i = 0; i < 3; i++) {
      const uint16_t m = Settings_AltBandM(i);
      if (m % 1000 == 0) snprintf(bounds[i], sizeof(bounds[i]), "%u", (unsigned)(m / 1000));
      else               snprintf(bounds[i], sizeof(bounds[i]), "%.1f", m / 1000.0f);
    }
    const int sw = 26;                 // swatch width
    const int barW = 4 * sw;
    const int lx = R_CX - barW / 2;
    const int ly = LY_LEGEND;

    // Continuous bar - the bands abut, matching the fact that the altitude
    // ranges themselves are contiguous with no gaps between them.
    for (int i = 0; i < 4; i++) {
      gfx->fillRect(lx + i * sw, ly, sw, 6, cols[i]);
    }

    // Boundary numbers, centred on each internal edge of the bar.
    gfx->setTextSize(1);
    gfx->setTextColor(C_GRAY);
    for (int i = 0; i < 3; i++) {
      int edge = lx + (i + 1) * sw;              // border between band i and i+1
      int tw = Layout_TextW(bounds[i], 1);
      gfx->setCursor(edge - tw / 2, ly + 10);
      gfx->print(bounds[i]);
    }
    // Unit, just past the right end of the bar.
    gfx->setCursor(lx + barW + 4, ly + 10);
    gfx->print("km");
  }

  // --- Compass marks ---
  // Bearing b appears on screen at angle (b - rotation), 0 = up, clockwise.
  // Small size-1 letters at r = 205 slot in between the screen dots (y=18),
  // the status line (y=30), the aircraft count (y=52) and the range row.
  {
    const int   cr = 205;
    const char* lbl[4] = { "S", "V", "J", "Z" };   // sever, vychod, jih, zapad
    const int   brg[4] = { 0, 90, 180, 270 };
    gfx->setTextSize(1);
    gfx->setTextColor(C_GRAY);
    for (int i = 0; i < 4; i++) {
      // Bearing b shows up at screen angle (b - topBearing).
      float a = (brg[i] - (int)s_topDeg) * 0.0174532925f;
      int cxp = R_CX + (int)(cr * sinf(a)) - 3;   // -3/-4 centres the glyph
      int cyp = R_CY - (int)(cr * cosf(a)) - 4;
      gfx->setCursor(cxp, cyp);
      gfx->print(lbl[i]);
    }
  }

  // Range indicator at the bottom (moved up a little to leave room for the
  // southern compass mark).
  char rbuf[16];
  snprintf(rbuf, sizeof(rbuf), "%.0f km", range);
  UI_TextCentered(rbuf, LY_RANGE, C_YELLOW, 2);

  int dotGap = 24;
  int totalW = (RANGE_COUNT - 1) * dotGap;
  int startX = R_CX - totalW / 2;
  int dotY = LY_RANGE_DOTS;
  for (int i = 0; i < RANGE_COUNT; i++) {
    int x = startX + i * dotGap;
    if (i == s_rangeIdx) gfx->fillCircle(x, dotY, 5, C_YELLOW);
    else                 gfx->drawCircle(x, dotY, 5, C_GRAY);
  }

  // --- Detail of the selected aircraft (overlay) ---
  // selIdx was resolved from the hex at the top of the frame, so this always
  // shows the aircraft that was actually tapped, with values refreshed from the
  // latest fetch (altitude, speed and climb rate update live while it is open).
  // Keep a copy of the selected aircraft so the panel can still be drawn while
  // the aircraft is temporarily missing from the data (grace period).
  if (selIdx >= 0 && selIdx < n) { s_selCache = list[selIdx]; s_selCacheOk = true; }
  const bool signalLost = (selIdx < 0) && s_selCacheOk;

  if (ScreenPlanes_DetailOpen() && s_selCacheOk) {
    const Aircraft& ac = s_selCache;
    const bool metric = Settings_MetricUnits();

    // Panel centred so it fits inside the round display.
    const int pw = 320, ph = 260;
    const int px = R_CX - pw / 2;
    const int py = R_CY - ph / 2;
    gfx->fillRoundRect(px, py, pw, ph, 14, C_DKGRAY);
    gfx->drawRoundRect(px, py, pw, ph, 14, C_CYAN);

    // Close button in the top-right corner.
    int cxx = px + pw - 24, cyy = py + 22;
    gfx->fillCircle(cxx, cyy, 15, C_RED);
    gfx->drawLine(cxx - 6, cyy - 6, cxx + 6, cyy + 6, C_WHITE);
    gfx->drawLine(cxx - 6, cyy + 6, cxx + 6, cyy - 6, C_WHITE);

    // Callsign (heading).
    gfx->setTextSize(3); gfx->setTextColor(C_YELLOW);
    gfx->setCursor(px + 18, py + 16);
    // Same fallback as the map label: no callsign broadcast -> show the ICAO
    // address, which is at least something the user can look up. It is only
    // ever a caption; the route lookup below gets the callsign or nothing.
    gfx->print(ac.callsign[0] ? ac.callsign : (ac.hex[0] ? ac.hex : "?"));

    // Data rows (font 2). Units converted according to the setting.
    char line[44];
    int ty = py + 56;
    gfx->setTextSize(2); gfx->setTextColor(C_WHITE);

    // Altitude: ft or m.
    if (metric) snprintf(line, sizeof(line), "%s: %.0f m", T(S_ALTITUDE), ac.altFt * 0.3048f);
    else        snprintf(line, sizeof(line), "%s: %.0f ft", T(S_ALTITUDE), ac.altFt);
    gfx->setCursor(px + 18, ty); gfx->print(line); ty += 26;

    // Speed: kt or km/h.
    if (metric) snprintf(line, sizeof(line), "%s: %.0f km/h", T(S_SPEED), ac.gsKt * 1.852f);
    else        snprintf(line, sizeof(line), "%s: %.0f kt", T(S_SPEED), ac.gsKt);
    gfx->setCursor(px + 18, ty); gfx->print(line); ty += 26;

    // Ground track.
    if (ac.hasTrack) snprintf(line, sizeof(line), "%s: %.0f deg", T(S_TRACK), ac.track);
    else             snprintf(line, sizeof(line), "%s: %s", T(S_TRACK), T(S_UNKNOWN));
    gfx->setCursor(px + 18, ty); gfx->print(line); ty += 26;

    // Climb/descent - short label + units so the line does not overflow.
    // An arrow instead of the words "climbing/descending".
    const char* ar = ac.baroRate > 100 ? "^" : (ac.baroRate < -100 ? "v" : "-");
    if (metric) snprintf(line, sizeof(line), "%s: %.1f m/s %s", T(S_CLIMB), ac.baroRate * 0.00508f, ar);
    else        snprintf(line, sizeof(line), "%s: %.0f ft/m %s", T(S_CLIMB), ac.baroRate, ar);
    gfx->setCursor(px + 18, ty); gfx->print(line); ty += 26;

    // Ask where this flight is going. The position goes with the request: the
    // server uses it to reject a route that does not fit where the aircraft
    // actually is, which is what used to put Athens - Istanbul on an aircraft
    // over Prague. Idempotent - a repeated call with the same aircraft does
    // nothing, and the answer is cached.
    Route_Select(ac.callsign, ac.lat, ac.lon);
    const RouteInfo* rt = Route_Get();

    // Type and registration share a row - the route below needs two lines and
    // this is where they come from. Both come straight from adsb.fi ("t" and
    // "r"), so neither costs an extra request; either can be missing.
    const char* typ = ac.type[0] ? ac.type : nullptr;
    if (typ || ac.reg[0]) {
      snprintf(line, sizeof(line), "%s: %s%s%s", T(S_TYPE),
               typ ? typ : "?",
               ac.reg[0] ? "  " : "",
               ac.reg[0] ? ac.reg : "");
      // The label plus a long type plus a registration can outgrow the panel.
      int maxCh = (pw - 36) / 12;
      if ((int)strlen(line) > maxCh) { line[maxCh - 1] = '.'; line[maxCh] = '\0'; }
      gfx->setCursor(px + 18, ty); gfx->print(line);
    }
    ty += 28;

    // Route on TWO lines - "Z:" above "Do:". One line with an arrow between
    // the cities had to shrink to fit two names side by side and ended up
    // unreadably small; split in half, each line has the whole panel width to
    // itself and can be drawn much larger.
    //
    // The route starts at the same size and in the same colour as the flight
    // data above it - it is the same kind of information, and a different
    // colour only suggested a difference that is not there. It shrinks only
    // when it has to, because city names vary wildly ("Doha" against
    // "Frankfurt am Main"). Anything that still would not fit is cut and ends
    // with a full stop rather than running over the border.
    const int avail = pw - 36;
    if (Route_GetState() == ROUTE_WAIT) {
      gfx->setTextSize(2); gfx->setTextColor(C_GRAY);
      gfx->setCursor(px + 18, ty); gfx->print(T(S_ROUTE_WAIT));
    } else if (rt && (rt->from[0] || rt->to[0])) {
      char l1[36], l2[36];
      snprintf(l1, sizeof(l1), "%s: %s", T(S_FROM), rt->from[0] ? rt->from : "?");
      snprintf(l2, sizeof(l2), "%s: %s", T(S_TO), rt->to[0] ? rt->to : "?");
      int len = (int)strlen(l1);
      if ((int)strlen(l2) > len) len = (int)strlen(l2);

      // The built-in font is 6 px wide per size step. Start at the size the
      // rest of the panel uses and only go down.
      int size = 2;
      if (len * 12 > avail) size = 1;
      int maxCh = avail / (6 * size);
      if ((int)strlen(l1) > maxCh) { l1[maxCh - 1] = '.'; l1[maxCh] = '\0'; }
      if ((int)strlen(l2) > maxCh) { l2[maxCh - 1] = '.'; l2[maxCh] = '\0'; }

      gfx->setTextSize(size); gfx->setTextColor(C_WHITE);
      gfx->setCursor(px + 18, ty);                  gfx->print(l1);
      gfx->setCursor(px + 18, ty + 8 * size + 6);   gfx->print(l2);
    }
    gfx->setTextSize(2); gfx->setTextColor(C_WHITE);

    // Why the panel opened by itself, and how far away the aircraft is.
    if (s_alert) {
      float dkm = s_alertKm;
      Geo_DistBearing(Settings_Lat(), Settings_Lon(), ac.lat, ac.lon, &dkm, nullptr);
      char note[40];
      const bool en = (Lang_Get() == LANG_EN);
      snprintf(note, sizeof(note), "%s  %.1f km",
               s_alertWindow ? (en ? "in the window" : CZT("v okne", "v okně"))
                             : (en ? "nearby" : CZT("nablizku", "nablízku")), dkm);
      gfx->setTextSize(1); gfx->setTextColor(C_CYAN);
      gfx->setCursor(px + 18, py + ph - 18);
      gfx->print(note);
    }

    // While the aircraft is missing from the data (grace period) say so, so the
    // frozen values are not mistaken for live ones.
    if (signalLost) {
      gfx->setTextSize(1); gfx->setTextColor(C_YELLOW);
      const char* lost = T(S_SIGNAL_LOST);
      gfx->setCursor(px + pw - 18 - Layout_TextW(lost, 1), py + ph - 18);
      gfx->print(lost);
    }

    // The units toggle used to live here. It moved to the settings screen -
    // it is a preference, not something you change per aircraft, and the panel
    // needed the room for the route.
  }
}
