// =============================================================================
//  MeteoPlaneRadar
//  ADS-B client - fetching aircraft data from adsb.fi.
//
//  Project: MeteoPlaneRadar - live aircraft radar on a round touchscreen
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
//  Web:     https://chiptron.cz
//  Board:   Waveshare ESP32-S3-Touch-LCD-2.1 (round 480x480 display, ST7701)
// =============================================================================
#include "ADSB.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <math.h>
#include <string.h>   // strcmp / strncpy for the hex identity handling

static const float KM_PER_NM = 1.852f;

static Aircraft s_list[ADSB_MAX];   // last GOOD snapshot shown on screen
static int s_count = 0;
// Scratch buffer we parse into. The visible list (s_list/s_count) is only
// overwritten once a fetch has fully and correctly parsed - so a truncated or
// otherwise broken JSON never blanks the radar, it just keeps the previous
// aircraft until the next good fetch.
static Aircraft s_tmp[ADSB_MAX];
static void (*s_poll)() = nullptr;

void ADSB_SetPollFn(void (*fn)()) { s_poll = fn; }
int  ADSB_Count() { return s_count; }
const Aircraft* ADSB_List() { return s_list; }

int ADSB_FindByHex(const char* hex) {
  if (!hex || !hex[0]) return -1;
  for (int i = 0; i < s_count; i++) {
    if (strcmp(s_list[i].hex, hex) == 0) return i;
  }
  return -1;   // no longer in the data
}

static void poll() { if (s_poll) s_poll(); }

// Reads a number even when the JSON holds it as a string.
static bool readFloat(JsonObjectConst o, const char* key, float* out) {
  JsonVariantConst v = o[key];
  if (v.is<float>() || v.is<double>() || v.is<int>()) { *out = v.as<float>(); return true; }
  if (v.is<const char*>()) {
    const char* s = v.as<const char*>();
    if (s && *s) { *out = (float)atof(s); return true; }
  }
  return false;
}

// Copies the ICAO hex address. This is the aircraft's stable identity and is
// stored for every target, even when a callsign is present - the callsign is a
// *flight* number (it changes between rotations, and two aircraft can carry the
// same one on different days), so it is no good as a key.
static void copyHex(Aircraft* a, JsonObjectConst plane) {
  const char* hex = plane["hex"] | "";
  strncpy(a->hex, hex, sizeof(a->hex) - 1);
  a->hex[sizeof(a->hex) - 1] = '\0';
}

static void copyCallsign(Aircraft* a, JsonObjectConst plane) {
  const char* flight = plane["flight"] | "";
  const char* hex = plane["hex"] | "";
  const char* src = (flight[0] != '\0') ? flight : hex;
  while (*src == ' ') src++;   // skip leading spaces
  int i = 0;
  while (src[i] && i < (int)sizeof(a->callsign) - 1) { a->callsign[i] = src[i]; i++; }
  while (i > 0 && a->callsign[i-1] == ' ') i--;   // trim trailing spaces
  a->callsign[i] = '\0';
}

bool ADSB_Fetch(double lat, double lon, float radiusKm) {
  // No link -> keep whatever we last drew (do NOT blank the radar).
  if (WiFi.status() != WL_CONNECTED) { Serial.println("ADSB: no WiFi"); return false; }

  float distNm = radiusKm / KM_PER_NM;
  char url[128];
  snprintf(url, sizeof(url), "%s%.5f/lon/%.5f/dist/%.1f",
           ADSB_API_BASE, lat, lon, distNm);

  Serial.printf("ADSB: %s\n", url);
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setTimeout(10000);
  if (!http.begin(client, url)) { Serial.println("ADSB: begin failed"); return false; }

  // adsb.fi answers only to a couple of headers; grab the content length so we
  // can tell a genuinely complete body from a connection that dropped mid-way.
  const char* wanted[] = { "Content-Length" };
  http.collectHeaders(wanted, 1);

  int code = http.GET();
  Serial.printf("ADSB: HTTP %d\n", code);
  if (code != HTTP_CODE_OK) { http.end(); return false; }   // keep last good data

  int declaredLen = http.getSize();   // -1 when the server does not declare it

  // Pobieramy cały ciąg tekstowy JSON do pamięci RAM urządzenia.
  // Zapobiega to przerwaniu pobierania ("IncompleteInput") w locie.
  String payload = http.getString();
  http.end();

  // Dynamiczne przydzielenie dokumentu w ArduinoJson 7
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, payload);

  // A truncated download shows up as IncompleteInput (the stream ended inside
  // the JSON). Any other error (EmptyInput, InvalidInput, NoMemory) is treated
  // the same way: reject the fetch and keep the previous aircraft on screen.
  if (err) {
    Serial.printf("ADSB: JSON error (%s) - keeping last %d aircraft\n",
                  err.c_str(), s_count);
    return false;
  }

  // Root must be an object with an "ac" array. adsb.fi always wraps the list in
  // {"ac":[...]}; a bare or unexpected shape means the body was not what we
  // asked for (e.g. an HTML error page that still parsed) - reject it.
  if (!doc.is<JsonObjectConst>() || doc["ac"].isNull()) {
    Serial.println("ADSB: no 'ac' array - keeping last data");
    return false;
  }

  JsonArrayConst ac = doc["ac"].as<JsonArrayConst>();
  int n = 0;
  for (JsonObjectConst plane : ac) {
    float plat, plon;
    if (!readFloat(plane, "lat", &plat) || !readFloat(plane, "lon", &plon)) continue;
    if (n >= ADSB_MAX) break;
    s_tmp[n].lat = plat;
    s_tmp[n].lon = plon;
    // Ground track - record whether it is present at all.
    float tr = 0;
    if (readFloat(plane, "track", &tr) || readFloat(plane, "true_heading", &tr)) {
      s_tmp[n].track = tr;
      s_tmp[n].hasTrack = true;
    } else {
      s_tmp[n].track = 0;
      s_tmp[n].hasTrack = false;
    }
    // Altitude (barometric), speed, climb rate.
    JsonVariantConst ab = plane["alt_baro"];
    s_tmp[n].onGround = ab.is<const char*>() && strcmp(ab.as<const char*>(), "ground") == 0;
    float f = 0;
    s_tmp[n].altFt = (!s_tmp[n].onGround && readFloat(plane, "alt_baro", &f)) ? f : 0;
    s_tmp[n].gsKt = readFloat(plane, "gs", &f) ? f : 0;
    s_tmp[n].baroRate = readFloat(plane, "baro_rate", &f) ? f : 0;
    // Aircraft type (the key varies by source).
    const char* ty = plane["t"] | (plane["type"] | "");
    strncpy(s_tmp[n].type, ty, sizeof(s_tmp[n].type) - 1);
    s_tmp[n].type[sizeof(s_tmp[n].type) - 1] = '\0';
    copyHex(&s_tmp[n], plane);
    copyCallsign(&s_tmp[n], plane);
    n++;
  }

  // Commit the scratch snapshot to the live list in one go.
  for (int i = 0; i < n; i++) s_list[i] = s_tmp[i];
  s_count = n;
  Serial.printf("ADSB: %d aircraft (declaredLen=%d)\n", n, declaredLen);
  return true;
}