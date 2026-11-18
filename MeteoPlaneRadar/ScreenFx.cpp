// =============================================================================
//  MeteoPlaneRadar
//  Screen: exchange rates (0.9.6). See ScreenFx.h; the table itself is
//  RateTable.cpp, shared with the Crypto screen.
//
//    "Kurzy v CZK"
//    "ČNB, 12. 11. 2026"        grey; orange with "staré" when the source has
//                               not answered for over a day
//    EUR  24,350  ▲ 0,16%
//
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
// =============================================================================
#include "ScreenFx.h"
#include "Fx.h"
#include "FxParse.h"
#include "RateTable.h"
#include "SrcHealth.h"
#include "Settings.h"
#include "Layout.h"
#include "Lang.h"
#include "UI.h"
#include "Display_ST7701.h"
#include "Config.h"
#include <math.h>
#include <string.h>

static uint32_t s_gen = 0xFFFFFFFF;

void ScreenFx_Enter() { s_gen = 0xFFFFFFFF; }

bool ScreenFx_Tick() {
  bool want = false;
  if (Fx_Gen() != s_gen) { s_gen = Fx_Gen(); want = true; }
  if (UI_StatusLineChanged()) want = true;
  return want;
}

// "Kurzy v CZK" / "Rates in EUR".
static void title(char* out, size_t cap) {
  snprintf(out, cap, "%s %s", LT("Kurzy v", "Kurzy v", "Rates in", "Kurzy v", "Kurzy v"),
           Settings_FxHomeEff());
}

// "ČNB, 12. 11. 2026" / "ECB, 12 Nov 2026", and " - staré" when it is old.
static void sourceLine(char* out, size_t cap) {
  const char* src = Fx_Source() == FX_SRC_CNB ? LT("CNB", "ČNB", "CNB", "CNB", "ČNB") : "ECB";
  const long d = Fx_Date();
  const int y = (int)(d / 10000), m = (int)(d / 100 % 100), dd = (int)(d % 100);
  if (!d)                         snprintf(out, cap, "%s", src);
  else if (Lang_Get() == LANG_EN) snprintf(out, cap, "%s, %d %.3s %d", src, dd, Lang_MonthName(m - 1), y);
  else                            snprintf(out, cap, "%s, %d. %d. %d", src, dd, m, y);
  if (Fx_Stale()) {
    const size_t l = strlen(out);
    snprintf(out + l, cap - l, "%s", LT(" - stare", " - staré", " - old", " - stare", " - staré"));
  }
}

void ScreenFx_Draw() {
  gfx->fillScreen(C_BLACK);
  Layout_Begin();

  Layout_ReserveBand(LY_DOTS - 6, 12);
  Layout_ReserveBand(LY_STATUS - 3, 22);
  UI_DrawStatusLine(LY_STATUS);

  char head[48];
  title(head, sizeof(head));
  if (!Fx_Valid()) {
    UI_NoData(head, Fx_Health());         // "Loading..." or "source unavailable"
    return;
  }

  char sub[48];
  sourceLine(sub, sizeof(sub));
  RateTable_Header(head, sub, Fx_Stale() ? C_ORANGE : C_GRAY);

  FxRow rows[FX_MAX];
  const int n = Fx_Rows(rows, FX_MAX);
  RateLine L[FX_MAX];
  for (int i = 0; i < n; i++) {
    RateLine& l = L[i];
    memset(&l, 0, sizeof(l));
    if (rows[i].amount > 1) snprintf(l.amt, sizeof(l.amt), "%u", rows[i].amount);
    snprintf(l.code, sizeof(l.code), "%.5s", rows[i].code);
    // The CNB publishes three decimals, so a CNB rate is never padded to four.
    FxParse_Format(rows[i].rate, RateTable_Comma(), l.val, sizeof(l.val),
                   Fx_Source() == FX_SRC_CNB ? 3 : 4);
    l.dim = isnan(rows[i].rate);
    if (!isnan(rows[i].rate) && !isnan(rows[i].prev) && rows[i].prev > 0)
      l.dir = RateTable_Pct((rows[i].rate - rows[i].prev) / rows[i].prev * 100.0f,
                            l.chg, sizeof(l.chg));
  }
  RateTable_Draw(L, n);
}
