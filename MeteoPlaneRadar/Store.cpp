// =============================================================================
//  MeteoPlaneRadar
//  Card store - download, verify, install. See Store.h.
// =============================================================================
#include "Store.h"
#include "StoreKey.h"
#include "StoreVerify.h"
#include "Cards.h"
#include "Config.h"
#include "Net.h"
#include "Watchdog.h"
#include <LittleFS.h>

#define SERIAL_FILE "/store-serial"

// The last index that passed the signature check, kept as downloaded. Listing
// and installing usually come seconds apart, and the second one should not
// cost two more requests.
static String        s_idx;
static unsigned long s_idxAt = 0;
static bool          s_idxOk = false;

// --- Highest serial accepted so far -----------------------------------------
static long readSerial() {
  if (!Cards_FsReady()) return 0;
  File f = LittleFS.open(SERIAL_FILE, "r");
  if (!f) return 0;
  char b[16];
  size_t n = 0;
  while (n + 1 < sizeof(b)) {
    const int c = f.read();
    if (c < '0' || c > '9') break;
    b[n++] = (char)c;
  }
  b[n] = '\0';
  f.close();
  return atol(b);
}

static void writeSerial(long v) {
  if (!Cards_FsReady()) return;
  File f = LittleFS.open(SERIAL_FILE, "w");
  if (!f) return;
  f.printf("%ld", v);
  f.close();
}

// --- The index --------------------------------------------------------------
static bool loadIndex(JsonDocument& doc, char* err, size_t errCap) {
  const unsigned long now = millis();
  if (!s_idxOk || now - s_idxAt >= STORE_CACHE_MS) {
    s_idxOk = false;
    s_idx = String();

    // Up to three downloads follow one another here and in Store_Install(),
    // all inside one web request. Each is bounded on its own; feeding the
    // watchdog between them keeps their sum from looking like a hang.
    String idx, sig;
    Watchdog_Feed();
    if (!Net_GetString(STORE_INDEX_URL, idx, "STORE")) {
      snprintf(err, errCap, "cannot download the index");
      return false;
    }
    if (idx.length() > STORE_INDEX_MAX) {
      snprintf(err, errCap, "the index is larger than %d bytes", (int)STORE_INDEX_MAX);
      return false;
    }
    Watchdog_Feed();
    if (!Net_GetString(STORE_INDEX_URL ".sig", sig, "STORE")) {
      snprintf(err, errCap, "cannot download the index signature");
      return false;
    }
    if (!StoreVerify_Signature((const uint8_t*)idx.c_str(), idx.length(),
                               sig.c_str(), sig.length(), STORE_PUBKEY_PEM)) {
      Serial.println("STORE: podpis indexu nesouhlasi");
      snprintf(err, errCap, "the index signature is not valid");
      return false;
    }
    s_idx = idx;
    s_idxAt = now;
    s_idxOk = true;
  }

  if (deserializeJson(doc, s_idx)) {
    s_idxOk = false;
    snprintf(err, errCap, "the index is not valid JSON");
    return false;
  }
  if ((doc["v"] | 0) != 1) {
    s_idxOk = false;
    snprintf(err, errCap, "unsupported index version");
    return false;
  }
  const long serial = doc["serial"] | 0L;
  const long seen = readSerial();
  if (serial < seen) {
    s_idxOk = false;
    Serial.printf("STORE: index %ld je starsi nez %ld\n", serial, seen);
    snprintf(err, errCap, "the index is older than one accepted before");
    return false;
  }
  if (serial > seen) writeSerial(serial);
  return true;
}

// An entry the device is willing to show and install.
static bool entryOk(JsonObjectConst e) {
  const char* id = e["id"] | "";
  const char* sha = e["sha256"] | "";
  const long size = e["size"] | 0L;
  return StoreVerify_FileName(e["file"] | "", id) && strlen(sha) == 64 &&
         size > 0 && size <= CARD_FILE_MAX;
}

static bool installed(const char* id) {
  for (int i = 0; i < Cards_Count(); i++)
    if (strcmp(Cards_Get(i)->id, id) == 0) return true;
  return false;
}

bool Store_List(JsonDocument& out, char* err, size_t errCap) {
  JsonDocument idx;
  if (!loadIndex(idx, err, errCap)) return false;

  out.clear();
  out["serial"] = idx["serial"] | 0L;
  JsonArray a = out["cards"].to<JsonArray>();
  for (JsonObjectConst e : idx["cards"].as<JsonArrayConst>()) {
    if (!entryOk(e)) continue;              // malformed entries are simply not offered
    JsonObject o = a.add<JsonObject>();
    o["id"] = e["id"];
    o["title"] = e["title"] | "";
    o["host"] = e["host"] | "";
    o["size"] = e["size"];
    o["desc"]["cs"] = e["desc"]["cs"] | "";
    o["desc"]["en"] = e["desc"]["en"] | "";
    o["installed"] = installed(e["id"] | "");
  }
  return true;
}

bool Store_Install(const char* id, char* err, size_t errCap) {
  if (!StoreVerify_Id(id)) { snprintf(err, errCap, "no such card in the store"); return false; }

  JsonDocument idx;
  if (!loadIndex(idx, err, errCap)) return false;

  JsonObjectConst entry;
  for (JsonObjectConst e : idx["cards"].as<JsonArrayConst>()) {
    if (strcmp(e["id"] | "", id) == 0) { entry = e; break; }
  }
  if (entry.isNull() || !entryOk(entry)) {
    snprintf(err, errCap, "no such card in the store");
    return false;
  }

  // The card sits next to the index: same address, the file name instead of
  // "index.json". entryOk() made sure that name is "<id>.json" and nothing else.
  char url[sizeof(STORE_INDEX_URL) + 40];
  strcpy(url, STORE_INDEX_URL);
  char* slash = strrchr(url, '/');
  if (!slash) { snprintf(err, errCap, "bad store address"); return false; }
  snprintf(slash + 1, sizeof(url) - (size_t)(slash + 1 - url), "%s.json", id);

  String body;
  Watchdog_Feed();
  if (!Net_GetString(url, body, "STORE")) {
    snprintf(err, errCap, "cannot download the card");
    return false;
  }
  if ((long)body.length() != (entry["size"] | 0L) ||
      !StoreVerify_Sha256Hex((const uint8_t*)body.c_str(), body.length(), entry["sha256"] | "")) {
    Serial.printf("STORE: karta %s nesouhlasi s indexem\n", id);
    snprintf(err, errCap, "the card does not match the index");
    return false;
  }

  JsonDocument card;
  if (deserializeJson(card, body)) { snprintf(err, errCap, "the card is not valid JSON"); return false; }
  if (strcmp(card["id"] | "", id) != 0) {
    snprintf(err, errCap, "the card has a different id than the index says");
    return false;
  }
  return Cards_Install(card.as<JsonObjectConst>(), err, errCap);
}
