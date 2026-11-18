// =============================================================================
//  MeteoPlaneRadar
//  Interface language (Czech / English / Slovak) - one table, two spellings.
//
//  The display and the web need DIFFERENT Czech. The built-in GFX font is 7-bit
//  ASCII, so anything drawn on the panel has to be written without diacritics
//  ("Predpoved"); a browser has no such problem and gets the real thing
//  ("Predpoved" with the accents). Keeping both in one table means a string can
//  never be updated in one place and forgotten in the other.
//
//  English needs only one spelling, so it is stored once and used for both.
//  Slovak (0.8.6) has both spellings like Czech.
//
//  The web PAGE does its own translation in JavaScript - it ships both
//  languages and picks one from the config. Only the strings that C code has to
//  produce (captive portal labels, JSON status text) live here.
//
//  Project: MeteoPlaneRadar - live aircraft radar on a round touchscreen
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
//  Web:     https://chiptron.cz
// =============================================================================
#pragma once
#include <Arduino.h>

#define LANG_CZ 0
#define LANG_EN 1
#define LANG_SK 2   // 0.8.6
#define LANG_N  3

// X(id, czech for the DISPLAY (ASCII only), czech for the WEB (UTF-8), english,
//    slovak ASCII, slovak UTF-8)
//
// One list, three columns - the enum and every table below are generated from
// it, so they cannot drift apart.
#define LANG_STRINGS(X) \
  X(S_WIFI_WAIT,     "Cekam na WiFi",      "Čekám na WiFi",      "Waiting for WiFi", "Cakam na WiFi", "Čakám na WiFi") \
  X(S_DOWNLOADING,   "Stahuji...",         "Stahuji...",         "Downloading...", "Stahujem...", "Sťahujem...") \
  X(S_LOADING,       "Nacitam...",         "Načítám...",         "Loading...", "Nacitavam...", "Načítavam...") \
  X(S_ERROR,         "Chyba",              "Chyba",              "Error", "Chyba", "Chyba") \
  X(S_SRC_DOWN,      "Zdroj nedostupny",   "Zdroj nedostupný",   "Source unavailable", "Zdroj nedostupny", "Zdroj nedostupný") \
  X(S_OK,            "OK",                 "OK",                 "OK", "OK", "OK") \
  X(S_NO_LOCATION,   "Nastav polohu",      "Nastavte polohu",    "Set your location", "Nastavte polohu", "Nastavte polohu") \
  X(S_KM,            "km",                 "km",                 "km", "km", "km") \
  X(S_SETTINGS,      "Nastaveni",          "Nastavení",          "Settings", "Nastavenia", "Nastavenia") \
  X(S_BRIGHTNESS,    "Jas",                "Jas",                "Brightness", "Jas", "Jas") \
  X(S_NOT_CONNECTED, "nepripojeno",        "nepřipojeno",        "not connected", "nepripojene", "nepripojené") \
  X(S_TOP,           "Nahore",             "Nahoře",             "Top", "Hore", "Hore") \
  X(S_UNITS_AVIA,    "Jednotky: letecke",  "Jednotky: letecké",  "Units: aviation", "Jednotky: letecke", "Jednotky: letecké") \
  X(S_UNITS_METRIC,  "Jednotky: metricke", "Jednotky: metrické", "Units: metric", "Jednotky: metricke", "Jednotky: metrické") \
  X(S_WEB_HINT,      "Nastaveni v prohlizeci:", "Nastavení v prohlížeči:", "Settings in a browser:", "Nastavenia v prehliadaci:", "Nastavenia v prehliadači:") \
  X(S_AIRCRAFT,      "Letadel",            "Letadel",            "Aircraft", "Lietadiel", "Lietadiel") \
  X(S_ALTITUDE,      "Vyska",              "Výška",              "Altitude", "Vyska", "Výška") \
  X(S_SPEED,         "Rychlost",           "Rychlost",           "Speed", "Rychlost", "Rýchlosť") \
  X(S_TRACK,         "Kurz",               "Kurz",               "Track", "Kurz", "Kurz") \
  X(S_CLIMB,         "Stoupani",           "Stoupání",           "Climb", "Stupanie", "Stúpanie") \
  X(S_TYPE,          "Typ",                "Typ",                "Type", "Typ", "Typ") \
  X(S_FROM,          "Z",                  "Z",                  "From", "Z", "Z") \
  X(S_TO,            "Do",                 "Do",                 "To", "Do", "Do") \
  X(S_ROUTE_WAIT,    "zjistuji trasu",     "zjišťuji trasu",     "looking up route", "zistujem trasu", "zisťujem trasu") \
  X(S_SIGNAL_LOST,   "signal ztracen",     "signál ztracen",     "signal lost", "signal strateny", "signál stratený") \
  X(S_UNKNOWN,       "neznamy",            "neznámý",            "unknown", "neznamy", "neznámy") \
  X(S_EMERGENCY,     "NOUZE",              "NOUZE",              "EMERGENCY", "NUDZA", "NÚDZA") \
  X(S_HIJACK,        "UNOS",               "ÚNOS",               "HIJACK", "UNOS", "ÚNOS") \
  X(S_RADIO_FAIL,    "BEZ RADIA",          "BEZ RÁDIA",          "RADIO FAIL", "BEZ RADIA", "BEZ RÁDIA") \
  X(S_METEORADAR,    "Meteoradar",         "Meteoradar",         "Weather radar", "Meteoradar", "Meteoradar") \
  X(S_NOW,           "nyni",               "nyní",               "now", "teraz", "teraz") \
  X(S_MIN,           "min",                "min",                "min", "min", "min") \
  X(S_WHOLE_CZ,      "cela CR",            "celá ČR",            "whole CZ", "cela CR", "celá ČR") \
  X(S_LOADING_NEWER, "nacitam novejsi snimky...", "načítám novější snímky...", "loading newer frames...", "nacitavam novsie snimky...", "načítavam novšie snímky...") \
  X(S_OLD_DATA,      "bez spojeni, zobrazena starsi data", "bez spojení, zobrazena starší data", "no link, showing older data", "bez spojenia, zobrazene starsie data", "bez spojenia, zobrazené staršie dáta") \
  X(S_FRAME_WIDE,    "snimek moc siroky",  "snímek moc široký",  "frame too wide", "snimka prilis siroka", "snímka príliš široká") \
  X(S_FORECAST,      "Predpoved",          "Předpověď",          "Forecast", "Predpoved", "Predpoveď") \
  X(S_POLLEN,        "Pyl",                "Pyl",                "Pollen", "Pel", "Peľ") \
  X(S_TODAY,         "dnes",               "dnes",               "today", "dnes", "dnes") \
  X(S_TOMORROW,      "zitra",              "zítra",              "tomorrow", "zajtra", "zajtra") \
  X(S_PRICE,         "Cena elektriny",     "Cena elektřiny",     "Electricity price", "Cena elektriny", "Cena elektriny") \
  X(S_NO_TOMORROW,   "zitrek zatim neni",  "zítřek zatím není",  "tomorrow not out yet", "zajtrajsok este nie je", "zajtrajšok ešte nie je") \
  X(S_SPOT,          "burza",              "burza",              "spot", "burza", "burza") \
  X(S_DATA_FROM,     "data z",             "data z",             "as of", "data z", "dáta z") \
  X(S_PRICE_CZ_ONLY, "jen pro CR",         "jen pro ČR",         "Czechia only", "len pre CR", "len pre ČR") \
  X(S_MIX_EU_ONLY,   "jen pro Evropu",     "jen pro Evropu",     "Europe only", "len pre Europu", "len pre Európu") \
  X(S_OUT_OF_AREA,   "mimo oblast zdroje", "mimo oblast zdroje", "outside the source area", "mimo oblasti zdroja", "mimo oblasti zdroja") \
  X(S_MIX,           "Vyroba CR",          "Výroba ČR",          "Czech generation", "Vyroba CR", "Výroba ČR") \
  X(S_MIX_GEN,       "Vyroba",             "Výroba",             "Generation", "Vyroba", "Výroba") \
  X(S_RENEWABLE,     "OZE",                "OZE",                "renewable", "OZE", "OZE") \
  X(S_CONSUMPTION,   "spotreba",           "spotřeba",           "load", "spotreba", "spotreba") \
  X(S_EXPORT,        "vyvoz",              "vývoz",              "export", "vyvoz", "vývoz") \
  X(S_IMPORT,        "dovoz",              "dovoz",              "import", "dovoz", "dovoz") \
  X(S_NUCLEAR,       "jadro",              "jádro",              "nuclear", "jadro", "jadro") \
  X(S_COAL,          "uhli",               "uhlí",               "coal", "uhlie", "uhlie") \
  X(S_GAS,           "plyn",               "plyn",               "gas", "plyn", "plyn") \
  X(S_SOLAR,         "slunce",             "slunce",             "solar", "slnko", "slnko") \
  X(S_WIND,          "vitr",               "vítr",               "wind", "vietor", "vietor") \
  X(S_HYDRO,         "voda",               "voda",               "hydro", "voda", "voda") \
  X(S_BIOMASS,       "biomasa",            "biomasa",            "biomass", "biomasa", "biomasa") \
  X(S_OTHER,         "ostatni",            "ostatní",            "other", "ostatne", "ostatné") \
  X(S_OLD_PLANES,    "stara data",         "stará data",         "old data", "stare data", "staré dáta") \
  X(S_NO_PLANE_DATA, "bez dat o letadlech", "bez dat o letadlech", "no aircraft data", "bez dat o lietadlach", "bez dát o lietadlách") \
  X(S_PAUSE,         "pauza",              "pauza",              "pause", "pauza", "pauza") \
  X(S_P_PRINTING,    "Tiskne", "Tiskne", "Printing", "Tlaci", "Tlačí") \
  X(S_P_PAUSED,      "Pozastaveno", "Pozastaveno", "Paused", "Pozastavene", "Pozastavené") \
  X(S_P_PREPARE,     "Pripravuje se", "Připravuje se", "Preparing", "Pripravuje sa", "Pripravuje sa") \
  X(S_P_FINISHED,    "Hotovo", "Hotovo", "Finished", "Hotovo", "Hotovo") \
  X(S_P_FAILED,      "Chyba", "Chyba", "Error", "Chyba", "Chyba") \
  X(S_P_CANCELLED,   "Zruseno", "Zrušeno", "Cancelled", "Zrusene", "Zrušené") \
  X(S_P_IDLE,        "Pripravena", "Připravena", "Ready", "Pripravena", "Pripravená") \
  X(S_P_OFFLINE,     "Tiskarna nedostupna", "Tiskárna nedostupná", "Printer not reachable", "Tlaciaren nedostupna", "Tlačiareň nedostupná") \
  X(S_P_RETRY,       "dalsi pokus za", "další pokus za", "next try in", "dalsi pokus o", "ďalší pokus o") \
  X(S_P_WAITING,     "Zjistuji stav...", "Zjišťuji stav...", "Checking...", "Zistujem stav...", "Zisťujem stav...") \
  X(S_P_AUTH,        "Chybi heslo", "Chybí heslo", "Password needed", "Chyba heslo", "Chýba heslo") \
  X(S_P_AUTH2,       "zadejte ho v prohlizeci", "zadejte ho v prohlížeči", "enter it in the browser", "zadajte ho v prehliadaci", "zadajte ho v prehliadači") \
  X(S_P_BAD,         "Nerozumim odpovedi", "Nerozumím odpovědi", "Unexpected reply", "Nerozumiem odpovedi", "Nerozumiem odpovedi") \
  X(S_P_BAD2,        "zkontrolujte typ a adresu", "zkontrolujte typ a adresu", "check type and address", "skontrolujte typ a adresu", "skontrolujte typ a adresu") \
  X(S_P_NOTSET,      "Nastavte tiskarnu", "Nastavte tiskárnu", "Set up the printer", "Nastavte tlaciaren", "Nastavte tlačiareň") \
  X(S_P_NOTSET2,     "v prohlizeci, zalozka Tiskarna", "v prohlížeči, záložka Tiskárna", "in the browser, Printer tab", "v prehliadaci, zalozka Tlaciaren", "v prehliadači, záložka Tlačiareň") \
  X(S_P_REMAIN,      "zbyva", "zbývá", "left", "zostava", "zostáva") \
  X(S_P_ELAPSED,     "tiskne se", "tiskne se", "printing for", "tlaci sa", "tlačí sa") \
  X(S_P_TOTAL,       "celkem", "celkem", "total", "celkom", "celkom") \
  X(S_P_DONE_AT,     "hotovo v", "hotovo v", "done at", "hotovo o", "hotovo o") \
  X(S_P_LAYER,       "vrstva", "vrstva", "layer", "vrstva", "vrstva") \
  X(S_P_NOZZLE,      "tryska", "tryska", "nozzle", "tryska", "tryska") \
  X(S_P_BED,         "podlozka", "podložka", "bed", "podlozka", "podložka") \
  X(S_P_ATTENTION,   "vyzaduje pozornost", "vyžaduje pozornost", "needs attention", "vyzaduje pozornost", "vyžaduje pozornosť") \
  X(S_P_ERR_PRINTER, "chyba tiskarny", "chyba tiskárny", "printer error", "chyba tlaciarne", "chyba tlačiarne") \
  X(S_PA_FINISHED,   "Tisk hotovy", "Tisk hotový", "Print finished", "Tlac hotova", "Tlač hotová") \
  X(S_PA_FAILED,     "Chyba tisku", "Chyba tisku", "Print failed", "Chyba tlace", "Chyba tlače") \
  X(S_PA_PAUSED,     "Tisk pozastaven", "Tisk pozastaven", "Print paused", "Tlac pozastavena", "Tlač pozastavená") \
  X(S_PA_CLOSE,      "dotykem zavrete", "dotykem zavřete", "tap to close", "dotykom zatvorite", "dotykom zatvoríte")

enum StrId : uint16_t {
#define X(id, cz, czw, en, sk, skw) id,
  LANG_STRINGS(X)
#undef X
  STR_COUNT
};

void    Lang_Set(uint8_t lang);     // LANG_CZ / LANG_EN / LANG_SK; anything else = CZ
uint8_t Lang_Get();

// For the PANEL - ASCII only, safe with the built-in font.
const char* T(StrId id);

// Calendar names in the active language. Both are ASCII-only: they are drawn on
// the clock and forecast screens, never sent to a browser.
// wday 0 = Sunday (matches struct tm), mon 0 = January.
const char* Lang_WeekdayShort(int wday);
const char* Lang_MonthName(int mon);

// A string written straight into the code rather than into the table above,
// in all three languages: Czech plain/accented, English, Slovak plain/accented.
// The accented spellings are used when a smooth typeface can draw them.
const char* Lang_Pick(const char* czPlain, const char* czAcc, const char* en,
                      const char* skPlain, const char* skAcc);
#define LT(czPlain, czAcc, en, skPlain, skAcc) Lang_Pick(czPlain, czAcc, en, skPlain, skAcc)
// The language code the web services understand: "cs", "en", "sk".
const char* Lang_Code();
// Czech / Slovak letters (UTF-8) down to ASCII for the built-in pixel font,
// which has no accents: "Švýcarsko" -> "Svycarsko".
void Lang_ToAscii(char* dst, size_t cap, const char* src);
