// =============================================================================
//  MeteoPlaneRadar - host simulator for the cards feature.
//
//  Runs the firmware's own Cards.cpp, CardPath.h, ScreenCards.cpp, UI.cpp,
//  Layout.cpp, AAFont.cpp, Lang.cpp and Canvas16.h on a PC. The network, the
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
#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <WiFi.h>
#include "Canvas16.h"
#include "UI.h"
#include "Layout.h"
#include "Lang.h"
#include "Cards.h"
#include "ScreenCards.h"
#include "Settings.h"
#include "Status.h"
#include "Net.h"
#include "Outside.h"
#include "Route.h"
#include "Config.h"
#include <fstream>
#include <sstream>

Arduino_GFX* gfx = nullptr;
SerialShim Serial;
int g_builtinGlyphs = 0;
static unsigned long s_ms = 5000;
unsigned long millis() { return s_ms; }

static std::string slurp(const std::string& p) { std::ifstream f(p, std::ios::binary); std::stringstream s; s << f.rdbuf(); return s.str(); }

// --- what the rest of the firmware would provide ---------------------------
uint8_t Settings_ClockFont() { return CLOCK_FONT_DEFAULT; }
bool Settings_ScreenEnabled(uint8_t idx) {
  // defaults (clock, planes, meteo, forecast) plus the cards screen switched on
  return idx == SCREEN_CLOCK_I || idx == SCREEN_PLANES_I || idx == SCREEN_METEO_I ||
         idx == SCREEN_FORECAST_I || idx == SCREEN_CARDS_I;
}
void Status_Set(StatusSlot slot, const char* fmt, ...) {
  char b[160]; va_list ap; va_start(ap, fmt); vsnprintf(b, sizeof(b), fmt, ap); va_end(ap);
  printf("STATUS[%d] %s\n", (int)slot, b);
}
static std::string s_answer; static bool s_netFail = false; static int s_requests = 0;
bool Net_GetString(const char* url, String& out, const char* tag) {
  s_requests++;
  printf("NET[%s] GET %s -> %s\n", tag, url, s_netFail ? "FAIL" : "200");
  if (s_netFail) return false;
  out.assign(s_answer);
  return true;
}
static std::string s_status;
void Outside_StatusText(char* buf, size_t cap) { snprintf(buf, cap, "%s", s_status.c_str()); }
void Route_ToAscii(char* dst, size_t cap, const char* src) {
  printf("UNEXPECTED: Route_ToAscii reached with a smooth typeface\n");
  snprintf(dst, cap, "%s", src);
}
uint16_t* LCD_FrameBuffer(int) { return nullptr; }
void LCD_Flush(const uint16_t*) {}

// --- the screen selector dots, as drawScreenDots() in MeteoPlaneRadar.ino ----
// Off by default like on the device (Settings_ScreenDots); SIM_DOTS=1 draws them.
static bool simDots() { const char* e = getenv("SIM_DOTS"); return e && *e && *e != '0'; }
static void drawScreenDots(int s_screen) {
  if (!simDots()) return;
  int vis[SCREEN_N]; int n = 0;
  for (int i = 0; i < SCREEN_N; i++)
    if (i == SCREEN_SETTINGS_I || Settings_ScreenEnabled(i)) vis[n++] = i;
  if (n <= 1) return;
  const int gap = 20;
  const int cx = LCD_WIDTH / 2;
  const int y = LY_DOTS;
  const int startX = cx - (n - 1) * gap / 2;
  for (int i = 0; i < n; i++) {
    int x = startX + i * gap;
    if (vis[i] == s_screen) gfx->fillCircle(x, y, 4, C_WHITE);
    else                    gfx->drawCircle(x, y, 4, C_GRAY);
  }
}

static void dump(const std::string& path) {
  uint16_t* fb = static_cast<Canvas16*>(gfx)->getFramebuffer();
  FILE* f = fopen(path.c_str(), "wb");
  fprintf(f, "P6\n%d %d\n255\n", LCD_WIDTH, LCD_HEIGHT);
  int outside = 0;                         // lit pixels the round glass would hide
  for (int y = 0; y < LCD_HEIGHT; y++) for (int x = 0; x < LCD_WIDTH; x++) {
    const uint16_t c = fb[y * LCD_WIDTH + x];
    const uint8_t r = (c >> 11) & 0x1F, g = (c >> 5) & 0x3F, b = c & 0x1F;
    const uint8_t px[3] = { (uint8_t)((r << 3) | (r >> 2)), (uint8_t)((g << 2) | (g >> 4)), (uint8_t)((b << 3) | (b >> 2)) };
    fwrite(px, 1, 3, f);
    const double dx = x - 239.5, dy = y - 239.5;
    if (c && dx * dx + dy * dy > 240.0 * 240.0) outside++;
  }
  fclose(f);
  printf("FRAME %s  lit pixels outside the circle: %d  built-in font glyphs: %d\n", path.c_str(), outside, g_builtinGlyphs);
}
static void frame(const std::string& path) {
  ScreenCards_Enter();
  ScreenCards_Draw();
  drawScreenDots(SCREEN_CARDS_I);
  dump(path);
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

  gfx = new Canvas16(LCD_WIDTH, LCD_HEIGHT);
  gfx->begin();
  Lang_Set(strcmp(argv[4], "en") == 0 ? LANG_EN : LANG_CZ);

  Cards_Begin();
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
  printf("after restart: %d card(s)\n", Cards_Count());
  frame(out + "/1-loading.ppm");

  Cards_Tick();                            // first fetch
  { const Card* c = Cards_Get(0);
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
