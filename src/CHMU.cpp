// MeteoPlaneRadar - vyvoj / development: chiptron.cz
// =============================================================================
//  MeteoPlaneRadar - meteoradar CHMU: stahovani do PSRAM (1 snimek + animace).
// =============================================================================
#include "CHMU.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "esp_heap_caps.h"

static const char* NAME_PREFIX = "pacz2gmaps3.z_max3d.";

static void (*s_poll)() = nullptr;
static void poll() { if (s_poll) s_poll(); }
void CHMU_SetPollFn(void (*fn)()) { s_poll = fn; }

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

static String timeTextFromName(const String& name) {
  String ts = extractTimestamp(name);
  if (ts.length() < 12) return "";
  int hh = ts.substring(8, 10).toInt();
  int mm = ts.substring(10, 12).toInt();
  time_t now = time(nullptr);
  struct tm lt; localtime_r(&now, &lt);
  struct tm gt; gmtime_r(&now, &gt);
  int off = lt.tm_hour - gt.tm_hour;
  if (off < -12) off += 24; if (off > 12) off -= 24;
  hh = (hh + off + 24) % 24;
  char out[6];
  snprintf(out, sizeof(out), "%02d:%02d", hh, mm);
  return String(out);
}

// Stahne dany PNG do zadaneho bufferu. Vraci true a naplni *outSize.
static bool downloadNameTo(const String& name, uint8_t* buf, size_t cap, size_t* outSize) {
  *outSize = 0;
  if (!buf) return false;
  String url = String(CHMU_INDEX_URL) + name;
  WiFiClientSecure client; client.setInsecure();
  HTTPClient http; http.setTimeout(15000);
  if (!http.begin(client, url)) return false;
  if (http.GET() != HTTP_CODE_OK) { http.end(); return false; }
  int total = http.getSize();
  if (total > (int)cap) { http.end(); return false; }
  WiFiClient* stream = http.getStreamPtr();
  size_t written = 0; unsigned long last = millis();
  while (http.connected() && (total < 0 || written < (size_t)total)) {
    poll();
    size_t avail = stream->available();
    if (avail) {
      size_t space = cap - written;
      size_t toRead = avail < space ? avail : space;
      int n = stream->readBytes(buf + written, toRead);
      if (n <= 0) break;
      written += n; last = millis();
      if (written >= cap) break;
    } else { if (millis() - last > 10000) break; delay(1); }
  }
  http.end();
  *outSize = written;
  return written > 100;
}

// -----------------------------------------------------------------------------
//  Jeden (nejnovejsi) snimek - puvodni API
// -----------------------------------------------------------------------------
static String   s_lastName;
static bool     s_hasSnapshot = false;
static uint8_t* s_pngBuf = nullptr;
static size_t   s_pngSize = 0;

bool     CHMU_HasSnapshot() { return s_hasSnapshot; }
uint8_t* CHMU_Data() { return s_pngBuf; }
size_t   CHMU_DataSize() { return s_pngSize; }

static void scanChunkLatest(const String& text, String& newestTs, String& latestName) {
  int pos = 0;
  while (true) {
    int idx = text.indexOf(NAME_PREFIX, pos); if (idx < 0) break;
    int end = text.indexOf(".png", idx); if (end < 0) break;
    String name = text.substring(idx, end + 4);
    String ts = extractTimestamp(name);
    if (ts.length() && ts > newestTs) { newestTs = ts; latestName = name; }
    pos = end + 4;
  }
}

bool CHMU_FetchLatest() {
  if (WiFi.status() != WL_CONNECTED) return s_hasSnapshot;
  WiFiClientSecure client; client.setInsecure();
  HTTPClient http; http.setTimeout(15000);
  if (!http.begin(client, CHMU_INDEX_URL)) return s_hasSnapshot;
  if (http.GET() != HTTP_CODE_OK) { http.end(); return s_hasSnapshot; }
  WiFiClient* stream = http.getStreamPtr();
  String window, newestTs, latest; uint8_t buf[512]; unsigned long last = millis();
  while (http.connected()) {
    poll();
    size_t avail = stream->available();
    if (avail) { int n = stream->readBytes(buf, avail < sizeof(buf) ? avail : sizeof(buf)); if (n <= 0) break;
      window.concat((const char*)buf, n); scanChunkLatest(window, newestTs, latest);
      if (window.length() > 400) window = window.substring(window.length() - 250); last = millis(); }
    else { if (millis() - last > 10000) break; delay(1); }
  }
  scanChunkLatest(window, newestTs, latest);
  http.end();
  if (latest.isEmpty()) return s_hasSnapshot;
  if (latest == s_lastName && s_hasSnapshot) return true;
  if (!s_pngBuf) {
    s_pngBuf = (uint8_t*)heap_caps_malloc(CHMU_MAX_PNG, MALLOC_CAP_SPIRAM);
    if (!s_pngBuf) s_pngBuf = (uint8_t*)malloc(CHMU_MAX_PNG);
  }
  if (downloadNameTo(latest, s_pngBuf, CHMU_MAX_PNG, &s_pngSize)) { s_lastName = latest; s_hasSnapshot = true; return true; }
  return s_hasSnapshot;
}

String CHMU_SnapshotTimeText() { return timeTextFromName(s_lastName); }

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

static void scanChunkTop(const String& text) {
  int pos = 0;
  while (true) {
    int idx = text.indexOf(NAME_PREFIX, pos); if (idx < 0) break;
    int end = text.indexOf(".png", idx); if (end < 0) break;
    String name = text.substring(idx, end + 4);
    String ts = extractTimestamp(name);
    if (ts.length()) topInsert(name, ts);
    pos = end + 4;
  }
}

static bool ensureAnimBuffer(int i) {
  if (s_animBuf[i]) return true;
  s_animBuf[i] = (uint8_t*)heap_caps_malloc(CHMU_MAX_PNG, MALLOC_CAP_SPIRAM);
  if (!s_animBuf[i]) s_animBuf[i] = (uint8_t*)malloc(CHMU_MAX_PNG);
  return s_animBuf[i] != nullptr;
}

int CHMU_FetchAnim(int wantN) {
  if (WiFi.status() != WL_CONNECTED) return s_animCount;
  if (wantN > CHMU_ANIM_MAX) wantN = CHMU_ANIM_MAX;
  if (wantN < 1) wantN = 1;

  // 1) projdi index a najdi N nejnovejsich nazvu
  s_topCount = 0;
  WiFiClientSecure client; client.setInsecure();
  HTTPClient http; http.setTimeout(15000);
  if (!http.begin(client, CHMU_INDEX_URL)) return s_animCount;
  if (http.GET() != HTTP_CODE_OK) { http.end(); return s_animCount; }
  WiFiClient* stream = http.getStreamPtr();
  String window; uint8_t buf[512]; unsigned long last = millis();
  while (http.connected()) {
    poll();
    size_t avail = stream->available();
    if (avail) { int n = stream->readBytes(buf, avail < sizeof(buf) ? avail : sizeof(buf)); if (n <= 0) break;
      window.concat((const char*)buf, n); scanChunkTop(window);
      if (window.length() > 400) window = window.substring(window.length() - 250); last = millis(); }
    else { if (millis() - last > 10000) break; delay(1); }
  }
  scanChunkTop(window);
  http.end();
  if (s_topCount == 0) return s_animCount;

  // 2) stahni N nejnovejsich (top pole je vzestupne, bereme konec)
  int n = s_topCount < wantN ? s_topCount : wantN;
  int startIdx = s_topCount - n;
  int got = 0;
  for (int i = 0; i < n; i++) {
    if (!ensureAnimBuffer(i)) break;
    size_t sz = 0;
    if (downloadNameTo(s_topName[startIdx + i], s_animBuf[i], CHMU_MAX_PNG, &sz)) {
      s_animSize[i] = sz; s_animName[i] = s_topName[startIdx + i]; got++;
    } else break;
  }
  s_animCount = got;
  Serial.printf("Meteoradar: %d ramcu\n", got);
  return got;
}
