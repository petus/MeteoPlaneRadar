// =============================================================================
//  MeteoPlaneRadar
//  Exchange rates (0.9.6): reading the two sources and turning them into the
//  numbers on the screen. No network, no Arduino - tools/fxcheck runs it on a
//  PC against saved answers.
//
//  CNB, denni_kurz.txt:
//      12.11.2026 #219
//      země|měna|množství|kód|kurz
//      EMU|euro|1|EUR|24,350
//      Maďarsko|forint|100|HUF|6,123
//    CZK for `množství` units, decimal comma. The header line is skipped
//    because its amount is not a number.
//
//  ECB, data-api csvdata with detail=dataonly:
//      KEY,FREQ,CURRENCY,CURRENCY_DENOM,EXR_TYPE,EXR_SUFFIX,TIME_PERIOD,OBS_VALUE
//      EXR.D.USD.EUR.SP00.A,D,USD,EUR,SP00,A,2026-11-11,1.0812
//    Units of CURRENCY for one euro. The columns are found by their names in
//    the header, not by position, so an added column does not break it.
//
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
// =============================================================================
#pragma once
#include <stdint.h>
#include <stddef.h>

// One CNB line: `rate` CZK for `amount` units of `code`.
struct FxCnbQuote {
  char     code[4];
  uint16_t amount;
  double   rate;
};

// Reads the whole file. *date gets the fixing date as YYYYMMDD (0 when the
// first line is not a date). Returns the number of quotes, -1 when the text is
// not a CNB rate sheet at all.
int FxParse_Cnb(const char* body, long* date, FxCnbQuote* out, int max);

// One ECB series: the newest two observations, [0] the newer. date is
// YYYYMMDD, 0 when there is no such observation.
struct FxEcbSeries {
  char   code[4];
  long   date[2];
  double val[2];      // units of code per EUR
};

// Reads the CSV. Returns the number of series, -1 when the header lacks the
// columns this needs.
int FxParse_Ecb(const char* body, FxEcbSeries* out, int max);

// The amount a rate is shown for, so it never reads "0,0025": 1 by default,
// 100 when one unit is worth less than 0.1, 1000 when 100 still are.
uint16_t FxParse_Amount(double perUnit);

// A rate as text, at most 6 characters plus the terminator: four significant
// digits ("24,35" would lose the hundredths people look at, so it is
// "24,350"; "401,25"; "1234,5"; "0,9234"). comma = decimal comma.
// maxDec caps the decimals: the CNB publishes three, so a CNB rate is never
// padded to "6,1230".
void FxParse_Format(double v, bool comma, char* out, size_t cap, int maxDec = 4);

// A crypto price (0.9.7). From a million up it is millions with a suffix the
// screen draws smaller - "2,05" + "mil." (English "2.05" + "M") - never
// "2 051 234" or thousands of thousands. From 10 000 a whole number grouped
// by a space ("75 432"); below that as FxParse_Format(). suf gets "" when
// there is none.
void FxParse_FormatPrice(double v, bool comma, bool english,
                         char* out, size_t cap, char* suf, size_t sufCap);

// Day before YYYYMMDD, also YYYYMMDD (calendar only - weekends are CNB's job).
long FxParse_DayBefore(long ymd);
