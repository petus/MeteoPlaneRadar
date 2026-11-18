// =============================================================================
//  MeteoPlaneRadar
//  Screen: 3D print (0.9.0). See ScreenPrint.h.
//
//  Layout, top to bottom, everything inside the ring:
//    y  30  clock + outside temperature (shared status line, above the ring)
//    y  96  job name                         size 1
//    y 116  state and percent                size 2
//    y 150  remaining time, h:mm             size 5
//    y 196  "left" / "total"                 size 1
//    y 218  labels: printing for | done at   size 1
//    y 232  values                           size 2
//    y 258  material, grams, layer           size 1
//    y 278  temperature graph                55 px
//    y 340  nozzle actual/target             size 1
//    y 354  bed actual/target                size 1
//
//  Every line is fitted to the width the ring leaves at its height and cut
//  with "..." when it does not fit - a long file name or a long translation
//  can not run into the ring. tools/textcheck draws this screen in every
//  state, language and typeface and checks no two texts touch.
//
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
// =============================================================================
#include "ScreenPrint.h"
#include "PrintWatch.h"
#include "Settings.h"
#include "Layout.h"
#include "Lang.h"
#include "UI.h"
#include "AAFont.h"
#include "Display_ST7701.h"
#include "Config.h"
#include <math.h>
#include <time.h>
#include <string.h>

#define CX (LCD_WIDTH / 2)
#define CY (LCD_HEIGHT / 2)
#define R_OUT 188
#define R_IN  172
#define R_TEXT 162          // text keeps this far from the centre at most

#define FILE_Y   96
#define STATE_Y  116
#define BIG_Y    150
#define BIGL_Y   196
#define COLL_Y   218
#define COLV_Y   232
#define INFO_Y   258
#define GR_Y0    278
#define GR_Y1    333
#define GR_X0    142
#define GR_X1    338
#define TN_Y     340
#define TB_Y     354
#define COL_DX   70         // the two columns sit this far left/right of centre

#define C_NOZ  0xFC00       // orange
#define C_BEDC 0x05FF       // cyan
#define C_TGT  0x4208       // target lines, dim

static PrintInfo s_i;
static bool s_alert = false;
static PrintEvent s_alertEv = PE_NONE;
static char s_alertMsg[64] = "";
static bool s_alertBlink = true;
static bool s_phase = false;
static unsigned long s_lastSecond = 0;
static uint32_t s_lastUpdated = 0;
static PrintState s_lastState = PS_DISABLED;

// --- text fitting ---------------------------------------------------------------
static int halfAt(int y) {
  const int dy = abs(y - CY);
  if (dy >= R_TEXT) return 0;
  return (int)sqrtf((float)(R_TEXT * R_TEXT - dy * dy));
}
// Room for a line whose top is y, as the narrower of its top and bottom edge.
static int roomAt(int y, uint8_t size) {
  const int a = halfAt(y), b = halfAt(y + 8 * size);
  return 2 * (a < b ? a : b);
}

// Cut s (UTF-8) until it fits w pixels, ending in "..." when cut.
static void fit(char* s, uint8_t size, int w) {
  if (Layout_TextW(s, size) <= w) return;
  size_t n = strlen(s);
  while (n > 0) {
    do { n--; } while (n > 0 && ((uint8_t)s[n] & 0xC0) == 0x80);   // whole characters
    while (n > 0 && s[n - 1] == ' ') n--;
    char tmp[96];
    if (n + 4 > sizeof(tmp)) continue;
    memcpy(tmp, s, n); strcpy(tmp + n, "...");
    if (Layout_TextW(tmp, size) <= w) { strcpy(s, tmp); return; }
  }
  strcpy(s, "...");
}

static void line(const char* text, int y, uint16_t col, uint8_t size, int maxW = 0) {
  char buf[96];
  strncpy(buf, text, sizeof(buf) - 1); buf[sizeof(buf) - 1] = '\0';
  int w = roomAt(y, size);
  if (maxW > 0 && maxW < w) w = maxW;
  fit(buf, size, w);
  UI_TextCentered(buf, y, col, size);
}
static void lineAt(const char* text, int cx, int y, uint16_t col, uint8_t size, int maxW) {
  char buf[64];
  strncpy(buf, text, sizeof(buf) - 1); buf[sizeof(buf) - 1] = '\0';
  fit(buf, size, maxW);
  UI_TextCenteredIn(buf, cx - maxW / 2, maxW, y, col, size);
}

// --- numbers --------------------------------------------------------------------
// A duration, written so it cannot be mistaken for a time of day: "16h 05m",
// "45 min". (The clock time "done at" is the only thing shown as 18:42.)
static void hm(int32_t s, char* out, size_t cap) {
  if (s < 0) { snprintf(out, cap, "--"); return; }
  const long m = (s + 30) / 60;
  if (m >= 60) snprintf(out, cap, "%ldh %02ldm", m / 60, m % 60);
  else         snprintf(out, cap, "%ld min", m);
}

static uint16_t stateColor(PrintState s) {
  switch (s) {
    case PS_PRINTING: return C_CYAN;
    case PS_PREPARE:  return C_YELLOW;
    case PS_PAUSED:   return C_ORANGE;
    case PS_FINISHED: return C_GREEN;
    case PS_FAILED:   return C_RED;
    default:          return C_GRAY;
  }
}

static const char* stateText(PrintState s) {
  switch (s) {
    case PS_PRINTING:  return T(S_P_PRINTING);
    case PS_PAUSED:    return T(S_P_PAUSED);
    case PS_PREPARE:   return T(S_P_PREPARE);
    case PS_FINISHED:  return T(S_P_FINISHED);
    case PS_FAILED:    return T(S_P_FAILED);
    case PS_CANCELLED: return T(S_P_CANCELLED);
    default:           return T(S_P_IDLE);
  }
}

// The printer's own words, or ours for the two codes PrintParse leaves.
static const char* messageText(const char* m) {
  if (!strcmp(m, "ATTENTION")) return T(S_P_ATTENTION);
  if (!strcmp(m, "ERROR"))     return T(S_P_ERR_PRINTER);
  return m;
}

// --- pieces ---------------------------------------------------------------------
// Pixel by pixel over the annulus: a thin ring drawn from radial lines shows
// pinholes, and this is only ~18 000 pixels. Clockwise from twelve o'clock.
static void ring(float frac, uint16_t col) {
  if (frac > 1) frac = 1;
  const float lim = (frac > 0.001f) ? frac * 2.0f * (float)M_PI : -1.0f;
  const int ro2 = R_OUT * R_OUT, ri2 = R_IN * R_IN;
  for (int dy = -R_OUT; dy <= R_OUT; dy++) {
    const int xo = (int)sqrtf((float)(ro2 - dy * dy));
    const int xi2 = ri2 - dy * dy;
    const int xi = xi2 > 0 ? (int)ceilf(sqrtf((float)xi2)) : 0;
    for (int dx = -xo; dx <= xo; dx++) {
      if (dx > -xi && dx < xi) { dx = xi - 1; continue; }      // the hole
      // Angle from twelve o'clock, clockwise, 0..2pi.
      float a = atan2f((float)dx, (float)-dy);
      if (a < 0) a += 2.0f * (float)M_PI;
      gfx->drawPixel(CX + dx, CY + dy, a <= lim ? col : C_DKGRAY);
    }
  }
}

static void temps() {
  char b[48];
  const char* dg = "\xF8" "C";
  if (isfinite(s_i.tNoz)) {
    if (isfinite(s_i.tNozT) && s_i.tNozT > 0)
      snprintf(b, sizeof(b), "%s %.0f / %.0f %s", T(S_P_NOZZLE), s_i.tNoz, s_i.tNozT, dg);
    else snprintf(b, sizeof(b), "%s %.0f %s", T(S_P_NOZZLE), s_i.tNoz, dg);
    line(b, TN_Y, C_NOZ, 1);
  }
  if (isfinite(s_i.tBed)) {
    if (isfinite(s_i.tBedT) && s_i.tBedT > 0)
      snprintf(b, sizeof(b), "%s %.0f / %.0f %s", T(S_P_BED), s_i.tBed, s_i.tBedT, dg);
    else snprintf(b, sizeof(b), "%s %.0f %s", T(S_P_BED), s_i.tBed, dg);
    line(b, TB_Y, C_BEDC, 1);
  }
}

// The last half hour of both temperatures, targets as dim lines underneath.
static void graph() {
  static int16_t n0[PRINT_HIST_N], n1[PRINT_HIST_N], b0[PRINT_HIST_N], b1[PRINT_HIST_N];
  static uint16_t age[PRINT_HIST_N];
  const int n = PrintWatch_History(n0, n1, b0, b1, age, PRINT_HIST_N);
  if (n < 2) return;
  const int span = PRINT_HIST_N * PRINT_HIST_STEP_S;          // 30 min
  int16_t hi = 500;                                          // 50 degC at least
  for (int i = 0; i < n; i++) {
    const int16_t v[4] = { n0[i], n1[i], b0[i], b1[i] };
    for (int16_t x : v) if (x != INT16_MIN && x > hi) hi = x;
  }
  hi = (int16_t)(((hi + 10 + 499) / 500) * 500);             // round up to 50 degC
  const int W = GR_X1 - GR_X0, H = GR_Y1 - GR_Y0;
  gfx->drawFastHLine(GR_X0, GR_Y1, W, C_DKGRAY);
  gfx->drawFastHLine(GR_X0, GR_Y0, W, C_DKGRAY);
  auto X = [&](int i) { const int a = age[i] > span ? span : age[i]; return GR_X1 - (int)((long)a * W / span); };
  auto Y = [&](int16_t v) { if (v < 0) v = 0; return GR_Y1 - (int)((long)v * H / hi); };
  auto plot = [&](const int16_t* v, uint16_t col) {
    for (int i = 1; i < n; i++) {
      if (v[i - 1] == INT16_MIN || v[i] == INT16_MIN) continue;
      if (age[i - 1] > span) continue;
      gfx->drawLine(X(i - 1), Y(v[i - 1]), X(i), Y(v[i]), col);
    }
  };
  plot(n1, C_TGT);
  plot(b1, C_TGT);
  plot(b0, C_BEDC);
  plot(n0, C_NOZ);
}

// Material, grams and layer - as many as fit, most useful first.
static void info() {
  char b[80] = "";
  if (s_i.material[0]) strncpy(b, s_i.material, sizeof(b) - 1);
  if (s_i.filamentG > 0) {
    char g[16]; snprintf(g, sizeof(g), "%.0f g", s_i.filamentG);
    if (*b) strncat(b, " - ", sizeof(b) - strlen(b) - 1);
    strncat(b, g, sizeof(b) - strlen(b) - 1);
  }
  if (s_i.layer >= 0 && s_i.layers > 0) {
    char l[32]; snprintf(l, sizeof(l), "%s %d/%d", T(S_P_LAYER), s_i.layer, s_i.layers);
    char t[128]; snprintf(t, sizeof(t), "%s%s%s", b, *b ? " - " : "", l);
    if (Layout_TextW(t, 1) <= roomAt(INFO_Y, 1)) strncpy(b, t, sizeof(b) - 1);
  }
  if (*b) line(b, INFO_Y, C_GRAY, 1);
}

static void jobView() {
  const PrintState s = s_i.state;
  const uint16_t col = stateColor(s);
  float frac = -1;
  if (s_i.elapsedS >= 0 && s_i.remainS >= 0 && s_i.elapsedS + s_i.remainS > 0)
    frac = (float)s_i.elapsedS / (float)(s_i.elapsedS + s_i.remainS);
  else if (s_i.pct >= 0) frac = s_i.pct / 100.0f;
  if (s == PS_FINISHED) frac = 1;
  ring(frac, col);

  // Paused because the printer wants someone (filament, Prusa ATTENTION):
  // that says more than the file name, so it takes its line.
  if (s == PS_PAUSED && s_i.message[0]) line(messageText(s_i.message), FILE_Y, C_ORANGE, 1);
  else if (s_i.file[0]) line(s_i.file, FILE_Y, C_WHITE, 1);

  char b[64];
  if (s_i.pct >= 0 && s != PS_FINISHED) snprintf(b, sizeof(b), "%s %d %%", stateText(s), (int)(s_i.pct + 0.5f));
  else snprintf(b, sizeof(b), "%s", stateText(s));
  line(b, STATE_Y, col, 2);

  const bool done = (s == PS_FINISHED || s == PS_FAILED || s == PS_CANCELLED);
  hm(done ? s_i.elapsedS : s_i.remainS, b, sizeof(b));
  line(b, BIG_Y, C_WHITE, 5);
  line(done ? T(S_P_TOTAL) : T(S_P_REMAIN), BIGL_Y, C_GRAY, 1);

  if (!done) {
    lineAt(T(S_P_ELAPSED), CX - COL_DX, COLL_Y, C_GRAY, 1, 2 * COL_DX - 10);
    hm(s_i.elapsedS, b, sizeof(b));
    lineAt(b, CX - COL_DX, COLV_Y, C_WHITE, 2, 2 * COL_DX - 10);
    lineAt(T(S_P_DONE_AT), CX + COL_DX, COLL_Y, C_GRAY, 1, 2 * COL_DX - 10);
    time_t now = time(nullptr);
    if (s_i.remainS >= 0 && now > 1700000000) {
      time_t at = now + s_i.remainS;
      struct tm a, t;
      localtime_r(&at, &a); localtime_r(&now, &t);
      // "+1" when it finishes tomorrow, "+2" the day after.
      const int days = (a.tm_yday - t.tm_yday + 366) % 366;
      if (days > 0) snprintf(b, sizeof(b), "%d:%02d+%d", a.tm_hour, a.tm_min, days);
      else          snprintf(b, sizeof(b), "%d:%02d", a.tm_hour, a.tm_min);
    } else snprintf(b, sizeof(b), "--:--");
    lineAt(b, CX + COL_DX, COLV_Y, C_WHITE, 2, 2 * COL_DX - 10);
  } else if (s_i.message[0]) {
    line(messageText(s_i.message), COLL_Y + 6, C_RED, 1);
  }
  info();
  graph();
  temps();
}

static void idleView() {
  ring(0, C_DKGRAY);
  if (s_i.file[0] && s_i.state == PS_CANCELLED) line(s_i.file, FILE_Y, C_GRAY, 1);
  line(stateText(s_i.state), BIG_Y + 8, C_GRAY, 3);
  graph();
  temps();
}

static void messageView(const char* a, const char* b, uint16_t col) {
  ring(0, C_DKGRAY);
  line(a, BIG_Y + 6, col, 2);
  if (b && *b) line(b, BIG_Y + 34, C_GRAY, 1);
}

static void alertBanner() {
  const int x = 115, y = 112, w = 250, h = 150;
  uint16_t col = s_alertEv == PE_FINISHED ? C_GREEN : s_alertEv == PE_FAILED ? C_RED : C_ORANGE;
  const bool solid = !s_alertBlink || s_phase;
  gfx->fillRoundRect(x, y, w, h, 22, solid ? col : C_BLACK);
  gfx->drawRoundRect(x, y, w, h, 22, col);
  gfx->drawRoundRect(x + 1, y + 1, w - 2, h - 2, 21, col);
  const uint16_t fg = solid ? C_BLACK : col;
  const char* head = s_alertEv == PE_FINISHED ? T(S_PA_FINISHED)
                   : s_alertEv == PE_FAILED ? T(S_PA_FAILED) : T(S_PA_PAUSED);
  const uint8_t hs = (Layout_TextW(head, 3) <= w - 24) ? 3 : 2;
  line(head, y + 22, fg, hs, w - 24);
  if (s_alertMsg[0]) line(messageText(s_alertMsg), y + 66, fg, 1, w - 28);
  if (s_i.file[0])   line(s_i.file, y + 86, fg, 1, w - 28);
  line(T(S_PA_CLOSE), y + 118, fg, 1, w - 40);
}

static void alertView();

// --- public ---------------------------------------------------------------------
void ScreenPrint_Enter() {
  PrintWatch_Get(&s_i);
  s_lastUpdated = s_i.updatedMs;
  s_lastState = s_i.state;
}

bool ScreenPrint_Tick() {
  bool redraw = false;
  PrintInfo n;
  PrintWatch_Get(&n);
  if (n.updatedMs != s_lastUpdated || n.state != s_lastState) {
    s_i = n; s_lastUpdated = n.updatedMs; s_lastState = n.state;
    redraw = true;
  }
  const unsigned long now = millis();
  if (s_alert && s_alertBlink) {
    const bool ph = (now / 600) & 1;
    if (ph != s_phase) { s_phase = ph; redraw = true; }
  }
  // Once a second: the offline countdown and the status line clock.
  if (now - s_lastSecond >= 1000) {
    s_lastSecond = now;
    if (s_i.state == PS_OFFLINE || UI_StatusLineChanged()) redraw = true;
  }
  return redraw;
}

void ScreenPrint_Draw() {
  gfx->fillScreen(C_BLACK);
  Layout_Begin();
  Layout_ReserveBand(LY_DOTS - 6, 12);
  Layout_ReserveBand(LY_STATUS - 3, 22);
  UI_DrawStatusLine(LY_STATUS);

  if (s_alert) { alertView(); return; }
  char b[64];
  switch (s_i.state) {
    case PS_DISABLED: messageView(T(S_P_NOTSET), T(S_P_NOTSET2), C_WHITE); break;
    case PS_UNKNOWN:  messageView(T(S_P_WAITING), Settings_PrintHost(), C_GRAY); break;
    case PS_OFFLINE: {
      long left = s_i.nextPollMs ? (long)(s_i.nextPollMs - millis()) / 1000 : 0;
      if (left < 0) left = 0;
      snprintf(b, sizeof(b), "%s %ld s", T(S_P_RETRY), left);
      messageView(T(S_P_OFFLINE), b, C_RED);
      line(Settings_PrintHost(), BIG_Y + 52, C_GRAY, 1);
      break;
    }
    case PS_AUTH:     messageView(T(S_P_AUTH), T(S_P_AUTH2), C_ORANGE); break;
    case PS_BADREPLY: messageView(T(S_P_BAD), T(S_P_BAD2), C_ORANGE); break;
    case PS_IDLE:     idleView(); break;
    case PS_CANCELLED:
      if (s_i.elapsedS > 0) jobView(); else idleView();
      break;
    default:          jobView(); break;
  }
  if (s_alert) alertBanner();
}

// With the alert up only the ring, the graph and the temperatures stay round
// the banner - the job's numbers under it would just be half-covered text.
static void alertView() {
  float frac = (s_alertEv == PE_FINISHED) ? 1.0f : -1.0f;
  if (frac < 0 && s_i.elapsedS >= 0 && s_i.remainS >= 0 && s_i.elapsedS + s_i.remainS > 0)
    frac = (float)s_i.elapsedS / (float)(s_i.elapsedS + s_i.remainS);
  ring(frac, s_alertEv == PE_FINISHED ? C_GREEN : s_alertEv == PE_FAILED ? C_RED : C_ORANGE);
  graph();
  temps();
  alertBanner();
}

void ScreenPrint_AlertBegin(PrintEvent e, const char* msg, bool blink) {
  s_alert = true;
  s_alertEv = e;
  strncpy(s_alertMsg, msg ? msg : "", sizeof(s_alertMsg) - 1);
  s_alertMsg[sizeof(s_alertMsg) - 1] = '\0';
  s_alertBlink = blink;
  s_phase = true;
  PrintWatch_Get(&s_i);
}

void ScreenPrint_AlertEnd() { s_alert = false; }
