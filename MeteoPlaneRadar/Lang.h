// =============================================================================
//  MeteoPlaneRadar
//  Interface language (Czech / English) - one table, two spellings.
//
//  The display and the web need DIFFERENT Czech. The built-in GFX font is 7-bit
//  ASCII, so anything drawn on the panel has to be written without diacritics
//  ("Predpoved"); a browser has no such problem and gets the real thing
//  ("Predpoved" with the accents). Keeping both in one table means a string can
//  never be updated in one place and forgotten in the other.
//
//  English needs only one spelling, so it is stored once and used for both.
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

// X(id, czech for the DISPLAY (ASCII only), czech for the WEB (UTF-8), english)
//
// One list, three columns - the enum and every table below are generated from
// it, so they cannot drift apart.
#define LANG_STRINGS(X) \
  X(S_WIFI_WAIT,     "Cekam na WiFi",      "Čekám na WiFi",      "Waiting for WiFi") \
  X(S_DOWNLOADING,   "Stahuji...",         "Stahuji...",         "Downloading...") \
  X(S_LOADING,       "Nacitam...",         "Načítám...",         "Loading...") \
  X(S_ERROR,         "Chyba",              "Chyba",              "Error") \
  X(S_OK,            "OK",                 "OK",                 "OK") \
  X(S_NO_LOCATION,   "Nastav polohu",      "Nastavte polohu",    "Set your location") \
  X(S_KM,            "km",                 "km",                 "km") \
  X(S_SETTINGS,      "Nastaveni",          "Nastavení",          "Settings") \
  X(S_BRIGHTNESS,    "Jas",                "Jas",                "Brightness") \
  X(S_NOT_CONNECTED, "nepripojeno",        "nepřipojeno",        "not connected") \
  X(S_LOCATION,      "Poloha:",            "Poloha:",            "Location:") \
  X(S_TOP,           "Nahore",             "Nahoře",             "Top") \
  X(S_UNITS_AVIA,    "Jednotky: letecke",  "Jednotky: letecké",  "Units: aviation") \
  X(S_UNITS_METRIC,  "Jednotky: metricke", "Jednotky: metrické", "Units: metric") \
  X(S_WIFI_LOC,      "WiFi / poloha",      "WiFi / poloha",      "WiFi / location") \
  X(S_FW_UPDATE,     "Aktualizace FW",     "Aktualizace FW",     "Firmware update") \
  X(S_WEB_HINT,      "Nastaveni v prohlizeci:", "Nastavení v prohlížeči:", "Settings in a browser:") \
  X(S_AIRCRAFT,      "Letadel",            "Letadel",            "Aircraft") \
  X(S_ALTITUDE,      "Vyska",              "Výška",              "Altitude") \
  X(S_SPEED,         "Rychlost",           "Rychlost",           "Speed") \
  X(S_TRACK,         "Kurz",               "Kurz",               "Track") \
  X(S_CLIMB,         "Stoupani",           "Stoupání",           "Climb") \
  X(S_TYPE,          "Typ",                "Typ",                "Type") \
  X(S_FROM,          "Z",                  "Z",                  "From") \
  X(S_TO,            "Do",                 "Do",                 "To") \
  X(S_ROUTE_WAIT,    "zjistuji trasu",     "zjišťuji trasu",     "looking up route") \
  X(S_SIGNAL_LOST,   "signal ztracen",     "signál ztracen",     "signal lost") \
  X(S_UNKNOWN,       "neznamy",            "neznámý",            "unknown") \
  X(S_EMERGENCY,     "NOUZE",              "NOUZE",              "EMERGENCY") \
  X(S_HIJACK,        "UNOS",               "ÚNOS",               "HIJACK") \
  X(S_RADIO_FAIL,    "BEZ RADIA",          "BEZ RÁDIA",          "RADIO FAIL") \
  X(S_METEORADAR,    "Meteoradar",         "Meteoradar",         "Weather radar") \
  X(S_NOW,           "nyni",               "nyní",               "now") \
  X(S_MIN,           "min",                "min",                "min") \
  X(S_WHOLE_CZ,      "cela CR",            "celá ČR",            "whole CZ") \
  X(S_LOADING_NEWER, "nacitam novejsi snimky...", "načítám novější snímky...", "loading newer frames...") \
  X(S_OLD_DATA,      "bez spojeni, zobrazena starsi data", "bez spojení, zobrazena starší data", "no link, showing older data") \
  X(S_FRAME_WIDE,    "snimek moc siroky",  "snímek moc široký",  "frame too wide") \
  X(S_FORECAST,      "Predpoved",          "Předpověď",          "Forecast") \
  X(S_AIR,           "Ovzdusi",            "Ovzduší",            "Air quality") \
  X(S_POLLEN,        "Pyl",                "Pyl",                "Pollen") \
  X(S_TODAY,         "dnes",               "dnes",               "today") \
  X(S_LAT_LABEL,     "Zemepisna sirka",    "Zeměpisná šířka",    "Latitude") \
  X(S_LON_LABEL,     "Zemepisna delka",    "Zeměpisná délka",    "Longitude") \
  X(S_TOMORROW,      "zitra",              "zítra",              "tomorrow") \
  X(S_PRICE,         "Cena elektriny",     "Cena elektřiny",     "Electricity price") \
  X(S_NO_TOMORROW,   "zitrek zatim neni",  "zítřek zatím není",  "tomorrow not out yet") \
  X(S_SPOT,          "burza",              "burza",              "spot") \
  X(S_DATA_FROM,     "data z",             "data z",             "as of") \
  X(S_PRICE_CZ_ONLY, "jen pro CR",         "jen pro ČR",         "Czechia only") \
  X(S_MIX_EU_ONLY,   "jen pro Evropu",     "jen pro Evropu",     "Europe only") \
  X(S_OUT_OF_AREA,   "mimo oblast zdroje", "mimo oblast zdroje", "outside the source area") \
  X(S_MIX,           "Vyroba CR",          "Výroba ČR",          "Czech generation") \
  X(S_MIX_GEN,       "Vyroba",             "Výroba",             "Generation") \
  X(S_RENEWABLE,     "OZE",                "OZE",                "renewable") \
  X(S_CONSUMPTION,   "spotreba",           "spotřeba",           "load") \
  X(S_EXPORT,        "vyvoz",              "vývoz",              "export") \
  X(S_IMPORT,        "dovoz",              "dovoz",              "import") \
  X(S_NUCLEAR,       "jadro",              "jádro",              "nuclear") \
  X(S_COAL,          "uhli",               "uhlí",               "coal") \
  X(S_GAS,           "plyn",               "plyn",               "gas") \
  X(S_SOLAR,         "slunce",             "slunce",             "solar") \
  X(S_WIND,          "vitr",               "vítr",               "wind") \
  X(S_HYDRO,         "voda",               "voda",               "hydro") \
  X(S_BIOMASS,       "biomasa",            "biomasa",            "biomass") \
  X(S_OTHER,         "ostatni",            "ostatní",            "other") \
  X(S_OLD_PLANES,    "stara data",         "stará data",         "old data") \
  X(S_NO_PLANE_DATA, "bez dat o letadlech", "bez dat o letadlech", "no aircraft data") \
  X(S_PAUSE,         "pauza",              "pauza",              "pause")

enum StrId : uint16_t {
#define X(id, cz, czw, en) id,
  LANG_STRINGS(X)
#undef X
  STR_COUNT
};

void    Lang_Set(uint8_t lang);     // LANG_CZ / LANG_EN; anything else = CZ
uint8_t Lang_Get();

// For the PANEL - ASCII only, safe with the built-in font.
const char* T(StrId id);

// For a browser / captive portal - real UTF-8 with diacritics.
const char* TW(StrId id);

// Calendar names in the active language. Both are ASCII-only: they are drawn on
// the clock and forecast screens, never sent to a browser.
// wday 0 = Sunday (matches struct tm), mon 0 = January.
const char* Lang_WeekdayShort(int wday);
const char* Lang_MonthName(int mon);
