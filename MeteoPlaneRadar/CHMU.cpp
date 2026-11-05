// MeteoPlaneRadar - vyvoj / development: chiptron.cz
// =============================================================================
//  MeteoPlaneRadar - meteoradar CHMU: stahovani do PSRAM (1 snimek + animace).
// =============================================================================
#include "CHMU.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "esp_heap_caps.h"
#include "Config.h"
#include "TimeUtil.h"
#include "Outside.h"
#include "NetSink.h"
#include <string.h>      // strstr / memcmp

static const char* NAME_PREFIX = "pacz2gmaps3.z_max3d.";

static void (*s_poll)() = nullptr;
void CHMU_SetPollFn(void (*fn)()) { s_poll = fn; }

static bool s_masked = false;
void CHMU_SetMasked(bool masked) { s_masked = masked; }
bool CHMU_Masked() { return s_masked; }
static const char* baseUrl(bool masked) { return masked ? CHMU_INDEX_URL_MASKED : CHMU_INDEX_URL; }

// -----------------------------------------------------------------------------
//  Spolecne pomucky
// -----------------------------------------------------------------------------
static String extractTimestamp(const String& name) {
  int start = name.indexOf(NAME_PREFIX);
  if (start < 0) return "";
  int ds = start + strlen(NAME_PREFIX);
  if ((int)name.length() < ds + 13) return "";
  if (name[ds + 8] != '.') return "";
  String date = name.substring(ds, ds + 8);
  String hhmm = name.substring(ds + 9, ds + 13);
  for (unsigned i = 0; i < date.length(); i++) if (!isDigit(date[i])) return "";
  for (unsigned i = 0; i < hhmm.length(); i++) if (!isDigit(hhmm[i])) return "";
  return date + hhmm;   // YYYYMMDDHHMM
}

// The name carries a UTC timestamp; the label wants local time. The conversion
// deliberately does NOT look at the current clock - it turns the frame's own
// date into an epoch and lets the TZ rules decide CET or CEST for THAT date.
// Reading "now" instead used to put the labels an hour out whenever NTP had not
// answered yet, because the unset clock sits in January (CET) while the frame
// is from summer (CEST).
static String timeTextFromName(const String& name) {
  String ts = extractTimestamp(name);
  if (ts.length() < 12) return "";
  int Y  = ts.substring(0, 4).toInt();
  int Mo = ts.substring(4, 6).toInt();
  int D  = ts.substring(6, 8).toInt();
  int hh = ts.substring(8, 10).toInt();
  int mm = ts.substring(10, 12).toInt();
  if (Y < 2000 || Mo < 1 || Mo > 12 || D < 1 || D > 31) return "";

  time_t utc = TimeUtil_UtcToEpoch(Y, Mo, D, hh, mm, 0);
  struct tm lt;
  localtime_r(&utc, &lt);
  char out[6];
  snprintf(out, sizeof(out), "%02d:%02d", lt.tm_hour, lt.tm_min);
  return String(out);
}

// Stahne dany PNG do zadaneho bufferu. Vraci true a naplni *outSize.
static bool downloadNameTo(bool masked, const String& name, uint8_t* buf, size_t cap, size_t* outSize) {
  *outSize = 0;
  if (!buf) return false;
  if (!Net_HeapOk("CHMU")) return false;
  String url = String(baseUrl(masked)) + name;
  WiFiClientSecure client; client.setInsecure();
  client.setHandshakeTimeout(NET_TLS_HANDSHAKE_S);
  HTTPClient http;
  http.setConnectTimeout(6000);   // TCP connect only, NOT the TLS handshake
  http.setTimeout(15000);
  if (!http.begin(client, url)) return false;
  if (http.GET() != HTTP_CODE_OK) { http.end(); return false; }
  int total = http.getSize();
  if (total > (int)cap) { http.end(); return false; }
  long got = Net_ReadBody(http, buf, cap, "CHMU", s_poll);
  http.end();
  if (got < 0) return false;

  // A PNG that is not a PNG means we were handed an error page or something
  // re-encoded in transit. Checking the signature here stops the decoder from
  // being fed rubbish and drawing a corrupt frame over a good radar image.
  if (got < 8 || memcmp(buf, "\x89PNG\r\n\x1a\n", 8) != 0) {
    Serial.printf("CHMU: %s neni PNG (%ld B)\n", name.c_str(), got);
    return false;
  }
  *outSize = (size_t)got;
  return true;
}

// -----------------------------------------------------------------------------
//  Animace - nejnovejsich N ramcu
// -----------------------------------------------------------------------------
static uint8_t* s_animBuf[CHMU_ANIM_MAX] = {0};
static size_t   s_animSize[CHMU_ANIM_MAX] = {0};
static String   s_animName[CHMU_ANIM_MAX];
static int      s_animCount = 0;

int      CHMU_AnimCount() { return s_animCount; }
uint8_t* CHMU_AnimData(int i) { return (i >= 0 && i < s_animCount) ? s_animBuf[i] : nullptr; }
size_t   CHMU_AnimSize(int i) { return (i >= 0 && i < s_animCount) ? s_animSize[i] : 0; }
String   CHMU_AnimTimeText(int i) { return (i >= 0 && i < s_animCount) ? timeTextFromName(s_animName[i]) : String(""); }
String   CHMU_TimeTextFromName(const String& name) { return timeTextFromName(name); }
String   CHMU_AnimName(int i) { return (i >= 0 && i < s_animCount) ? s_animName[i] : String(""); }

// Bezici "top-N" nejnovejsich nazvu (vzestupne dle casu).
static String s_topName[CHMU_ANIM_MAX];
static String s_topTs[CHMU_ANIM_MAX];
static int    s_topCount = 0;

static void topInsert(const String& name, const String& ts) {
  for (int i = 0; i < s_topCount; i++) if (s_topTs[i] == ts) return;   // duplicita
  if (s_topCount < CHMU_ANIM_MAX) {
    int p = s_topCount;
    while (p > 0 && s_topTs[p - 1] > ts) { s_topTs[p] = s_topTs[p - 1]; s_topName[p] = s_topName[p - 1]; p--; }
    s_topTs[p] = ts; s_topName[p] = name; s_topCount++;
  } else if (ts > s_topTs[0]) {   // nahradime nejstarsi
    int p = 0;
    while (p < CHMU_ANIM_MAX - 1 && s_topTs[p + 1] < ts) { s_topTs[p] = s_topTs[p + 1]; s_topName[p] = s_topName[p + 1]; p++; }
    s_topTs[p] = ts; s_topName[p] = name;
  }
}

// -----------------------------------------------------------------------------
//  Index directory listing
//
//  Scanned as it arrives, never stored. Reading it into a buffer first put a
//  ceiling on it, and the radar stopped updating the day the listing grew past
//  that. NetScanSink keeps the chunked decoding of writeToStream() and hands
//  the scan a sliding window, so nothing here grows with the listing. Windows
//  overlap, so the scan below has to tolerate seeing a name twice.
// -----------------------------------------------------------------------------

// topInsert() drops a timestamp it already holds, so a name seen twice at a
// window boundary does not take two slots.
static void scanTop(const char* text, void* user) {
  (void)user;
  const char* pos = text;
  while (true) {
    const char* idx = strstr(pos, NAME_PREFIX); if (!idx) break;
    const char* end = strstr(idx, ".png");      if (!end) break;
    String name; name.concat(idx, (size_t)(end + 4 - idx));
    String ts = extractTimestamp(name);
    if (ts.length()) topInsert(name, ts);
    pos = end + 4;
  }
}

static bool ensureAnimBuffer(int i) {
  if (s_animBuf[i]) return true;
  s_animBuf[i] = (uint8_t*)heap_caps_malloc(CHMU_MAX_PNG, MALLOC_CAP_SPIRAM);   // PSRAM only, viz vyse
  return s_animBuf[i] != nullptr;
}

// Scan the directory listing into the top-N table. Shared by the animation and
// the storm watch's single-frame fetch.
static bool scanIndex(bool masked) {
  if (!Net_HeapOk("CHMU")) return false;
  s_topCount = 0;
  WiFiClientSecure client; client.setInsecure();
  client.setHandshakeTimeout(NET_TLS_HANDSHAKE_S);
  HTTPClient http;
  http.setConnectTimeout(6000);
  http.setTimeout(15000);
  static const char* WANTED[] = { "Date" };
  http.collectHeaders(WANTED, 1);
  if (!http.begin(client, baseUrl(masked))) return false;
  int code = http.GET();
  if (http.hasHeader("Date")) Outside_NoteHttpDate(http.header("Date").c_str());
  if (code != HTTP_CODE_OK) { http.end(); return false; }
  long ilen = Net_ScanBody(http, scanTop, nullptr, "CHMU", s_poll);
  http.end();
  return ilen > 0 && s_topCount > 0;
}

bool CHMU_FetchLatest(bool masked, uint8_t* buf, size_t cap, size_t* outSize, String* name) {
  if (outSize) *outSize = 0;
  if (WiFi.status() != WL_CONNECTED || !buf) return false;
  if (!scanIndex(masked)) return false;
  const String& newest = s_topName[s_topCount - 1];
  size_t sz = 0;
  if (!downloadNameTo(masked, newest, buf, cap, &sz)) return false;
  if (outSize) *outSize = sz;
  if (name) *name = newest;
  return true;
}

int CHMU_FetchAnim(int wantN) {
  if (WiFi.status() != WL_CONNECTED) return s_animCount;
  if (wantN > CHMU_ANIM_MAX) wantN = CHMU_ANIM_MAX;
  if (wantN < 1) wantN = 1;

  // 1) projdi index a najdi N nejnovejsich nazvu
  if (!scanIndex(s_masked)) return s_animCount;
  Serial.printf("CHMU%s: nalezeno %d nazvu\n", s_masked ? " (masked)" : "", s_topCount);

  // 2) stahni N nejnovejsich (top pole je vzestupne, bereme konec)
  int n = s_topCount < wantN ? s_topCount : wantN;
  int startIdx = s_topCount - n;
  int got = 0;
  for (int i = 0; i < n; i++) {
    if (!ensureAnimBuffer(i)) break;
    size_t sz = 0;
    if (downloadNameTo(s_masked, s_topName[startIdx + i], s_animBuf[i], CHMU_MAX_PNG, &sz)) {
      s_animSize[i] = sz; s_animName[i] = s_topName[startIdx + i]; got++;
    } else break;
  }
  s_animCount = got;
  Serial.printf("Meteoradar: %d ramcu\n", got);
  return got;
}
