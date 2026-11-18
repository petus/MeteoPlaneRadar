// =============================================================================
//  MeteoPlaneRadar
//  Screen: cryptocurrencies (0.9.7). See ScreenCrypto.h; the table is
//  RateTable.cpp, shared with the Exchange rates screen.
//
//    "Kryptoměny v CZK"
//    "CoinGecko 18:45, 24 h"    grey - the time of CoinGecko's price and the
//                               period of the arrows. When nothing new came
//                               for over half an hour: orange, "- staré";
//                               after six hours the prices themselves go grey.
//    BTC  2,05 mil.  ▼ 1,41%
//
//  Millions are written as millions ("2,05 mil."), never as a seven-digit
//  number or as thousands of thousands.
//
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
// =============================================================================
#include "ScreenCrypto.h"
#include "Crypto.h"
#include "FxParse.h"
#include "RateTable.h"
#include "SrcHealth.h"
#include "Settings.h"
#include "Outside.h"
#include "Layout.h"
#include "Lang.h"
#include "UI.h"
#include "Display_ST7701.h"
#include "Config.h"
#include <math.h>
#include <string.h>
#include <time.h>

static uint32_t s_gen = 0xFFFFFFFF;

void ScreenCrypto_Enter() { s_gen = 0xFFFFFFFF; }

bool ScreenCrypto_Tick() {
  bool want = false;
  if (Crypto_Gen() != s_gen) { s_gen = Crypto_Gen(); want = true; }
  // Every minute anyway: the clock at the top, and "stale" turning on.
  if (UI_StatusLineChanged()) want = true;
  return want;
}

static bool stale() { return Crypto_Health().age() > CR_STALE_MS; }
static bool dim()   { return Crypto_Health().age() > CR_DIM_MS; }

void ScreenCrypto_Draw() {
  gfx->fillScreen(C_BLACK);
  Layout_Begin();

  Layout_ReserveBand(LY_DOTS - 6, 12);
  Layout_ReserveBand(LY_STATUS - 3, 22);
  UI_DrawStatusLine(LY_STATUS);

  char head[48];
  snprintf(head, sizeof(head), "%s %s",
           LT("Kryptomeny v", "Kryptoměny v", "Crypto in", "Kryptomeny v", "Kryptomeny v"),
           Settings_FxHomeEff());
  if (!Crypto_Valid()) {
    UI_NoData(head, Crypto_Health());     // "Loading..." or "source unavailable"
    return;
  }

  // "CoinGecko 18:45, 24 h" - or with "- staré" in place of the period.
  char sub[48];
  int l = snprintf(sub, sizeof(sub), "CoinGecko");
  const time_t at = Crypto_Updated();
  if (at && Outside_TimeValid()) {
    struct tm lt; localtime_r(&at, &lt);
    l += snprintf(sub + l, sizeof(sub) - l, " %02d:%02d", lt.tm_hour, lt.tm_min);
  }
  if (stale()) snprintf(sub + l, sizeof(sub) - l, "%s", LT(" - stare", " - staré", " - old", " - stare", " - staré"));
  else         snprintf(sub + l, sizeof(sub) - l, ", 24 h");
  RateTable_Header(head, sub, stale() ? C_ORANGE : C_GRAY);

  CrRow rows[CR_MAX];
  const int n = Crypto_Rows(rows, CR_MAX);
  RateLine L[CR_MAX];
  const bool en = Lang_Get() == LANG_EN;
  for (int i = 0; i < n; i++) {
    RateLine& r = L[i];
    memset(&r, 0, sizeof(r));
    snprintf(r.code, sizeof(r.code), "%.5s", rows[i].sym);
    FxParse_FormatPrice(rows[i].price, RateTable_Comma(), en, r.val, sizeof(r.val),
                        r.suf, sizeof(r.suf));
    r.dim = isnan(rows[i].price) || dim();
    if (!isnan(rows[i].price) && !isnan(rows[i].chg24))
      r.dir = RateTable_Pct(rows[i].chg24, r.chg, sizeof(r.chg));
  }
  RateTable_Draw(L, n);
}
