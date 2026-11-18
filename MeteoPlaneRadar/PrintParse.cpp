// =============================================================================
//  MeteoPlaneRadar
//  3D printer watch (0.9.0) - parsing. See PrintParse.h.
//
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
// =============================================================================
#include "PrintParse.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

// -----------------------------------------------------------------------------
//  Shared
// -----------------------------------------------------------------------------
void PrintParse_CleanName(char* dst, size_t cap, const char* src, bool stripExt) {
  if (!cap) return;
  size_t n = 0;
  const uint8_t* s = (const uint8_t*)(src ? src : "");
  while (*s) {
    uint8_t c = *s;
    int len = (c < 0x80) ? 1 : ((c & 0xE0) == 0xC0) ? 2 : ((c & 0xF0) == 0xE0) ? 3
            : ((c & 0xF8) == 0xF0) ? 4 : 0;
    bool ok = len > 0;
    for (int i = 1; ok && i < len; i++) ok = (s[i] & 0xC0) == 0x80;
    if (!ok) { s++; continue; }                         // broken byte: drop it
    if (len == 1 && (c < 0x20 || c == 0x7F)) { s++; continue; }   // control
    if (n + len >= cap) break;                          // would not fit whole
    memcpy(dst + n, s, len);
    n += len; s += len;
  }
  dst[n] = '\0';
  if (stripExt) {
    static const char* const EXT[] = { ".bgcode", ".gcode", ".gco", ".g", ".3mf" };
    for (const char* e : EXT) {
      const size_t el = strlen(e);
      if (n > el && strcasecmp(dst + n - el, e) == 0) { dst[n - el] = '\0'; break; }
    }
  }
}

PrintEvent PrintParse_Event(PrintState from, PrintState to) {
  if (from == to) return PE_NONE;
  const bool wasActive = PrintState_Active(from);
  if (to == PS_FINISHED && wasActive) return PE_FINISHED;
  if (to == PS_FAILED && from != PS_FAILED && PrintState_Online(from)) return PE_FAILED;
  if (to == PS_PAUSED && (from == PS_PRINTING || from == PS_PREPARE)) return PE_PAUSED;
  return PE_NONE;
}

static float numOr(JsonVariantConst v, float dflt) {
  return v.is<float>() || v.is<int>() ? v.as<float>() : dflt;
}
// Durations and progress: a float would round 3500.2 s to 3500.19995.
static double numOrD(JsonVariantConst v, double dflt) {
  return v.is<double>() || v.is<long>() ? v.as<double>() : dflt;
}

// "PLA;PETG" (multi-material) -> "PLA, PETG"; ["PLA","PETG"] the same.
static void material(JsonVariantConst v, char* dst, size_t cap) {
  dst[0] = '\0';
  char tmp[64] = "";
  if (v.is<const char*>()) {
    strncpy(tmp, v.as<const char*>(), sizeof(tmp) - 1);
  } else if (v.is<JsonArrayConst>()) {
    size_t n = 0;
    for (JsonVariantConst e : v.as<JsonArrayConst>()) {
      const char* s = e | "";
      if (!*s) continue;
      int w = snprintf(tmp + n, sizeof(tmp) - n, "%s%s", n ? ";" : "", s);
      if (w < 0 || (size_t)w >= sizeof(tmp) - n) break;
      n += w;
    }
  }
  // Collapse duplicates ("PLA;PLA;PLA" from a multi-tool profile).
  char out[64] = "";
  char* save = nullptr;
  for (char* t = strtok_r(tmp, ";,", &save); t; t = strtok_r(nullptr, ";,", &save)) {
    while (*t == ' ') t++;
    if (!*t || strstr(out, t)) continue;
    if (*out) strncat(out, ", ", sizeof(out) - strlen(out) - 1);
    strncat(out, t, sizeof(out) - strlen(out) - 1);
  }
  PrintParse_CleanName(dst, cap, out, false);
}

// -----------------------------------------------------------------------------
//  PrusaLink
// -----------------------------------------------------------------------------
void PrintParse_PrusaStatus(JsonObjectConst root, PrintInfo& io, int32_t* jobId) {
  JsonObjectConst pr = root["printer"];
  JsonObjectConst job = root["job"];
  const char* st = pr["state"] | "";
  PrintState s;
  io.message[0] = '\0';
  if      (!strcasecmp(st, "PRINTING")) s = PS_PRINTING;
  else if (!strcasecmp(st, "PAUSED"))   s = PS_PAUSED;
  else if (!strcasecmp(st, "ATTENTION")) {
    s = PS_PAUSED;                         // waits for a human: filament, crash...
    strcpy(io.message, "ATTENTION");      // translated by the screen
  }
  else if (!strcasecmp(st, "FINISHED")) s = PS_FINISHED;
  else if (!strcasecmp(st, "STOPPED"))  s = PS_CANCELLED;
  else if (!strcasecmp(st, "ERROR"))    { s = PS_FAILED; strcpy(io.message, "ERROR"); }
  else if (!strcasecmp(st, "BUSY"))     s = PS_PREPARE;
  else if (!*st)                        s = PS_BADREPLY;
  else                                  s = PS_IDLE;   // IDLE, READY
  io.state = s;

  io.tNoz  = numOr(pr["temp_nozzle"], NAN);
  io.tNozT = numOr(pr["target_nozzle"], NAN);
  io.tBed  = numOr(pr["temp_bed"], NAN);
  io.tBedT = numOr(pr["target_bed"], NAN);

  if (!job.isNull()) {
    if (jobId) *jobId = job["id"] | -1;
    io.pct      = numOr(job["progress"], -1);
    io.remainS  = job["time_remaining"] | -1;
    io.elapsedS = job["time_printing"] | -1;
  } else {
    if (jobId) *jobId = -1;
    // FINISHED keeps the last job's numbers - the screen shows the total time.
    if (!(s == PS_FINISHED || s == PS_FAILED || s == PS_CANCELLED)) {
      io.pct = -1; io.remainS = -1; io.elapsedS = -1;
    }
  }
  // PrusaLink does not report layers.
  io.layer = -1; io.layers = -1;
}

void PrintParse_PrusaJob(JsonObjectConst root, PrintInfo& io) {
  JsonObjectConst f = root["file"];
  const char* name = f["display_name"] | (f["name"] | "");
  PrintParse_CleanName(io.file, sizeof(io.file), name, true);
  JsonObjectConst meta = f["meta"];
  material(meta["filament_type"], io.material, sizeof(io.material));
  io.filamentG = numOr(meta["filament used [g]"], -1);
}

// -----------------------------------------------------------------------------
//  Moonraker
// -----------------------------------------------------------------------------
void PrintParse_Moonraker(JsonObjectConst root, PrintInfo& io,
                          char* file, size_t fileCap, double* printDur, double* progress) {
  JsonObjectConst st = root["result"]["status"];
  JsonObjectConst ps = st["print_stats"];
  if (ps.isNull()) { io.state = PS_BADREPLY; return; }
  const char* s = ps["state"] | "";
  io.message[0] = '\0';
  if      (!strcmp(s, "printing"))  io.state = PS_PRINTING;
  else if (!strcmp(s, "paused"))    io.state = PS_PAUSED;
  else if (!strcmp(s, "complete"))  io.state = PS_FINISHED;
  else if (!strcmp(s, "cancelled")) io.state = PS_CANCELLED;
  else if (!strcmp(s, "error")) {
    io.state = PS_FAILED;
    PrintParse_CleanName(io.message, sizeof(io.message), ps["message"] | "", false);
  }
  else io.state = PS_IDLE;                 // standby

  const char* fn = ps["filename"] | "";
  if (file) { strncpy(file, fn, fileCap - 1); file[fileCap - 1] = '\0'; }
  // Shown without the folder.
  const char* base = strrchr(fn, '/');
  PrintParse_CleanName(io.file, sizeof(io.file), base ? base + 1 : fn, true);

  double prog = numOrD(st["display_status"]["progress"], 0);
  if (prog <= 0) prog = numOrD(st["virtual_sdcard"]["progress"], 0);
  if (progress) *progress = prog;
  if (printDur) *printDur = numOrD(ps["print_duration"], 0);
  io.pct = (*fn) ? (float)(prog * 100.0) : -1;
  // Wall time since the start, pauses included - what "how long has it been
  // printing" means to a person standing next to it.
  io.elapsedS = (*fn) ? (int32_t)numOrD(ps["total_duration"], 0) : -1;

  io.tNoz  = numOr(st["extruder"]["temperature"], NAN);
  io.tNozT = numOr(st["extruder"]["target"], NAN);
  io.tBed  = numOr(st["heater_bed"]["temperature"], NAN);
  io.tBedT = numOr(st["heater_bed"]["target"], NAN);

  JsonObjectConst info = ps["info"];
  io.layer  = info["current_layer"].is<int>() ? (int16_t)info["current_layer"].as<int>() : -1;
  io.layers = info["total_layer"].is<int>() ? (int16_t)info["total_layer"].as<int>() : -1;
}

void PrintParse_MoonrakerMeta(JsonObjectConst root, PrintInfo& io, double* estTime) {
  JsonObjectConst r = root["result"];
  if (estTime) *estTime = numOrD(r["estimated_time"], 0);
  material(r["filament_type"], io.material, sizeof(io.material));
  if (!io.material[0]) material(r["filament_name"], io.material, sizeof(io.material));
  io.filamentG = numOr(r["filament_weight_total"], -1);
  if (io.layers < 0 && r["layer_count"].is<int>()) io.layers = (int16_t)r["layer_count"].as<int>();
}

int32_t PrintParse_MoonrakerRemain(double printDur, double progress, double estTime) {
  if (progress >= 0.05 && printDur > 0) return (int32_t)(printDur / progress - printDur);
  if (estTime > 0) { double r = estTime - printDur; return r > 0 ? (int32_t)r : 0; }
  if (progress > 0.005 && printDur > 0) return (int32_t)(printDur / progress - printDur);
  return -1;
}

// -----------------------------------------------------------------------------
//  MD5 (RFC 1321) - small and self-contained, so the same code runs on the
//  ESP32 and in the PC tests.
// -----------------------------------------------------------------------------
namespace {
struct Md5 {
  uint32_t a = 0x67452301, b = 0xefcdab89, c = 0x98badcfe, d = 0x10325476;
  uint8_t buf[64]; uint64_t len = 0; size_t fill = 0;
  static uint32_t rol(uint32_t x, int s) { return (x << s) | (x >> (32 - s)); }
  void block(const uint8_t* p) {
    static const uint32_t K[64] = {
      0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,0xf57c0faf,0x4787c62a,0xa8304613,0xfd469501,
      0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be,0x6b901122,0xfd987193,0xa679438e,0x49b40821,
      0xf61e2562,0xc040b340,0x265e5a51,0xe9b6c7aa,0xd62f105d,0x02441453,0xd8a1e681,0xe7d3fbc8,
      0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,0xa9e3e905,0xfcefa3f8,0x676f02d9,0x8d2a4c8a,
      0xfffa3942,0x8771f681,0x6d9d6122,0xfde5380c,0xa4beea44,0x4bdecfa9,0xf6bb4b60,0xbebfbc70,
      0x289b7ec6,0xeaa127fa,0xd4ef3085,0x04881d05,0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665,
      0xf4292244,0x432aff97,0xab9423a7,0xfc93a039,0x655b59c3,0x8f0ccc92,0xffeff47d,0x85845dd1,
      0x6fa87e4f,0xfe2ce6e0,0xa3014314,0x4e0811a1,0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391 };
    static const int R[64] = { 7,12,17,22,7,12,17,22,7,12,17,22,7,12,17,22,
                               5,9,14,20,5,9,14,20,5,9,14,20,5,9,14,20,
                               4,11,16,23,4,11,16,23,4,11,16,23,4,11,16,23,
                               6,10,15,21,6,10,15,21,6,10,15,21,6,10,15,21 };
    uint32_t w[16];
    for (int i = 0; i < 16; i++)
      w[i] = p[i * 4] | (p[i * 4 + 1] << 8) | (p[i * 4 + 2] << 16) | ((uint32_t)p[i * 4 + 3] << 24);
    uint32_t A = a, B = b, C = c, D = d;
    for (int i = 0; i < 64; i++) {
      uint32_t f; int g;
      if (i < 16)      { f = (B & C) | (~B & D); g = i; }
      else if (i < 32) { f = (D & B) | (~D & C); g = (5 * i + 1) % 16; }
      else if (i < 48) { f = B ^ C ^ D;          g = (3 * i + 5) % 16; }
      else             { f = C ^ (B | ~D);       g = (7 * i) % 16; }
      const uint32_t t = D; D = C; C = B;
      B = B + rol(A + f + K[i] + w[g], R[i]);
      A = t;
    }
    a += A; b += B; c += C; d += D;
  }
  void add(const uint8_t* p, size_t n) {
    len += n;
    while (n--) { buf[fill++] = *p++; if (fill == 64) { block(buf); fill = 0; } }
  }
  void hex(char out[33]) {
    const uint64_t bits = len * 8;
    uint8_t pad = 0x80; add(&pad, 1);
    pad = 0; while (fill != 56) add(&pad, 1);
    uint8_t L[8]; for (int i = 0; i < 8; i++) L[i] = (uint8_t)(bits >> (8 * i));
    add(L, 8);
    const uint32_t v[4] = { a, b, c, d };
    for (int i = 0; i < 16; i++) snprintf(out + i * 2, 3, "%02x", (unsigned)((v[i / 4] >> (8 * (i % 4))) & 0xFF));
  }
};
}  // namespace

void Md5Hex(const char* s, size_t n, char out[33]) {
  Md5 m; m.add((const uint8_t*)s, n); m.hex(out);
}

// key="value" or key=value, comma separated, after "Digest ".
static bool digestParam(const char* h, const char* key, char* out, size_t cap) {
  const size_t kl = strlen(key);
  for (const char* p = h; (p = strstr(p, key)); p += kl) {
    if (p != h && p[-1] != ' ' && p[-1] != ',') continue;
    const char* v = p + kl;
    while (*v == ' ') v++;
    if (*v != '=') continue;
    v++;
    while (*v == ' ') v++;
    size_t n = 0;
    if (*v == '"') { v++; while (*v && *v != '"' && n + 1 < cap) out[n++] = *v++; }
    else           { while (*v && *v != ',' && *v != ' ' && n + 1 < cap) out[n++] = *v++; }
    out[n] = '\0';
    return true;
  }
  return false;
}

bool Digest_Parse(const char* h, DigestChallenge& c) {
  if (!h || strncasecmp(h, "Digest", 6) != 0) return false;
  const char* p = h + 6;
  c = DigestChallenge();
  if (!digestParam(p, "realm", c.realm, sizeof(c.realm))) return false;
  if (!digestParam(p, "nonce", c.nonce, sizeof(c.nonce))) return false;
  digestParam(p, "opaque", c.opaque, sizeof(c.opaque));
  char qop[32] = "";
  if (digestParam(p, "qop", qop, sizeof(qop))) c.qopAuth = strstr(qop, "auth") != nullptr;
  return true;
}

void Digest_Header(const DigestChallenge& c, const char* method, const char* uri,
                   const char* user, const char* pass, uint32_t nc, const char* cnonce,
                   char* out, size_t cap) {
  char a1[256], a2[256], h1[33], h2[33], resp[33], buf[512];
  snprintf(a1, sizeof(a1), "%s:%s:%s", user, c.realm, pass);
  snprintf(a2, sizeof(a2), "%s:%s", method, uri);
  Md5Hex(a1, strlen(a1), h1);
  Md5Hex(a2, strlen(a2), h2);
  char ncs[9];
  snprintf(ncs, sizeof(ncs), "%08x", (unsigned)nc);
  if (c.qopAuth) snprintf(buf, sizeof(buf), "%s:%s:%s:%s:auth:%s", h1, c.nonce, ncs, cnonce, h2);
  else           snprintf(buf, sizeof(buf), "%s:%s:%s", h1, c.nonce, h2);
  Md5Hex(buf, strlen(buf), resp);
  int n = snprintf(out, cap, "Digest username=\"%s\", realm=\"%s\", nonce=\"%s\", uri=\"%s\", response=\"%s\"",
                   user, c.realm, c.nonce, uri, resp);
  if (c.qopAuth && n > 0 && (size_t)n < cap)
    n += snprintf(out + n, cap - n, ", qop=auth, nc=%s, cnonce=\"%s\"", ncs, cnonce);
  if (c.opaque[0] && n > 0 && (size_t)n < cap)
    snprintf(out + n, cap - n, ", opaque=\"%s\"", c.opaque);
}
