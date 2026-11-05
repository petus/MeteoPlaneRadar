// =============================================================================
//  MeteoPlaneRadar
//  Shared HTTPS fetch helper. See Net.h.
//
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
// =============================================================================
#include "Net.h"
#include "Config.h"
#include "Outside.h"
#include "NetSink.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <esp_heap_caps.h>
#include <string.h>

// Kept open across a burst of tile requests - see Net.h.
static WiFiClientSecure* s_sess = nullptr;

static void (*s_poll)() = nullptr;
void Net_SetPollFn(void (*fn)()) { s_poll = fn; }
static inline void poll() { if (s_poll) s_poll(); }

// Collect the Date header on every request. The clock is seeded from whatever
// we happen to be fetching anyway - see Outside.h for why there is no NTP.
static const char* DATE_HDR[] = { "Date" };

void Net_SessionBegin() {
  if (s_sess) return;
  s_sess = new WiFiClientSecure();
  if (s_sess) { s_sess->setInsecure();
                s_sess->setHandshakeTimeout(NET_TLS_HANDSHAKE_S); }
}

void Net_SessionEnd() {
  if (!s_sess) return;
  s_sess->stop();
  delete s_sess;
  s_sess = nullptr;
}

// Shared scratch for text bodies (PSRAM, reused across calls). It grows to what
// the server declares and never past NET_MAX_TEXT - http.getString() used to
// take the body with no ceiling at all, onto the heap TLS draws from.
static char*  s_txt    = nullptr;
static size_t s_txtCap = 0;

static bool txtReserve(size_t need) {
  if (need <= s_txtCap) return true;
  char* nb = (char*)heap_caps_realloc(s_txt, need, MALLOC_CAP_SPIRAM);
  if (!nb) { Serial.println("NET: telo se nevejde do PSRAM"); return false; }
  s_txt = nb; s_txtCap = need;
  return true;
}

bool Net_GetString(const char* url, String& out, const char* tag) {
  out = "";
  if (WiFi.status() != WL_CONNECTED) return false;

  // Plain http:// endpoints exist too - ip-api.com serves TLS to paying users
  // only - and they belong on the same path as everything else instead of
  // growing a second copy of this function elsewhere. The heap guard is about
  // the handshake, so it only applies when there is one.
  const bool tls = (strncmp(url, "http://", 7) != 0);
  if (tls && !Net_HeapOk(tag)) return false;

  WiFiClient       plain;
  WiFiClientSecure secure;
  if (tls) { secure.setInsecure(); secure.setHandshakeTimeout(NET_TLS_HANDSHAKE_S); }
  WiFiClient& client = tls ? static_cast<WiFiClient&>(secure)
                           : static_cast<WiFiClient&>(plain);

  HTTPClient http;
  http.setConnectTimeout(6000);
  http.setTimeout(10000);
  http.setReuse(false);
  if (!http.begin(client, url)) { Serial.printf("%s: begin() selhalo\n", tag); return false; }
  http.collectHeaders(DATE_HDR, 1);

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("%s: HTTP %d\n", tag, code);
    // Say what was asked and what the server said about it. A bare "HTTP 404"
    // cannot tell a wrong parameter from a source that simply has no data yet.
    if (code > 0) {
      Serial.printf("%s: URL %s\n", tag, url);
      String err = http.getString();
      if (err.length() > 240) err = err.substring(0, 240);
      Serial.printf("%s: odpoved: %s\n", tag, err.c_str());
    }
    http.end();
    return false;
  }
  if (http.hasHeader("Date")) Outside_NoteHttpDate(http.header("Date").c_str());

  int declared = http.getSize();               // -1 when chunked / unknown
  if (declared > (int)NET_MAX_TEXT) {
    Serial.printf("%s: odpoved %d B, strop je %u B\n",
                  tag, declared, (unsigned)NET_MAX_TEXT);
    http.end();
    return false;
  }
  if (!txtReserve((declared > 0) ? (size_t)declared + 1 : NET_MAX_TEXT)) {
    http.end();
    return false;
  }

  long len = Net_ReadBody(http, (uint8_t*)s_txt, s_txtCap, tag, s_poll);
  http.end();
  poll();

  if (len <= 0) { Serial.printf("%s: prazdna odpoved\n", tag); return false; }
  out = s_txt;
  return true;
}

bool Net_GetBinary(const char* url, uint8_t* buf, size_t cap, size_t* outLen,
                   const char* tag) {
  if (outLen) *outLen = 0;
  if (!buf || cap == 0) return false;
  if (WiFi.status() != WL_CONNECTED) return false;

  // Inside a session the connection is already up, so the big handshake
  // allocation is not about to happen and the heap guard would only get in the
  // way. Outside one it still applies.
  const bool sess = (s_sess != nullptr);
  if (!sess && !Net_HeapOk(tag)) return false;

  WiFiClientSecure  own;
  WiFiClientSecure& client = sess ? *s_sess : own;
  if (!sess) { own.setInsecure(); own.setHandshakeTimeout(NET_TLS_HANDSHAKE_S); }

  HTTPClient http;
  http.setConnectTimeout(6000);
  http.setTimeout(10000);
  http.setReuse(sess);            // keep the socket open for the next tile
  if (!http.begin(client, url)) return false;
  http.collectHeaders(DATE_HDR, 1);

  int code = http.GET();
  if (code != HTTP_CODE_OK) { Serial.printf("%s: HTTP %d\n", tag, code); http.end(); return false; }
  if (http.hasHeader("Date")) Outside_NoteHttpDate(http.header("Date").c_str());

  int declared = http.getSize();
  if (declared > 0 && (size_t)declared > cap) {
    Serial.printf("%s: odpoved %d B se nevejde do %u B\n", tag, declared, (unsigned)cap);
    http.end();
    return false;
  }

  long got = Net_ReadBody(http, buf, cap, tag, s_poll);
  http.end();
  if (got <= 0) return false;
  if (outLen) *outLen = got;
  return true;
}

bool Net_TouchDate(const char* url) {
  if (WiFi.status() != WL_CONNECTED) return false;
  if (!Net_HeapOk("HODINY")) return false;

  WiFiClientSecure client;
  client.setInsecure();
  client.setHandshakeTimeout(NET_TLS_HANDSHAKE_S);
  HTTPClient http;
  http.setConnectTimeout(5000);
  http.setTimeout(6000);
  http.setReuse(false);
  if (!http.begin(client, url)) return false;
  http.collectHeaders(DATE_HDR, 1);
  int code = http.sendRequest("HEAD");
  bool ok = false;
  if (code > 0 && http.hasHeader("Date")) {
    Outside_NoteHttpDate(http.header("Date").c_str());
    ok = true;
  }
  http.end();
  return ok;
}
