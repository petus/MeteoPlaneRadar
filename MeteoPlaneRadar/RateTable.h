// =============================================================================
//  MeteoPlaneRadar
//  The table on the Exchange rates and Crypto screens (0.9.7): a title, a
//  source line, then up to six rows
//
//      100 HUF   6,123        ▲ 0,42%
//          BTC   2,05  mil.   ▼ 1,3%
//
//  Each column is as wide as its widest entry, so the decimal places line up.
//  The whole table is fitted to the narrowest chord of the disc it crosses:
//  when it does not fit, the percentages go first (the arrows stay), then
//  everything drops to size 2. tools/textcheck draws it in every language and
//  typeface.
//
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
// =============================================================================
#pragma once
#include <Arduino.h>

struct RateLine {
  char    amt[8];     // "100" in front of the code, "" for none
  char    code[6];    // "EUR", "DOGE"
  char    val[12];    // "24,350", "2,05", "--"
  char    suf[8];     // after the value, smaller: "mil." or ""
  char    chg[12];    // "0,42%"; "" = nothing to compare with (no arrow)
  int8_t  dir;        // +1 up, -1 down, 0 unchanged
  bool    dim;        // value in grey: unknown, or too old to trust
};

// Title (white) and the line under it (subColor), both size 2, centred.
void RateTable_Header(const char* title, const char* sub, uint16_t subColor);
// The rows, centred in the space below the header.
void RateTable_Draw(const RateLine* rows, int n);
// A change in per cent as "0,42%" (no sign - the arrow is the sign), "12,5%",
// "123%". Returns the direction; "0%" and 0 when below 0.005 %.
int8_t RateTable_Pct(float pct, char* out, size_t cap);
// Decimal comma in Czech and Slovak, a point in English.
bool RateTable_Comma();
