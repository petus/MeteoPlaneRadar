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
#include <stdlib.h>   // strtof
#include "esp_heap_caps.h"   // PSRAM body buffer
#include "Outside.h"         // clock is seeded from the response header
#include "Status.h"          // one-line health note for the web status page
#include "Config.h"          // SQUAWK_*
#include "NetSink.h"        // chunked-safe body reader
#include "PlaneIcon.h"      // icon kind from the type code

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

// --- Overload protection ---------------------------------------------------
static bool          s_everOk     = false;
static unsigned long s_lastOkMs   = 0;
static unsigned long s_blockUntil = 0;     // gate: no request before this
static bool          s_blocked    = false;
static uint8_t       s_failCount  = 0;
static int           s_lastCode   = 0;

unsigned long ADSB_DataAgeMs() { return s_everOk ? millis() - s_lastOkMs : 0xFFFFFFFFUL; }
bool ADSB_DataStale()  { return ADSB_DataAgeMs() > ADSB_STALE_MS; }
bool ADSB_DataHidden() { return ADSB_DataAgeMs() > ADSB_HIDE_MS; }
int  ADSB_LastHttpCode() { return s_lastCode; }
unsigned long ADSB_PauseLeftMs() {
  if (!s_blocked) return 0;
  const long left = (long)(s_blockUntil - millis());
  if (left <= 0) { s_blocked = false; return 0; }
  return (unsigned long)left;
}

// One failed poll (after its attempts). retryAfterS > 0 = the server said so.
static void noteFailure(long retryAfterS) {
  if (s_failCount < 10) s_failCount++;
  unsigned long pause = ADSB_BACKOFF_BASE_MS << (s_failCount - 1);
  if (pause > ADSB_BACKOFF_MAX_MS) pause = ADSB_BACKOFF_MAX_MS;
  if (retryAfterS > 0) {
    if (retryAfterS > ADSB_RETRY_AFTER_MAX_S) retryAfterS = ADSB_RETRY_AFTER_MAX_S;
    if ((unsigned long)retryAfterS * 1000UL > pause) pause = (unsigned long)retryAfterS * 1000UL;
  }
  s_blockUntil = millis() + pause;
  s_blocked = true;
  Serial.printf("ADSB: chyba %d, %u. v rade, pauza %lu s\n",
                s_lastCode, (unsigned)s_failCount, pause / 1000UL);
  Status_Set(ST_ADSB, "chyba %d, pauza %lu s", s_lastCode, pause / 1000UL);
}

static void noteSuccess() {
  s_everOk = true;
  s_lastOkMs = millis();
  s_failCount = 0;
  s_blocked = false;
}
int  ADSB_Count() { return s_count; }
const Aircraft* ADSB_List() { return s_list; }

const char* ADSB_EmergencyCode(const Aircraft& a) {
  if (!a.squawk[0]) return nullptr;
  if (strcmp(a.squawk, SQUAWK_HIJACK) == 0) return SQUAWK_HIJACK;
  if (strcmp(a.squawk, SQUAWK_RADIO)  == 0) return SQUAWK_RADIO;
  if (strcmp(a.squawk, SQUAWK_EMERG)  == 0) return SQUAWK_EMERG;
  return nullptr;
}

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
    // ArduinoJson converts numeric strings itself, but that cannot tell a
    // missing value from a non-numeric one - atof("ground") used to return 0.0
    // and report success. strtof's end pointer makes the difference visible, so
    // an unexpected literal is rejected instead of becoming an altitude of 0.
    const char* str = v.as<const char*>();
    if (!str || !*str) return false;
    char* end = nullptr;
    float f = strtof(str, &end);
    if (end == str) return false;
    *out = f;
    return true;
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

// The callsign, and ONLY the callsign. There used to be a fallback to the hex
// address here, so that an aircraft which broadcasts no callsign (TIS-B, MLAT,
// private and military traffic) still showed something. That was wrong: the
// callsign is what gets sent to the route API, and a hex address normalises
// into a perfectly valid IATA flight number - "a31234" becomes "A31234", which
// is Aegean Airlines 1234, so an aircraft over Prague was shown as flying
// Athens - Istanbul. When there is no callsign, this stays empty and nothing
// asks about a route; whoever needs something to draw picks the fallback
// themselves (ScreenPlanes prints the hex under the icon).
static void copyCallsign(Aircraft* a, JsonObjectConst plane) {
  const char* src = plane["flight"] | "";
  while (*src == ' ' || *src == '\t') src++;   // adsb.fi pads to eight chars
  int i = 0;
  while (src[i] && i < (int)sizeof(a->callsign) - 1) { a->callsign[i] = src[i]; i++; }
  while (i > 0 && (a->callsign[i-1] == ' ' || a->callsign[i-1] == '\t')) i--;
  a->callsign[i] = '\0';
}

// -----------------------------------------------------------------------------
//  Body buffer (PSRAM). Reused across fetches so we do not fragment the internal
//  heap with a big String on every poll. The WHOLE HTTP body is read here before
//  parsing, so the JSON parser always sees a complete document. The old
//  "IncompleteInput" came from parsing straight off a TLS stream that ended
//  mid-object.
// -----------------------------------------------------------------------------
static char*  s_body    = nullptr;
static size_t s_bodyCap = 0;
static const size_t ADSB_MAX_BODY = 1024 * 1024;   // 1 MB hard cap
// Reserved when the server sends no Content-Length (chunked). Below the hard
// cap, above anything adsb.fi realistically returns.
static const size_t ADSB_UNKNOWN_BODY = 384 * 1024;

static bool bodyReserve(size_t need) {
  if (need <= s_bodyCap) return true;
  size_t cap = need + 2048;
  // PSRAM only. Hundreds of kB out of the internal heap would either fail
  // anyway or succeed and starve the ~45 kB every TLS handshake needs after it.
  char* nb = (char*)heap_caps_realloc(s_body, cap, MALLOC_CAP_SPIRAM);
  if (!nb) return false;
  s_body = nb; s_bodyCap = cap;
  return true;
}

// Read the whole HTTP body into s_body. Returns the byte count (>= 0), or -1 on
// a hard error: allocation, overflow, a stall, or a transfer that ended short of
// the declared length.
static long readBody(HTTPClient& http) {
  // writeToStream() decodes chunked encoding; the old hand-rolled loop over
  // getStreamPtr() did not, which is what left chunk size headers in the body.
  // Overflow and stalls are failures now, not silent truncation, so the old
  // "complete" out-param had no reachable false branch left and is gone.

  int declared = http.getSize();               // -1 when chunked / unknown
  if (declared > (int)ADSB_MAX_BODY) {
    Serial.printf("ADSB: hlaseno %d B, strop je %u B\n",
                  declared, (unsigned)ADSB_MAX_BODY);
    return -1;
  }
  // The sink writes into a fixed buffer and cannot grow mid-transfer, so a
  // chunked response (no Content-Length) has to be given room up front. The
  // widest range offered is 100 km, where the answer runs to tens of kB; the
  // buffer sits in PSRAM and is reused across polls, so the reserve is free.
  size_t want = (declared > 0) ? (size_t)declared + 1 : ADSB_UNKNOWN_BODY;
  if (!bodyReserve(want)) return -1;

  return Net_ReadBody(http, (uint8_t*)s_body, s_bodyCap, "ADSB", s_poll);
}

// Filter document: only the keys we actually use are kept, so the parsed
// JsonDocument stays small no matter how much adsb.fi sends. alt_baro MUST stay
// - it carries the literal "ground" used to detect aircraft on the ground.
static void fillFields(JsonObject o) {
  if (o.isNull()) return;
  o["hex"]          = true;
  o["flight"]       = true;
  o["lat"]          = true;
  o["lon"]          = true;
  o["track"]        = true;
  o["true_heading"] = true;
  o["alt_baro"]     = true;
  o["gs"]           = true;
  o["baro_rate"]    = true;
  o["t"]            = true;
  o["r"]            = true;   // registration - free, in the same answer
  o["squawk"]       = true;
  o["category"]     = true;   // emitter category - icon fallback
}

static void buildFilter(JsonDocument& filter) {
  // Fill each object through a fresh reference. Copying one filter entry into
  // the other with set() looked tidier but silently produced an empty filter,
  // because adding the second key can invalidate the JsonObject handle taken
  // for the first - and an empty filter drops EVERY key, which deserializeJson
  // still reports as Ok.
  fillFields(filter["ac"].add<JsonObject>());

  // The other name this family of APIs uses for the same array. adsb.fi's v3
  // endpoint sends "ac"; ADSBexchange-derived servers send "aircraft". Taking
  // both means a rename upstream costs nothing instead of blanking the radar.
  fillFields(filter["aircraft"].add<JsonObject>());

  // Not payload - diagnostics. adsb.fi puts its status text in "msg" ("No
  // error" on success). The filter used to drop it, so when the shape was
  // wrong the firmware could only say "no ac array" and nothing about why.
  filter["msg"] = true;
}

bool ADSB_Fetch(double lat, double lon, float radiusKm) {
  // No link -> keep whatever we last drew (do NOT blank the radar).
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("ADSB: no WiFi");
    Status_Set(ST_ADSB, "bez WiFi");
    return false;
  }
  if (!Net_HeapOk("ADSB")) { Status_Set(ST_ADSB, "malo pameti"); return false; }
  // The gate. Returning at once is the point: the caller's screen and the
  // cycling keep running instead of waiting on a server that is not answering.
  if (ADSB_PauseLeftMs() > 0) return false;
  long retryAfter = 0;

  float distNm = radiusKm / KM_PER_NM;
  char url[128];
  snprintf(url, sizeof(url), "%s%.5f/lon/%.5f/dist/%.1f",
           ADSB_API_BASE, lat, lon, distNm);
  Serial.printf("ADSB: %s\n", url);

  // Up to two attempts. A single transient hiccup (dropped TLS, truncated body)
  // no longer skips a whole poll cycle; two failures fall through and the
  // previous snapshot stays on screen.
  const int MAX_ATTEMPTS = 2;
  for (int attempt = 1; attempt <= MAX_ATTEMPTS; attempt++) {
    poll();
    WiFiClientSecure client;
    client.setInsecure();
    client.setHandshakeTimeout(NET_TLS_HANDSHAKE_S);

    HTTPClient http;
    http.setConnectTimeout(8000);   // ms - TCP connect only, NOT the handshake
    http.setTimeout(12000);         // ms - per-read timeout
    http.setReuse(false);           // clean close, do not pool the socket
    if (!http.begin(client, url)) {
      Serial.println("ADSB: begin failed");
      if (attempt < MAX_ATTEMPTS) { delay(200); continue; }
      noteFailure(0);
      return false;
    }
    // The response header carries the current time - that is where the clock
    // comes from, so ask HTTPClient to keep it (see Outside.h).
    static const char* WANTED[] = { "Date", "Retry-After" };
    http.collectHeaders(WANTED, 2);
    // Identify ourselves - adsb.fi's free API asks callers to be polite, and
    // adsb.lol flatly refuses a User-Agent without contact info (see Config.h).
    // One string for both, so there is one place to change.
    // It MUST go through setUserAgent(): addHeader() silently drops User-Agent
    // (along with Connection, Host and Accept-Encoding), so this used to send
    // the default "ESP32HTTPClient" no matter what was passed here.
    http.setUserAgent(HTTP_USER_AGENT);
    http.addHeader("Accept", "application/json");

    int code = http.GET();
    // Seed the clock even from a non-OK answer - the header is there either way.
    if (http.hasHeader("Date")) Outside_NoteHttpDate(http.header("Date").c_str());
    s_lastCode = code > 0 ? code : 0;
    if (code != HTTP_CODE_OK) {
      Serial.printf("ADSB: HTTP %d (attempt %d)\n", code, attempt);
      if (http.hasHeader("Retry-After")) retryAfter = http.header("Retry-After").toInt();
      http.end();
      // Too many requests, or the server itself in trouble: asking again 200 ms
      // later only adds to the pile. A dropped connection (code < 0) or a
      // stray 4xx is still worth the one quick retry.
      const bool overloaded = (code == 429 || code >= 500);
      if (!overloaded && attempt < MAX_ATTEMPTS) { delay(200); continue; }
      noteFailure(retryAfter);
      return false;   // keep last good data
    }

    // Read the WHOLE body first (into PSRAM), then parse - no live-stream parse.
    long len = readBody(http);
    http.end();

    if (len < 0) {
      Serial.println("ADSB: body read failed");
      if (attempt < MAX_ATTEMPTS) { delay(200); continue; }
      noteFailure(0);
      return false;
    }
    if (len < 8) {   // empty / nonsense
      Serial.printf("ADSB: short body %ld (attempt %d)\n", len, attempt);
      if (attempt < MAX_ATTEMPTS) { delay(200); continue; }
      noteFailure(0);
      return false;
    }

    // Cheap shape check before the parser sees it. Catches a proxy error page,
    // a gzipped body, or chunk residue - all of which would otherwise reach
    // ArduinoJson and come back as something unhelpful.
    const char* head = s_body;
    while (*head == ' ' || *head == '\r' || *head == '\n' || *head == '\t') head++;
    if (*head != '{') {
      Serial.printf("ADSB: odpoved nezacina JSON objektem, telo[0..120]: %.120s\n", s_body);
      noteFailure(0);
      Status_Set(ST_ADSB, "neocekavana odpoved");
      return false;
    }

    // Filtered parse of the complete in-memory buffer.
    JsonDocument filter;
    buildFilter(filter);
    JsonDocument doc;
    DeserializationError err =
        deserializeJson(doc, s_body, (size_t)len, DeserializationOption::Filter(filter));
    if (err) {
      Serial.printf("ADSB: JSON %s (attempt %d) - keeping last %d\n",
                    err.c_str(), attempt, s_count);
      if (attempt < MAX_ATTEMPTS) { delay(200); continue; }
      noteFailure(0);
      return false;
    }

    // "ac" is what the v3 endpoint sends; "aircraft" is the same array under
    // the name the upstream format uses. Take whichever is actually there.
    // NOTE: convert straight to JsonArrayConst. Going via a JsonVariantConst
    // local reads back as null when the document is non-const, which would
    // reject every good response.
    JsonArrayConst ac = doc["ac"].as<JsonArrayConst>();
    if (ac.isNull()) ac = doc["aircraft"].as<JsonArrayConst>();

    if (ac.isNull()) {
      // Say WHAT came back, not just that it was wrong. "msg" is the server's
      // own status text; the prefix catches responses that were never the
      // shape we expected in the first place.
      const char* msg = doc["msg"] | "(zadne msg)";
      Serial.printf("ADSB: chybi pole letadel - msg: %s\n", msg);
      Serial.printf("ADSB: telo[0..200]: %.200s\n", s_body);
      noteFailure(0);
      Status_Set(ST_ADSB, "neocekavana odpoved");
      return false;   // valid JSON but wrong shape; a retry would not help
    }

    // Parse into the SCRATCH list; commit to the live list only on success.
    int n = 0;
    for (JsonObjectConst plane : ac) {
      float plat, plon;
      if (!readFloat(plane, "lat", &plat) || !readFloat(plane, "lon", &plon)) continue;

      // Aircraft on the ground are dropped here so they never take a slot from
      // an airborne target (near an airport they could otherwise fill ADSB_MAX).
      JsonVariantConst ab = plane["alt_baro"];
      bool ground = ab.is<const char*>() && strcmp(ab.as<const char*>(), "ground") == 0;
      if (ground) continue;

      if (n >= ADSB_MAX) break;
      s_tmp[n].lat = plat;
      s_tmp[n].lon = plon;
      s_tmp[n].onGround = false;
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
      float f = 0;
      s_tmp[n].altFt    = readFloat(plane, "alt_baro", &f) ? f : 0;
      s_tmp[n].gsKt     = readFloat(plane, "gs", &f) ? f : 0;
      s_tmp[n].baroRate = readFloat(plane, "baro_rate", &f) ? f : 0;
      // Aircraft type. ONLY "t" - that is the airframe type code ("A320").
      // "type" is the MESSAGE source ("adsb_icao", "mlat", "tisb_icao"), and
      // using it as a fallback is why aircraft missing from the database showed
      // "adsb_icao" in their detail panel. Left empty when adsb.fi does not
      // know the airframe; the detail panel then simply omits the row.
      const char* ty = plane["t"] | "";
      strncpy(s_tmp[n].type, ty, sizeof(s_tmp[n].type) - 1);
      s_tmp[n].type[sizeof(s_tmp[n].type) - 1] = '\0';
      // Registration, same deal - "r" is carried in this very answer, so no
      // second API is needed to put "OK-TVU" next to the type.
      const char* rg = plane["r"] | "";
      strncpy(s_tmp[n].reg, rg, sizeof(s_tmp[n].reg) - 1);
      s_tmp[n].reg[sizeof(s_tmp[n].reg) - 1] = '\0';
      // Squawk. adsb.fi sends it as a string; some feeds send a number, in
      // which case a leading zero would already be lost - pad it back so the
      // comparison against "7700" still works.
      {
        JsonVariantConst sq = plane["squawk"];
        s_tmp[n].squawk[0] = '\0';
        if (sq.is<const char*>()) {
          const char* s = sq.as<const char*>();
          if (s) { strncpy(s_tmp[n].squawk, s, sizeof(s_tmp[n].squawk) - 1);
                   s_tmp[n].squawk[sizeof(s_tmp[n].squawk) - 1] = '\0'; }
        } else if (sq.is<int>()) {
          snprintf(s_tmp[n].squawk, sizeof(s_tmp[n].squawk), "%04d", sq.as<int>());
        }
      }
      {
        const char* cat = plane["category"] | "";
        strncpy(s_tmp[n].cat, cat, sizeof(s_tmp[n].cat) - 1);
        s_tmp[n].cat[sizeof(s_tmp[n].cat) - 1] = '\0';
      }
      s_tmp[n].icon = PlaneIcon_Classify(s_tmp[n].type, s_tmp[n].cat);
      copyHex(&s_tmp[n], plane);
      copyCallsign(&s_tmp[n], plane);
      n++;
    }

    // Commit the scratch snapshot to the live list in one go.
    for (int i = 0; i < n; i++) s_list[i] = s_tmp[i];
    s_count = n;
    noteSuccess();
    Serial.printf("ADSB: %d aircraft (%ld bytes)\n", n, len);
    Status_Set(ST_ADSB, "OK, %d", n);
    return true;
  }
  noteFailure(retryAfter);
  return false;
}
