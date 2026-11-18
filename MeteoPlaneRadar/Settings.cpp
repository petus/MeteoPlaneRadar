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
// 0.9.2: what the automatic switch follows, and the clock window for it.
static uint8_t  s_nightSrc  = NIGHT_SRC_SUN;
static uint16_t s_nightFrom = 22 * 60, s_nightTo = 6 * 60;   // minutes of the day
// 0.9.2: the display switched off for part of the night.
static bool     s_darkOn = false;
static uint16_t s_darkFrom = 23 * 60, s_darkTo = 6 * 60;
static bool     s_darkAlerts = true;          // an alert lights it up
// 0.9.2: the name day on the clock (Czech and Slovak only).
static bool     s_nameDay = true;
// 0.9.3: clock screen layout.
static uint8_t  s_clockLay = CLOCK_LAYOUT_BASIC;

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
// 0.9.6: 16 bits - the low byte stays in "scrM", bit 8 and up in "scrM2".
static uint16_t s_scrMask = (1 << SCREEN_CLOCK_I) | (1 << SCREEN_PLANES_I) |
                            (1 << SCREEN_METEO_I) | (1 << SCREEN_FORECAST_I);
static uint16_t s_autoRot = 0;
static uint8_t  s_dotsHide = 0;               // 0.9.1: seconds, 0 = always shown
static uint8_t s_radarSrc = RADAR_SRC_CHMU;
static bool    s_mtLegend = true;
static bool    s_route = true;                // 0.9.1: "from - to" in the aircraft detail

// --- Energy ---
static uint16_t s_priceFee = 0;     // CZK/MWh on top of the spot price
static uint8_t  s_priceVat = 0;     // per cent, 0 = none
static char     s_mixCC[4] = MIX_COUNTRY_DEFAULT;
static char     s_fxHome[4] = "";     // 0.9.6: "" = automatic
static char     s_fxList[FX_MAX * 4] = "";   // "EUR,USD,..." or "" = default
static char     s_crList[CR_MAX * 5 + 1] = "";  // "BTC,ETH,..." or "" = default
static char     s_cgKey[72] = "";               // CoinGecko Demo key, optional
static uint8_t  s_routeSrc = ROUTE_SRC_LOL;     // 0.9.8: route source asked first

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
static uint8_t  s_paMode = PA_MODE_DETAIL;   // 0.9.9: see Settings.h
static bool     s_winOn  = false;
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
static int      s_oldWinAz = -1;    // 0.8.6 migration, see Settings_Begin

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

// --- 3D printer (0.9.0) ------------------------------------------------------
static bool     s_prOn = false;
static uint8_t  s_prType = PRINT_TYPE_PRUSA;
static char     s_prHost[64] = "";
static char     s_prUser[32] = "maker";        // PrusaLink's default user
static char     s_prKey[64] = "";              // kept in NVS
static char     s_prPass[64] = "";
static bool     s_prExp = false;               // secrets go into the export/import
static bool     s_prAFin = true, s_prAErr = true, s_prAPau = true;
static uint8_t  s_prNight = PRINT_NIGHT_BLINK;
static uint32_t s_prGen = 1;     // loop task only

// Printable ASCII only, trimmed: these end up in an HTTP header or URL.
static void copyClean(char* dst, size_t cap, const char* src) {
  size_t n = 0;
  if (src) {
    while (*src == ' ') src++;
    for (; *src && n + 1 < cap; src++)
      if ((uint8_t)*src >= 0x21 && (uint8_t)*src <= 0x7E) dst[n++] = *src;
  }
  dst[n] = '\0';
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
    s_nightSrc  = prefs.getUChar("nSrc", NIGHT_SRC_SUN);
    if (s_nightSrc > NIGHT_SRC_CLOCK) s_nightSrc = NIGHT_SRC_SUN;
    s_nightFrom = prefs.getUShort("nFrom", 22 * 60) % 1440;
    s_nightTo   = prefs.getUShort("nTo", 6 * 60) % 1440;
    s_darkOn    = prefs.getBool("dkOn", false);
    s_darkFrom  = prefs.getUShort("dkFrom", 23 * 60) % 1440;
    s_darkTo    = prefs.getUShort("dkTo", 6 * 60) % 1440;
    s_darkAlerts = prefs.getBool("dkAl", true);
    s_nameDay   = prefs.getBool("nDay", true);
    s_clockLay  = prefs.getUChar("clkLay", CLOCK_LAYOUT_BASIC);
    if (s_clockLay > CLOCK_LAYOUT_HOURS) s_clockLay = CLOCK_LAYOUT_BASIC;
    s_metric = prefs.getBool("metric", false);
    s_lang   = prefs.getUChar("lang", LANG_CZ);
    if (s_lang >= LANG_N) s_lang = LANG_CZ;
    s_scrMask = (uint16_t)(prefs.getUChar("scrM", (uint8_t)s_scrMask) |
                           ((uint16_t)prefs.getUChar("scrM2", 0) << 8));
    // Cycling interval moved from minutes to seconds - see Settings.h. The old
    // key is converted exactly once, so an updated device keeps its setting.
    if (prefs.isKey("autoRS")) {
      s_autoRot = prefs.getUShort("autoRS", 0);
    } else {
      s_autoRot = (uint16_t)prefs.getUChar("autoR", 0) * 60;
      migrateRotate = true;          // written below, the handle is read-only here
    }
    s_dotsHide = prefs.getUChar("dotsH", 0);
    if (s_dotsHide > 60) s_dotsHide = 60;
    s_radarSrc = prefs.getUChar("radSrc", RADAR_SRC_CHMU);
    s_mtLegend = prefs.getBool("mtLeg", true);
    s_route    = prefs.getBool("route", true);
    s_priceFee = prefs.getUShort("priceFee", 0);
    s_priceVat = prefs.getUChar("priceVat", 0);
    if (prefs.getString("mixCC", s_mixCC, sizeof(s_mixCC)) == 0)
      strncpy(s_mixCC, MIX_COUNTRY_DEFAULT, sizeof(s_mixCC) - 1);
    if (prefs.isKey("fxHome")) prefs.getString("fxHome", s_fxHome, sizeof(s_fxHome));
    if (prefs.isKey("fxList")) prefs.getString("fxList", s_fxList, sizeof(s_fxList));
    if (prefs.isKey("crList")) prefs.getString("crList", s_crList, sizeof(s_crList));
    if (prefs.isKey("cgKey"))  prefs.getString("cgKey", s_cgKey, sizeof(s_cgKey));
    s_routeSrc = prefs.getUChar("rtSrc", ROUTE_SRC_LOL) == ROUTE_SRC_DB ? ROUTE_SRC_DB : ROUTE_SRC_LOL;
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
    // 0.9.9: 0.9.8 stored a checkbox; "no detail" there is "radar only" here.
    s_paMode = prefs.isKey("paMode") ? prefs.getUChar("paMode", PA_MODE_DETAIL)
             : (prefs.getBool("paDet", true) ? PA_MODE_DETAIL : PA_MODE_RADAR);
    if (s_paMode > PA_MODE_RADAR) s_paMode = PA_MODE_DETAIL;
    s_winOn  = prefs.getBool("winOn", false);
    // 0.8.6: the window direction is the top of the radar now (see
    // Settings_WindowAz). The old key is read once below and then dropped.
    if (prefs.isKey("winAz")) { s_oldWinAz = (int)prefs.getUShort("winAz", 0); }
    s_winW   = prefs.getUShort("winW", WINDOW_WIDTH_DEFAULT);
    s_winKm  = prefs.getUShort("winKm", WINDOW_KM_DEFAULT);
    s_haOn   = prefs.getBool("haOn", HA_DEFAULT_ON);
    s_prOn   = prefs.getBool("prOn", false);
    s_prType = prefs.getUChar("prType", PRINT_TYPE_PRUSA);
    if (s_prType > PRINT_TYPE_KLIPPER) s_prType = PRINT_TYPE_PRUSA;
    if (prefs.isKey("prHost")) prefs.getString("prHost", s_prHost, sizeof(s_prHost));
    if (prefs.isKey("prUser")) prefs.getString("prUser", s_prUser, sizeof(s_prUser));
    s_prExp  = prefs.getBool("prExp", false);
    if (prefs.isKey("prKey"))  prefs.getString("prKey", s_prKey, sizeof(s_prKey));
    if (prefs.isKey("prPass")) prefs.getString("prPass", s_prPass, sizeof(s_prPass));
    s_prAFin = prefs.getBool("prAFin", true);
    s_prAErr = prefs.getBool("prAErr", true);
    s_prAPau = prefs.getBool("prAPau", true);
    s_prNight = prefs.getUChar("prNight", PRINT_NIGHT_BLINK);
    if (s_prNight > PRINT_NIGHT_CALM) s_prNight = PRINT_NIGHT_BLINK;
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

  // 0.8.6 migration: a window direction set before the merge becomes the
  // orientation of the radar, if the window view was actually in use.
  if (s_oldWinAz >= 0) {
    if (prefs.begin(NS, false)) {
      if (s_winOn) {
        s_top = (uint16_t)((((s_oldWinAz % 360) + 22) / 45 * 45) % 360);
        prefs.putUShort("topb", s_top);
      }
      prefs.remove("winAz");
      prefs.end();
    }
    s_oldWinAz = -1;
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

// 0.9.2 -----------------------------------------------------------------------
uint8_t  Settings_NightSource() { return s_nightSrc; }
void     Settings_SetNightSource(uint8_t s) {
  if (s > NIGHT_SRC_CLOCK) s = NIGHT_SRC_SUN;
  if (s != s_nightSrc) { s_nightSrc = s; putU8("nSrc", s); }
}
uint16_t Settings_NightFromMin() { return s_nightFrom; }
uint16_t Settings_NightToMin()   { return s_nightTo; }
void     Settings_SetNightWindow(uint16_t from, uint16_t to) {
  from %= 1440; to %= 1440;
  if (from != s_nightFrom) { s_nightFrom = from; putU16("nFrom", from); }
  if (to != s_nightTo)     { s_nightTo = to;     putU16("nTo", to); }
}
bool     Settings_DarkOn() { return s_darkOn; }
void     Settings_SetDarkOn(bool on) { if (on != s_darkOn) { s_darkOn = on; putBool("dkOn", on); } }
uint16_t Settings_DarkFromMin() { return s_darkFrom; }
uint16_t Settings_DarkToMin()   { return s_darkTo; }
void     Settings_SetDarkWindow(uint16_t from, uint16_t to) {
  from %= 1440; to %= 1440;
  if (from != s_darkFrom) { s_darkFrom = from; putU16("dkFrom", from); }
  if (to != s_darkTo)     { s_darkTo = to;     putU16("dkTo", to); }
}
bool     Settings_DarkAlerts() { return s_darkAlerts; }
void     Settings_SetDarkAlerts(bool on) { if (on != s_darkAlerts) { s_darkAlerts = on; putBool("dkAl", on); } }
bool     Settings_NameDay() { return s_nameDay; }
void     Settings_SetNameDay(bool on) { if (on != s_nameDay) { s_nameDay = on; putBool("nDay", on); } }
uint8_t  Settings_ClockLayout() { return s_clockLay; }
void     Settings_SetClockLayout(uint8_t l) {
  if (l > CLOCK_LAYOUT_HOURS) l = CLOCK_LAYOUT_BASIC;
  if (l != s_clockLay) { s_clockLay = l; putU8("clkLay", l); }
}

// --- Units, language --------------------------------------------------------
bool Settings_MetricUnits() { return s_metric; }
void Settings_SetMetricUnits(bool metric) { s_metric = metric; putBool("metric", metric); }
void    Settings_SetLanguage(uint8_t l) {
  s_lang = (l < LANG_N) ? l : LANG_CZ;
  Lang_Set(s_lang);
  putU8("lang", s_lang);
}

// --- Screens ----------------------------------------------------------------
bool Settings_ScreenEnabled(uint8_t idx) {
  if (idx == SCREEN_SETTINGS_I) return true;      // always reachable
  if (idx >= SCREEN_N) return false;
  return (s_scrMask >> idx) & 1;
}

void Settings_SetScreenEnabled(uint8_t idx, bool on) {
  if (idx == SCREEN_SETTINGS_I || idx >= SCREEN_N) return;
  uint16_t next = on ? (uint16_t)(s_scrMask | (1u << idx))
                     : (uint16_t)(s_scrMask & ~(1u << idx));
  // Never allow the last data screen to be turned off. With all of them gone
  // the device would boot into Settings and show nothing else - technically
  // recoverable, but it looks broken.
  if (next == 0) return;
  s_scrMask = next;
  putU8("scrM", (uint8_t)(s_scrMask & 0xFF));
  putU8("scrM2", (uint8_t)(s_scrMask >> 8));
}

uint16_t Settings_AutoRotateSec() { return s_autoRot; }
void     Settings_SetAutoRotateSec(uint16_t s) {
  if (s > 3600) s = 3600;
  s_autoRot = s;
  putU16("autoRS", s);
}

uint8_t Settings_DotsHideSec() { return s_dotsHide; }
void    Settings_SetDotsHideSec(uint8_t s) {
  if (s > 60) s = 60;
  if (s == s_dotsHide) return;
  s_dotsHide = s;
  putU8("dotsH", s);
}

// --- Weather radar ----------------------------------------------------------
uint8_t Settings_RadarSource() { return s_radarSrc; }
void    Settings_SetRadarSource(uint8_t s) {
  s_radarSrc = (s <= RADAR_SRC_MAX) ? s : RADAR_SRC_CHMU;
  putU8("radSrc", s_radarSrc);
}

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
uint8_t  Settings_PlaneAlertMode() { return s_paMode; }
void     Settings_SetPlaneAlertMode(uint8_t m) {
  if (m > PA_MODE_RADAR) m = PA_MODE_DETAIL;
  s_paMode = m; putU8("paMode", m);
}
bool     Settings_WindowOn() { return s_winOn; }
void     Settings_SetWindowOn(bool on) { s_winOn = on; putBool("winOn", on); }
// 0.8.6: no setting of its own. The radar is turned so that the direction you
// look out of the window is at the top (that is what the orientation is for),
// so the window faces the top of the radar by definition. Two settings for one
// thing could contradict each other - and the orientation also changes from
// the buttons on the display, which the window setting never followed.
uint16_t Settings_WindowAz() { return s_top; }
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
// --- 3D printer (0.9.0) ------------------------------------------------------
bool        Settings_PrintOn()   { return s_prOn; }
void        Settings_SetPrintOn(bool on) { s_prOn = on; putBool("prOn", on); s_prGen++; }
uint8_t     Settings_PrintType() { return s_prType; }
void        Settings_SetPrintType(uint8_t t) {
  if (t > PRINT_TYPE_KLIPPER) return;
  s_prType = t; putU8("prType", t); s_prGen++;
}
const char* Settings_PrintHost() { return s_prHost; }
void        Settings_SetPrintHost(const char* h) {
  // A pasted "http://192.168.1.5/" is accepted and cut down to the host.
  char tmp[64];
  if (h && !strncmp(h, "http://", 7)) h += 7;
  copyClean(tmp, sizeof(tmp), h);
  char* slash = strchr(tmp, '/');
  if (slash) *slash = '\0';
  strcpy(s_prHost, tmp); putStr("prHost", s_prHost); s_prGen++;
}
const char* Settings_PrintUser() { return s_prUser; }
void        Settings_SetPrintUser(const char* u) {
  copyClean(s_prUser, sizeof(s_prUser), u); putStr("prUser", s_prUser); s_prGen++;
}
const char* Settings_PrintKey()  { return s_prKey; }
const char* Settings_PrintPass() { return s_prPass; }
// The secrets live in NVS like the rest, so they survive a restart. NVS is not
// encrypted - whoever holds the board can read them. An empty one is removed
// rather than stored as "".
static void storeSecrets() {
  if (!prefs.begin(NS, false)) return;
  if (s_prKey[0])  prefs.putString("prKey", s_prKey);
  else if (prefs.isKey("prKey"))  prefs.remove("prKey");
  if (s_prPass[0]) prefs.putString("prPass", s_prPass);
  else if (prefs.isKey("prPass")) prefs.remove("prPass");
  prefs.end();
}
void Settings_SetPrintKey(const char* k) {
  if (!k || !*k) return;
  copyClean(s_prKey, sizeof(s_prKey), k); storeSecrets(); s_prGen++;
}
void Settings_SetPrintPass(const char* p) {
  if (!p || !*p) return;
  // A password may contain anything printable, spaces included.
  size_t n = 0;
  for (; p[n] && n + 1 < sizeof(s_prPass); n++) s_prPass[n] = p[n];
  s_prPass[n] = '\0';
  storeSecrets(); s_prGen++;
}
void Settings_ForgetPrintSecrets() {
  memset(s_prKey, 0, sizeof(s_prKey));
  memset(s_prPass, 0, sizeof(s_prPass));
  storeSecrets(); s_prGen++;
}
bool Settings_PrintExport() { return s_prExp; }
void Settings_SetPrintExport(bool on) {
  if (on == s_prExp) return;
  s_prExp = on; putBool("prExp", on);
}
bool Settings_PrintAlertFinish() { return s_prAFin; }
bool Settings_PrintAlertError()  { return s_prAErr; }
bool Settings_PrintAlertPause()  { return s_prAPau; }
void Settings_SetPrintAlerts(bool f, bool e, bool p) {
  s_prAFin = f; s_prAErr = e; s_prAPau = p;
  if (prefs.begin(NS, false)) {
    prefs.putBool("prAFin", f); prefs.putBool("prAErr", e); prefs.putBool("prAPau", p); prefs.end();
  }
}
uint8_t Settings_PrintNight() { return s_prNight; }
void    Settings_SetPrintNight(uint8_t m) {
  if (m > PRINT_NIGHT_CALM) return;
  s_prNight = m; putU8("prNight", m);
}
uint32_t Settings_PrintGen() { return s_prGen; }

bool Settings_HaEnabled() { return s_haOn; }
void Settings_SetHaEnabled(bool on) { s_haOn = on; putBool("haOn", on); }

bool Settings_MeteoLegend() { return s_mtLegend; }
void Settings_SetMeteoLegend(bool on) { s_mtLegend = on; putBool("mtLeg", on); }
bool Settings_ShowRoute() { return s_route; }
void Settings_SetShowRoute(bool on) { if (on != s_route) { s_route = on; putBool("route", on); } }

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

// --- Exchange rates (0.9.6) -------------------------------------------------
// Is code (three letters, any case) one of the space-separated list? Writes the
// upper-case form to out.
static bool fxKnown(const char* list, const char* code, char out[4]) {
  if (!code) return false;
  char c[4] = {0};
  for (int i = 0; i < 3; i++) {
    char ch = code[i];
    if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 'a' + 'A');
    if (ch < 'A' || ch > 'Z') return false;
    c[i] = ch;
  }
  if (code[3] != '\0') return false;
  for (const char* p = list; *p; p += (p[3] ? 4 : 3)) {
    if (!strncmp(p, c, 3)) { memcpy(out, c, 4); return true; }
    if (!p[3]) break;
  }
  return false;
}

void Settings_SetFxHome(const char* code) {
  char c[4];
  if (code && fxKnown(FX_HOMES, code, c)) strcpy(s_fxHome, c);
  else s_fxHome[0] = '\0';
  putStr("fxHome", s_fxHome);
}

// What "automatic" means right now - also sent to the web page, which shows it
// next to the "Automatic" choice.
static const char* fxHomeAuto() {
  if (s_lang == LANG_CZ) return "CZK";
  if (s_lang == LANG_SK) return "EUR";
  // English: whatever the country of the mix pays with, if the sources know it.
  static const struct { const char cc[3]; const char cur[4]; } M[] = {
    {"cz", "CZK"}, {"pl", "PLN"}, {"hu", "HUF"}, {"ch", "CHF"}, {"uk", "GBP"},
    {"dk", "DKK"}, {"se", "SEK"}, {"no", "NOK"}, {"ro", "RON"} };
  for (const auto& m : M) if (!strcmp(m.cc, s_mixCC)) return m.cur;
  return "EUR";
}

const char* Settings_FxHomeEff() { return s_fxHome[0] ? s_fxHome : fxHomeAuto(); }

void Settings_SetFxList(const char* csv) {
  char out[sizeof(s_fxList)] = "";
  int n = 0;
  if (csv) {
    const char* p = csv;
    while (*p && n < FX_MAX) {
      while (*p == ',' || *p == ' ') p++;
      char tok[8] = {0}; int k = 0;
      while (*p && *p != ',' && *p != ' ') { if (k < 7) tok[k++] = *p; p++; }
      char c[4];
      if (k && fxKnown(FX_CODES, tok, c) && !strstr(out, c)) {
        if (n) strcat(out, ",");
        strcat(out, c);
        n++;
      }
    }
  }
  strcpy(s_fxList, out);
  putStr("fxList", s_fxList);
}

int Settings_FxCodesEff(char out[][4], int max) {
  const char* home = Settings_FxHomeEff();
  const char* src = s_fxList;
  // Defaults (an empty stored list): the currencies people in that country
  // actually look up - the second pass below.
  int n = 0;
  for (int pass = 0; pass < 2 && n == 0; pass++) {
    // Second pass: the stored list held nothing but the home currency (it was
    // "EUR" and the home currency became EUR) - the defaults, not a blank screen.
    if (pass == 1) src = !strcmp(home, "CZK") ? "EUR,USD,GBP,PLN"
                       : !strcmp(home, "EUR") ? "USD,GBP,CHF,CZK" : "EUR,USD,GBP,CHF";
    for (const char* p = src; *p && n < max; ) {
      if (strlen(p) < 3) break;
      char c[4] = { p[0], p[1], p[2], 0 };
      if (strcmp(c, home)) { memcpy(out[n], c, 4); n++; }
      p += 3;
      if (*p == ',') p++;
    }
  }
  return n;
}

// --- Flight route source (0.9.8) ---------------------------------------------
uint8_t Settings_RouteSrc() { return s_routeSrc; }
void Settings_SetRouteSrc(uint8_t src) {
  src = src == ROUTE_SRC_DB ? ROUTE_SRC_DB : ROUTE_SRC_LOL;
  if (src == s_routeSrc) return;           // NVS is written only on a change
  s_routeSrc = src;
  putU8("rtSrc", s_routeSrc);
}

// --- Cryptocurrencies (0.9.7) -----------------------------------------------
static const struct { const char* sym; const char* id; } CR_IDS[] = {
  {"BTC", "bitcoin"},  {"ETH", "ethereum"}, {"SOL", "solana"},   {"XRP", "ripple"},
  {"BNB", "binancecoin"}, {"ADA", "cardano"}, {"DOGE", "dogecoin"}, {"TRX", "tron"},
  {"LTC", "litecoin"}, {"DOT", "polkadot"}, {"LINK", "chainlink"}, {"USDT", "tether"} };

const char* Settings_CryptoId(const char* sym) {
  for (const auto& c : CR_IDS) if (!strcmp(c.sym, sym)) return c.id;
  return nullptr;
}

void Settings_SetCryptoList(const char* csv) {
  char out[sizeof(s_crList)] = "";
  int n = 0;
  for (const char* p = csv ? csv : ""; *p && n < CR_MAX; ) {
    while (*p == ',' || *p == ' ') p++;
    char tok[8] = {0}; int k = 0;
    while (*p && *p != ',' && *p != ' ') {
      char ch = *p++;
      if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 'a' + 'A');
      if (k < 7) tok[k++] = ch;
    }
    if (!k || !Settings_CryptoId(tok)) continue;
    // Whole-token duplicate check ("ETH" must not match inside "ETHX").
    bool dup = false;
    for (const char* q = out; *q && !dup; ) {
      const char* e = strchr(q, ',');
      const size_t l = e ? (size_t)(e - q) : strlen(q);
      if (l == strlen(tok) && !strncmp(q, tok, l)) dup = true;
      q = e ? e + 1 : q + l;
    }
    if (dup) continue;
    if (n) strcat(out, ",");
    strcat(out, tok);
    n++;
  }
  strcpy(s_crList, out);
  putStr("crList", s_crList);
}

int Settings_CryptoCodesEff(char out[][6], int max) {
  const char* src = s_crList[0] ? s_crList : "BTC,ETH,SOL,XRP";
  int n = 0;
  for (const char* p = src; *p && n < max; ) {
    const char* e = strchr(p, ',');
    const size_t l = e ? (size_t)(e - p) : strlen(p);
    if (l >= 2 && l <= 5) { memcpy(out[n], p, l); out[n][l] = '\0'; n++; }
    if (!e) break;
    p = e + 1;
  }
  return n;
}

const char* Settings_CgKey() { return s_cgKey; }
void Settings_SetCgKey(const char* key) {
  // Letters, digits, '-' and '_' only - it goes into a URL as it is.
  char k[sizeof(s_cgKey)] = "";
  size_t n = 0;
  for (const char* p = key ? key : ""; *p && n < sizeof(k) - 1; p++) {
    const char c = *p;
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_')
      k[n++] = c;
  }
  k[n] = '\0';
  strcpy(s_cgKey, k);
  putStr("cgKey", s_cgKey);
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
  // "" switches the protection off. Since 0.8.6 this is only called from the
  // dedicated /api/password endpoint, never as a side effect of Save, so an
  // empty field there is a deliberate choice. A single space - the old way
  // the form said "remove" - still means the same.
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
  o["nightSrc"]   = s_nightSrc;      // 0.9.2
  o["nightFrom"]  = s_nightFrom;     // minutes of the day
  o["nightTo"]    = s_nightTo;
  o["darkOn"]     = s_darkOn;
  o["darkFrom"]   = s_darkFrom;
  o["darkTo"]     = s_darkTo;
  o["darkAlerts"] = s_darkAlerts;
  o["nameDay"]    = s_nameDay;
  o["clockLayout"] = s_clockLay;   // 0.9.3
  o["radarSrc"] = s_radarSrc;
  o["meteoLegend"] = s_mtLegend;
  o["showRoute"]   = s_route;
  o["priceFee"] = s_priceFee;
  o["priceVat"] = s_priceVat;
  o["mixCountry"] = s_mixCC;
  o["fxHome"] = s_fxHome;                 // 0.9.6: "" = automatic
  o["fxHomeAuto"] = fxHomeAuto();         // what "automatic" is right now
  {
    JsonArray fl = o["fxList"].to<JsonArray>();   // stored, empty = default
    char c[FX_MAX][4];
    if (s_fxList[0]) {
      for (const char* p = s_fxList; strlen(p) >= 3; ) {
        char t[4] = { p[0], p[1], p[2], 0 }; fl.add(t); p += 3; if (*p == ',') p++; else break;
      }
    }
    JsonArray fe = o["fxListEff"].to<JsonArray>(); // what the screen shows
    const int n = Settings_FxCodesEff(c, FX_MAX);
    for (int i = 0; i < n; i++) fe.add(c[i]);
  }
  {
    // 0.9.7: cryptocurrencies - stored list, what is shown, whether a key is set.
    JsonArray cl = o["crList"].to<JsonArray>();
    for (const char* p = s_crList; *p; ) {
      const char* e = strchr(p, ',');
      const size_t l = e ? (size_t)(e - p) : strlen(p);
      char t[8] = {0}; memcpy(t, p, l < 7 ? l : 7); cl.add(t);
      if (!e) break;
      p = e + 1;
    }
    char c[CR_MAX][6];
    JsonArray ce = o["crListEff"].to<JsonArray>();
    const int n = Settings_CryptoCodesEff(c, CR_MAX);
    for (int i = 0; i < n; i++) ce.add(c[i]);
    o["cgHasKey"] = s_cgKey[0] != '\0';
  }
  o["autoRotate"] = s_autoRot;   // seconds
  o["dotsHide"]   = s_dotsHide;  // seconds, 0 = always shown
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
  o["planeAlertMode"] = s_paMode;
  o["windowOn"]    = s_winOn;
  o["windowWidth"] = s_winW;
  o["windowKm"]    = s_winKm;
  o["haEnabled"]   = s_haOn;
  // Printer: everything but the secrets, of which only "is one set" leaves.
  o["printOn"]       = s_prOn;
  o["printType"]     = s_prType;
  o["printHost"]     = s_prHost;
  o["printUser"]     = s_prUser;
  o["printExport"]   = s_prExp;
  o["printHasKey"]   = s_prKey[0] != '\0';
  o["printHasPass"]  = s_prPass[0] != '\0';
  o["printAlertFinish"] = s_prAFin;
  o["printAlertError"]  = s_prAErr;
  o["printAlertPause"]  = s_prAPau;
  o["printNight"]    = s_prNight;
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
  scr["print"]    = Settings_ScreenEnabled(SCREEN_PRINT_I);
  scr["fx"]       = Settings_ScreenEnabled(SCREEN_FX_I);
  scr["crypto"]   = Settings_ScreenEnabled(SCREEN_CRYPTO_I);
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
  setIf("nightSrc",     [](JsonVariantConst v){ Settings_SetNightSource(v.as<uint8_t>()); });
  setIf("nightFrom",    [](JsonVariantConst v){ Settings_SetNightWindow(v.as<uint16_t>(), s_nightTo); });
  setIf("nightTo",      [](JsonVariantConst v){ Settings_SetNightWindow(s_nightFrom, v.as<uint16_t>()); });
  setIf("darkOn",       [](JsonVariantConst v){ Settings_SetDarkOn(v.as<bool>()); });
  setIf("darkFrom",     [](JsonVariantConst v){ Settings_SetDarkWindow(v.as<uint16_t>(), s_darkTo); });
  setIf("darkTo",       [](JsonVariantConst v){ Settings_SetDarkWindow(s_darkFrom, v.as<uint16_t>()); });
  setIf("darkAlerts",   [](JsonVariantConst v){ Settings_SetDarkAlerts(v.as<bool>()); });
  setIf("nameDay",      [](JsonVariantConst v){ Settings_SetNameDay(v.as<bool>()); });
  setIf("clockLayout",  [](JsonVariantConst v){ Settings_SetClockLayout(v.as<uint8_t>()); });
  setIf("radarSrc",     [](JsonVariantConst v){ Settings_SetRadarSource(v.as<uint8_t>()); });
  setIf("meteoLegend",  [](JsonVariantConst v){ Settings_SetMeteoLegend(v.as<bool>()); });
  setIf("showRoute",    [](JsonVariantConst v){ Settings_SetShowRoute(v.as<bool>()); });
  setIf("priceFee",     [](JsonVariantConst v){ Settings_SetPriceFee(v.as<uint16_t>()); });
  setIf("priceVat",     [](JsonVariantConst v){ Settings_SetPriceVat(v.as<uint8_t>()); });
  setIf("mixCountry",   [](JsonVariantConst v){ Settings_SetMixCountry(v.as<const char*>()); });
  setIf("fxHome",       [](JsonVariantConst v){ Settings_SetFxHome(v.as<const char*>()); });
  setIf("cgKey",        [](JsonVariantConst v){ Settings_SetCgKey(v | ""); });
  setIf("crList",       [](JsonVariantConst v){
    if (v.is<JsonArrayConst>()) {
      char csv[64] = "";
      for (JsonVariantConst e : v.as<JsonArrayConst>()) {
        const char* c = e | "";
        if (strlen(csv) + strlen(c) + 2 >= sizeof(csv)) break;
        if (csv[0]) strcat(csv, ",");
        strcat(csv, c);
      }
      Settings_SetCryptoList(csv);
    } else {
      Settings_SetCryptoList(v | "");
    }
  });
  setIf("fxList",       [](JsonVariantConst v){
    // An array of codes (the page, an export) or a comma-separated string.
    if (v.is<JsonArrayConst>()) {
      char csv[64] = "";
      for (JsonVariantConst e : v.as<JsonArrayConst>()) {
        const char* c = e | "";
        if (strlen(csv) + strlen(c) + 2 >= sizeof(csv)) break;
        if (csv[0]) strcat(csv, ",");
        strcat(csv, c);
      }
      Settings_SetFxList(csv);
    } else {
      Settings_SetFxList(v | "");
    }
  });
  setIf("autoRotate",   [](JsonVariantConst v){ Settings_SetAutoRotateSec(v.as<uint16_t>()); });
  setIf("dotsHide",     [](JsonVariantConst v){ Settings_SetDotsHideSec(v.as<uint8_t>()); });
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
  // An export from 0.9.8 has the checkbox; the newer key, when there, wins.
  setIf("planeAlertDetail",[](JsonVariantConst v){ Settings_SetPlaneAlertMode(v.as<bool>() ? PA_MODE_DETAIL : PA_MODE_RADAR); });
  setIf("planeAlertMode",[](JsonVariantConst v){ Settings_SetPlaneAlertMode(v.as<uint8_t>()); });
  setIf("windowOn",     [](JsonVariantConst v){ Settings_SetWindowOn(v.as<bool>()); });
  // "windowAz" from a backup made before 0.8.6 is ignored: see Settings_WindowAz().
  setIf("windowWidth",  [](JsonVariantConst v){ Settings_SetWindowWidth(v.as<uint16_t>()); });
  setIf("windowKm",     [](JsonVariantConst v){ Settings_SetWindowKm(v.as<uint16_t>()); });
  setIf("haEnabled",    [](JsonVariantConst v){ Settings_SetHaEnabled(v.as<bool>()); });
  // Printer. The secrets ("printKey", "printPass") are NOT taken here - the
  // config POST handler and the import (only with printExport) apply them.
  // "printHasKey"/"printHasPass" from an export are ignored.
  setIf("printOn",      [](JsonVariantConst v){ Settings_SetPrintOn(v.as<bool>()); });
  setIf("printType",    [](JsonVariantConst v){ Settings_SetPrintType(v.as<uint8_t>()); });
  setIf("printHost",    [](JsonVariantConst v){ Settings_SetPrintHost(v.as<const char*>()); });
  setIf("printUser",    [](JsonVariantConst v){ Settings_SetPrintUser(v.as<const char*>()); });
  setIf("printExport",  [](JsonVariantConst v){ Settings_SetPrintExport(v.as<bool>()); });
  setIf("printNight",   [](JsonVariantConst v){ Settings_SetPrintNight(v.as<uint8_t>()); });
  if (!in["printAlertFinish"].isNull() || !in["printAlertError"].isNull() || !in["printAlertPause"].isNull()) {
    Settings_SetPrintAlerts(in["printAlertFinish"] | s_prAFin, in["printAlertError"] | s_prAErr,
                            in["printAlertPause"] | s_prAPau);
    changed = true;
  }

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
      { "print",    SCREEN_PRINT_I },
      { "fx",       SCREEN_FX_I },
      { "crypto",   SCREEN_CRYPTO_I },
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
  s_nightSrc = NIGHT_SRC_SUN; s_nightFrom = 22 * 60; s_nightTo = 6 * 60;
  s_darkOn = false; s_darkFrom = 23 * 60; s_darkTo = 6 * 60; s_darkAlerts = true;
  s_nameDay = true; s_clockLay = CLOCK_LAYOUT_BASIC;
  s_metric = false; s_lang = LANG_CZ; Lang_Set(s_lang);
  s_scrMask = (1 << SCREEN_CLOCK_I) | (1 << SCREEN_PLANES_I) |
              (1 << SCREEN_METEO_I) | (1 << SCREEN_FORECAST_I);
  s_autoRot = 0; s_dotsHide = 0; s_radarSrc = RADAR_SRC_CHMU; s_mtLegend = true; s_route = true;
  s_priceFee = 0; s_priceVat = 0;
  strncpy(s_mixCC, MIX_COUNTRY_DEFAULT, sizeof(s_mixCC) - 1);
  s_fxHome[0] = '\0'; s_fxList[0] = '\0'; s_crList[0] = '\0'; s_cgKey[0] = '\0';
  s_routeSrc = ROUTE_SRC_LOL;
  s_secStyle = SEC_STYLE_DOTS; s_clockCol = 0xFFFF; s_secCol = 0x05FF;
  s_clkFont = CLOCK_FONT_DEFAULT;
  s_altBand[0] = ALT_BAND1_M_DEFAULT; s_altBand[1] = ALT_BAND2_M_DEFAULT; s_altBand[2] = ALT_BAND3_M_DEFAULT;
  s_altCol[0] = ALT_COL0_DEFAULT; s_altCol[1] = ALT_COL1_DEFAULT;
  s_altCol[2] = ALT_COL2_DEFAULT; s_altCol[3] = ALT_COL3_DEFAULT;
  s_stOnly = false; s_stDbz = STORM_DBZ_DEFAULT; s_stKm = STORM_KM_DEFAULT;
  s_paOn = false; s_paKm = PLANE_ALERT_KM_DEFAULT; s_paSec = PLANE_ALERT_SEC_DEFAULT; s_paMode = PA_MODE_DETAIL;
  s_winOn = false; s_winW = WINDOW_WIDTH_DEFAULT; s_winKm = WINDOW_KM_DEFAULT;
  s_haOn = HA_DEFAULT_ON;
  s_prOn = false; s_prType = PRINT_TYPE_PRUSA; s_prHost[0] = '\0'; strcpy(s_prUser, "maker");
  memset(s_prKey, 0, sizeof(s_prKey)); memset(s_prPass, 0, sizeof(s_prPass)); s_prExp = false;
  s_prAFin = s_prAErr = s_prAPau = true; s_prNight = PRINT_NIGHT_BLINK; s_prGen++;
  s_altMin = 0; s_altMax = 60000; s_onlyCs = false; s_sqAlert = true; s_watch[0] = '\0';
  s_rngP = 1; s_rngM = 1; s_scr = SCREEN_PLANES_I; s_top = 0;
  s_pw[0] = '\0';
  memset(s_ssid, 0, sizeof(s_ssid));
  memset(s_wpass, 0, sizeof(s_wpass));
  s_uiDirty = false;
}
