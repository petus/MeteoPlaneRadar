// =============================================================================
//  MeteoPlaneRadar
//  Rate table (0.9.7). See RateTable.h.
//
//  Layout, top to bottom:
//    y  30  clock + outside temperature (drawn by the screen)
//    y  62  title                      size 2
//    y  86  source line                size 2
//    y 122..402  the rows, centred as a block
//
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
// =============================================================================
#include "RateTable.h"
#include "Layout.h"
#include "Lang.h"
#include "UI.h"
#include "AAFont.h"
#include "Display_ST7701.h"
#include "Config.h"
#include <math.h>
#include <string.h>

#define TITLE_Y   62
#define SRC_Y     86
#define ROWS_TOP  122
#define ROWS_BOT  402
#define ROW_STEP  46          // size-3 rows; less when six would not fit
#define EDGE      14          // black kept between a row and the rim
#define MAX_ROWS  6

#define GAP_AMT    6          // "100" .. "HUF"
#define GAP_VAL   18          // code .. value
#define GAP_SUF    5          // value .. "mil."
#define GAP_CHG   14          // value (or suffix) .. arrow
#define ARROW_W   12
#define ARROW_GAP  4

bool RateTable_Comma() { return Lang_Get() != LANG_EN; }

int8_t RateTable_Pct(float pct, char* out, size_t cap) {
  const float a = fabsf(pct);
  if (isnan(pct)) { out[0] = '\0'; return 0; }
  if (a < 0.005f) { snprintf(out, cap, "0%%"); return 0; }
  if (a >= 99.5f)     snprintf(out, cap, "%.0f%%", a);
  else if (a >= 10)   snprintf(out, cap, "%.1f%%", a);
  else                snprintf(out, cap, "%.2f%%", a);
  if (RateTable_Comma()) for (char* c = out; *c; c++) if (*c == '.') *c = ',';
  return pct > 0 ? 1 : -1;
}

void RateTable_Header(const char* title, const char* sub, uint16_t subColor) {
  Layout_ReserveTextCentered(title, 2, LCD_WIDTH / 2, TITLE_Y);
  UI_TextCentered(title, TITLE_Y, C_WHITE, 2);
  if (sub && sub[0]) {
    Layout_ReserveTextCentered(sub, 2, LCD_WIDTH / 2, SRC_Y);
    UI_TextCentered(sub, SRC_Y, subColor, 2);
  }
}

static void arrow(int x, int cy, int dir) {
  if (dir > 0)      gfx->fillTriangle(x, cy + 5, x + ARROW_W, cy + 5, x + ARROW_W / 2, cy - 6, C_GREEN);
  else if (dir < 0) gfx->fillTriangle(x, cy - 5, x + ARROW_W, cy - 5, x + ARROW_W / 2, cy + 6, C_RED);
  else { gfx->fillRect(x + 1, cy - 4, ARROW_W - 2, 3, C_GRAY); gfx->fillRect(x + 1, cy + 2, ARROW_W - 2, 3, C_GRAY); }
}

static void printAt(const char* s, int x, int y, uint16_t col, uint8_t size) {
  gfx->setTextColor(col);
  gfx->setTextSize(size);
  gfx->setCursor(x, y);
  gfx->print(s);
}

// The room a row has: the chord at the narrower of its two edges, minus EDGE.
static int roomAt(int y, int h) {
  const int a = Layout_ChordHalf(y), b = Layout_ChordHalf(y + h);
  return 2 * ((a < b ? a : b) - EDGE);
}

void RateTable_Draw(const RateLine* rows, int n) {
  if (n <= 0) return;
  if (n > MAX_ROWS) n = MAX_ROWS;

  bool anyChg = false;
  for (int i = 0; i < n; i++) if (rows[i].chg[0]) anyChg = true;

  int step = ROW_STEP;
  if (n > 1 && (n - 1) * step + 24 > ROWS_BOT - ROWS_TOP) step = (ROWS_BOT - ROWS_TOP - 24) / (n - 1);

  // The big table with percentages, then without them, then size 2.
  struct Try { uint8_t s; bool pct; };
  static const Try TRIES[] = { {3, true}, {3, false}, {2, true}, {2, false} };
  for (const Try& t : TRIES) {
    const uint8_t S = t.s;
    const int h = LY_CHAR_H(S);
    int wAmt = 0, wCode = 0, wVal = 0, wSuf = 0, wPct = 0;
    for (int i = 0; i < n; i++) {
      const RateLine& r = rows[i];
      int w;
      if (r.amt[0]) { w = Layout_TextW(r.amt, 2); if (w > wAmt) wAmt = w; }
      w = Layout_TextW(r.code, S); if (w > wCode) wCode = w;
      w = Layout_TextW(r.val, S);  if (w > wVal) wVal = w;
      if (r.suf[0]) { w = Layout_TextW(r.suf, 2); if (w > wSuf) wSuf = w; }
      if (t.pct && r.chg[0]) { w = Layout_TextW(r.chg, 2); if (w > wPct) wPct = w; }
    }
    const int wChg = anyChg ? ARROW_W + (wPct ? ARROW_GAP + wPct : 0) : 0;
    const int total = (wAmt ? wAmt + GAP_AMT : 0) + wCode + GAP_VAL + wVal +
                      (wSuf ? GAP_SUF + wSuf : 0) + (wChg ? GAP_CHG + wChg : 0);

    const int block = (n - 1) * step + h;
    const int y0 = (ROWS_TOP + ROWS_BOT) / 2 - block / 2;
    bool fits = true;
    for (int i = 0; i < n && fits; i++) if (total > roomAt(y0 + i * step, h)) fits = false;
    if (!fits && !(S == 2 && !t.pct)) continue;      // the last try is drawn anyway

    const int x0 = LCD_WIDTH / 2 - total / 2;
    const int xCode = x0 + (wAmt ? wAmt + GAP_AMT : 0);
    const int xValR = xCode + wCode + GAP_VAL + wVal;      // right edge of the values
    const int xSuf = xValR + GAP_SUF;
    const int xChg = xValR + (wSuf ? GAP_SUF + wSuf : 0) + GAP_CHG;
    const int smallDy = h - LY_CHAR_H(2);                  // size 2 on the same baseline
    for (int i = 0; i < n; i++) {
      const RateLine& r = rows[i];
      const int y = y0 + i * step;
      Layout_Reserve(x0 - 2, y - 2, total + 4, h + 4);
      if (r.amt[0])
        printAt(r.amt, xCode - GAP_AMT - Layout_TextW(r.amt, 2), y + smallDy, C_GRAY, 2);
      printAt(r.code, xCode, y, C_CYAN, S);
      printAt(r.val, xValR - Layout_TextW(r.val, S), y, r.dim ? C_GRAY : C_WHITE, S);
      if (r.suf[0]) printAt(r.suf, xSuf, y + smallDy, C_GRAY, 2);
      if (wChg && r.chg[0]) {
        const int cy = y + h / 2;
        const uint16_t col = r.dir > 0 ? C_GREEN : (r.dir < 0 ? C_RED : C_GRAY);
        arrow(xChg, cy, r.dir);
        if (wPct) printAt(r.chg, xChg + ARROW_W + ARROW_GAP, cy - LY_CHAR_H(2) / 2, col, 2);
      }
    }
    return;
  }
}
