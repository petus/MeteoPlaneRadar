// =============================================================================
//  MeteoPlaneRadar
//  Interface language - the string tables.
//
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
// =============================================================================
#include "Lang.h"

static uint8_t s_lang = LANG_CZ;

// --- CZECH STRINGS ---
static const char* const CZ_DISP[STR_COUNT] = {
#define X(id, cz, czw, sk, skw, en) cz,
  LANG_STRINGS(X)
#undef X
};

static const char* const CZ_WEB[STR_COUNT] = {
#define X(id, cz, czw, sk, skw, en) czw,
  LANG_STRINGS(X)
#undef X
};

// --- SLOVAK STRINGS ---
static const char* const SK_DISP[STR_COUNT] = {
#define X(id, cz, czw, sk, skw, en) sk,
  LANG_STRINGS(X)
#undef X
};

static const char* const SK_WEB[STR_COUNT] = {
#define X(id, cz, czw, sk, skw, en) skw,
  LANG_STRINGS(X)
#undef X
};

// --- ENGLISH STRINGS ---
// English has no diacritics, so the panel and the browser share one table.
static const char* const EN_ALL[STR_COUNT] = {
#define X(id, cz, czw, sk, skw, en) en,
  LANG_STRINGS(X)
#undef X
};

// --- LOGIC ---
void Lang_Set(uint8_t lang) { 
  if (lang == LANG_EN) s_lang = LANG_EN;
  else if (lang == LANG_SK) s_lang = LANG_SK;
  else s_lang = LANG_CZ; 
}

uint8_t Lang_Get() { return s_lang; }

const char* T(StrId id) {
  if (id >= STR_COUNT) return "";
  if (s_lang == LANG_EN) return EN_ALL[id];
  if (s_lang == LANG_SK) return SK_DISP[id];
  return CZ_DISP[id];
}

const char* TW(StrId id) {
  if (id >= STR_COUNT) return "";
  if (s_lang == LANG_EN) return EN_ALL[id];
  if (s_lang == LANG_SK) return SK_WEB[id];
  return CZ_WEB[id];
}

// Sunday first, to line up with struct tm's tm_wday.
static const char* const WD_CZ[7] = { "Ne", "Po", "Ut", "St", "Ct", "Pa", "So" };
static const char* const WD_SK[7] = { "Ned", "Pon", "Uto", "Str", "Stv", "Pia", "Sob" };
static const char* const WD_EN[7] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };

static const char* const MON_CZ[12] = { "ledna", "unora", "brezna", "dubna", "kvetna",
                                        "cervna", "cervence", "srpna", "zari",
                                        "rijna", "listopadu", "prosince" };
static const char* const MON_SK[12] = { "Januar", "Februar", "Marec", "April", "Maj",
                                        "Jun", "Jul", "August", "September",
                                        "Oktober", "November", "December" };                                        
static const char* const MON_EN[12] = { "January", "February", "March", "April", "May",
                                        "June", "July", "August", "September",
                                        "October", "November", "December" };

const char* Lang_WeekdayShort(int wday) {
  if (wday < 0 || wday > 6) return "";
  if (s_lang == LANG_EN) return WD_EN[wday];
  if (s_lang == LANG_SK) return WD_SK[wday];
  return WD_CZ[wday];
}

const char* Lang_MonthName(int mon) {
  if (mon < 0 || mon > 11) return "";
  if (s_lang == LANG_EN) return MON_EN[mon];
  if (s_lang == LANG_SK) return MON_SK[mon];
  return MON_CZ[mon];
}
