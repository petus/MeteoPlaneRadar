// =============================================================================
//  MeteoPlaneRadar
//  Persisted settings - storage in NVS + JSON serialisation for the web UI.
//
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
// =============================================================================
#include "Settings.h"
#include "Lang.h"
#include <Preferences.h>
#include <string.h>
#include <ctype.h>

static Preferences prefs;
static const char* NS = "planeradar";

// --- WiFi ---
// Slot 0 lives under the original keys, so an updated device keeps its network.
static char s_ssid[WIFI_SLOTS][33]  = {};
static char s_wpass[WIFI_SLOTS][65] = {};
static const char* const SSID_KEY[WIFI_SLOTS] = { "ssid", "ssid1", "ssid2" };
static const char* const PASS_KEY[WIFI_SLOTS] = { "wpass", "wpass1", "wpass2" };

// --- Location ---
static double  s_lat = DEFAULT_LAT;
static double  s_lon = DEFAULT_LON;
static bool    s_hasLoc = false;

// --- Brightness / night mode ---
static uint8_t s_briDay   = 80;
static uint8_t s_briNight = 25;
static bool    s_nightAuto = true;
static int8_t  s_nightOff  = 0;
static bool    s_isNight   = false;

// --- Misc ---
static bool    s_metric = false;
static uint8_t s_lang   = LANG_CZ;

// Bit per data screen (bit 0 = clock ... bit 5 = generation mix). The default
// leaves the two energy screens OFF, which is the same courtesy the forecast
// screen got when it was new: an updated device looks exactly as it did, and
// the new screens are found in the web UI rather than sprung on the user.
//
// It also means Energy_Tick() makes no requests at all on a device whose owner
// never asked for them - the enabled bits are what that decision is read from.
static uint8_t s_scrMask = (1 << SCREEN_CLOCK_I) | (1 << SCREEN_PLANES_I) |
                           (1 << SCREEN_METEO_I) | (1 << SCREEN_FORECAST_I);
static uint16_t s_autoRot = 0;
static uint8_t s_radarSrc = RADAR_SRC_CHMU;
static bool    s_mtLegend = true;

// --- Energy ---
static uint16_t s_priceFee = 0;     // CZK/MWh on top of the spot price
static uint8_t  s_priceVat = 0;     // per cent, 0 = none
static char     s_mixCC[4] = MIX_COUNTRY_DEFAULT;

// --- Clock appearance ---
static uint8_t  s_secStyle = SEC_STYLE_DOTS;
static uint16_t s_clockCol = 0xFFFF;   // white
static uint16_t s_secCol   = 0x05FF;   // cyan
static uint8_t  s_clkFont  = CLOCK_FONT_DEFAULT;

// --- Altitude colours ---
static uint16_t s_altBand[3] = { ALT_BAND1_M_DEFAULT, ALT_BAND2_M_DEFAULT, ALT_BAND3_M_DEFAULT };
static uint16_t s_altCol[4]  = { ALT_COL0_DEFAULT, ALT_COL1_DEFAULT, ALT_COL2_DEFAULT, ALT_COL3_DEFAULT };
static const char* const ALT_BAND_KEY[3] = { "ab0", "ab1", "ab2" };
static const char* const ALT_COL_KEY[4]  = { "acol0", "acol1", "acol2", "acol3" };

// --- Storm watch, aircraft nearby, window, Home Assistant ---
static bool     s_stOnly = false;
static uint8_t  s_stDbz  = STORM_DBZ_DEFAULT;
static uint16_t s_stKm   = STORM_KM_DEFAULT;
static bool     s_paOn   = false;
static uint16_t s_paKm   = PLANE_ALERT_KM_DEFAULT;
static uint16_t s_paSec  = PLANE_ALERT_SEC_DEFAULT;
static bool     s_winOn  = false;
static uint16_t s_winAz  = WINDOW_AZ_DEFAULT;
static uint16_t s_winW   = WINDOW_WIDTH_DEFAULT;
static uint16_t s_winKm  = WINDOW_KM_DEFAULT;
static bool     s_haOn   = HA_DEFAULT_ON;

// --- Aircraft filters ---
static uint16_t s_altMin = 0;
static uint16_t s_altMax = 60000;
static bool     s_onlyCs = false;
static bool     s_sqAlert = true;
static char     s_watch[10] = "";

// --- UI state ---
static uint8_t  s_rngP = 1;
static uint8_t  s_rngM = 1;
static uint8_t  s_scr  = SCREEN_PLANES_I;
static uint16_t s_top  = 0;

// --- Admin password (see the note in Settings.h) ---
static char s_pw[33] = "";

static bool          s_uiDirty   = false;
static unsigned long s_uiDirtyAt = 0;
static void markDirty() { s_uiDirty = true; s_uiDirtyAt = millis(); }

// Immediate write for the settings that are changed rarely and deliberately
// (web UI, portal) rather than by dragging a finger.
static void putU8(const char* k, uint8_t v) {
  if (prefs.begin(NS, false)) { prefs.putUChar(k, v); prefs.end(); }
}
static void putU16(const char* k, uint16_t v) {
  if (prefs.begin(NS, false)) { prefs.putUShort(k, v); prefs.end(); }
}
static void putBool(const char* k, bool v) {
  if (prefs.begin(NS, false)) { prefs.putBool(k, v); prefs.end(); }
}
static void putStr(const char* k, const char* v) {
  if (prefs.begin(NS, false)) { prefs.putString(k, v); prefs.end(); }
}

void Settings_Begin() {
  bool migrateRotate = false;
  if (prefs.begin(NS, true)) {
    s_lat    = prefs.getDouble("lat", DEFAULT_LAT);
    s_lon    = prefs.getDouble("lon", DEFAULT_LON);
    s_hasLoc = prefs.getBool("hasLoc", false);
    // "bl" was the single brightness value up to 0.5.5 - reuse it as the day
    // level so an updated device does not suddenly go dark.
    s_briDay   = prefs.getUChar("bl", 80);
    s_briNight = prefs.getUChar("blN", 25);
    s_nightAuto = prefs.getBool("nAuto", true);
    s_nightOff  = (int8_t)prefs.getChar("nOff", 0);
    s_metric = prefs.getBool("metric", false);
    s_lang   = prefs.getUChar("lang", LANG_CZ);
    s_scrMask = prefs.getUChar("scrM", s_scrMask);
    // Cycling interval moved from minutes to seconds - see Settings.h. The old
    // key is converted exactly once, so an updated device keeps its setting.
    if (prefs.isKey("autoRS")) {
      s_autoRot = prefs.getUShort("autoRS", 0);
    } else {
      s_autoRot = (uint16_t)prefs.getUChar("autoR", 0) * 60;
      migrateRotate = true;          // written below, the handle is read-only here
    }
    s_radarSrc = prefs.getUChar("radSrc", RADAR_SRC_CHMU);
    s_mtLegend = prefs.getBool("mtLeg", true);
    s_priceFee = prefs.getUShort("priceFee", 0);
    s_priceVat = prefs.getUChar("priceVat", 0);
    if (prefs.getString("mixCC", s_mixCC, sizeof(s_mixCC)) == 0)
      strncpy(s_mixCC, MIX_COUNTRY_DEFAULT, sizeof(s_mixCC) - 1);
    s_secStyle = prefs.getUChar("secSt", SEC_STYLE_DOTS);
    s_clockCol = prefs.getUShort("clkC", 0xFFFF);
    s_secCol   = prefs.getUShort("secC", 0x05FF);
    s_clkFont  = prefs.getUChar("clkF", CLOCK_FONT_DEFAULT);
    for (int i = 0; i < 3; i++) s_altBand[i] = prefs.getUShort(ALT_BAND_KEY[i], s_altBand[i]);
    for (int i = 0; i < 4; i++) s_altCol[i]  = prefs.getUShort(ALT_COL_KEY[i], s_altCol[i]);
    s_stOnly = prefs.getBool("stOnly", false);
    s_stDbz  = prefs.getUChar("stDbz", STORM_DBZ_DEFAULT);
    s_stKm   = prefs.getUShort("stKm", STORM_KM_DEFAULT);
    s_paOn   = prefs.getBool("paOn", false);
    s_paKm   = prefs.getUShort("paKm", PLANE_ALERT_KM_DEFAULT);
    s_paSec  = prefs.getUShort("paSec", PLANE_ALERT_SEC_DEFAULT);
    s_winOn  = prefs.getBool("winOn", false);
    s_winAz  = prefs.getUShort("winAz", WINDOW_AZ_DEFAULT);
    s_winW   = prefs.getUShort("winW", WINDOW_WIDTH_DEFAULT);
    s_winKm  = prefs.getUShort("winKm", WINDOW_KM_DEFAULT);
    s_haOn   = prefs.getBool("haOn", HA_DEFAULT_ON);
    s_altMin = prefs.getUShort("altLo", 0);
    s_altMax = prefs.getUShort("altHi", 60000);
    s_onlyCs = prefs.getBool("onlyCs", false);
    s_sqAlert = prefs.getBool("sqAl", true);
    prefs.getString("watch", s_watch, sizeof(s_watch));
    s_rngP   = prefs.getUChar("rngP", 1);
    s_rngM   = prefs.getUChar("rngM", 1);
    s_scr    = prefs.getUChar("scr", SCREEN_PLANES_I);
    s_top    = prefs.getUShort("topb", 0);
    prefs.getString("pw", s_pw, sizeof(s_pw));
    for (int i = 0; i < WIFI_SLOTS; i++) {
      // isKey() first: getString() on a missing key logs an error line for
      // every empty slot on every boot.
      if (prefs.isKey(SSID_KEY[i])) prefs.getString(SSID_KEY[i], s_ssid[i], sizeof(s_ssid[i]));
      if (prefs.isKey(PASS_KEY[i])) prefs.getString(PASS_KEY[i], s_wpass[i], sizeof(s_wpass[i]));
    }
    prefs.end();
  }
  if (s_altMax == 0) s_altMax = 60000;
  if (s_autoRot > 3600) s_autoRot = 3600;
  if (s_radarSrc > RADAR_SRC_MAX) s_radarSrc = RADAR_SRC_CHMU;
  if (s_clkFont > CLOCK_FONT_MAX) s_clkFont = CLOCK_FONT_DEFAULT;
  if (!(s_altBand[0] < s_altBand[1] && s_altBand[1] < s_altBand[2])) {
    s_altBand[0] = ALT_BAND1_M_DEFAULT; s_altBand[1] = ALT_BAND2_M_DEFAULT; s_altBand[2] = ALT_BAND3_M_DEFAULT;
  }
  if (migrateRotate && prefs.begin(NS, false)) {
    prefs.putUShort("autoRS", s_autoRot);
    prefs.remove("autoR");           // the old key would only confuse later
    prefs.end();
    if (s_autoRot) Serial.printf("Nastaveni: stridani prevedeno na %u s\n", s_autoRot);
  }
  Lang_Set(s_lang);
}

// --- WiFi -------------------------------------------------------------------
const char* Settings_WifiSsid(uint8_t slot) { return slot < WIFI_SLOTS ? s_ssid[slot]  : ""; }
const char* Settings_WifiPass(uint8_t slot) { return slot < WIFI_SLOTS ? s_wpass[slot] : ""; }
bool Settings_HasWifi() {
  for (int i = 0; i < WIFI_SLOTS; i++) if (s_ssid[i][0]) return true;
  return false;
}

static void storeWifiSlots() {
  if (!prefs.begin(NS, false)) return;
  for (int i = 0; i < WIFI_SLOTS; i++) {
    prefs.putString(SSID_KEY[i], s_ssid[i]);
    prefs.putString(PASS_KEY[i], s_wpass[i]);
  }
  prefs.end();
}

static void copyStr(char* dst, size_t cap, const char* src) {
  strncpy(dst, src ? src : "", cap - 1);
  dst[cap - 1] = '\0';
}

void Settings_SetWifiSlot(uint8_t slot, const char* ssid, const char* pass) {
  if (slot >= WIFI_SLOTS) return;
  if (!ssid || !*ssid) {
    s_ssid[slot][0] = '\0';
    s_wpass[slot][0] = '\0';
  } else {
    // A renamed slot with no password typed keeps nothing: the old password
    // belonged to a different network.
    const bool sameNet = strcmp(s_ssid[slot], ssid) == 0;
    copyStr(s_ssid[slot], sizeof(s_ssid[slot]), ssid);
    if (pass)          copyStr(s_wpass[slot], sizeof(s_wpass[slot]), pass);
    else if (!sameNet) s_wpass[slot][0] = '\0';
  }
  storeWifiSlots();
}

void Settings_SetWifi(const char* ssid, const char* pass) {
  if (!ssid || !*ssid) return;
  // Already known: just the password.
  for (int i = 0; i < WIFI_SLOTS; i++) {
    if (strcmp(s_ssid[i], ssid) == 0) {
      copyStr(s_wpass[i], sizeof(s_wpass[i]), pass);
      storeWifiSlots();
      return;
    }
  }
  // New: first place, the others move down and the last one falls off. The
  // network just typed in is the one the user wants now.
  for (int i = WIFI_SLOTS - 1; i > 0; i--) {
    memcpy(s_ssid[i],  s_ssid[i - 1],  sizeof(s_ssid[i]));
    memcpy(s_wpass[i], s_wpass[i - 1], sizeof(s_wpass[i]));
  }
  copyStr(s_ssid[0],  sizeof(s_ssid[0]),  ssid);
  copyStr(s_wpass[0], sizeof(s_wpass[0]), pass);
  storeWifiSlots();
}

void Settings_ClearWifi() {
  memset(s_ssid, 0, sizeof(s_ssid));
  memset(s_wpass, 0, sizeof(s_wpass));
  storeWifiSlots();
}

// --- Location ---------------------------------------------------------------
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

// --- Brightness -------------------------------------------------------------
uint8_t Settings_Backlight()   { return s_isNight ? s_briNight : s_briDay; }
uint8_t Settings_BrightDay()   { return s_briDay; }
uint8_t Settings_BrightNight() { return s_briNight; }

// Dragging the slider used to write flash on every touch sample - dozens of
// erase/write cycles for one adjustment. It goes through the same debounce as
// the rest of the UI state.
void Settings_SetBacklight(uint8_t pct) {
  if (s_isNight) { if (pct == s_briNight) return; s_briNight = pct; }
  else           { if (pct == s_briDay)   return; s_briDay   = pct; }
  markDirty();
}
void Settings_SetBrightDay(uint8_t pct)   { if (pct != s_briDay)   { s_briDay = pct;   markDirty(); } }
void Settings_SetBrightNight(uint8_t pct) { if (pct != s_briNight) { s_briNight = pct; markDirty(); } }
bool Settings_NightAuto() { return s_nightAuto; }
void Settings_SetNightAuto(bool on) { s_nightAuto = on; putBool("nAuto", on); }
int8_t Settings_NightOffsetMin() { return s_nightOff; }
void   Settings_SetNightOffsetMin(int8_t m) {
  if (m >  NIGHT_OFFSET_MIN_LIMIT) m =  NIGHT_OFFSET_MIN_LIMIT;
  if (m < -NIGHT_OFFSET_MIN_LIMIT) m = -NIGHT_OFFSET_MIN_LIMIT;
  s_nightOff = m;
  if (prefs.begin(NS, false)) { prefs.putChar("nOff", m); prefs.end(); }
}
bool Settings_IsNight() { return s_isNight; }
void Settings_SetNight(bool night) { s_isNight = night; }

// --- Units, language --------------------------------------------------------
bool Settings_MetricUnits() { return s_metric; }
void Settings_SetMetricUnits(bool metric) { s_metric = metric; putBool("metric", metric); }
uint8_t Settings_Language() { return s_lang; }
void    Settings_SetLanguage(uint8_t l) {
  s_lang = (l == LANG_EN) ? LANG_EN : LANG_CZ;
  Lang_Set(s_lang);
  putU8("lang", s_lang);
}

// --- Screens ----------------------------------------------------------------
bool Settings_ScreenEnabled(uint8_t idx) {
  if (idx == SCREEN_SETTINGS_I) return true;      // always reachable
  if (idx >= SCREEN_SETTINGS_I) return false;
  return (s_scrMask >> idx) & 1;
}

void Settings_SetScreenEnabled(uint8_t idx, bool on) {
  if (idx >= SCREEN_SETTINGS_I) return;
  uint8_t next = on ? (s_scrMask | (1 << idx)) : (s_scrMask & ~(1 << idx));
  // Never allow the last data screen to be turned off. With all of them gone
  // the device would boot into Settings and show nothing else - technically
  // recoverable, but it looks broken.
  if (next == 0) return;
  s_scrMask = next;
  putU8("scrM", s_scrMask);
}

uint8_t Settings_EnabledCount() {
  uint8_t n = 0;
  for (uint8_t i = 0; i < SCREEN_SETTINGS_I; i++) if (Settings_ScreenEnabled(i)) n++;
  return n;
}

uint16_t Settings_AutoRotateSec() { return s_autoRot; }
void     Settings_SetAutoRotateSec(uint16_t s) {
  if (s > 3600) s = 3600;
  s_autoRot = s;
  putU16("autoRS", s);
}

// --- Weather radar ----------------------------------------------------------
uint8_t Settings_RadarSource() { return s_radarSrc; }
void    Settings_SetRadarSource(uint8_t s) {
  s_radarSrc = (s <= RADAR_SRC_MAX) ? s : RADAR_SRC_CHMU;
  putU8("radSrc", s_radarSrc);
}
bool Settings_RadarIsChmu() { return s_radarSrc != RADAR_SRC_RAINVIEWER; }

// --- Storm watch ------------------------------------------------------------
bool     Settings_StormOnly() { return s_stOnly; }
void     Settings_SetStormOnly(bool on) { s_stOnly = on; putBool("stOnly", on); }
uint8_t  Settings_StormDbz() { return s_stDbz; }
void     Settings_SetStormDbz(uint8_t dbz) {
  if (dbz < 4) dbz = 4;
  if (dbz > 60) dbz = 60;
  s_stDbz = dbz; putU8("stDbz", dbz);
}
uint16_t Settings_StormKm() { return s_stKm; }
void     Settings_SetStormKm(uint16_t km) {
  if (km < 5) km = 5;
  if (km > 250) km = 250;          // CHMU covers about 500 x 280 km
  s_stKm = km; putU16("stKm", km);
}

// --- Aircraft nearby, window ------------------------------------------------
bool     Settings_PlaneAlert() { return s_paOn; }
void     Settings_SetPlaneAlert(bool on) { s_paOn = on; putBool("paOn", on); }
uint16_t Settings_PlaneAlertKm() { return s_paKm; }
void     Settings_SetPlaneAlertKm(uint16_t km) {
  if (km < 1) km = 1;
  if (km > 50) km = 50;
  s_paKm = km; putU16("paKm", km);
}
uint16_t Settings_PlaneAlertSec() { return s_paSec; }
void     Settings_SetPlaneAlertSec(uint16_t s) {
  if (s < 5) s = 5;
  if (s > 300) s = 300;
  s_paSec = s; putU16("paSec", s);
}
bool     Settings_WindowOn() { return s_winOn; }
void     Settings_SetWindowOn(bool on) { s_winOn = on; putBool("winOn", on); }
uint16_t Settings_WindowAz() { return s_winAz; }
void     Settings_SetWindowAz(uint16_t deg) { deg %= 360; s_winAz = deg; putU16("winAz", deg); }
uint16_t Settings_WindowWidth() { return s_winW; }
void     Settings_SetWindowWidth(uint16_t deg) {
  if (deg < 10) deg = 10;
  if (deg > 300) deg = 300;
  s_winW = deg; putU16("winW", deg);
}
uint16_t Settings_WindowKm() { return s_winKm; }
void     Settings_SetWindowKm(uint16_t km) {
  if (km < 1) km = 1;
  if (km > 100) km = 100;
  s_winKm = km; putU16("winKm", km);
}

// --- Home Assistant ---------------------------------------------------------
bool Settings_HaEnabled() { return s_haOn; }
void Settings_SetHaEnabled(bool on) { s_haOn = on; putBool("haOn", on); }

bool Settings_MeteoLegend() { return s_mtLegend; }
void Settings_SetMeteoLegend(bool on) { s_mtLegend = on; putBool("mtLeg", on); }

// --- Energy -----------------------------------------------------------------
uint16_t Settings_PriceFee() { return s_priceFee; }
void     Settings_SetPriceFee(uint16_t czkPerMwh) {
  // A surcharge larger than the highest spot price ever recorded is a typo -
  // almost always CZK/kWh typed into a CZK/MWh field. Clamping keeps it from
  // turning the whole ring one colour without silently discarding the value.
  if (czkPerMwh > 20000) czkPerMwh = 20000;
  s_priceFee = czkPerMwh;
  putU16("priceFee", czkPerMwh);
}
uint8_t Settings_PriceVat() { return s_priceVat; }
void    Settings_SetPriceVat(uint8_t pct) {
  if (pct > 100) pct = 100;
  s_priceVat = pct;
  putU8("priceVat", pct);
}

const char* Settings_MixCountry() { return s_mixCC; }
void Settings_SetMixCountry(const char* code) {
  // Exactly two letters, lower cased. Anything else is refused rather than
  // stored: a bad code would turn into a 404 every fifteen minutes and the
  // screen would look broken with no clue why.
  if (!code) return;
  char c[4] = {0};
  int n = 0;
  for (const char* p = code; *p && n < 2; p++) {
    if (*p >= 'A' && *p <= 'Z')      c[n++] = (char)(*p - 'A' + 'a');
    else if (*p >= 'a' && *p <= 'z') c[n++] = *p;
    else return;
  }
  if (n != 2) return;
  strncpy(s_mixCC, c, sizeof(s_mixCC) - 1);
  s_mixCC[sizeof(s_mixCC) - 1] = '\0';
  putStr("mixCC", s_mixCC);
}

// --- Clock appearance -------------------------------------------------------
uint8_t  Settings_SecondsStyle() { return s_secStyle; }
void     Settings_SetSecondsStyle(uint8_t s) { if (s > SEC_STYLE_COMET) s = 0; s_secStyle = s; putU8("secSt", s); }
uint16_t Settings_ClockColor() { return s_clockCol; }
void     Settings_SetClockColor(uint16_t c) { s_clockCol = c; putU16("clkC", c); }
uint16_t Settings_SecondsColor() { return s_secCol; }
void     Settings_SetSecondsColor(uint16_t c) { s_secCol = c; putU16("secC", c); }
uint8_t  Settings_ClockFont() { return s_clkFont; }
void     Settings_SetClockFont(uint8_t f) {
  if (f > CLOCK_FONT_MAX) f = CLOCK_FONT_DEFAULT;
  s_clkFont = f; putU8("clkF", f);
}

// --- Altitude colours -------------------------------------------------------
uint16_t Settings_AltBandM(uint8_t i) { return i < 3 ? s_altBand[i] : 0; }
void     Settings_SetAltBandsM(uint16_t b0, uint16_t b1, uint16_t b2) {
  // Out of order is refused as a whole rather than half-applied: the bands
  // would overlap and an aircraft would fit two colours at once.
  if (!(b0 > 0 && b0 < b1 && b1 < b2 && b2 <= 20000)) return;
  s_altBand[0] = b0; s_altBand[1] = b1; s_altBand[2] = b2;
  if (prefs.begin(NS, false)) {
    for (int i = 0; i < 3; i++) prefs.putUShort(ALT_BAND_KEY[i], s_altBand[i]);
    prefs.end();
  }
}
uint16_t Settings_AltColor(uint8_t band) { return band < 4 ? s_altCol[band] : 0xFFFF; }
void     Settings_SetAltColor(uint8_t band, uint16_t c) {
  if (band >= 4) return;
  s_altCol[band] = c;
  putU16(ALT_COL_KEY[band], c);
}

// --- Aircraft filters -------------------------------------------------------
uint16_t Settings_AltMinFt() { return s_altMin; }
uint16_t Settings_AltMaxFt() { return s_altMax; }
void     Settings_SetAltRangeFt(uint16_t lo, uint16_t hi) {
  if (hi <= lo) { lo = 0; hi = 60000; }           // nonsense range = no filter
  s_altMin = lo; s_altMax = hi;
  if (prefs.begin(NS, false)) {
    prefs.putUShort("altLo", lo); prefs.putUShort("altHi", hi); prefs.end();
  }
}
bool Settings_OnlyWithCallsign() { return s_onlyCs; }
void Settings_SetOnlyWithCallsign(bool on) { s_onlyCs = on; putBool("onlyCs", on); }
bool Settings_SquawkAlert() { return s_sqAlert; }
void Settings_SetSquawkAlert(bool on) { s_sqAlert = on; putBool("sqAl", on); }
const char* Settings_WatchCallsign() { return s_watch; }
void Settings_SetWatchCallsign(const char* s) {
  if (!s) s = "";
  strncpy(s_watch, s, sizeof(s_watch) - 1);
  s_watch[sizeof(s_watch) - 1] = '\0';
  // Compared against callsigns and hex addresses, both of which arrive upper
  // case from adsb.fi - normalise here so the user does not have to.
  for (char* p = s_watch; *p; p++) *p = toupper((unsigned char)*p);
  putStr("watch", s_watch);
}

// --- UI state ---------------------------------------------------------------
uint8_t Settings_PlaneRange() { return s_rngP; }
void    Settings_SetPlaneRange(uint8_t idx) { if (idx != s_rngP) { s_rngP = idx; markDirty(); } }
uint8_t Settings_MeteoRange() { return s_rngM; }
void    Settings_SetMeteoRange(uint8_t idx) { if (idx != s_rngM) { s_rngM = idx; markDirty(); } }
uint16_t Settings_TopBearing() { return s_top; }
void     Settings_SetTopBearing(uint16_t deg) {
  deg %= 360;
  if (deg != s_top) { s_top = deg; markDirty(); }
}
uint8_t Settings_Screen() { return s_scr; }
void    Settings_SetScreen(uint8_t idx) { if (idx != s_scr) { s_scr = idx; markDirty(); } }

// --- Admin password ---------------------------------------------------------
bool Settings_HasAdminPassword() { return s_pw[0] != '\0'; }
const char* Settings_AdminPassword() { return s_pw; }

void Settings_SetAdminPassword(const char* plain) {
  if (!plain) plain = "";
  // A single space is the agreed way to say "remove the password" from the web
  // form, where an empty field has to mean "leave it alone" - otherwise every
  // save without retyping it would silently switch the protection off.
  if (strcmp(plain, " ") == 0) plain = "";
  strncpy(s_pw, plain, sizeof(s_pw) - 1);
  s_pw[sizeof(s_pw) - 1] = '\0';
  putStr("pw", s_pw);
}

bool Settings_CheckAdminPassword(const char* plain) {
  if (!Settings_HasAdminPassword()) return true;    // protection disabled
  if (!plain) return false;
  // Constant time over the full buffer: comparing only up to the first
  // difference leaks how much of a guess was right.
  uint8_t diff = 0;
  size_t n = sizeof(s_pw);
  for (size_t i = 0; i < n; i++) {
    char a = (i < strlen(plain)) ? plain[i] : '\0';
    diff |= (uint8_t)(a ^ s_pw[i]);
  }
  return diff == 0;
}

// --- Serialisation ----------------------------------------------------------
void Settings_ToJson(JsonObject o) {
  o["lat"] = s_lat;
  o["lon"] = s_lon;
  o["hasLoc"] = s_hasLoc;
  o["lang"] = s_lang;
  o["metric"] = s_metric;
  o["briDay"] = s_briDay;
  o["briNight"] = s_briNight;
  o["nightAuto"] = s_nightAuto;
  o["nightOffset"] = s_nightOff;
  o["radarSrc"] = s_radarSrc;
  o["meteoLegend"] = s_mtLegend;
  o["priceFee"] = s_priceFee;
  o["priceVat"] = s_priceVat;
  o["mixCountry"] = s_mixCC;
  o["autoRotate"] = s_autoRot;   // seconds
  o["topBearing"] = s_top;
  o["secStyle"] = s_secStyle;
  o["clockColor"] = s_clockCol;
  o["secColor"] = s_secCol;
  o["clockFont"] = s_clkFont;
  {
    JsonArray b = o["altBands"].to<JsonArray>();
    for (int i = 0; i < 3; i++) b.add(s_altBand[i]);
    JsonArray c = o["altColors"].to<JsonArray>();
    for (int i = 0; i < 4; i++) c.add(s_altCol[i]);
  }
  o["stormOnly"] = s_stOnly;
  o["stormDbz"]  = s_stDbz;
  o["stormKm"]   = s_stKm;
  o["planeAlert"]    = s_paOn;
  o["planeAlertKm"]  = s_paKm;
  o["planeAlertSec"] = s_paSec;
  o["windowOn"]    = s_winOn;
  o["windowAz"]    = s_winAz;
  o["windowWidth"] = s_winW;
  o["windowKm"]    = s_winKm;
  o["haEnabled"]   = s_haOn;
  o["altMin"] = s_altMin;
  o["altMax"] = s_altMax;
  o["onlyCallsign"] = s_onlyCs;
  o["squawkAlert"] = s_sqAlert;
  o["watch"] = s_watch;
  o["hasPassword"] = Settings_HasAdminPassword();
  JsonObject scr = o["screens"].to<JsonObject>();
  scr["clock"]    = Settings_ScreenEnabled(SCREEN_CLOCK_I);
  scr["planes"]   = Settings_ScreenEnabled(SCREEN_PLANES_I);
  scr["meteo"]    = Settings_ScreenEnabled(SCREEN_METEO_I);
  scr["forecast"] = Settings_ScreenEnabled(SCREEN_FORECAST_I);
  scr["price"]    = Settings_ScreenEnabled(SCREEN_PRICE_I);
  scr["mix"]      = Settings_ScreenEnabled(SCREEN_MIX_I);
}

bool Settings_FromJson(JsonObjectConst in) {
  bool changed = false;
  auto setIf = [&](const char* key, auto fn) {
    JsonVariantConst v = in[key];
    if (!v.isNull()) { fn(v); changed = true; }
  };

  // Location only counts when both halves are there and sane - a half-applied
  // position would put the radar in the Gulf of Guinea.
  if (!in["lat"].isNull() && !in["lon"].isNull()) {
    double la = in["lat"].as<double>(), lo = in["lon"].as<double>();
    if (la >= -90 && la <= 90 && lo >= -180 && lo <= 180 && (la != 0 || lo != 0)) {
      Settings_SetLocation(la, lo);
      changed = true;
    }
  }
  setIf("lang",         [](JsonVariantConst v){ Settings_SetLanguage(v.as<uint8_t>()); });
  setIf("metric",       [](JsonVariantConst v){ Settings_SetMetricUnits(v.as<bool>()); });
  setIf("briDay",       [](JsonVariantConst v){ Settings_SetBrightDay(v.as<uint8_t>()); });
  setIf("briNight",     [](JsonVariantConst v){ Settings_SetBrightNight(v.as<uint8_t>()); });
  setIf("nightAuto",    [](JsonVariantConst v){ Settings_SetNightAuto(v.as<bool>()); });
  setIf("nightOffset",  [](JsonVariantConst v){ Settings_SetNightOffsetMin(v.as<int8_t>()); });
  setIf("radarSrc",     [](JsonVariantConst v){ Settings_SetRadarSource(v.as<uint8_t>()); });
  setIf("meteoLegend",  [](JsonVariantConst v){ Settings_SetMeteoLegend(v.as<bool>()); });
  setIf("priceFee",     [](JsonVariantConst v){ Settings_SetPriceFee(v.as<uint16_t>()); });
  setIf("priceVat",     [](JsonVariantConst v){ Settings_SetPriceVat(v.as<uint8_t>()); });
  setIf("mixCountry",   [](JsonVariantConst v){ Settings_SetMixCountry(v.as<const char*>()); });
  setIf("autoRotate",   [](JsonVariantConst v){ Settings_SetAutoRotateSec(v.as<uint16_t>()); });
  setIf("topBearing",   [](JsonVariantConst v){ Settings_SetTopBearing(v.as<uint16_t>()); });
  setIf("secStyle",     [](JsonVariantConst v){ Settings_SetSecondsStyle(v.as<uint8_t>()); });
  setIf("clockColor",   [](JsonVariantConst v){ Settings_SetClockColor(v.as<uint16_t>()); });
  setIf("secColor",     [](JsonVariantConst v){ Settings_SetSecondsColor(v.as<uint16_t>()); });
  setIf("clockFont",    [](JsonVariantConst v){ Settings_SetClockFont(v.as<uint8_t>()); });
  setIf("stormOnly",    [](JsonVariantConst v){ Settings_SetStormOnly(v.as<bool>()); });
  setIf("stormDbz",     [](JsonVariantConst v){ Settings_SetStormDbz(v.as<uint8_t>()); });
  setIf("stormKm",      [](JsonVariantConst v){ Settings_SetStormKm(v.as<uint16_t>()); });
  setIf("planeAlert",   [](JsonVariantConst v){ Settings_SetPlaneAlert(v.as<bool>()); });
  setIf("planeAlertKm", [](JsonVariantConst v){ Settings_SetPlaneAlertKm(v.as<uint16_t>()); });
  setIf("planeAlertSec",[](JsonVariantConst v){ Settings_SetPlaneAlertSec(v.as<uint16_t>()); });
  setIf("windowOn",     [](JsonVariantConst v){ Settings_SetWindowOn(v.as<bool>()); });
  setIf("windowAz",     [](JsonVariantConst v){ Settings_SetWindowAz(v.as<uint16_t>()); });
  setIf("windowWidth",  [](JsonVariantConst v){ Settings_SetWindowWidth(v.as<uint16_t>()); });
  setIf("windowKm",     [](JsonVariantConst v){ Settings_SetWindowKm(v.as<uint16_t>()); });
  setIf("haEnabled",    [](JsonVariantConst v){ Settings_SetHaEnabled(v.as<bool>()); });

  // Altitude bands go in as a set of three, for the same reason the filter
  // range goes in as a pair: applied one at a time they would pass through an
  // out-of-order state and be refused.
  {
    JsonArrayConst b = in["altBands"];
    if (!b.isNull() && b.size() == 3) {
      Settings_SetAltBandsM(b[0].as<uint16_t>(), b[1].as<uint16_t>(), b[2].as<uint16_t>());
      changed = true;
    }
    JsonArrayConst c = in["altColors"];
    if (!c.isNull()) {
      for (size_t i = 0; i < 4 && i < c.size(); i++) Settings_SetAltColor((uint8_t)i, c[i].as<uint16_t>());
      changed = true;
    }
  }
  setIf("onlyCallsign", [](JsonVariantConst v){ Settings_SetOnlyWithCallsign(v.as<bool>()); });
  setIf("squawkAlert",  [](JsonVariantConst v){ Settings_SetSquawkAlert(v.as<bool>()); });
  setIf("watch",        [](JsonVariantConst v){ Settings_SetWatchCallsign(v.as<const char*>()); });

  if (!in["altMin"].isNull() || !in["altMax"].isNull()) {
    uint16_t lo = in["altMin"].isNull() ? s_altMin : in["altMin"].as<uint16_t>();
    uint16_t hi = in["altMax"].isNull() ? s_altMax : in["altMax"].as<uint16_t>();
    Settings_SetAltRangeFt(lo, hi);
    changed = true;
  }

  JsonObjectConst scr = in["screens"];
  if (!scr.isNull()) {
    struct { const char* key; uint8_t idx; } M[] = {
      { "clock",    SCREEN_CLOCK_I },
      { "planes",   SCREEN_PLANES_I },
      { "meteo",    SCREEN_METEO_I },
      { "forecast", SCREEN_FORECAST_I },
      { "price",    SCREEN_PRICE_I },
      { "mix",      SCREEN_MIX_I },
    };
    for (auto& m : M) {
      JsonVariantConst v = scr[m.key];
      if (!v.isNull()) { Settings_SetScreenEnabled(m.idx, v.as<bool>()); changed = true; }
    }
  }
  return changed;
}

// --- Debounced flush --------------------------------------------------------
void Settings_Tick() {
  if (!s_uiDirty) return;
  if (millis() - s_uiDirtyAt < 2000) return;
  if (prefs.begin(NS, false)) {
    prefs.putUChar("rngP", s_rngP);
    prefs.putUChar("rngM", s_rngM);
    prefs.putUChar("scr",  s_scr);
    prefs.putUShort("topb", s_top);
    prefs.putUChar("bl",   s_briDay);
    prefs.putUChar("blN",  s_briNight);
    prefs.end();
  }
  s_uiDirty = false;
}

void Settings_ClearAll() {
  if (prefs.begin(NS, false)) { prefs.clear(); prefs.end(); }
  s_lat = DEFAULT_LAT; s_lon = DEFAULT_LON; s_hasLoc = false;
  s_briDay = 80; s_briNight = 25; s_nightAuto = true; s_nightOff = 0; s_isNight = false;
  s_metric = false; s_lang = LANG_CZ; Lang_Set(s_lang);
  s_scrMask = (1 << SCREEN_CLOCK_I) | (1 << SCREEN_PLANES_I) |
              (1 << SCREEN_METEO_I) | (1 << SCREEN_FORECAST_I);
  s_autoRot = 0; s_radarSrc = RADAR_SRC_CHMU; s_mtLegend = true;
  s_priceFee = 0; s_priceVat = 0;
  strncpy(s_mixCC, MIX_COUNTRY_DEFAULT, sizeof(s_mixCC) - 1);
  s_secStyle = SEC_STYLE_DOTS; s_clockCol = 0xFFFF; s_secCol = 0x05FF;
  s_clkFont = CLOCK_FONT_DEFAULT;
  s_altBand[0] = ALT_BAND1_M_DEFAULT; s_altBand[1] = ALT_BAND2_M_DEFAULT; s_altBand[2] = ALT_BAND3_M_DEFAULT;
  s_altCol[0] = ALT_COL0_DEFAULT; s_altCol[1] = ALT_COL1_DEFAULT;
  s_altCol[2] = ALT_COL2_DEFAULT; s_altCol[3] = ALT_COL3_DEFAULT;
  s_stOnly = false; s_stDbz = STORM_DBZ_DEFAULT; s_stKm = STORM_KM_DEFAULT;
  s_paOn = false; s_paKm = PLANE_ALERT_KM_DEFAULT; s_paSec = PLANE_ALERT_SEC_DEFAULT;
  s_winOn = false; s_winAz = WINDOW_AZ_DEFAULT; s_winW = WINDOW_WIDTH_DEFAULT; s_winKm = WINDOW_KM_DEFAULT;
  s_haOn = HA_DEFAULT_ON;
  s_altMin = 0; s_altMax = 60000; s_onlyCs = false; s_sqAlert = true; s_watch[0] = '\0';
  s_rngP = 1; s_rngM = 1; s_scr = SCREEN_PLANES_I; s_top = 0;
  s_pw[0] = '\0';
  memset(s_ssid, 0, sizeof(s_ssid));
  memset(s_wpass, 0, sizeof(s_wpass));
  s_uiDirty = false;
}
