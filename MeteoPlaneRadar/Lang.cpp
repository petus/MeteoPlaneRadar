// =============================================================================
//  MeteoPlaneRadar
//  Interface language - the string tables.
//
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
// =============================================================================
#include "Lang.h"
#include "AAFont.h"   // AAText_Enabled - the smooth faces carry the accents

static uint8_t s_lang = LANG_CZ;

static const char* const CZ_DISP[STR_COUNT] = {
#define X(id, cz, czw, en, sk, skw) cz,
  LANG_STRINGS(X)
#undef X
};

static const char* const CZ_WEB[STR_COUNT] = {
#define X(id, cz, czw, en, sk, skw) czw,
  LANG_STRINGS(X)
#undef X
};

// English has no diacritics, so the panel and the browser share one table.
static const char* const EN_ALL[STR_COUNT] = {
#define X(id, cz, czw, en, sk, skw) en,
  LANG_STRINGS(X)
#undef X
};

static const char* const SK_DISP[STR_COUNT] = {
#define X(id, cz, czw, en, sk, skw) sk,
  LANG_STRINGS(X)
#undef X
};

static const char* const SK_WEB[STR_COUNT] = {
#define X(id, cz, czw, en, sk, skw) skw,
  LANG_STRINGS(X)
#undef X
};

void    Lang_Set(uint8_t lang) { s_lang = (lang < LANG_N) ? lang : LANG_CZ; }
uint8_t Lang_Get()             { return s_lang; }

// 0.8.5: with a smooth typeface selected the display gets the accented
// strings too - the fonts have the Czech letters, the built-in one never did.
const char* T(StrId id) {
  if (id >= STR_COUNT) return "";
  if (s_lang == LANG_EN) return EN_ALL[id];
  if (s_lang == LANG_SK) return AAText_Enabled() ? SK_WEB[id] : SK_DISP[id];
  return AAText_Enabled() ? CZ_WEB[id] : CZ_DISP[id];
}

const char* Lang_Pick(const char* czPlain, const char* czAcc, const char* en,
                      const char* skPlain, const char* skAcc) {
  if (s_lang == LANG_EN) return en;
  const bool acc = AAText_Enabled();
  if (s_lang == LANG_SK) return acc ? skAcc : skPlain;
  return acc ? czAcc : czPlain;
}

const char* Lang_Code() {
  return s_lang == LANG_EN ? "en" : (s_lang == LANG_SK ? "sk" : "cs");
}

// Sunday first, to line up with struct tm's tm_wday.
static const char* const WD_CZ[7] = { "Ne", "Po", "Ut", "St", "Ct", "Pa", "So" };
static const char* const WD_CZ_U[7] = { "Ne", "Po", "Út", "St", "Čt", "Pá", "So" };
static const char* const WD_SK[7] = { "Ne", "Po", "Ut", "St", "St", "Pi", "So" };
static const char* const WD_SK_U[7] = { "Ne", "Po", "Ut", "St", "Št", "Pi", "So" };
static const char* const WD_EN[7] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };

static const char* const MON_CZ[12] = { "ledna", "unora", "brezna", "dubna", "kvetna",
                                        "cervna", "cervence", "srpna", "zari",
                                        "rijna", "listopadu", "prosince" };
static const char* const MON_CZ_U[12] = { "ledna", "února", "března", "dubna", "května",
                                          "června", "července", "srpna", "září",
                                          "října", "listopadu", "prosince" };
static const char* const MON_SK[12] = { "januara", "februara", "marca", "aprila", "maja",
                                        "juna", "jula", "augusta", "septembra",
                                        "oktobra", "novembra", "decembra" };
static const char* const MON_SK_U[12] = { "januára", "februára", "marca", "apríla", "mája",
                                          "júna", "júla", "augusta", "septembra",
                                          "októbra", "novembra", "decembra" };
static const char* const MON_EN[12] = { "January", "February", "March", "April", "May",
                                        "June", "July", "August", "September",
                                        "October", "November", "December" };

const char* Lang_WeekdayShort(int wday) {
  if (wday < 0 || wday > 6) return "";
  if (s_lang == LANG_EN) return WD_EN[wday];
  if (s_lang == LANG_SK) return AAText_Enabled() ? WD_SK_U[wday] : WD_SK[wday];
  return AAText_Enabled() ? WD_CZ_U[wday] : WD_CZ[wday];
}

const char* Lang_MonthName(int mon) {
  if (mon < 0 || mon > 11) return "";
  if (s_lang == LANG_EN) return MON_EN[mon];
  if (s_lang == LANG_SK) return AAText_Enabled() ? MON_SK_U[mon] : MON_SK[mon];
  return AAText_Enabled() ? MON_CZ_U[mon] : MON_CZ[mon];
}

// 0.9.4: shared by the name days and the country names (was in NameDay.cpp).
// Czech and Slovak letters down to ASCII for the 7-bit pixel font - the same
// approach as the route names (Route.cpp). U+00C0..U+017F covers both.
static const char LAT1_MAP[65]  = "AAAAAAACEEEEIIIIDNOOOOOxOUUUUYTsaaaaaaaceeeeiiiidnooooo/ouuuuyty";
static const char LATA_MAP[129] = "AaAaAaCcCcCcCcDdDdEeEeEeEeEeGgGgGgGgHhHhIiIiIiIiIiIiJjKkkLlLlLlLlLlNnNnNnnNnOoOoOoOoRrRrRrSsSsSsSsTtTtTtUuUuUuUuUuUuWwYyYZzZzZzs";

void Lang_ToAscii(char* dst, size_t cap, const char* src) {
  size_t o = 0;
  for (const unsigned char* p = (const unsigned char*)src; *p && o + 1 < cap; ) {
    const unsigned char c = *p;
    uint32_t cp;
    if (c < 0x80) { cp = c; p += 1; }
    else if ((c & 0xE0) == 0xC0 && p[1]) { cp = ((c & 0x1F) << 6) | (p[1] & 0x3F); p += 2; }
    else { p += 1; while ((*p & 0xC0) == 0x80) p++; continue; }   // nothing longer in the tables
    char ch;
    if (cp < 0x80)                     ch = (char)cp;
    else if (cp >= 0xC0 && cp <= 0xFF) ch = LAT1_MAP[cp - 0xC0];
    else if (cp >= 0x100 && cp <= 0x17F) ch = LATA_MAP[cp - 0x100];
    else continue;
    dst[o++] = ch;
  }
  dst[o] = '\0';
}

