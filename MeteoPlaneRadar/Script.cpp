// =============================================================================
//  MeteoPlaneRadar
//  Script cards - the Berry interpreter, its API and its limits. See Script.h.
// =============================================================================
#include "Script.h"
#include "AAFont.h"
#include "Config.h"
#include "Lang.h"
#include "Layout.h"
#include "Route.h"
#include "ScreenCards.h"
#include "UI.h"
#include <LittleFS.h>
#include <esp_attr.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <math.h>
#include <time.h>
#include "src/berry/src/berry.h"

#define SCRIPTS_ON_FILE "/scripts-on"
#define OFF_DIR         "/cards"          // "<id>.off" next to "<id>.json"

// --- Memory limit -----------------------------------------------------------
// Berry allocates through these three (berry_conf.h). Every block carries its
// size in front, so the total a card's interpreter holds is known exactly and
// a request that would cross SCRIPT_HEAP_MAX is refused - which Berry turns
// into an out-of-memory stop of the script, not of the firmware. The blocks
// come from PSRAM, like the firmware's other large buffers.
struct MemHdr { size_t n; size_t pad; };
static size_t s_memUsed = 0, s_memPeak = 0;

extern "C" void* mpr_be_malloc(size_t n) {
  if (n == 0) n = 1;
  if (n > SCRIPT_HEAP_MAX || s_memUsed + n > SCRIPT_HEAP_MAX) return nullptr;
  MemHdr* h = (MemHdr*)heap_caps_malloc(n + sizeof(MemHdr), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!h) h = (MemHdr*)malloc(n + sizeof(MemHdr));
  if (!h) return nullptr;
  h->n = n;
  s_memUsed += n;
  if (s_memUsed > s_memPeak) s_memPeak = s_memUsed;
  return h + 1;
}

extern "C" void mpr_be_free(void* p) {
  if (!p) return;
  MemHdr* h = (MemHdr*)p - 1;
  s_memUsed -= h->n;
  free(h);
}

extern "C" void* mpr_be_realloc(void* p, size_t n) {
  if (!p) return mpr_be_malloc(n);
  if (n == 0) { mpr_be_free(p); return nullptr; }
  const size_t old = ((MemHdr*)p - 1)->n;
  if (n <= old) return p;                       // shrinking keeps the block
  if (n > SCRIPT_HEAP_MAX || s_memUsed - old + n > SCRIPT_HEAP_MAX) return nullptr;
  void* q = mpr_be_malloc(n);
  if (!q) return nullptr;
  memcpy(q, p, old);
  mpr_be_free(p);
  return q;
}

size_t Script_HeapPeak() { return s_memPeak; }
size_t Script_HeapUsed() { return s_memUsed; }

// print() in a script goes to the serial log, one prefix per line.
extern "C" void mpr_be_print(const char* buffer, size_t length) {
  static bool atLineStart = true;
  for (size_t i = 0; i < length; i++) {
    if (atLineStart) { Serial.print("SKRIPT: "); atLineStart = false; }
    char one[2] = { buffer[i], '\0' };
    Serial.print(one);
    if (buffer[i] == '\n') atLineStart = true;
  }
}

// --- State ------------------------------------------------------------------
static bvm*          s_vm = nullptr;
static char          s_vmCard[24] = "";     // whose script the interpreter holds
static uint32_t      s_vmInstall = 0;       // ...and which install of that card
static uint32_t      s_vmDataRev = 0;
static const Card*   s_card = nullptr;      // the card being drawn (valid during a call)
static unsigned long s_deadline = 0;        // millis() by which the current run must end
static bool          s_timedOut = false;
static bool          s_enabled = false;

// Survives a reset (not a power cycle): which card's script was running.
#define RUN_MAGIC 0x5C21B7A9u
RTC_NOINIT_ATTR static uint32_t s_runMagic;
RTC_NOINIT_ATTR static char     s_runCard[24];

static void runMark(const char* id) {
  snprintf(s_runCard, sizeof(s_runCard), "%s", id);
  s_runMagic = RUN_MAGIC;
}
static void runClear() { s_runMagic = 0; }

// --- Switches on the filesystem ---------------------------------------------
static void offPath(char* out, size_t cap, const char* id) {
  snprintf(out, cap, OFF_DIR "/%s.off", id);
}

static void switchOff(const char* id, const char* why) {
  if (!Cards_FsReady()) return;
  char p[48];
  offPath(p, sizeof(p), id);
  File f = LittleFS.open(p, "w");
  if (!f) return;
  f.print(why);
  f.close();
  Serial.printf("SKRIPT: %s vypnut: %s\n", id, why);
}

bool Script_IsOff(const char* id, char* why, size_t whyCap) {
  if (!Cards_FsReady()) return false;
  char p[48];
  offPath(p, sizeof(p), id);
  if (!LittleFS.exists(p)) return false;
  if (why && whyCap) {
    why[0] = '\0';
    File f = LittleFS.open(p, "r");
    if (f) {
      size_t n = 0;
      for (int c = f.read(); c >= 0 && n + 1 < whyCap; c = f.read()) why[n++] = (char)c;
      why[n] = '\0';
      f.close();
    }
  }
  return true;
}

void Script_SwitchOn(const char* id) {
  if (!Cards_FsReady()) return;
  char p[48];
  offPath(p, sizeof(p), id);
  LittleFS.remove(p);
}

bool Script_Enabled() { return s_enabled; }

void Script_SetEnabled(bool on) {
  s_enabled = on;
  if (!on) Script_Release();
  if (!Cards_FsReady()) return;
  if (on) { File f = LittleFS.open(SCRIPTS_ON_FILE, "w"); if (f) { f.print("1"); f.close(); } }
  else    LittleFS.remove(SCRIPTS_ON_FILE);
}

void Script_Begin() {
  s_enabled = Cards_FsReady() && LittleFS.exists(SCRIPTS_ON_FILE);

  // Safe mode. After a power-on the RTC memory holds whatever it likes, so
  // the note only counts when the chip says it restarted without losing power.
  const esp_reset_reason_t why = esp_reset_reason();
  if (why != ESP_RST_POWERON && why != ESP_RST_UNKNOWN && s_runMagic == RUN_MAGIC) {
    s_runCard[sizeof(s_runCard) - 1] = '\0';
    switchOff(s_runCard, "the device restarted while this script was running");
  }
  runClear();
}

// --- Time limit -------------------------------------------------------------
// The interpreter calls this every few thousand instructions. be_exit() ends
// the run with a status no try/except in the script can catch.
static void obsHook(bvm* vm, int event, ...) {
  if (event == BE_OBS_VM_HEARTBEAT && s_deadline && (long)(millis() - s_deadline) >= 0) {
    s_timedOut = true;
    be_exit(vm, 1);
  }
}

// Every API call checks the clock too: a script can spend its time inside
// drawing calls without executing many instructions of its own.
static inline void checkClock(bvm* vm) {
  if (s_deadline && (long)(millis() - s_deadline) >= 0) { s_timedOut = true; be_exit(vm, 1); }
}

// --- Arguments --------------------------------------------------------------
static int argInt(bvm* vm, int i, int def, int lo, int hi) {
  int v = def;
  if (be_top(vm) >= i && be_isnumber(vm, i)) {
    const breal r = be_toreal(vm, i);
    v = (r < (breal)lo) ? lo : (r > (breal)hi) ? hi : (int)r;
  }
  return v;
}
static const char* argStr(bvm* vm, int i) {
  return (be_top(vm) >= i && be_isstring(vm, i)) ? be_tostring(vm, i) : "";
}
#define XY_MIN (-2000)
#define XY_MAX 2000
static inline uint16_t argColor(bvm* vm, int i) { return (uint16_t)argInt(vm, i, C_WHITE, 0, 0xFFFF); }

// Text as the panel can draw it (accents folded for the built-in font).
static const char* panel(char* buf, size_t cap, const char* utf8) {
  if (AAText_Enabled()) return utf8;
  Route_ToAscii(buf, cap, utf8);
  return buf;
}

// --- g: drawing ---------------------------------------------------------------
static int g_fill(bvm* vm) {                    // g.fill(color)
  checkClock(vm);
  gfx->fillScreen(argColor(vm, 1));
  be_return_nil(vm);
}
static int g_text(bvm* vm) {                    // g.text(s, x, y, size, color)
  checkClock(vm);
  char buf[96];
  gfx->setTextSize((uint8_t)argInt(vm, 4, 2, 1, 5));
  gfx->setTextColor(argColor(vm, 5));
  gfx->setCursor(argInt(vm, 2, 0, XY_MIN, XY_MAX), argInt(vm, 3, 0, XY_MIN, XY_MAX));
  gfx->print(panel(buf, sizeof(buf), argStr(vm, 1)));
  be_return_nil(vm);
}
static int g_label(bvm* vm) {                   // g.label(s, y, size, color): centred, shrunk to fit the circle
  checkClock(vm);
  ScreenCards_Label(argStr(vm, 1), argInt(vm, 2, 0, 0, LCD_HEIGHT), argColor(vm, 4),
                    (uint8_t)argInt(vm, 3, 2, 1, 5));
  be_return_nil(vm);
}
static int g_text_w(bvm* vm) {                  // g.text_w(s, size) -> pixels
  char buf[96];
  be_pushint(vm, Layout_TextW(panel(buf, sizeof(buf), argStr(vm, 1)), (uint8_t)argInt(vm, 2, 2, 1, 5)));
  be_return(vm);
}
static int g_line(bvm* vm) {                    // g.line(x0, y0, x1, y1, color)
  checkClock(vm);
  gfx->drawLine(argInt(vm, 1, 0, XY_MIN, XY_MAX), argInt(vm, 2, 0, XY_MIN, XY_MAX),
                argInt(vm, 3, 0, XY_MIN, XY_MAX), argInt(vm, 4, 0, XY_MIN, XY_MAX), argColor(vm, 5));
  be_return_nil(vm);
}
static int g_rect(bvm* vm) {                    // g.rect(x, y, w, h, color): outline
  checkClock(vm);
  const int x = argInt(vm, 1, 0, XY_MIN, XY_MAX), y = argInt(vm, 2, 0, XY_MIN, XY_MAX);
  const int w = argInt(vm, 3, 0, 0, XY_MAX), h = argInt(vm, 4, 0, 0, XY_MAX);
  const uint16_t c = argColor(vm, 5);
  if (w > 0 && h > 0) {
    gfx->fillRect(x, y, w, 1, c); gfx->fillRect(x, y + h - 1, w, 1, c);
    gfx->fillRect(x, y, 1, h, c); gfx->fillRect(x + w - 1, y, 1, h, c);
  }
  be_return_nil(vm);
}
static int g_fill_rect(bvm* vm) {               // g.fill_rect(x, y, w, h, color)
  checkClock(vm);
  gfx->fillRect(argInt(vm, 1, 0, XY_MIN, XY_MAX), argInt(vm, 2, 0, XY_MIN, XY_MAX),
                argInt(vm, 3, 0, 0, XY_MAX), argInt(vm, 4, 0, 0, XY_MAX), argColor(vm, 5));
  be_return_nil(vm);
}
static int g_circle(bvm* vm) {                  // g.circle(x, y, r, color)
  checkClock(vm);
  gfx->drawCircle(argInt(vm, 1, 0, XY_MIN, XY_MAX), argInt(vm, 2, 0, XY_MIN, XY_MAX),
                  argInt(vm, 3, 0, 0, LCD_WIDTH), argColor(vm, 4));
  be_return_nil(vm);
}
static int g_fill_circle(bvm* vm) {             // g.fill_circle(x, y, r, color)
  checkClock(vm);
  gfx->fillCircle(argInt(vm, 1, 0, XY_MIN, XY_MAX), argInt(vm, 2, 0, XY_MIN, XY_MAX),
                  argInt(vm, 3, 0, 0, LCD_WIDTH), argColor(vm, 4));
  be_return_nil(vm);
}
// A sector of a ring, filled row by row. (UI_FillRing() paints radial lines,
// which is right for the thin dials it was written for; on a wide ring at an
// arbitrary radius neighbouring lines leave streaks of unpainted pixels.)
// Angles in degrees, clockwise from twelve o'clock, a0 < a1 <= a0 + 360.
static void fillSector(int cx, int cy, int rIn, int rOut, float a0, float a1, uint16_t color) {
  if (rIn < 0) rIn = 0;
  if (rOut <= 0 || rIn > rOut || a1 <= a0) return;
  const float span = a1 - a0;
  const bool full = span >= 360.0f, wide = span > 180.0f;
  const float k = 3.14159265f / 180.0f;
  // unit vectors along the two edges; on screen y grows downwards
  const float ux0 = sinf(a0 * k), uy0 = -cosf(a0 * k), ux1 = sinf(a1 * k), uy1 = -cosf(a1 * k);
  const long ro2 = (long)rOut * rOut, ri2 = (long)rIn * rIn;

  for (int dy = -rOut; dy <= rOut; dy++) {
    const int y = cy + dy;
    if (y < 0 || y >= LCD_HEIGHT) continue;
    const int half = (int)sqrtf((float)(ro2 - (long)dy * dy));
    int run = 0, runX = 0;
    for (int dx = -half; dx <= half + 1; dx++) {
      bool in = false;
      if (dx <= half) {
        const long d2 = (long)dx * dx + (long)dy * dy;
        if (d2 <= ro2 && d2 >= ri2) {
          if (full) in = true;
          else {
            const float c0 = ux0 * dy - uy0 * dx;     // >= 0: clockwise of the first edge
            const float c1 = dx * uy1 - dy * ux1;     // >= 0: not yet past the second
            in = wide ? (c0 >= 0 || c1 >= 0) : (c0 >= 0 && c1 >= 0);
          }
        }
      }
      if (in) { if (!run) runX = dx; run++; }
      else if (run) { gfx->fillRect(cx + runX, y, run, 1, color); run = 0; }
    }
  }
}

// g.ring(cx, cy, r_in, r_out, from_deg, to_deg, color): a sector of a ring.
// Degrees run clockwise from twelve o'clock, as on a dial.
static int g_ring(bvm* vm) {
  checkClock(vm);
  const float a0 = (float)argInt(vm, 5, 0, -3600, 3600), a1 = (float)argInt(vm, 6, 360, -3600, 3600);
  float lo = a0 < a1 ? a0 : a1, hi = a0 < a1 ? a1 : a0;
  if (hi - lo > 360.0f) hi = lo + 360.0f;
  fillSector(argInt(vm, 1, LCD_WIDTH / 2, XY_MIN, XY_MAX), argInt(vm, 2, LCD_HEIGHT / 2, XY_MIN, XY_MAX),
             argInt(vm, 3, 0, 0, LCD_WIDTH), argInt(vm, 4, 0, 0, LCD_WIDTH), lo, hi, argColor(vm, 7));
  be_return_nil(vm);
}
static int g_rgb(bvm* vm) {                     // g.rgb(r, g, b) -> colour
  const int r = argInt(vm, 1, 0, 0, 255), g = argInt(vm, 2, 0, 0, 255), b = argInt(vm, 3, 0, 0, 255);
  be_pushint(vm, ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
  be_return(vm);
}
static int g_half(bvm* vm) {                    // g.half(y) -> half the width of the round panel at that height
  be_pushint(vm, Layout_ChordHalf(argInt(vm, 1, LCD_HEIGHT / 2, XY_MIN, XY_MAX)));
  be_return(vm);
}

// --- net, cfg ---------------------------------------------------------------
static int net_ok(bvm* vm) {                    // net.ok(): the last fetch succeeded
  be_pushbool(vm, s_card && s_card->valid && !s_card->stale);
  be_return(vm);
}
static int net_age(bvm* vm) {                   // net.age(): seconds since the last good answer, or nil
  if (!s_card || !s_card->valid) be_return_nil(vm);
  be_pushint(vm, (bint)((millis() - s_card->lastOk) / 1000UL));
  be_return(vm);
}
static int cfg_lang(bvm* vm) {                  // cfg.lang() -> "cs" / "en"
  be_pushstring(vm, Lang_Get() == LANG_EN ? "en" : "cs");
  be_return(vm);
}
static int cfg_title(bvm* vm) {                 // cfg.title() -> the card's title
  be_pushstring(vm, s_card ? s_card->title : "");
  be_return(vm);
}
static int cfg_hms(bvm* vm) {                   // cfg.hms() -> seconds since local midnight, or nil before the clock is set
  const time_t now = time(nullptr);
  if (now < 1600000000) be_return_nil(vm);
  struct tm lt;
  localtime_r(&now, &lt);
  be_pushint(vm, lt.tm_hour * 3600 + lt.tm_min * 60 + lt.tm_sec);
  be_return(vm);
}

// The modules are put together in Berry itself: fewer hand-built objects on
// the C side, and the script sees ordinary modules.
static const char BOOT[] =
  "import json\n"
  "var g = module('g')\n"
  "g.fill = _g_fill  g.text = _g_text  g.label = _g_label  g.text_w = _g_text_w\n"
  "g.line = _g_line  g.rect = _g_rect  g.fill_rect = _g_fill_rect\n"
  "g.circle = _g_circle  g.fill_circle = _g_fill_circle  g.ring = _g_ring\n"
  "g.rgb = _g_rgb  g.half = _g_half\n"
  "g.W = 480  g.H = 480\n"
  "g.BLACK = 0x0000  g.WHITE = 0xFFFF  g.GRAY = 0x8410  g.DKGRAY = 0x2124\n"
  "g.RED = 0xF800  g.GREEN = 0x07E0  g.BLUE = 0x001F  g.YELLOW = 0xFFE0\n"
  "g.CYAN = 0x05FF  g.ORANGE = 0xFC00\n"
  "var net = module('net')\n"
  "var _nd = nil\n"
  "def _net_set(s)\n"
  "  _nd = nil\n"
  "  if s != nil _nd = json.load(s) end\n"
  "end\n"
  "net.data = def () return _nd end\n"
  "net.ok = _net_ok  net.age = _net_age\n"
  "var cfg = module('cfg')\n"
  "var _cp = {}\n"
  "def _cfg_set(s)\n"
  "  _cp = json.load(s)\n"
  "  if classname(_cp) != 'map' _cp = {} end\n"
  "end\n"
  "cfg.get = def (k, d) return _cp.find(k, d) end\n"
  "cfg.lang = _cfg_lang  cfg.title = _cfg_title  cfg.hms = _cfg_hms\n";

static const struct { const char* name; bntvfunc f; } NATIVES[] = {
  { "_g_fill", g_fill }, { "_g_text", g_text }, { "_g_label", g_label }, { "_g_text_w", g_text_w },
  { "_g_line", g_line }, { "_g_rect", g_rect }, { "_g_fill_rect", g_fill_rect },
  { "_g_circle", g_circle }, { "_g_fill_circle", g_fill_circle }, { "_g_ring", g_ring },
  { "_g_rgb", g_rgb }, { "_g_half", g_half },
  { "_net_ok", net_ok }, { "_net_age", net_age },
  { "_cfg_lang", cfg_lang }, { "_cfg_title", cfg_title }, { "_cfg_hms", cfg_hms },
};

// --- Running things under the limits ----------------------------------------
// Runs the function on top of the stack with argc arguments above it. On
// failure fills `why` and returns false; the interpreter is then not to be
// used again (the caller releases it).
static bool guarded(int argc, unsigned long budgetMs, char* why, size_t whyCap) {
  s_timedOut = false;
  s_deadline = millis() + budgetMs;
  if (s_deadline == 0) s_deadline = 1;
  const int res = be_pcall(s_vm, argc);
  s_deadline = 0;
  if (res == BE_OK) { be_pop(s_vm, argc + 1); return true; }

  if (s_timedOut)                 snprintf(why, whyCap, "ran longer than %d ms", (int)SCRIPT_TIME_MS);
  else if (res == BE_MALLOC_FAIL) snprintf(why, whyCap, "needs more than %d kB", (int)(SCRIPT_HEAP_MAX / 1024));
  else if (res == BE_EXCEPTION) {
    // the exception value and its message are the two topmost values
    const char* kind = be_isstring(s_vm, -2) ? be_tostring(s_vm, -2) : "error";
    const char* msg  = be_isstring(s_vm, -1) ? be_tostring(s_vm, -1) : "";
    snprintf(why, whyCap, "%s: %s", kind, msg);
  } else snprintf(why, whyCap, "stopped (%d)", res);
  return false;
}

static bool compileAndRun(const char* name, const char* src, size_t len, unsigned long budgetMs,
                          char* why, size_t whyCap) {
  if (be_loadbuffer(s_vm, name, src, len) != BE_OK) {
    const char* msg = be_isstring(s_vm, -1) ? be_tostring(s_vm, -1) : "does not compile";
    snprintf(why, whyCap, "%s", msg);
    return false;
  }
  return guarded(0, budgetMs, why, whyCap);
}

// Calls a global Berry function with one string (or nil) argument.
static bool callWithString(const char* fn, const char* arg, char* why, size_t whyCap) {
  if (!be_getglobal(s_vm, fn)) { be_pop(s_vm, 1); snprintf(why, whyCap, "internal: %s", fn); return false; }
  if (arg) be_pushstring(s_vm, arg); else be_pushnil(s_vm);
  return guarded(1, SCRIPT_TIME_MS, why, whyCap);
}

void Script_Release() {
  if (s_vm) { be_vm_delete(s_vm); s_vm = nullptr; }
  s_vmCard[0] = '\0';
  s_vmDataRev = 0;
}

// Builds the interpreter for this card: API, then the card's own script.
static bool start(const Card* c, char* why, size_t whyCap) {
  Script_Release();

  // The script and the params live in the card's file, not in RAM.
  char path[48];
  snprintf(path, sizeof(path), "/cards/%s.json", c->id);
  File f = LittleFS.open(path, "r");
  if (!f) { snprintf(why, whyCap, "card file is missing"); return false; }
  JsonDocument doc;
  const DeserializationError e = deserializeJson(doc, f);
  f.close();
  const char* src = doc["script"] | "";
  if (e || !*src) { snprintf(why, whyCap, "card file has no script"); return false; }

  s_vm = be_vm_new();
  if (!s_vm) { snprintf(why, whyCap, "no memory for the interpreter"); return false; }
  be_set_obs_hook(s_vm, obsHook);
  for (const auto& n : NATIVES) be_regfunc(s_vm, n.name, n.f);

  if (!compileAndRun("api", BOOT, sizeof(BOOT) - 1, 2000, why, whyCap)) return false;

  String params;
  if (doc["params"].is<JsonObjectConst>()) serializeJson(doc["params"], params);
  if (params.length() && !callWithString("_cfg_set", params.c_str(), why, whyCap)) return false;

  if (!compileAndRun(c->id, src, strlen(src), SCRIPT_TIME_MS, why, whyCap)) return false;

  snprintf(s_vmCard, sizeof(s_vmCard), "%s", c->id);
  s_vmInstall = c->installNo;
  s_vmDataRev = 0;
  return true;
}

bool Script_Draw(const Card* c, char* why, size_t whyCap) {
  why[0] = '\0';
  if (!c || !c->hasScript) { snprintf(why, whyCap, "not a script card"); return false; }
  if (!s_enabled) { Script_Release(); snprintf(why, whyCap, "scripts are switched off"); return false; }
  char off[64];
  if (Script_IsOff(c->id, off, sizeof(off))) {
    Script_Release();
    snprintf(why, whyCap, "script is off: %s", off);
    return false;
  }

  s_card = c;
  runMark(c->id);                               // safe mode: from here until runClear()
  bool ok = true;
  if (!s_vm || strcmp(s_vmCard, c->id) != 0 || s_vmInstall != c->installNo) ok = start(c, why, whyCap);

  // Hand over a new answer when there is one.
  if (ok && s_vmDataRev != c->dataRev) {
    ok = callWithString("_net_set", Cards_Data(c), why, whyCap);
    if (!ok && strncmp(why, "ran longer", 10) != 0 && strncmp(why, "needs more", 10) != 0)
      snprintf(why, whyCap, "the answer is not usable JSON");
    s_vmDataRev = c->dataRev;
  }

  if (ok) {
    if (be_getglobal(s_vm, "draw") && be_isfunction(s_vm, -1)) {
      ok = guarded(0, SCRIPT_TIME_MS, why, whyCap);
    } else {
      be_pop(s_vm, 1);
      snprintf(why, whyCap, "the script has no draw() function");
      ok = false;
    }
  }
  runClear();
  s_card = nullptr;

  Card* m = Cards_Mutable(c->id);
  if (ok) {
    if (m) m->scriptFails = 0;
    return true;
  }
  Serial.printf("SKRIPT: %s: %s\n", c->id, why);
  Script_Release();                             // a failed interpreter is not reused
  if (m && ++m->scriptFails >= SCRIPT_FAIL_MAX) {
    switchOff(c->id, why);
    m->scriptFails = 0;
  }
  return false;
}

#ifdef MPR_HOST_TEST
// Leaves the "a script is running" note behind, as a crash in the middle of a
// script would. Only the host tests have this.
void Script_TestLeaveRunMark(const char* id) { runMark(id); }
#endif
