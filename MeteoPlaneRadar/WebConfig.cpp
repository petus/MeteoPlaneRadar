// =============================================================================
//  MeteoPlaneRadar
//  The configuration web server. See WebConfig.h.
//
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
// =============================================================================
#include "WebConfig.h"
#include "WebPage.h"
#include "ScreenPlanes.h"
#include "ScreenWeather.h"
#include "ScreenPrice.h"
#include "Settings.h"
#include "Status.h"
#include "Version.h"
#include "Config.h"
#include "Lang.h"
#include "Net.h"
#include "Forecast.h"
#include "Energy.h"
#include "NightMode.h"
#include "UI.h"
#include "Display_ST7701.h"
#include "Watchdog.h"

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include "PrintWatch.h"
#include <ArduinoJson.h>
#include <Update.h>
#include <esp_heap_caps.h>
#include "Board.h"
#include <esp_system.h>
#include <math.h>
#include <string.h>
#include "TimeZone.h"
#include "StormWatch.h"
#include "PlaneWatch.h"
#include "PlaneIcon.h"
#include "ADSB.h"
#include "Outside.h"
#include <time.h>

// Defined in the .ino - the screen actually on the display.
extern int  MPR_CurrentScreen();
extern bool MPR_AlertActive();
extern bool MPR_PrintAlertActive();

static WebServer  s_srv(WEB_PORT);
static DNSServer  s_dns;
static bool s_apMode = false;
static bool s_running = false;
static bool s_wantConnect = false;
static bool s_wantRestart = false;

// 0.9.8: settings that need a restart - the location, the radar source and the
// set of screens - are no longer waiting for a Save button. The page sends them
// the moment they change, like everything else, and they wait HERE, in RAM,
// untouched by the running firmware (which still has the old state cached).
// They are written and the device restarts when the user presses "Restart now"
// on the page, or by itself RESTART_DELAY_MS after the last such change - so a
// change is never left hanging until the next power cut, and ticking five
// boxes in a row still costs one restart.
#define RESTART_DELAY_MS 120000UL
static JsonDocument  s_pend;             // lat, lon, radarSrc, screens{...}
static bool          s_pendOn = false;
static unsigned long s_pendAt = 0;       // millis() of the last such change
static volatile bool s_updating = false;

// Queued remote-control requests - see the note in WebConfig.h.
static int s_reqScreen     = -1;
static int s_reqScreenStep = 0;
static int s_reqRangeStep  = 0;

bool WebConfig_UpdateBusy()        { return s_updating; }
bool WebConfig_WantsWifiConnect()  { return s_wantConnect; }
void WebConfig_ClearWifiConnect()  { s_wantConnect = false; }
bool WebConfig_WantsRestart()      { return s_wantRestart; }

int WebConfig_TakeScreen()     { int v = s_reqScreen;     s_reqScreen = -1;    return v; }
int WebConfig_TakeScreenStep() { int v = s_reqScreenStep; s_reqScreenStep = 0; return v; }
int WebConfig_TakeRangeStep()  { int v = s_reqRangeStep;  s_reqRangeStep = 0;  return v; }

// --- Helpers ----------------------------------------------------------------
static void sendJson(int code, JsonDocument& doc) {
  String out;
  serializeJson(doc, out);
  s_srv.send(code, "application/json", out);
}

static bool readBody(JsonDocument& doc) {
  if (!s_srv.hasArg("plain")) return false;
  return deserializeJson(doc, s_srv.arg("plain")) == DeserializationError::Ok;
}

// The screens by the names the page and the JSON use.
static const struct { const char* key; uint8_t idx; } SCR_KEYS[] = {
  { "clock",    SCREEN_CLOCK_I },  { "planes", SCREEN_PLANES_I }, { "meteo", SCREEN_METEO_I },
  { "forecast", SCREEN_FORECAST_I }, { "price", SCREEN_PRICE_I }, { "mix",   SCREEN_MIX_I },
  { "print",    SCREEN_PRINT_I },  { "fx",     SCREEN_FX_I },     { "crypto", SCREEN_CRYPTO_I },
};

// Does anything held back differ from what the device runs with? Ticking a
// box and unticking it again leaves nothing to restart for.
static bool pendDiffers() {
  JsonObjectConst p = s_pend.as<JsonObjectConst>();
  if (!p["lat"].isNull() && fabs(p["lat"].as<double>() - Settings_Lat()) > 1e-6) return true;
  if (!p["lon"].isNull() && fabs(p["lon"].as<double>() - Settings_Lon()) > 1e-6) return true;
  if (!p["radarSrc"].isNull() && p["radarSrc"].as<uint8_t>() != Settings_RadarSource()) return true;
  JsonObjectConst scr = p["screens"];
  if (!scr.isNull())
    for (auto& m : SCR_KEYS) {
      JsonVariantConst v = scr[m.key];
      if (!v.isNull() && v.as<bool>() != Settings_ScreenEnabled(m.idx)) return true;
    }
  return false;
}

// Seconds until the device restarts by itself; 0 = nothing waiting.
static uint32_t pendLeftS() {
  if (!s_pendOn) return 0;
  const unsigned long gone = millis() - s_pendAt;
  return gone >= RESTART_DELAY_MS ? 1 : (uint32_t)((RESTART_DELAY_MS - gone + 999) / 1000);
}

// Write what was held back and ask the loop for a restart.
static void applyPending() {
  if (!s_pendOn) return;
  s_pendOn = false;
  const double oldLat = Settings_Lat(), oldLon = Settings_Lon();
  Settings_FromJson(s_pend.as<JsonObjectConst>());
  s_pend.clear();
  const bool moved = (fabs(oldLat - Settings_Lat()) > 1e-6) ||
                     (fabs(oldLon - Settings_Lon()) > 1e-6);
  if (moved) {
    Forecast_Invalidate();
    Energy_Invalidate();

    // Moving the device out of the area a source covers switches that screen
    // off, ONCE, at the moment the location changes. Not a permanent lock: the
    // checkbox is still there and turning it back on sticks, which matters
    // because the boxes in Config.h are rectangles and somebody just over the
    // border may well want Czech prices on purpose.
    //
    // Doing it here rather than in the drawing code is deliberate. A screen
    // that shows "not available here" every time the cycling reaches it is a
    // screen that has to be switched off by hand anyway; the device may as well
    // do the obvious thing and say so in the log.
    if (!Energy_PriceInArea() && Settings_ScreenEnabled(SCREEN_PRICE_I)) {
      Settings_SetScreenEnabled(SCREEN_PRICE_I, false);
      Serial.println("Poloha mimo CR: vypinam obrazovku s cenou elektriny");
    }
    if (!Energy_MixInArea() && Settings_ScreenEnabled(SCREEN_MIX_I)) {
      Settings_SetScreenEnabled(SCREEN_MIX_I, false);
      Serial.println("Poloha mimo Evropu: vypinam obrazovku s vyrobou");
    }
    // Still on the Czech default after moving abroad? Europe as a whole is a
    // better guess than a country the user does not live in. An explicit choice
    // is left alone.
    if (!Energy_PriceInArea() && Energy_MixInArea() &&
        !strcmp(Settings_MixCountry(), MIX_COUNTRY_DEFAULT)) {
      Settings_SetMixCountry("eu");
      Serial.println("Poloha mimo CR: mix prepnut na celou Evropu");
    }
  }
  // These reach too far into cached state (decoded radar frames, allocated
  // buffers, which screen is even reachable) to be worth unpicking at runtime.
  // A restart is a second and a half and guarantees a clean result.
  s_wantRestart = true;
}

// What the page should show: the held-back values over the running ones, and
// how long until the restart.
static void pendOverlay(JsonObject o) {
  if (s_pendOn) {
    JsonObjectConst p = s_pend.as<JsonObjectConst>();
    if (!p["lat"].isNull())      o["lat"] = p["lat"];
    if (!p["lon"].isNull())      o["lon"] = p["lon"];
    if (!p["radarSrc"].isNull()) o["radarSrc"] = p["radarSrc"];
    JsonObjectConst scr = p["screens"];
    if (!scr.isNull()) {
      JsonObject os = o["screens"];
      for (JsonPairConst kv : scr) os[kv.key()] = kv.value();
    }
  }
  o["restartPending"] = s_pendOn;
  o["restartIn"] = pendLeftS();
}

// Every destructive endpoint goes through here. When no password is set this
// waves everything through - that is the documented default, and the device
// only listens on the home LAN.
static bool authed(JsonDocument& body) {
  const char* pw = body["password"] | "";
  if (Settings_CheckAdminPassword(pw)) return true;
  s_srv.send(403, "application/json", "{\"error\":\"password\"}");
  return false;
}

// --- Endpoints --------------------------------------------------------------
static void handleRoot() {
  s_srv.sendHeader("Cache-Control", "no-store");
  // The charset belongs in the HEADER, not only in a meta tag: a browser that
  // reads the header first would otherwise guess, and the Czech accents on the
  // page would come out as mojibake.
  s_srv.send_P(200, "text/html; charset=utf-8", PAGE_HTML);
}

static void handleGetConfig() {
  JsonDocument doc;
  JsonObject o = doc.to<JsonObject>();
  Settings_ToJson(o);
  // The remembered networks: names only. A password never leaves the device;
  // the page is told whether one is stored, so it can say so.
  {
    JsonArray w = o["wifi"].to<JsonArray>();
    for (uint8_t i = 0; i < WIFI_SLOTS; i++) {
      JsonObject e = w.add<JsonObject>();
      e["ssid"] = Settings_WifiSsid(i);
      e["hasPass"] = Settings_WifiPass(i)[0] != '\0';
    }
  }
  pendOverlay(o);
  o["version"] = FW_VERSION;
  o["apMode"]  = s_apMode;
  o["ip"]      = s_apMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
  sendJson(200, doc);
}

static void handlePostConfig() {
  JsonDocument doc;
  if (!readBody(doc)) { s_srv.send(400, "application/json", "{\"error\":\"json\"}"); return; }

  // 0.8.6: the admin password is NOT changed here any more - see
  // handlePassword(). A "newPassword" or "password" key in the body is ignored.

  // 0.9.8: the restart-bound keys are taken out and held back (see s_pend).
  JsonObject in = doc.as<JsonObject>();
  bool held = false;
  for (const char* k : { "lat", "lon", "radarSrc" }) {
    if (!in[k].isNull()) { s_pend[k] = in[k]; in.remove(k); held = true; }
  }
  if (!in["screens"].isNull()) {
    JsonObject ps = s_pend["screens"].is<JsonObject>() ? s_pend["screens"].as<JsonObject>()
                                                      : s_pend["screens"].to<JsonObject>();
    for (JsonPair kv : in["screens"].as<JsonObject>()) ps[kv.key()] = kv.value();
    in.remove("screens");
    held = true;
  }
  if (held) {
    s_pendOn = pendDiffers();
    if (s_pendOn) s_pendAt = millis();
    else          s_pend.clear();
  }

  Settings_FromJson(doc.as<JsonObjectConst>());

  // 0.9.0: the printer's secrets come in here (and through an import with
  // printExport on) - not through Settings_FromJson.
  // Empty means "leave it as it is"; printForget wipes both.
  if (doc["printForget"] | false) Settings_ForgetPrintSecrets();
  Settings_SetPrintKey(doc["printKey"] | "");
  Settings_SetPrintPass(doc["printPass"] | "");

  // Applied straight away - these are the ones you want to see change while
  // you are still looking at the slider.
  NightMode_Apply();

  JsonDocument res;
  res["ok"] = true;
  res["restartPending"] = s_pendOn;
  res["restartIn"] = pendLeftS();
  sendJson(200, res);
}

// Jump to a screen, or step one along. Disabled screens are refused rather than
// silently ignored, so the page can say why nothing happened.
static void handleScreen() {
  JsonDocument doc;
  if (!readBody(doc)) { s_srv.send(400, "application/json", "{\"error\":\"json\"}"); return; }

  JsonVariantConst idx = doc["index"];
  if (!idx.isNull()) {
    int i = idx.as<int>();
    if (i < 0 || i >= SCREEN_N) {
      s_srv.send(400, "application/json", "{\"error\":\"range\"}");
      return;
    }
    if (!Settings_ScreenEnabled((uint8_t)i)) {
      s_srv.send(409, "application/json", "{\"error\":\"disabled\"}");
      return;
    }
    s_reqScreen = i;
  } else {
    int st = doc["step"] | 0;
    s_reqScreenStep = (st < 0) ? -1 : ((st > 0) ? +1 : 0);
  }
  s_srv.send(200, "application/json", "{\"ok\":true}");
}

// Change the range on whichever radar screen is showing. The screens that have
// no range simply ignore it, exactly as a swipe does.
static void handleRange() {
  JsonDocument doc;
  if (!readBody(doc)) { s_srv.send(400, "application/json", "{\"error\":\"json\"}"); return; }
  int st = doc["step"] | 0;
  s_reqRangeStep = (st < 0) ? -1 : ((st > 0) ? +1 : 0);
  s_srv.send(200, "application/json", "{\"ok\":true}");
}

static void handleStatus() {
  JsonDocument doc;
  doc["version"] = FW_VERSION;
  // Which panel the firmware decided it is running on. One binary serves both
  // boards, so this is the first thing to ask about when a bug report says the
  // picture rolls or the touch is dead. A "?" means detection fell back.
  doc["board"] = Board_Detected() ? Board_Name() : "? -> " + String(Board_Name());
  doc["ip"]   = s_apMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
  doc["restartIn"] = pendLeftS();        // 0.9.8: 0 = nothing waiting
  doc["ssid"] = s_apMode ? String(AP_SSID) : WiFi.SSID();
  doc["rssi"] = s_apMode ? 0 : WiFi.RSSI();

  unsigned long up = millis() / 1000UL;
  char ub[32];
  snprintf(ub, sizeof(ub), "%lud %02lu:%02lu:%02lu",
           up / 86400UL, (up / 3600UL) % 24UL, (up / 60UL) % 60UL, up % 60UL);
  doc["uptime"] = ub;

  doc["heap"]  = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  doc["psram"] = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_SPIRAM);

  const char* rr = "?";
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON:  rr = "power on"; break;
    case ESP_RST_SW:       rr = "software"; break;
    case ESP_RST_PANIC:    rr = "PANIC"; break;
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:      rr = "WATCHDOG"; break;
    case ESP_RST_BROWNOUT: rr = "BROWNOUT"; break;
    case ESP_RST_EXT:      rr = "reset pin"; break;
    default: break;
  }
  // 0.9.2: a watchdog restart the stuck-loop guard caused says what hung.
  if (const char* w = Watchdog_StuckWhat()) {
    static char rb[48];
    snprintf(rb, sizeof(rb), "WATCHDOG (zaseknute: %s)", w);
    rr = rb;
  }
  doc["resetReason"] = rr;

  // The time zone, because "the clock is wrong" is otherwise impossible to
  // diagnose remotely. Says where the offset came from: the compiled-in Czech
  // rule, or the forecast for the user's own coordinates.
  {
    char tzb[48];
    const int off = TimeZone_Offset();
    snprintf(tzb, sizeof(tzb), "%s (UTC%+d:%02d, %s)", TimeZone_Text(),
             off / 3600, (off < 0 ? -off : off) % 3600 / 60,
             TimeZone_FromNetwork() ? "podle polohy" : "z Config.h");
    doc["timezone"] = tzb;
  }

  // What the device is showing right now, so the remote control can highlight
  // the active screen and print the range instead of guessing.
  const uint8_t scr = (uint8_t)MPR_CurrentScreen();
  doc["screen"] = scr;
  char rb[24] = "";
  if      (scr == SCREEN_PLANES_I) ScreenPlanes_RangeText(rb, sizeof(rb));
  else if (scr == SCREEN_METEO_I)  ScreenWeather_RangeText(rb, sizeof(rb));
  // The price screen has no range in kilometres, but it does have two states
  // the remote control can step through, so it answers here too and the -/+
  // buttons stay live instead of greying out.
  else if (scr == SCREEN_PRICE_I)  ScreenPrice_RangeText(rb, sizeof(rb));
  doc["range"] = rb;                       // empty on screens without one

  JsonArray en = doc["enabled"].to<JsonArray>();
  for (uint8_t i = 0; i < SCREEN_N; i++) en.add(Settings_ScreenEnabled(i));

  char b[64];
  Status_Text(ST_ADSB, b, sizeof(b));     doc["adsb"] = b;
  Status_Text(ST_RADAR, b, sizeof(b));    doc["radar"] = b;
  Status_Text(ST_FORECAST, b, sizeof(b)); doc["forecast"] = b;
  Status_Text(ST_PRICE, b, sizeof(b));    doc["price"] = b;
  Status_Text(ST_MIX, b, sizeof(b));      doc["mix"] = b;
  Status_Text(ST_STORM, b, sizeof(b));    doc["storm"] = b;
  Status_Text(ST_PLANEWATCH, b, sizeof(b)); doc["planeWatch"] = b;
  Status_Text(ST_FX, b, sizeof(b));       doc["fx"] = b;          // 0.9.6
  Status_Text(ST_CRYPTO, b, sizeof(b));   doc["crypto"] = b;      // 0.9.7
  Status_Text(ST_ROUTE, b, sizeof(b));    doc["route"] = b;       // 0.9.7
  PrintWatch_StatusText(b, sizeof(b));    doc["printWatch"] = b;
  {
    PrintInfo pi;
    PrintWatch_Get(&pi);
    JsonObject p = doc["print"].to<JsonObject>();
    p["state"] = PrintState_Name(pi.state);
    if (pi.pct >= 0) p["pct"] = (int)lroundf(pi.pct);
    if (pi.remainS >= 0) p["remain_s"] = pi.remainS;
    if (pi.elapsedS >= 0) p["elapsed_s"] = pi.elapsedS;
    if (pi.file[0]) p["file"] = pi.file;
    if (pi.material[0]) p["material"] = pi.material;
    if (pi.nextPollMs) p["next_s"] = (long)(pi.nextPollMs - millis()) / 1000;
  }
  sendJson(200, doc);
}

// --- Remembered WiFi networks (0.8.0) ----------------------------------------
// Saves the three slots WITHOUT connecting. The point is to enter the office
// network at home, before the device is carried there; it is tried whenever
// the current network goes away. A missing or empty "pass" keeps whatever is
// stored, because the page never knows the stored passwords.
static void handleWifiList() {
  JsonDocument doc;
  if (!readBody(doc)) { s_srv.send(400, "application/json", "{\"error\":\"json\"}"); return; }
  JsonArrayConst nets = doc["nets"];
  if (nets.isNull()) { s_srv.send(400, "application/json", "{\"error\":\"nets\"}"); return; }
  for (uint8_t i = 0; i < WIFI_SLOTS && i < nets.size(); i++) {
    const char* ssid = nets[i]["ssid"] | "";
    const char* pass = nets[i]["pass"] | "";
    Settings_SetWifiSlot(i, ssid, (pass && *pass) ? pass : nullptr);
  }
  s_srv.send(200, "application/json", "{\"ok\":true}");
}

// --- Home Assistant (0.8.0) ---------------------------------------------------
// One flat-ish JSON object for Home Assistant's RESTful integration: poll it
// every minute or so and pull the values out with value_template. Read-only,
// no password - it says nothing the screen does not already show to anyone in
// the room. Off unless switched on in the web UI. NOT TESTED against a real
// Home Assistant: the shape follows the documented REST sensor, but nobody has
// pointed one at it yet.
static const char* const SCREEN_KEYS[SCREEN_N] = {
  "clock", "planes", "meteo", "forecast", "price", "mix", "settings", "print", "fx", "crypto" };

static void handleHa() {
  if (!Settings_HaEnabled()) {
    s_srv.send(404, "application/json", "{\"error\":\"disabled\"}");
    return;
  }
  JsonDocument doc;
  doc["version"] = FW_VERSION;
  doc["uptime_s"] = (uint32_t)(millis() / 1000UL);
  doc["ip"]   = WiFi.localIP().toString();
  doc["ssid"] = WiFi.SSID();
  doc["rssi"] = WiFi.RSSI();
  doc["lat"]  = Settings_Lat();
  doc["lon"]  = Settings_Lon();

  const int scr = MPR_CurrentScreen();
  doc["screen"] = (scr >= 0 && scr < SCREEN_N) ? SCREEN_KEYS[scr] : "?";
  doc["night"] = Settings_IsNight();
  doc["brightness"] = Settings_Backlight();

  // Weather. null rather than 0 when not known yet: a 0 degC reading and "no
  // data" must not look the same in a graph.
  if (Forecast_CurrentValid()) {
    doc["temperature"]  = roundf(Forecast_CurrentTemp() * 10) / 10;
    doc["wind_kmh"]     = roundf(Forecast_CurrentWind() * 10) / 10;
    doc["precip_mm"]    = roundf(Forecast_CurrentPrecip() * 10) / 10;
    doc["weather_code"] = Forecast_CurrentCode();
  } else {
    doc["temperature"] = nullptr; doc["wind_kmh"] = nullptr;
    doc["precip_mm"] = nullptr;   doc["weather_code"] = nullptr;
  }
  if (Forecast_DayCount() > 0) {
    const FcDay& d = Forecast_Days()[0];
    doc["today_min"] = roundf(d.tmin * 10) / 10;
    doc["today_max"] = roundf(d.tmax * 10) / 10;
  }
  time_t rise, set;
  if (Forecast_SunTimes(&rise, &set)) {
    char t[8]; struct tm lt;
    localtime_r(&rise, &lt); snprintf(t, sizeof(t), "%02d:%02d", lt.tm_hour, lt.tm_min); doc["sunrise"] = t;
    localtime_r(&set, &lt);  snprintf(t, sizeof(t), "%02d:%02d", lt.tm_hour, lt.tm_min); doc["sunset"] = t;
  }
  if (AirQuality_Valid()) {
    if (AirQuality_Aqi() >= 0) doc["aqi"] = AirQuality_Aqi();
    doc["pm25"] = roundf(AirQuality_Pm25() * 10) / 10;
  }

  // Storm watch. Runs whenever this endpoint is on, so these are live even with
  // the "radar only when a storm is coming" option off.
  {
    JsonObject st = doc["storm"].to<JsonObject>();
    st["active"]    = StormWatch_Active();
    st["valid"]     = StormWatch_Valid();
    st["coverage"]  = StormWatch_InCoverage();
    st["threshold_dbz"] = Settings_StormDbz();
    st["radius_km"] = Settings_StormKm();
    if (StormWatch_Valid()) {
      st["max_dbz"] = StormWatch_MaxDbz() >= 0 ? (int)StormWatch_MaxDbz() : 0;
      if (StormWatch_NearestKm() >= 0) st["nearest_km"] = roundf(StormWatch_NearestKm());
      else                             st["nearest_km"] = nullptr;
      st["area_km2"] = StormWatch_Pixels();
      st["frame"]    = StormWatch_FrameTime();
    }
  }

  // Aircraft. The list is whatever was fetched last: the small watch area
  // while another screen is up, the radar range while the aircraft screen is.
  {
    JsonObject ac = doc["aircraft"].to<JsonObject>();
    ac["count"] = ADSB_DataStale() ? 0 : ADSB_Count();
    if (ADSB_DataAgeMs() != 0xFFFFFFFFUL) ac["data_age_s"] = ADSB_DataAgeMs() / 1000UL;
    ac["stale"] = ADSB_DataStale();
    ac["alert"] = MPR_AlertActive();
    float km = 0, brg = 0; bool win = false;
    const int i = PlaneWatch_Best(&km, &brg, &win);
    if (i >= 0) {
      const Aircraft& a = ADSB_List()[i];
      ac["callsign"]    = a.callsign[0] ? a.callsign : a.hex;
      ac["hex"]         = a.hex;
      ac["type"]        = a.type;
      ac["registration"]= a.reg;
      ac["kind"]        = PlaneIcon_Name(a.icon);
      ac["distance_km"] = roundf(km * 10) / 10;
      ac["bearing"]     = (int)lroundf(brg) % 360;
      ac["altitude_m"]  = (int)lroundf(a.altFt * 0.3048f);
      ac["speed_kmh"]   = (int)lroundf(a.gsKt * 1.852f);
      ac["in_window"]   = win;
      ac["squawk"]      = a.squawk;
    } else {
      ac["callsign"] = nullptr;
      ac["distance_km"] = nullptr;
    }
  }

  // Electricity, only when those screens are on (otherwise nothing is fetched).
  if (Energy_PriceValid()) {
    doc["price_spot_czk_mwh"] = roundf(Energy_PriceNow());
    doc["price_czk_kwh"] = roundf(Energy_FinalCzkKwh(Energy_PriceNow()) * 1000) / 1000;
  }
  if (Energy_MixValid()) doc["renewable_pct"] = roundf(Energy_RenewableShare() * 10) / 10;

  // 3D printer (0.9.0). Only when a printer is set up.
  if (Settings_PrintOn()) {
    PrintInfo pi;
    PrintWatch_Get(&pi);
    JsonObject p = doc["print"].to<JsonObject>();
    p["state"]     = PrintState_Name(pi.state);
    p["online"]    = PrintState_Online(pi.state);
    p["printing"]  = PrintState_Active(pi.state);
    p["alert"]     = MPR_PrintAlertActive();
    if (pi.pct >= 0) p["progress"] = roundf(pi.pct * 10) / 10; else p["progress"] = nullptr;
    if (pi.remainS >= 0) p["remaining_s"] = pi.remainS; else p["remaining_s"] = nullptr;
    if (pi.elapsedS >= 0) p["elapsed_s"] = pi.elapsedS; else p["elapsed_s"] = nullptr;
    p["file"]      = pi.file;
    p["material"]  = pi.material;
    if (isfinite(pi.tNoz)) p["nozzle_c"] = roundf(pi.tNoz * 10) / 10; else p["nozzle_c"] = nullptr;
    if (isfinite(pi.tBed)) p["bed_c"] = roundf(pi.tBed * 10) / 10;    else p["bed_c"] = nullptr;
    if (pi.layer >= 0) p["layer"] = pi.layer;
    if (pi.layers > 0) p["layers"] = pi.layers;
    p["message"]   = pi.message;
  }

  sendJson(200, doc);
}

static void handleScan() {
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  int n = WiFi.scanNetworks();
  for (int i = 0; i < n && i < 20; i++) {
    JsonObject o = arr.add<JsonObject>();
    o["ssid"] = WiFi.SSID(i);
    o["rssi"] = WiFi.RSSI(i);
  }
  WiFi.scanDelete();
  sendJson(200, doc);
}

static void handleWifi() {
  JsonDocument doc;
  if (!readBody(doc)) { s_srv.send(400, "application/json", "{\"error\":\"json\"}"); return; }
  const char* ssid = doc["ssid"] | "";
  const char* pass = doc["pass"] | "";
  if (!*ssid) { s_srv.send(400, "application/json", "{\"error\":\"ssid\"}"); return; }
  Settings_SetWifi(ssid, pass);
  s_wantConnect = true;
  JsonDocument res; res["ok"] = true;
  sendJson(200, res);
}

// Town name -> coordinates, so nobody has to look up their latitude by hand.
// Proxied through the device because the page is served from the device and a
// browser would refuse the cross-origin call.
static void handleGeocode() {
  String q = s_srv.arg("q");
  if (q.length() == 0) { s_srv.send(400, "application/json", "[]"); return; }

  String enc;
  for (size_t i = 0; i < q.length(); i++) {
    char ch = q[i];
    if (isalnum((unsigned char)ch)) enc += ch;
    else { char b[4]; snprintf(b, sizeof(b), "%%%02X", (unsigned char)ch); enc += b; }
  }

  char url[220];
  snprintf(url, sizeof(url), "%s?name=%s&count=8&language=%s&format=json",
           GEOCODE_URL, enc.c_str(), Lang_Code());

  String body;
  if (!Net_GetString(url, body, "GEOKOD")) { s_srv.send(502, "application/json", "[]"); return; }

  JsonDocument filter;
  JsonObject f = filter["results"].add<JsonObject>();
  f["name"] = true; f["latitude"] = true; f["longitude"] = true; f["country"] = true;
  JsonDocument doc;
  if (deserializeJson(doc, body, DeserializationOption::Filter(filter))) {
    s_srv.send(502, "application/json", "[]");
    return;
  }

  JsonDocument out;
  JsonArray arr = out.to<JsonArray>();
  for (JsonObjectConst r : doc["results"].as<JsonArrayConst>()) {
    JsonObject o = arr.add<JsonObject>();
    o["name"] = r["name"];
    o["country"] = r["country"];
    o["lat"] = r["latitude"];
    o["lon"] = r["longitude"];
  }
  sendJson(200, out);
}

// GET: the plain export, never with the printer's secrets - a link anyone on
// the network can open. POST {password}: the same behind the admin password,
// and with the printer's password and API key when "printExport" is ticked.
static void sendExport(bool secrets) {
  JsonDocument doc;
  JsonObject o = doc.to<JsonObject>();
  Settings_ToJson(o);
  o["version"] = FW_VERSION;
  // Never in it: the WiFi passwords and the admin password. A backup file ends
  // up in a download folder, in an email, in a support ticket.
  // The printer's secrets only when the user asked for them (0.9.0).
  if (secrets && Settings_PrintExport()) {
    o["printKey"]  = Settings_PrintKey();
    o["printPass"] = Settings_PrintPass();
  }
  String out;
  serializeJsonPretty(doc, out);
  s_srv.sendHeader("Cache-Control", "no-store");
  s_srv.sendHeader("Content-Disposition", "attachment; filename=meteoplaneradar.json");
  s_srv.send(200, "application/json", out);
}
static void handleExport() { sendExport(false); }
static void handleExportPost() {
  JsonDocument doc;
  if (!readBody(doc)) { s_srv.send(400, "application/json", "{\"error\":\"json\"}"); return; }
  if (!authed(doc)) return;
  sendExport(true);
}

static void handleImport() {
  JsonDocument doc;
  if (!readBody(doc)) { s_srv.send(400, "application/json", "{\"error\":\"json\"}"); return; }
  if (!authed(doc)) return;
  JsonObjectConst cfg = doc["config"];
  if (cfg.isNull()) { s_srv.send(400, "application/json", "{\"error\":\"config\"}"); return; }
  Settings_FromJson(cfg);
  // The printer's secrets come along only with "printExport" on (in the file
  // or already on the device). Empty or missing leaves them as they are.
  if (Settings_PrintExport()) {
    Settings_SetPrintKey(cfg["printKey"] | "");
    Settings_SetPrintPass(cfg["printPass"] | "");
  }
  NightMode_Apply();
  s_pend.clear(); s_pendOn = false;      // the import is the newer word
  s_wantRestart = true;
  JsonDocument res; res["ok"] = true;
  sendJson(200, res);
}

// 0.8.6: the admin password has an endpoint of its own. Before, the same field
// unlocked update/import/reset AND went out with every Save as the new
// password - a typo made while unlocking a reset was quietly saved as the new
// password. Now: {password: current, newPassword: new}; the current one goes
// through the same check as every other protected action, and an empty new
// password switches the protection off.
static void handlePassword() {
  JsonDocument doc;
  if (!readBody(doc)) { s_srv.send(400, "application/json", "{\"error\":\"json\"}"); return; }
  if (!authed(doc)) return;                     // sends 403 and explains itself
  Settings_SetAdminPassword(doc["newPassword"] | "");
  // The update page checks the password on every request, so the new one is
  // live immediately - no restart needed.
  s_srv.send(200, "application/json",
             Settings_HasAdminPassword() ? "{\"ok\":true,\"hasPassword\":true}"
                                         : "{\"ok\":true,\"hasPassword\":false}");
}

static void handleReboot() {
  // 0.9.8: "Restart now" on the page - what was held back goes in first.
  if (s_pendOn) { applyPending(); Serial.println("Nastaveni zmeneno, restartuji na zadost"); }
  s_srv.send(200, "application/json", "{\"ok\":true}");
  delay(200);
  ESP.restart();
}

static void handleReset() {
  JsonDocument doc;
  readBody(doc);
  if (!authed(doc)) return;
  s_srv.send(200, "application/json", "{\"ok\":true}");
  delay(200);
  Settings_ClearAll();
  ESP.restart();
}

// Captive portal: whatever the phone asks for, hand it the setup page. Without
// this the "sign in to network" banner never appears and the user is left
// typing an IP address they have not been told.
static void handleNotFound() {
  if (s_apMode) {
    s_srv.sendHeader("Location", String("http://") + WiFi.softAPIP().toString() + "/", true);
    s_srv.send(302, "text/plain", "");
    return;
  }
  s_srv.send(404, "text/plain", "404");
}

// --- OTA callbacks ----------------------------------------------------------
// The RGB panel streams its framebuffer out of PSRAM continuously, and writing
// flash suspends the cache from under it - the picture tears and jumps. Nothing
// the drawing code can do about it, so the backlight goes off for the duration
// and the browser shows the real progress bar.
static void otaStart() {
  s_updating = true;
  gfx->fillScreen(C_BLACK);
  UI_TextCentered(LT("Probiha aktualizace...", "Probíhá aktualizace...", "Updating firmware...",
                     "Prebieha aktualizacia...", "Prebieha aktualizácia..."),
                  LCD_HEIGHT / 2 - 10, C_WHITE, 2);
  UI_TextCentered(LT("Neodpojujte napajeni", "Neodpojujte napájení", "Do not disconnect power",
                     "Neodpajajte napajanie", "Neodpájajte napájanie"),
                  LCD_HEIGHT / 2 + 20, C_GRAY, 1);
  gfx->flush();
  delay(700);
  Set_Backlight(0);
}

static void otaEnd(bool ok) {
  Set_Backlight(Settings_Backlight());
  gfx->fillScreen(C_BLACK);
  UI_TextCentered(ok ? LT("Hotovo, restartuji...", "Hotovo, restartuji...", "Done, restarting...",
                          "Hotovo, restartujem...", "Hotovo, reštartujem...")
                     : LT("Aktualizace selhala", "Aktualizace selhala", "Update failed",
                          "Aktualizacia zlyhala", "Aktualizácia zlyhala"),
                  LCD_HEIGHT / 2, ok ? C_GREEN : C_RED, 2);
  gfx->flush();
  s_updating = false;
}

// --- Firmware update --------------------------------------------------------
// This replaces the ElegantOTA library. That library is AGPL-3.0, which would
// have made every binary built from this project AGPL too; the Update class
// ships with the ESP32 core and the web server was already here, so the page
// below was the only piece actually missing.

static bool   s_updOk = false;
static String s_updErr;          // empty = no failure yet

// HTTP Basic, and only when a password is set - the open default is documented
// in Settings.h and in the README.
static bool updateAuthed() {
  if (!Settings_HasAdminPassword()) return true;
  return s_srv.authenticate(WEB_ADMIN_USER, Settings_AdminPassword());
}

static const char UPDATE_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="{{LANG}}"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>{{TITLE}}</title><style>
body{background:#111;color:#eee;font:16px system-ui,sans-serif;margin:0;padding:24px;
 max-width:520px;margin-inline:auto}
h1{font-size:20px;margin:0 0 4px}
p{color:#aaa;line-height:1.5}
input[type=file]{width:100%;padding:12px;background:#1c1c1c;border:1px solid #333;
 border-radius:8px;color:#eee;box-sizing:border-box}
button{width:100%;padding:14px;margin-top:12px;font-size:16px;border:0;border-radius:8px;
 background:#2d7;color:#000;font-weight:600;cursor:pointer}
button:disabled{background:#444;color:#888;cursor:default}
progress{width:100%;height:10px;margin-top:16px}
#s{margin-top:10px;min-height:1.4em;font-weight:600}
a{color:#5bf}
</style></head><body>
<h1>{{TITLE}}</h1>
<p>{{HINT}}</p>
<input type="file" id="f" accept=".bin">
<button id="b">{{SEND}}</button>
<progress id="p" value="0" max="100"></progress>
<div id="s"></div>
<p><a href="/">{{BACK}}</a></p>
<script>
var f=document.getElementById('f'),b=document.getElementById('b'),
    p=document.getElementById('p'),s=document.getElementById('s');
b.onclick=function(){
 if(!f.files.length){s.textContent='{{PICK}}';return;}
 var fd=new FormData();fd.append('update',f.files[0]);
 var x=new XMLHttpRequest();x.open('POST','/update');
 x.upload.onprogress=function(e){if(e.lengthComputable){
   var v=Math.round(e.loaded/e.total*100);p.value=v;s.textContent=v+' %';}};
 x.onload=function(){b.disabled=false;f.disabled=false;
   if(x.status==200){p.value=100;s.textContent='{{OK}}';}
   else{s.textContent='{{FAIL}}: '+(x.responseText||x.status);}};
 x.onerror=function(){b.disabled=false;f.disabled=false;s.textContent='{{FAIL}}';};
 b.disabled=true;f.disabled=true;s.textContent='0 %';
 x.send(fd);};
</script></body></html>)rawliteral";

static void handleUpdatePage() {
  if (!updateAuthed()) { s_srv.requestAuthentication(); return; }
  // The page is UTF-8, so Czech and Slovak get their accents here.
  const uint8_t l = Lang_Get();
  auto W = [l](const char* cz, const char* en, const char* sk) {
    return l == LANG_EN ? en : (l == LANG_SK ? sk : cz);
  };
  String p = FPSTR(UPDATE_HTML);
  p.replace("{{LANG}}",  Lang_Code());
  p.replace("{{TITLE}}", W("Aktualizace firmwaru", "Firmware update", "Aktualizácia firmvéru"));
  p.replace("{{HINT}}",  W("Vyberte soubor <b>.ino.bin</b> (ten bez <i>merged</i>). "
                           "Displej po dobu zápisu zhasne a po dokončení se sám rozsvítí.",
                           "Pick the <b>.ino.bin</b> file (the one without "
                           "<i>merged</i>). The display goes dark while the "
                           "flash is written and comes back on when it is done.",
                           "Vyberte súbor <b>.ino.bin</b> (ten bez <i>merged</i>). "
                           "Displej počas zápisu zhasne a po dokončení sa sám rozsvieti."));
  p.replace("{{SEND}}",  W("Nahrát", "Upload", "Nahrať"));
  p.replace("{{BACK}}",  W("Zpět na nastavení", "Back to settings", "Späť na nastavenia"));
  p.replace("{{PICK}}",  W("Nejdřív vyberte soubor.", "Pick a file first.", "Najprv vyberte súbor."));
  p.replace("{{OK}}",    W("Hotovo. Zařízení se restartuje.", "Done. The device is restarting.",
                           "Hotovo. Zariadenie sa reštartuje."));
  p.replace("{{FAIL}}",  W("Aktualizace selhala", "Update failed", "Aktualizácia zlyhala"));
  s_srv.sendHeader("Cache-Control", "no-store");
  s_srv.send(200, "text/html; charset=utf-8", p);
}

// Called repeatedly by WebServer as the body arrives. The whole transfer runs
// inside one handleClient(), so nothing else can be drawing meanwhile - but the
// watchdog still has to be fed by hand.
static void handleUpdateUpload() {
  HTTPUpload& up = s_srv.upload();

  switch (up.status) {
    case UPLOAD_FILE_START:
      s_updOk = false;
      s_updErr = "";
      if (!updateAuthed()) { s_updErr = "auth"; return; }
      Serial.printf("OTA: %s\n", up.filename.c_str());
      otaStart();
      // The browser does not announce the image size up front, so let Update
      // take the whole free OTA slot.
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
        s_updErr = Update.errorString();
        otaEnd(false);
      }
      break;

    case UPLOAD_FILE_WRITE:
      if (s_updErr.length()) return;              // already failed, drain the body
      if (Update.write(up.buf, up.currentSize) != up.currentSize) {
        s_updErr = Update.errorString();
        Update.abort();
        otaEnd(false);
        return;
      }
      Watchdog_Feed();
      break;

    case UPLOAD_FILE_END:
      if (s_updErr.length()) return;
      if (Update.end(true)) {
        s_updOk = true;
        Serial.printf("OTA: hotovo, %u B\n", (unsigned)up.totalSize);
        otaEnd(true);
      } else {
        s_updErr = Update.errorString();
        otaEnd(false);
      }
      break;

    case UPLOAD_FILE_ABORTED:
      if (s_updErr == "auth") return;             // never started, keep the 401
      if (Update.isRunning()) Update.abort();
      if (s_updating) otaEnd(false);              // only if the screen was taken over
      s_updErr = "aborted";
      break;
  }
}

// Runs once the body has been consumed, so this is where the verdict is sent.
static void handleUpdateDone() {
  if (s_updErr == "auth") { s_updErr = ""; s_srv.requestAuthentication(); return; }
  s_srv.sendHeader("Connection", "close");
  if (s_updOk) {
    s_srv.send(200, "text/plain", "OK");
    delay(400);
    ESP.restart();
    return;
  }
  s_srv.send(500, "text/plain", s_updErr.length() ? s_updErr : String("update failed"));
  s_updErr = "";
}

// --- Lifecycle --------------------------------------------------------------
void WebConfig_Begin(bool apMode) {
  s_apMode = apMode;

  // The handlers are registered ONCE for the life of the process. This function
  // is called at least twice in a normal boot - first for the access point,
  // then again after joining the home network - and WebServer::on() appends to
  // a list rather than replacing, so registering again would leave a duplicate
  // of every route behind.
  if (s_running) {
    // Only the role-specific parts change.
    if (apMode) {
      s_dns.setErrorReplyCode(DNSReplyCode::NoError);
      s_dns.start(53, "*", WiFi.softAPIP());
    } else {
      s_dns.stop();
      if (MDNS.begin(WEB_HOSTNAME)) MDNS.addService("http", "tcp", WEB_PORT);
      Serial.printf("Web: http://%s.local/ nebo http://%s/\n",
                    WEB_HOSTNAME, WiFi.localIP().toString().c_str());
    }
    return;
  }

  s_srv.on("/", HTTP_GET, handleRoot);
  s_srv.on("/api/config", HTTP_GET, handleGetConfig);
  s_srv.on("/api/config", HTTP_POST, handlePostConfig);
  s_srv.on("/api/status", HTTP_GET, handleStatus);
  s_srv.on("/api/screen", HTTP_POST, handleScreen);
  s_srv.on("/api/range", HTTP_POST, handleRange);
  s_srv.on("/api/scan", HTTP_GET, handleScan);
  s_srv.on("/api/wifi", HTTP_POST, handleWifi);
  s_srv.on("/api/wifilist", HTTP_POST, handleWifiList);
  s_srv.on("/api/ha", HTTP_GET, handleHa);
  s_srv.on("/api/geocode", HTTP_GET, handleGeocode);
  s_srv.on("/api/export", HTTP_GET, handleExport);
  s_srv.on("/api/export", HTTP_POST, handleExportPost);
  s_srv.on("/api/import", HTTP_POST, handleImport);
  s_srv.on("/api/password", HTTP_POST, handlePassword);
  s_srv.on("/api/reboot", HTTP_POST, handleReboot);
  s_srv.on("/api/reset", HTTP_POST, handleReset);
  // The update page authenticates with HTTP Basic when a password is set. See
  // the note in Settings.h about why the password is stored in the clear.
  s_srv.on("/update", HTTP_GET, handleUpdatePage);
  s_srv.on("/update", HTTP_POST, handleUpdateDone, handleUpdateUpload);
  s_srv.onNotFound(handleNotFound);

  s_srv.begin();
  s_running = true;

  if (apMode) {
    s_dns.setErrorReplyCode(DNSReplyCode::NoError);
    s_dns.start(53, "*", WiFi.softAPIP());
    Serial.printf("Web: portal na http://%s/\n", WiFi.softAPIP().toString().c_str());
  } else {
    if (MDNS.begin(WEB_HOSTNAME)) {
      MDNS.addService("http", "tcp", WEB_PORT);
      Serial.printf("Web: http://%s.local/ nebo http://%s/\n",
                    WEB_HOSTNAME, WiFi.localIP().toString().c_str());
    } else {
      Serial.printf("Web: http://%s/  (mDNS se nespustilo)\n",
                    WiFi.localIP().toString().c_str());
    }
  }
}

void WebConfig_Loop() {
  if (!s_running) return;
  if (s_apMode) s_dns.processNextRequest();
  s_srv.handleClient();
  if (s_pendOn && millis() - s_pendAt >= RESTART_DELAY_MS) {
    Serial.println("Nastaveni ceka na restart 2 min, restartuji");
    applyPending();
  }
}
