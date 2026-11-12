// =============================================================================
//  MeteoPlaneRadar - host test of script cards.
//
//  Runs the firmware's own Script.cpp with the Berry interpreter, Cards.cpp and
//  the Cards screen on a PC and checks the things that have to hold however a
//  script behaves: it is off until switched on; a script that loops, hogs
//  memory, recurses without end or simply has a bug is stopped and reported,
//  and the firmware carries on; repeated failures switch that card's script
//  off; a restart in the middle of a script switches it off at the next boot;
//  and a script gets its data and parameters, and nothing else.
//
//    tools/sim/run_script_test.sh
// =============================================================================
#define MPR_HOST_TEST 1
#include "common.h"
#include <esp_system.h>
void Script_TestLeaveRunMark(const char* id);

bool Net_GetString(const char*, String& out, const char*) {
  out = "{\"rates\":[{\"currencyCode\":\"EUR\",\"rate\":24.38,\"amount\":1},{\"currencyCode\":\"USD\",\"rate\":21.91,\"amount\":1}],\"big\":\"0123456789\"}";
  return true;
}

static int fails = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

// Installs a card whose script is `src`; extra goes into the card as is.
static bool put(const char* id, const char* src, const char* extra = "") {
  // The device holds CARDS_MAX cards; make room by dropping the one-off cards of earlier cases.
  while (Cards_Count() >= CARDS_MAX) {
    bool dropped = false;
    for (int i = 0; i < Cards_Count() && !dropped; i++) {
      const std::string cid = Cards_Get(i)->id;
      if (cid != "ok" && cid != "runtime" && cid != id) { Cards_Remove(cid.c_str()); dropped = true; }
    }
    if (!dropped) break;
  }
  JsonDocument d;
  d["id"] = id; d["title"] = "Test";
  d["script"] = src;
  if (*extra) {
    JsonDocument x; deserializeJson(x, extra);
    for (JsonPairConst kv : x.as<JsonObjectConst>()) d[kv.key()] = kv.value();
  }
  char err[80] = "";
  const bool ok = Cards_Install(d.as<JsonObjectConst>(), err, sizeof(err));
  if (!ok) printf("install %s refused: %s\n", id, err);
  return ok;
}
static std::string why;
static bool draw(const char* id) {
  char w[72] = "";
  const Card* c = Cards_Mutable(id);
  gfx->fillScreen(C_BLACK);
  const unsigned long t0 = millis();
  const bool ok = Script_Draw(c, w, sizeof(w));
  const unsigned long ms = millis() - t0;
  why = w;
  printf("  %-12s %s%s%s  (%lu ms)\n", id, ok ? "drawn" : "stopped: ", ok ? "" : w, "", ms);
  CHECK(ms < 1500);                       // nothing may hold the firmware for long
  return ok;
}
static int litPixels() {
  uint16_t* fb = static_cast<Canvas16*>(gfx)->getFramebuffer();
  int n = 0;
  for (int i = 0; i < LCD_WIDTH * LCD_HEIGHT; i++) if (fb[i]) n++;
  return n;
}
static void fresh(const char* id) { Script_SwitchOn(id); Cards_Mutable(id)->scriptFails = 0; }

int main() {
  s_quietStatus = true;
  simBegin("cz");
  Cards_Begin();
  Script_Begin();

  // --- off until switched on ------------------------------------------------
  CHECK(put("ok", "def draw() g.fill_rect(100, 100, 50, 50, g.RED) end"));
  CHECK(!Script_Enabled());
  CHECK(!draw("ok"));
  CHECK(why == "scripts are switched off");
  CHECK(litPixels() == 0);
  Script_SetEnabled(true);
  CHECK(draw("ok"));
  CHECK(litPixels() == 50 * 50);

  // --- the format ------------------------------------------------------------
  { JsonDocument d; char err[80];
    d["id"] = "norows"; d["title"] = "x"; d["url"] = "https://a.b/";
    CHECK(!Cards_Install(d.as<JsonObjectConst>(), err, sizeof(err)));     // no script: rows are still required
    std::string big(SCRIPT_SRC_MAX + 1, ' ');
    d["script"] = big;
    CHECK(!Cards_Install(d.as<JsonObjectConst>(), err, sizeof(err)));     // script too long
    CHECK(std::string(err).find("script is longer") == 0); }

  // --- bugs: reported, not fatal ----------------------------------------------
  CHECK(put("syntax", "def draw( g.fill(1) end"));
  CHECK(!draw("syntax"));
  CHECK(put("nodraw", "var x = 1"));
  CHECK(!draw("nodraw"));
  CHECK(why == "the script has no draw() function");
  CHECK(put("runtime", "def draw() var m = nil  g.text(m['x'], 0, 0, 2, g.WHITE) end"));
  CHECK(!draw("runtime"));
  CHECK(put("noimport", "import os\ndef draw() end"));                    // there is no os module
  CHECK(!draw("noimport"));
  CHECK(put("nofile", "def draw() var f = open('/cards/ok.json') end"));  // ...and no files
  CHECK(!draw("nofile"));
  CHECK(put("badargs", "def draw() g.text(nil, 'a', {}, 99, -5)  g.ring()  g.line(1e9, -1e9, 0, 0, 7)  g.fill_circle(240, 240, 100000, g.BLUE) end"));
  CHECK(draw("badargs"));                                                 // nonsense arguments are clamped, not trusted

  // --- the ring is solid: its pixel count is its area -----------------------------
  CHECK(put("ring", "def draw() g.ring(240, 240, 150, 168, 0, 360, g.WHITE) end"));
  CHECK(draw("ring"));
  { const double area = 3.14159265 * (168.0 * 168.0 - 150.0 * 150.0);
    const int lit = litPixels();
    printf("     full ring: %d px, area %.0f\n", lit, area);
    CHECK(lit > area * 0.98 && lit < area * 1.02); }
  CHECK(put("ring", "def draw() g.ring(240, 240, 150, 168, 0, 216, g.WHITE) end"));    // 60 % of the way round
  CHECK(draw("ring"));
  { const double area = 0.6 * 3.14159265 * (168.0 * 168.0 - 150.0 * 150.0);
    const int lit = litPixels();
    printf("     60 %% ring: %d px, area %.0f\n", lit, area);
    CHECK(lit > area * 0.98 && lit < area * 1.02); }
  CHECK(put("ring", "def draw() g.ring(240, 240, 0, 100, 300, 30, g.WHITE) end"));     // three quarters of a pie, the angles given backwards
  CHECK(draw("ring"));
  { const double area = 0.75 * 3.14159265 * 100.0 * 100.0;
    const int lit = litPixels();
    printf("     270 deg pie: %d px, area %.0f\n", lit, area);
    CHECK(lit > area * 0.98 && lit < area * 1.02); }

  // --- limits -----------------------------------------------------------------
  CHECK(put("loop", "def draw() while true end end"));
  CHECK(!draw("loop"));
  CHECK(why.find("ran longer than") == 0);
  CHECK(put("looptry", "def draw() while true try while true end except .. end end end"));
  CHECK(!draw("looptry"));                                                // a script cannot catch its way out
  CHECK(why.find("ran longer than") == 0);
  CHECK(put("looptop", "while true end\ndef draw() end"));                // looping before draw() is even defined
  CHECK(!draw("looptop"));
  CHECK(why.find("ran longer than") == 0);
  CHECK(put("slowapi", "def draw() while true g.ring(240, 240, 0, 230, 0, 360, g.RED) end end"));
  CHECK(!draw("slowapi"));                                                // time spent inside drawing calls counts
  CHECK(why.find("ran longer than") == 0);
  CHECK(put("memory", "def draw() var s = 'x' while true s = s + s end end"));
  CHECK(!draw("memory"));
  printf("     (peak %zu B of %d allowed)\n", Script_HeapPeak(), (int)SCRIPT_HEAP_MAX);
  CHECK(Script_HeapPeak() <= SCRIPT_HEAP_MAX);
  CHECK(put("memlist", "def draw() var l = [] while true l.push([1, 2, 3, 4, 5, 6, 7, 8]) end end"));
  CHECK(!draw("memlist"));
  CHECK(Script_HeapPeak() <= SCRIPT_HEAP_MAX);
  CHECK(put("recurse", "def f(n) return f(n + 1) end\ndef draw() f(0) end"));
  CHECK(!draw("recurse"));
  CHECK(Script_HeapUsed() == 0);                                          // a stopped interpreter is given back whole

  // ...and after all that an ordinary card still draws
  fresh("ok");
  CHECK(draw("ok"));
  CHECK(litPixels() == 50 * 50);

  // --- three failures in a row switch the script off ---------------------------
  fresh("runtime");
  CHECK(!draw("runtime")); CHECK(!Script_IsOff("runtime", nullptr, 0));
  CHECK(!draw("runtime")); CHECK(!Script_IsOff("runtime", nullptr, 0));
  CHECK(!draw("runtime")); CHECK(Script_IsOff("runtime", nullptr, 0));
  CHECK(!draw("runtime"));
  CHECK(why.find("script is off: ") == 0);
  CHECK(put("runtime", "def draw() g.fill(g.BLUE) end"));                 // installing it again is a fresh start
  CHECK(!Script_IsOff("runtime", nullptr, 0));
  CHECK(draw("runtime"));

  // --- safe mode ---------------------------------------------------------------
  Script_TestLeaveRunMark("ok");                 // "the device died while this card's script ran"
  g_simResetReason = ESP_RST_POWERON;            // after a power cut the note means nothing
  Script_Begin();
  CHECK(!Script_IsOff("ok", nullptr, 0));
  Script_TestLeaveRunMark("ok");
  g_simResetReason = ESP_RST_TASK_WDT;           // after a watchdog reset it does
  Script_Begin();
  { char w[64] = ""; CHECK(Script_IsOff("ok", w, sizeof(w))); printf("  safe mode: ok -> %s\n", w); }
  CHECK(Script_Enabled());                       // the switch itself survives; only that card is held off
  CHECK(!draw("ok"));
  Script_Begin();                                // the next ordinary boot changes nothing
  CHECK(Script_IsOff("ok", nullptr, 0));
  Script_SwitchOn("ok");                         // "switch on again" in the web UI
  CHECK(draw("ok"));

  // --- data, parameters, and nothing else ----------------------------------------
  CHECK(put("data",
    "def draw()\n"
    "  var d = net.data()\n"
    "  if d == nil g.fill_rect(0, 0, 10, 10, g.RED) return end\n"
    "  var n = 0\n"
    "  for r : d['rates'] if r['currencyCode'] == cfg.get('code', '?') n = int(r['rate'] * 100) end end\n"
    "  g.fill_rect(0, 200, n / 100, 1, g.WHITE)\n"                       // 24 px for EUR
    "  if d.find('big') != nil g.fill_rect(0, 300, 5, 1, g.WHITE) end\n" // must NOT be there: not in keep
    "  if cfg.title() == 'Test' && cfg.lang() == 'cs' && net.ok() g.fill_rect(0, 400, 7, 1, g.WHITE) end\n"
    "end\n",
    "{\"url\":\"https://example.invalid/x\",\"keep\":[\"rates[0].currencyCode\",\"rates[0].rate\"],\"params\":{\"code\":\"EUR\"}}"));
  CHECK(draw("data"));
  CHECK(litPixels() == 10 * 10);                 // no answer yet
  for (int i = 0; i < 20; i++) Cards_Tick();     // fetch everything that has an address
  const Card* dc = Cards_Mutable("data");
  CHECK(dc->valid && Cards_Data(dc) != nullptr);
  printf("  kept for the script: %s\n", Cards_Data(dc) ? Cards_Data(dc) : "(nothing)");
  CHECK(draw("data"));
  CHECK(litPixels() == 24 + 7);                  // the EUR rate and the title/lang/ok line; "big" was filtered out

  Script_SetEnabled(false);
  CHECK(Script_HeapUsed() == 0);
  CHECK(!draw("data"));

  // --- every card is a place of its own in the stepping ----------------------------
  // (the walk the .ino does: step inside the cards screen first, leave it at either end)
  while (Cards_Count() > 0) { const std::string cid = Cards_Get(0)->id; Cards_Remove(cid.c_str()); }
  CHECK(ScreenCards_Count() == 1);               // no card: one place, the "add a card" hint
  CHECK(!ScreenCards_Step(+1) && !ScreenCards_Step(-1));
  CHECK(put("a", "def draw() end") && put("b", "def draw() end") && put("c", "def draw() end"));
  CHECK(ScreenCards_Count() == 3);
  ScreenCards_Select(0);                         // arriving from the screen before
  CHECK(ScreenCards_Index() == 0);
  CHECK(!ScreenCards_Step(-1));                  // stepping back leaves the cards screen
  CHECK(ScreenCards_Step(+1) && ScreenCards_Index() == 1);
  CHECK(ScreenCards_Step(+1) && ScreenCards_Index() == 2);
  CHECK(!ScreenCards_Step(+1));                  // past the last card: on to the next screen
  CHECK(ScreenCards_Index() == 2);
  ScreenCards_Select(-1);                        // arriving back from the screen after
  CHECK(ScreenCards_Index() == 2);
  CHECK(ScreenCards_Step(-1) && ScreenCards_Index() == 1);
  CHECK(Cards_Remove("c") && Cards_Remove("b"));   // cards removed under the screen's feet
  CHECK(ScreenCards_Count() == 1 && ScreenCards_Index() == 0);
  CHECK(!ScreenCards_Step(+1));

  if (fails) printf("%d of %d checks FAILED\n", fails, checks);
  else printf("all %d checks passed\n", checks);
  return fails ? 1 : 0;
}
