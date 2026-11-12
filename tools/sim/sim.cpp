// =============================================================================
//  MeteoPlaneRadar - host simulator for the cards feature.
//
//  Runs the firmware's own Cards.cpp, CardPath.h, Script.cpp (with the Berry
//  interpreter), ScreenCards.cpp, UI.cpp, Layout.cpp, AAFont.cpp, Lang.cpp and
//  Canvas16.h on a PC. The network, the
//  filesystem and the panel are replaced by the stand-ins in shim/, nothing
//  else is. It installs a card, "restarts", fetches a saved answer, walks the
//  refresh / failure / retry timing and writes each screen state as a picture.
//
//    tools/sim/run.sh <card.json> <answer.json> <outdir> [cz|en] ["HH:MM   6 ~C"]
//
//  The status line argument is what the clock + outside temperature line would
//  show; '~' stands for the degree sign.
//
//  What this does NOT cover: the real HTTP client, the real LittleFS, the web
//  UI, touch, timing on the device. A picture from here is a preview, not a
//  photo of the hardware.
// =============================================================================
#include "common.h"

static std::string s_answer; static bool s_netFail = false; static int s_requests = 0;
bool Net_GetString(const char* url, String& out, const char* tag) {
  s_requests++;
  printf("NET[%s] GET %s -> %s\n", tag, url, s_netFail ? "FAIL" : "200");
  if (s_netFail) return false;
  out.assign(s_answer);
  return true;
}

static bool install(const char* what, const std::string& json) {
  JsonDocument doc; char err[80] = "";
  if (deserializeJson(doc, json)) { printf("INSTALL %-22s -> not JSON\n", what); return false; }
  const bool ok = Cards_Install(doc.as<JsonObjectConst>(), err, sizeof(err));
  printf("INSTALL %-22s -> %s%s%s\n", what, ok ? "ok" : "refused", ok ? "" : ": ", err);
  return ok;
}

int main(int argc, char** argv) {
  if (argc < 6) return 2;
  const std::string card = slurp(argv[1]), out = argv[3];
  s_answer = slurp(argv[2]);
  s_status = argv[5];
  // the harness passes the degree sign as '~'; the firmware's is the byte 0xF8
  for (auto& ch : s_status) if (ch == '~') ch = (char)0xF8;

  simBegin(argv[4]);

  Cards_Begin();
  Script_Begin();
  Script_SetEnabled(true);                 // script cards are off until someone switches them on
  frame(out + "/0-no-card.ppm");

  // things the device must refuse
  install("bad id", R"({"id":"Kurzy CNB","title":"x","url":"https://a.b/","rows":[{"path":"a"}]})");
  install("no rows", R"({"id":"x","title":"x","url":"https://a.b/","rows":[]})");
  install("five rows", R"({"id":"x","title":"x","url":"https://a.b/","rows":[{"path":"a"},{"path":"a"},{"path":"a"},{"path":"a"},{"path":"a"}]})");
  install("ftp url", R"({"id":"x","title":"x","url":"ftp://a.b/","rows":[{"path":"a"}]})");
  install("broken path", R"({"id":"x","title":"x","url":"https://a.b/","rows":[{"path":"rates[currencyCode=EUR"}]})");
  install("title too long", R"({"id":"x","title":"Tento nadpis je urcite delsi nez povoluje format","url":"https://a.b/","rows":[{"path":"a"}]})");
  install("future version", R"({"v":2,"id":"x","title":"x","url":"https://a.b/","rows":[{"path":"a"}]})");
  printf("cards after refusals: %d, files: %zu\n", Cards_Count(), g_simfs.files.size());

  if (!install("the sample card", card)) return 1;
  for (auto& kv : g_simfs.files) printf("FS %s  %zu B\n", kv.first.c_str(), kv.second.size());

  Cards_Begin();                           // "restart": the card must come back from the filesystem
  Script_Begin();
  printf("after restart: %d card(s)\n", Cards_Count());
  frame(out + "/1-loading.ppm");

  Cards_Tick();                            // first fetch
  { const Card* c = Cards_Get(0);
    if (c->hasScript) printf("SCRIPT card, kept data: %zu B, interpreter peak %zu B\n", Cards_Data(c) ? strlen(Cards_Data(c)) : (size_t)0, Script_HeapPeak());
    for (int i = 0; i < c->nRows; i++) printf("ROW %d  '%s'  value '%s' unit '%s' numeric=%d\n", i, c->rows[i].label, c->rows[i].value, c->rows[i].unit, (int)c->rows[i].numeric);
    printf("NOTE '%s' '%s'  valid=%d stale=%d\n", c->noteLabel, c->note, (int)c->valid, (int)c->stale); }
  frame(out + "/2-card.ppm");

  const int before = s_requests;
  s_ms += 59UL * 60000UL; Cards_Tick();    // 59 min later: not due yet
  printf("requests after 59 min: +%d\n", s_requests - before);
  s_ms += 2UL * 60000UL; s_netFail = true; Cards_Tick();   // due, and the network is down
  frame(out + "/3-stale.ppm");
  s_ms += 60000UL; Cards_Tick();
  printf("requests 1 min after a failure: +%d (retry is due after 2 min)\n", s_requests - before - 1);
  s_ms += 61000UL; s_netFail = false; Cards_Tick();        // retry succeeds
  { const Card* c = Cards_Get(0); printf("after retry: valid=%d stale=%d\n", (int)c->valid, (int)c->stale); }

  char cardId[24];
  snprintf(cardId, sizeof(cardId), "%s", Cards_Get(0)->id);
  const bool removed = Cards_Remove(cardId);
  printf("remove: %d, cards left: %d, files: %zu\n", (int)removed, Cards_Count(), g_simfs.files.size());
  return 0;
}
