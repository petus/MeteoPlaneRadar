// =============================================================================
//  MeteoPlaneRadar
//  APRS client - fetching one station's position from aprs.fi.
//
//  Mirrors ADSB.cpp: WiFiClientSecure + HTTPClient + a filtered ArduinoJson
//  parse, two attempts, "keep the last good snapshot on a transport error". The
//  response is tiny (one station), so the whole body is read with getString()
//  instead of the streaming PSRAM buffer the aircraft feed needs.
//
//  Project: MeteoPlaneRadar - live aircraft radar on a round touchscreen
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
//           Ondra OK1CDJ / apps.ok1cdj.com
//  Web:     https://chiptron.cz
//  Board:   Waveshare ESP32-S3-Touch-LCD-2.1 (round 480x480 display, ST7701)
// =============================================================================
#include "APRS.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <string.h>
#include <stdlib.h>
#include "esp_heap_caps.h"   // internal-heap check before the TLS handshake

static AprsStation s_st;                 // last good snapshot
static bool  s_hasFix = false;           // is s_st a real position?
static char  s_status[48] = "";          // human message when there is no fix
static void (*s_poll)() = nullptr;

void APRS_SetPollFn(void (*fn)()) { s_poll = fn; }
const AprsStation* APRS_Get() { return &s_st; }
bool APRS_HasFix() { return s_hasFix; }
const char* APRS_Status() { return s_status; }

static void poll() { if (s_poll) s_poll(); }

// aprs.fi returns lat/lng/speed/course as JSON strings; read them numerically.
static bool readNum(JsonObjectConst o, const char* key, float* out) {
  JsonVariantConst v = o[key];
  if (v.is<float>() || v.is<double>() || v.is<int>()) { *out = v.as<float>(); return true; }
  if (v.is<const char*>()) {
    const char* s = v.as<const char*>();
    if (s && *s) { *out = (float)atof(s); return true; }
  }
  return false;
}

// Same guard as ADSB: a TLS handshake needs ~45 kB of INTERNAL RAM, and running
// out surfaces only as a bare "HTTP -1". Skip and keep the previous data.
static bool netHeapOk() {
  size_t freeInt = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (freeInt >= NET_MIN_HEAP) return true;
  Serial.printf("APRS: malo volne pameti (%u B < %u B), stahovani preskoceno\n",
                (unsigned)freeInt, (unsigned)NET_MIN_HEAP);
  return false;
}

bool APRS_Fetch(const char* call, const char* apikey) {
  if (!call || !call[0] || !apikey || !apikey[0]) {
    snprintf(s_status, sizeof(s_status), "Chybi volaci znak / API klic");
    s_hasFix = false;
    return false;
  }
  if (WiFi.status() != WL_CONNECTED) return false;   // keep last snapshot
  if (!netHeapOk()) return false;

  // Callsigns and aprs.fi keys are plain tokens (letters/digits/-/.), so no URL
  // encoding is required.
  char url[256];
  snprintf(url, sizeof(url), "%s?name=%s&what=loc&apikey=%s&format=json",
           APRS_API_BASE, call, apikey);

  const int MAX_ATTEMPTS = 2;
  for (int attempt = 1; attempt <= MAX_ATTEMPTS; attempt++) {
    poll();
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.setConnectTimeout(8000);
    http.setTimeout(12000);
    http.setReuse(false);
    if (!http.begin(client, url)) {
      if (attempt < MAX_ATTEMPTS) { delay(200); continue; }
      return false;
    }
    http.addHeader("User-Agent", "MeteoPlaneRadar/1.0 (+https://chiptron.cz)");
    http.addHeader("Accept", "application/json");

    int code = http.GET();
    if (code != HTTP_CODE_OK) {
      Serial.printf("APRS: HTTP %d (attempt %d)\n", code, attempt);
      http.end();
      if (attempt < MAX_ATTEMPTS) { delay(200); continue; }
      return false;
    }

    poll();
    String body = http.getString();
    http.end();
    if (body.length() < 8) {
      if (attempt < MAX_ATTEMPTS) { delay(200); continue; }
      return false;
    }

    // Keep only the fields we use, so a big "comment" or extra keys cannot bloat
    // the parsed document.
    JsonDocument filter;
    filter["result"]      = true;
    filter["found"]       = true;
    filter["description"] = true;
    {
      JsonObject e = filter["entries"].add<JsonObject>();
      e["name"]     = true;
      e["lat"]      = true;
      e["lng"]      = true;
      e["comment"]  = true;
      e["speed"]    = true;
      e["course"]   = true;
      e["lasttime"] = true;
    }
    JsonDocument doc;
    DeserializationError err =
        deserializeJson(doc, body, DeserializationOption::Filter(filter));
    if (err) {
      Serial.printf("APRS: JSON %s (attempt %d)\n", err.c_str(), attempt);
      if (attempt < MAX_ATTEMPTS) { delay(200); continue; }
      return false;
    }

    // Transport succeeded from here on: return true and let the screen react to
    // s_hasFix / s_status. A retry would not change an "invalid key" answer.
    const char* result = doc["result"] | "";
    if (strcmp(result, "ok") != 0) {
      const char* desc = doc["description"] | "chyba API";
      snprintf(s_status, sizeof(s_status), "%s", desc);
      Serial.printf("APRS: result=%s (%s)\n", result, desc);
      s_hasFix = false;
      return true;
    }

    int found = doc["found"] | 0;
    JsonArrayConst entries = doc["entries"].as<JsonArrayConst>();
    if (found <= 0 || entries.isNull() || entries.size() == 0) {
      snprintf(s_status, sizeof(s_status), "Stanice nenalezena");
      s_hasFix = false;
      return true;
    }

    JsonObjectConst e = entries[0];
    float la = 0, lo = 0;
    if (!readNum(e, "lat", &la) || !readNum(e, "lng", &lo)) {
      snprintf(s_status, sizeof(s_status), "Stanice bez polohy");
      s_hasFix = false;
      return true;
    }

    s_st.lat = la;
    s_st.lon = lo;
    const char* nm = e["name"] | call;
    strncpy(s_st.name, nm, sizeof(s_st.name) - 1);
    s_st.name[sizeof(s_st.name) - 1] = '\0';
    const char* cm = e["comment"] | "";
    strncpy(s_st.comment, cm, sizeof(s_st.comment) - 1);
    s_st.comment[sizeof(s_st.comment) - 1] = '\0';
    float f = 0;
    s_st.speedKmh = readNum(e, "speed", &f) ? f : 0;
    s_st.course   = readNum(e, "course", &f) ? f : 0;
    // "lasttime" is a 10-digit epoch - parse as long, NOT via float (float has
    // only ~7 significant digits and would round it off by minutes).
    const char* lts = e["lasttime"] | "";
    s_st.lastTimeUtc = lts[0] ? atol(lts) : (long)(e["lasttime"] | 0L);
    s_st.hasPos = true;
    s_hasFix = true;
    s_status[0] = '\0';
    Serial.printf("APRS: %s %.5f,%.5f\n", s_st.name, s_st.lat, s_st.lon);
    return true;
  }
  return false;
}
