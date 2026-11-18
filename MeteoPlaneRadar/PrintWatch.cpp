// =============================================================================
//  MeteoPlaneRadar
//  3D printer watch (0.9.0) - the task. See PrintWatch.h and PRINT_* in Config.h.
//
//  Threading, in one paragraph: the task owns the network side and everything
//  marked "task only" below. loop() owns the settings; PrintWatch_Tick() copies
//  them into s_cfg under the mutex whenever Settings_PrintGen() moves, and the
//  task takes a copy of s_cfg at the start of every round. The results go the
//  other way through s_info / s_hist / s_event, again only under the mutex.
//  Nothing in here calls into Settings, the display or the web server.
//
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
// =============================================================================
#include "PrintWatch.h"
#include "PrintParse.h"
#include "Settings.h"
#include "Config.h"
#include "WebConfig.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <math.h>
#include <string.h>
#include <ctype.h>
#include <esp_random.h>

// --- shared, under s_mtx -------------------------------------------------------
struct Cfg {
  bool on = false;
  uint8_t type = PRINT_TYPE_PRUSA;
  char host[64] = "";
  char user[32] = "";
  char key[64] = "";
  char pass[64] = "";
  uint32_t gen = 0;
};
struct Sample { uint32_t t; int16_t noz, nozT, bed, bedT; };

static SemaphoreHandle_t s_mtx = nullptr;
static TaskHandle_t      s_task = nullptr;
static Cfg       s_cfg;
static PrintInfo s_info;
static Sample    s_hist[PRINT_HIST_N];
static int       s_histN = 0, s_histHead = 0;   // ring: head = next write
static PrintEvent s_event = PE_NONE;
static char      s_eventMsg[64] = "";
static volatile bool s_view = false;            // the Print screen is up
static uint32_t  s_cfgGenSeen = 0;              // loop side

// --- task only -----------------------------------------------------------------
static uint32_t  t_gen = 0;                     // config the state below belongs to
static bool      t_baseline = false;            // first answer seen since (re)start
static PrintState t_lastOnline = PS_UNKNOWN;    // last state the printer reported
static char      t_lastEvFile[48] = "";
static PrintEvent t_lastEv = PE_NONE;
static int32_t   t_prusaJob = -2;               // job whose name/material we have
static bool      t_prusaLegacy = false;         // old PrusaLink without /api/v1
static char      t_klipFile[128] = "";          // file whose metadata we have
static double    t_klipEst = 0;
static DigestChallenge t_dig;
static bool      t_digOk = false;
static uint32_t  t_nc = 0;
static IPAddress t_ip;                          // resolved host
static bool      t_ipOk = false;

static inline void lock()   { xSemaphoreTake(s_mtx, portMAX_DELAY); }
static inline void unlock() { xSemaphoreGive(s_mtx); }

// -----------------------------------------------------------------------------
//  HTTP
// -----------------------------------------------------------------------------
// The printer's address as host and port. Klipper without a port means
// Moonraker's own 7125; Prusa without one means 80.
static void splitHost(const Cfg& c, char* host, size_t cap, uint16_t* port) {
  strncpy(host, c.host, cap - 1); host[cap - 1] = '\0';
  *port = (c.type == PRINT_TYPE_KLIPPER) ? PRINT_MOONRAKER_PORT : 80;
  char* colon = strrchr(host, ':');
  if (colon) { *port = (uint16_t)atoi(colon + 1); *colon = '\0'; if (!*port) *port = 80; }
}

// A name is looked up once and the address kept: a lookup that times out
// every round would be the slowest part of asking a switched-off printer.
// ".local" names go through mDNS, everything else through DNS.
static bool resolve(const char* host) {
  if (t_ipOk) return true;
  if (t_ip.fromString(host)) { t_ipOk = true; return true; }
  const size_t n = strlen(host);
  if (n > 6 && !strcasecmp(host + n - 6, ".local")) {
    char name[64]; memcpy(name, host, n - 6); name[n - 6] = '\0';
    IPAddress ip = MDNS.queryHost(name, 1500);
    if (ip != IPAddress((uint32_t)0)) { t_ip = ip; t_ipOk = true; }
  } else if (WiFi.hostByName(host, t_ip) == 1) {
    t_ipOk = true;
  }
  return t_ipOk;
}

enum { HTTP_NET = -1, HTTP_BIG = -2, HTTP_JSON = -3 };

// GET path, parse the JSON through the filter. Returns the HTTP status, or
// one of the negative codes above.
static int getJson(const Cfg& c, const char* path, JsonDocument& filter, JsonDocument& out) {
  char host[64]; uint16_t port;
  splitHost(c, host, sizeof(host), &port);
  if (!resolve(host)) return HTTP_NET;

  for (int attempt = 0; attempt < 2; attempt++) {
    WiFiClient client;
    HTTPClient http;
    http.setConnectTimeout(PRINT_CONNECT_MS);
    http.setTimeout(PRINT_READ_MS);
    http.setReuse(false);
    http.useHTTP10(true);                      // no chunked body: stream straight into the parser
    if (!http.begin(client, t_ip.toString(), port, path)) return HTTP_NET;
    if (c.key[0]) http.addHeader("X-Api-Key", c.key);
    const bool digest = (c.type == PRINT_TYPE_PRUSA) && c.user[0] && c.pass[0] && !c.key[0];
    if (digest && t_digOk) {
      char cn[17];
      snprintf(cn, sizeof(cn), "%08lx%08lx", (unsigned long)esp_random(), (unsigned long)esp_random());
      char auth[512];
      Digest_Header(t_dig, "GET", path, c.user, c.pass, ++t_nc, cn, auth, sizeof(auth));
      http.addHeader("Authorization", auth);
    }
    static const char* HDR[] = { "WWW-Authenticate" };
    http.collectHeaders(HDR, 1);

    const int code = http.GET();
    if (code <= 0) { http.end(); t_ipOk = false; return HTTP_NET; }
    if (code == 401 && digest && attempt == 0) {
      // First contact, or the nonce went stale: take the new challenge and
      // ask once more. A second 401 means the password is wrong.
      t_digOk = Digest_Parse(http.header("WWW-Authenticate").c_str(), t_dig);
      t_nc = 0;
      http.end();
      if (!t_digOk) return 401;
      continue;
    }
    if (code != 200) { http.end(); return code; }
    const int size = http.getSize();
    if (size > PRINT_MAX_BODY) { http.end(); return HTTP_BIG; }
    out.clear();
    DeserializationError e = deserializeJson(out, http.getStream(),
                                             DeserializationOption::Filter(filter),
                                             DeserializationOption::NestingLimit(12));
    http.end();
    return e ? HTTP_JSON : 200;
  }
  return HTTP_NET;
}

// What a failed request means for the state.
static PrintState failState(int code) {
  if (code == 401 || code == 403) return PS_AUTH;
  if (code == HTTP_NET) return PS_OFFLINE;
  return PS_BADREPLY;                          // 404, 500, too big, not JSON
}

// -----------------------------------------------------------------------------
//  Prusa
// -----------------------------------------------------------------------------
static PrintState pollPrusaLegacy(const Cfg& c, PrintInfo& n) {
  // PrusaLink before /api/v1: OctoPrint-like /api/job and /api/printer.
  JsonDocument f, d;
  f["state"] = true;
  f["progress"]["completion"] = true; f["progress"]["printTime"] = true; f["progress"]["printTimeLeft"] = true;
  f["job"]["file"]["display_name"] = true; f["job"]["file"]["name"] = true;
  int code = getJson(c, "/api/job", f, d);
  if (code != 200) return failState(code);
  String st = d["state"] | "";
  st.toLowerCase();
  n.message[0] = '\0';
  if      (st.indexOf("print") >= 0) n.state = PS_PRINTING;
  else if (st.indexOf("paus") >= 0)  n.state = PS_PAUSED;
  else if (st.indexOf("finish") >= 0) n.state = PS_FINISHED;
  else if (st.indexOf("error") >= 0) { n.state = PS_FAILED; strcpy(n.message, "ERROR"); }
  else n.state = PS_IDLE;
  double comp = d["progress"]["completion"] | -1.0;
  n.pct = comp < 0 ? -1 : (float)(comp <= 1.0 ? comp * 100.0 : comp);
  n.elapsedS = d["progress"]["printTime"] | -1;
  n.remainS  = d["progress"]["printTimeLeft"] | -1;
  const char* fn = d["job"]["file"]["display_name"] | (d["job"]["file"]["name"] | "");
  PrintParse_CleanName(n.file, sizeof(n.file), fn, true);
  JsonDocument f2, p;
  f2["temperature"]["tool0"]["actual"] = true; f2["temperature"]["tool0"]["target"] = true;
  f2["temperature"]["bed"]["actual"] = true;   f2["temperature"]["bed"]["target"] = true;
  if (getJson(c, "/api/printer", f2, p) == 200) {
    n.tNoz  = p["temperature"]["tool0"]["actual"] | NAN;
    n.tNozT = p["temperature"]["tool0"]["target"] | NAN;
    n.tBed  = p["temperature"]["bed"]["actual"] | NAN;
    n.tBedT = p["temperature"]["bed"]["target"] | NAN;
  }
  return n.state;
}

static PrintState pollPrusa(const Cfg& c, PrintInfo& n) {
  if (t_prusaLegacy) return pollPrusaLegacy(c, n);
  JsonDocument f, d;
  JsonObject fp = f["printer"].to<JsonObject>();
  fp["state"] = true; fp["temp_nozzle"] = true; fp["target_nozzle"] = true;
  fp["temp_bed"] = true; fp["target_bed"] = true;
  JsonObject fj = f["job"].to<JsonObject>();
  fj["id"] = true; fj["progress"] = true; fj["time_remaining"] = true; fj["time_printing"] = true;
  const int code = getJson(c, "/api/v1/status", f, d);
  if (code == 404) { t_prusaLegacy = true; return pollPrusaLegacy(c, n); }
  if (code != 200) return failState(code);

  int32_t job = -1;
  PrintParse_PrusaStatus(d.as<JsonObjectConst>(), n, &job);
  if (job >= 0 && job != t_prusaJob) {
    // A new job: its name and material, once.
    JsonDocument f2, j;
    f2["file"]["display_name"] = true; f2["file"]["name"] = true;
    f2["file"]["meta"]["filament_type"] = true; f2["file"]["meta"]["filament used [g]"] = true;
    if (getJson(c, "/api/v1/job", f2, j) == 200) {
      n.material[0] = '\0'; n.filamentG = -1;
      PrintParse_PrusaJob(j.as<JsonObjectConst>(), n);
      t_prusaJob = job;
    }
  } else if (job < 0 && n.state == PS_IDLE) {
    // Back to idle: forget the last job.
    t_prusaJob = -2;
    n.file[0] = '\0'; n.material[0] = '\0'; n.filamentG = -1;
  }
  return n.state;
}

// -----------------------------------------------------------------------------
//  Klipper
// -----------------------------------------------------------------------------
static void urlEncode(const char* s, char* out, size_t cap) {
  static const char* HEX_ = "0123456789ABCDEF";
  size_t n = 0;
  for (; *s && n + 4 < cap; s++) {
    const uint8_t c = (uint8_t)*s;
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~' || c == '/') out[n++] = (char)c;
    else { out[n++] = '%'; out[n++] = HEX_[c >> 4]; out[n++] = HEX_[c & 15]; }
  }
  out[n] = '\0';
}

static PrintState pollKlipper(const Cfg& c, PrintInfo& n) {
  JsonDocument f, d;
  JsonObject ps = f["result"]["status"]["print_stats"].to<JsonObject>();
  ps["state"] = true; ps["filename"] = true; ps["print_duration"] = true;
  ps["total_duration"] = true; ps["message"] = true;
  ps["info"]["current_layer"] = true; ps["info"]["total_layer"] = true;
  JsonObject st = f["result"]["status"];
  st["virtual_sdcard"]["progress"] = true;
  st["display_status"]["progress"] = true;
  st["extruder"]["temperature"] = true; st["extruder"]["target"] = true;
  st["heater_bed"]["temperature"] = true; st["heater_bed"]["target"] = true;
  const int code = getJson(c, "/printer/objects/query?print_stats&virtual_sdcard&display_status&extruder&heater_bed", f, d);
  if (code != 200) return failState(code);

  char file[128]; double dur = 0, prog = 0;
  PrintParse_Moonraker(d.as<JsonObjectConst>(), n, file, sizeof(file), &dur, &prog);
  if (n.state == PS_BADREPLY) return n.state;
  if (file[0] && strcmp(file, t_klipFile) != 0) {
    char enc[200], path[240];
    urlEncode(file, enc, sizeof(enc));
    snprintf(path, sizeof(path), "/server/files/metadata?filename=%s", enc);
    JsonDocument f2, m;
    JsonObject r = f2["result"].to<JsonObject>();
    r["estimated_time"] = true; r["filament_type"] = true; r["filament_name"] = true;
    r["filament_weight_total"] = true; r["layer_count"] = true;
    n.material[0] = '\0'; n.filamentG = -1; t_klipEst = 0;
    if (getJson(c, path, f2, m) == 200) PrintParse_MoonrakerMeta(m.as<JsonObjectConst>(), n, &t_klipEst);
    strncpy(t_klipFile, file, sizeof(t_klipFile) - 1);    // tried once - not again every round
  } else if (!file[0]) {
    t_klipFile[0] = '\0'; t_klipEst = 0;
    n.material[0] = '\0'; n.filamentG = -1;
  }
  n.remainS = file[0] ? PrintParse_MoonrakerRemain(dur, prog, t_klipEst) : -1;
  if (n.state == PS_FINISHED) n.remainS = 0;
  return n.state;
}

// -----------------------------------------------------------------------------
//  The task
// -----------------------------------------------------------------------------
static int16_t tenth(float v) { return isfinite(v) ? (int16_t)lroundf(v * 10.0f) : INT16_MIN; }

static void addSample(const PrintInfo& n) {
  if (!isfinite(n.tNoz) && !isfinite(n.tBed)) return;
  const uint32_t now = millis() / 1000;
  if (s_histN) {
    const Sample& last = s_hist[(s_histHead + PRINT_HIST_N - 1) % PRINT_HIST_N];
    if (now - last.t < PRINT_HIST_STEP_S) return;
  }
  s_hist[s_histHead] = { now, tenth(n.tNoz), tenth(n.tNozT), tenth(n.tBed), tenth(n.tBedT) };
  s_histHead = (s_histHead + 1) % PRINT_HIST_N;
  if (s_histN < PRINT_HIST_N) s_histN++;
}

static uint32_t interval(const PrintInfo& n, bool view) {
  if (n.state == PS_OFFLINE || n.state == PS_UNKNOWN) {
    if (view) return PRINT_POLL_VIEW_OFF_MS;
    return n.fails >= PRINT_POLL_DEAD_AFTER ? PRINT_POLL_DEAD_MS : PRINT_POLL_OFF_MS;
  }
  if (n.state == PS_AUTH || n.state == PS_BADREPLY) return PRINT_POLL_OFF_MS;
  if (view) return PRINT_POLL_VIEW_MS;
  return PrintState_Active(n.state) ? PRINT_POLL_BUSY_MS : PRINT_POLL_IDLE_MS;
}

static void resetForConfig(uint32_t gen) {
  t_gen = gen;
  t_baseline = false;
  t_lastOnline = PS_UNKNOWN;
  t_lastEvFile[0] = '\0'; t_lastEv = PE_NONE;
  t_prusaJob = -2; t_prusaLegacy = false;
  t_klipFile[0] = '\0'; t_klipEst = 0;
  t_digOk = false; t_nc = 0;
  t_ipOk = false;
  lock();
  const uint8_t type = s_cfg.type;
  const bool on = s_cfg.on && s_cfg.host[0];
  s_info = PrintInfo();
  s_info.type = type;
  // "Checking..." rather than "set up the printer" for the few seconds the
  // first answer takes.
  s_info.state = on ? PS_UNKNOWN : PS_DISABLED;
  s_histN = 0; s_histHead = 0;
  s_event = PE_NONE;
  unlock();
}

static void task(void*) {
  for (;;) {
    Cfg c;
    lock(); c = s_cfg; unlock();

    if (c.gen != t_gen) resetForConfig(c.gen);

    if (!c.on || !c.host[0]) {
      lock(); s_info.state = PS_DISABLED; s_info.nextPollMs = 0; unlock();
      ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(5000));
      continue;
    }
    // No network, or a firmware upload in progress: wait without counting it
    // against the printer.
    if (WiFi.status() != WL_CONNECTED || WebConfig_UpdateBusy()) {
      ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(2000));
      continue;
    }

    PrintInfo n;
    lock(); n = s_info; unlock();
    n.type = c.type;
    const PrintState got = (c.type == PRINT_TYPE_KLIPPER) ? pollKlipper(c, n) : pollPrusa(c, n);
    n.state = got;
    const uint32_t now = millis();

    PrintEvent ev = PE_NONE;
    if (PrintState_Online(got)) {
      n.fails = 0;
      n.updatedMs = now;
      // The first answer after a start or a config change is only a starting
      // point: a Prusa still saying FINISHED from a print hours ago must not
      // ring again because the device was restarted.
      if (t_baseline) ev = PrintParse_Event(t_lastOnline, got);
      t_baseline = true;
      // Between two online answers the printer may have been unreachable for
      // a while; comparing against the last ONLINE state means a finish that
      // happened during a WiFi hiccup is still noticed.
      t_lastOnline = got;
      // A new job is running: the next finish is news again, even for the
      // same file printed a second time.
      if (got == PS_PRINTING) t_lastEv = PE_NONE;
      if ((ev == PE_FINISHED || ev == PE_FAILED) && ev == t_lastEv && !strcmp(n.file, t_lastEvFile))
        ev = PE_NONE;                          // the same job, the same news - once is enough
      if (ev == PE_FINISHED || ev == PE_FAILED) {
        t_lastEv = ev; strncpy(t_lastEvFile, n.file, sizeof(t_lastEvFile) - 1);
      }
    } else {
      if (n.fails < 0xFFFF) n.fails++;
      // A password problem is not "offline" - keep what we last knew on show
      // for the other cases, but the state tells the screen what is going on.
    }

    lock();
    // The config may have changed while we were waiting for the printer.
    if (s_cfg.gen == t_gen) {
      n.nextPollMs = now + interval(n, s_view);
      s_info = n;
      if (PrintState_Online(got)) addSample(n);
      if (ev != PE_NONE) {
        s_event = ev;
        strncpy(s_eventMsg, n.message, sizeof(s_eventMsg) - 1);
        s_eventMsg[sizeof(s_eventMsg) - 1] = '\0';
      }
    }
    const uint32_t wait = interval(n, s_view);
    unlock();
    // Woken early when the Print screen comes up or the settings change.
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(wait));
  }
}

// -----------------------------------------------------------------------------
//  The rest of the firmware
// -----------------------------------------------------------------------------
static void copyCfg() {
  lock();
  s_cfg.on = Settings_PrintOn();
  s_cfg.type = Settings_PrintType();
  strncpy(s_cfg.host, Settings_PrintHost(), sizeof(s_cfg.host) - 1);
  strncpy(s_cfg.user, Settings_PrintUser(), sizeof(s_cfg.user) - 1);
  strncpy(s_cfg.key,  Settings_PrintKey(),  sizeof(s_cfg.key) - 1);
  strncpy(s_cfg.pass, Settings_PrintPass(), sizeof(s_cfg.pass) - 1);
  s_cfg.gen = Settings_PrintGen();
  unlock();
}

void PrintWatch_Begin() {
  if (s_task) return;
  s_mtx = xSemaphoreCreateMutex();
  copyCfg();
  s_cfgGenSeen = s_cfg.gen;
  // Core 0, beside the WiFi stack, at a low priority: the display loop on
  // core 1 never waits for it. 10 kB is plenty for HTTPClient + a filtered
  // JsonDocument (the documents themselves live on the heap).
  xTaskCreatePinnedToCore(task, "printwatch", 10240, nullptr, 1, &s_task, 0);
}

void PrintWatch_Tick(bool screenShown) {
  if (!s_task) return;
  bool wake = false;
  if (Settings_PrintGen() != s_cfgGenSeen) {
    s_cfgGenSeen = Settings_PrintGen();
    copyCfg();
    wake = true;
  }
  if (screenShown && !s_view) wake = true;      // just came up: fresh numbers now
  s_view = screenShown;
  if (wake) xTaskNotifyGive(s_task);
}

bool PrintWatch_Get(PrintInfo* out) {
  if (!s_mtx || !out) return false;
  lock(); *out = s_info; unlock();
  return true;
}

PrintEvent PrintWatch_TakeEvent(char* msg, size_t cap) {
  if (!s_mtx) return PE_NONE;
  lock();
  const PrintEvent e = s_event;
  s_event = PE_NONE;
  if (msg && cap) { strncpy(msg, s_eventMsg, cap - 1); msg[cap - 1] = '\0'; }
  unlock();
  return e;
}

int PrintWatch_History(int16_t* noz, int16_t* nozT, int16_t* bed, int16_t* bedT,
                       uint16_t* ageS, int max) {
  if (!s_mtx) return 0;
  lock();
  const int n = s_histN < max ? s_histN : max;
  const int start = (s_histHead + PRINT_HIST_N - n) % PRINT_HIST_N;
  const uint32_t now = millis() / 1000;
  for (int i = 0; i < n; i++) {
    const Sample& s = s_hist[(start + i) % PRINT_HIST_N];
    if (noz) noz[i] = s.noz;
    if (nozT) nozT[i] = s.nozT;
    if (bed) bed[i] = s.bed;
    if (bedT) bedT[i] = s.bedT;
    if (ageS) ageS[i] = (uint16_t)((now - s.t) > 65535 ? 65535 : (now - s.t));
  }
  unlock();
  return n;
}

const char* PrintState_Name(PrintState s) {
  switch (s) {
    case PS_DISABLED:  return "disabled";
    case PS_UNKNOWN:   return "unknown";
    case PS_OFFLINE:   return "offline";
    case PS_AUTH:      return "auth";
    case PS_BADREPLY:  return "bad_reply";
    case PS_IDLE:      return "idle";
    case PS_PREPARE:   return "preparing";
    case PS_PRINTING:  return "printing";
    case PS_PAUSED:    return "paused";
    case PS_FINISHED:  return "finished";
    case PS_FAILED:    return "failed";
    case PS_CANCELLED: return "cancelled";
  }
  return "unknown";
}

void PrintWatch_StatusText(char* out, size_t cap) {
  PrintInfo i;
  if (!PrintWatch_Get(&i)) { snprintf(out, cap, "-"); return; }
  const long next = i.nextPollMs ? ((long)(i.nextPollMs - millis()) / 1000) : -1;
  switch (i.state) {
    case PS_DISABLED: snprintf(out, cap, "vypnuto"); break;
    case PS_UNKNOWN:  snprintf(out, cap, "zjistuji..."); break;
    case PS_OFFLINE:  snprintf(out, cap, "nedostupna (%ux), dalsi pokus za %ld s", i.fails, next < 0 ? 0 : next); break;
    case PS_AUTH:     snprintf(out, cap, "chybi nebo nesedi heslo / API klic"); break;
    case PS_BADREPLY: snprintf(out, cap, "odpovida, ale ne jako %s", i.type == PRINT_TYPE_KLIPPER ? "Moonraker" : "PrusaLink"); break;
    case PS_PRINTING: case PS_PAUSED: case PS_PREPARE:
      snprintf(out, cap, "%s %d %%, zbyva %ld min", PrintState_Name(i.state),
               i.pct < 0 ? 0 : (int)i.pct, i.remainS < 0 ? -1L : (long)(i.remainS / 60));
      break;
    default: snprintf(out, cap, "%s", PrintState_Name(i.state)); break;
  }
}
