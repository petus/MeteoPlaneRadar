// =============================================================================
//  MeteoPlaneRadar - host test of the card store.
//
//  Runs the firmware's own Store.cpp, StoreVerify.h and Cards.cpp on a PC with
//  the stand-ins in shim/ and a "network" that serves files from a fixture
//  directory (prepared by run_store_test.sh). Every case below is something a
//  device must get right without anyone watching: accept the genuine index,
//  and refuse a changed index, a changed card, a foreign signature, an older
//  index, and an entry that points anywhere but at "<id>.json".
//
//    tools/sim/run_store_test.sh
// =============================================================================
#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <WiFi.h>
#include "Cards.h"
#include "Store.h"
#include "Settings.h"
#include "Status.h"
#include "Net.h"
#include "Config.h"
#include <fstream>
#include <map>
#include <sstream>

SerialShim Serial;
static unsigned long s_ms = 5000;
unsigned long millis() { return s_ms; }

bool Settings_ScreenEnabled(uint8_t) { return true; }
void Status_Set(StatusSlot, const char*, ...) {}
void Watchdog_Feed() {}

// The "network": address -> body. An address that is not in the map fails,
// the way a 404 or a dead connection does.
static std::map<std::string, std::string> s_web;
static int s_requests = 0;
bool Net_GetString(const char* url, String& out, const char* tag) {
  (void)tag;
  s_requests++;
  auto it = s_web.find(url);
  if (it == s_web.end()) return false;
  out.assign(it->second);
  return true;
}

static std::string slurp(const std::string& p) {
  std::ifstream f(p, std::ios::binary);
  std::stringstream s; s << f.rdbuf();
  return s.str();
}
static bool exists(const std::string& p) { std::ifstream f(p); return f.good(); }

static int fails = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

static const std::string IDX = STORE_INDEX_URL;
static const std::string BASE = IDX.substr(0, IDX.rfind('/') + 1);
static std::string fx;      // fixture directory

static void serve(const char* index, const char* sig) {
  s_web.clear();
  s_web[IDX] = slurp(fx + "/" + index);
  if (sig) s_web[IDX + ".sig"] = slurp(fx + "/" + sig);
  s_web[BASE + "kurzy-cnb.json"] = slurp(fx + "/kurzy-cnb.json");
  s_web[BASE + "svatek.json"] = slurp(fx + "/svatek.json");
  s_ms += STORE_CACHE_MS + 1000;          // never answer from the cached index
}
static bool list(JsonDocument& d, std::string* why = nullptr) {
  char err[80] = "";
  const bool ok = Store_List(d, err, sizeof(err));
  if (why) *why = err;
  return ok;
}
static bool install(const char* id, std::string* why = nullptr) {
  char err[80] = "";
  const bool ok = Store_Install(id, err, sizeof(err));
  if (why) *why = err;
  return ok;
}

int main(int argc, char** argv) {
  if (argc < 2) return 2;
  fx = argv[1];
  Cards_Begin();
  JsonDocument d;
  std::string why;

  // 1. The genuine index, signed with the key the firmware carries.
  serve("index.json", "index.json.sig");
  CHECK(list(d, &why));
  printf("genuine index: %s, serial %ld, %zu card(s)\n", why.empty() ? "accepted" : why.c_str(),
         (long)(d["serial"] | 0L), d["cards"].size());
  CHECK(d["cards"].size() == 2);
  CHECK(std::string(d["cards"][0]["id"] | "") == "kurzy-cnb");
  CHECK((d["cards"][0]["installed"] | true) == false);
  CHECK(std::string(d["cards"][0]["desc"]["en"] | "").size() > 10);

  // 2. Installing from it. The second call rides on the cached index: one request, the card.
  const int before = s_requests;
  CHECK(install("kurzy-cnb", &why));
  CHECK(s_requests - before == 1);
  CHECK(Cards_Count() == 1);
  CHECK(std::string(Cards_Get(0)->id) == "kurzy-cnb");
  CHECK(list(d));
  CHECK((d["cards"][0]["installed"] | false) == true);
  CHECK((d["cards"][1]["installed"] | true) == false);
  printf("install kurzy-cnb: ok, %d card(s) on the device\n", Cards_Count());

  // 3. What is not in the index cannot be installed, whatever the id looks like.
  CHECK(!install("nothing-here", &why));
  CHECK(!install("../kurzy-cnb", &why));
  CHECK(!install("", &why));

  // 4. A card that changed after the index was signed (same length, one byte).
  serve("index.json", "index.json.sig");
  { std::string c = slurp(fx + "/svatek.json"); c[c.find("60")] = '9'; s_web[BASE + "svatek.json"] = c; }
  CHECK(!install("svatek", &why));
  printf("changed card: %s\n", why.c_str());
  CHECK(why == "the card does not match the index");
  CHECK(Cards_Count() == 1);

  // 5. An index that changed after it was signed.
  serve("index-changed.json", "index.json.sig");
  CHECK(!list(d, &why));
  printf("changed index: %s\n", why.c_str());
  CHECK(why == "the index signature is not valid");
  CHECK(!install("svatek", &why));

  // 6. A well-formed signature made with somebody else's key.
  serve("index.json", "index-foreign.sig");
  CHECK(!list(d, &why));
  printf("foreign signature: %s\n", why.c_str());
  CHECK(why == "the index signature is not valid");

  // 7. No signature, an empty one, and rubbish.
  serve("index.json", nullptr);
  CHECK(!list(d, &why));
  CHECK(why == "cannot download the index signature");
  serve("index.json", "empty.sig");
  CHECK(!list(d, &why));
  serve("index.json", "rubbish.sig");
  CHECK(!list(d, &why));
  CHECK(why == "the index signature is not valid");

  // 8. The store cannot be reached at all.
  s_web.clear(); s_ms += STORE_CACHE_MS + 1000;
  CHECK(!list(d, &why));
  CHECK(why == "cannot download the index");

  // The next two need indexes signed with the real private key; the script
  // makes them when the key is on this machine.
  if (exists(fx + "/index-evil.json")) {
    // 9. A correctly signed index whose entry points somewhere it should not.
    serve("index-evil.json", "index-evil.json.sig");
    s_web[BASE + "../../../secret.json"] = "{}";
    CHECK(list(d, &why));
    printf("signed index with a bad entry: %zu of 2 entries offered\n", d["cards"].size());
    CHECK(d["cards"].size() == 1);                      // the bad entry is not offered
    CHECK(!install("svatek", &why));                    // ...and cannot be installed
    CHECK(why == "no such card in the store");
  } else {
    printf("SKIPPED: signed index with a bad entry (no private key here)\n");
  }
  if (exists(fx + "/index-newer.json")) {
    // 10. After a newer index has been accepted, the older one is refused.
    serve("index-newer.json", "index-newer.json.sig");
    CHECK(list(d, &why));
    CHECK((d["serial"] | 0L) > 1);
    serve("index.json", "index.json.sig");
    CHECK(!list(d, &why));
    printf("older index after a newer one: %s\n", why.c_str());
    CHECK(why == "the index is older than one accepted before");
    CHECK(!install("svatek", &why));
  } else {
    printf("SKIPPED: older index after a newer one (no private key here)\n");
  }

  if (fails) printf("%d of %d checks FAILED\n", fails, checks);
  else printf("all %d checks passed\n", checks);
  return fails ? 1 : 0;
}
