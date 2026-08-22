// =============================================================================
//  MeteoPlaneRadar
//  Interface language (Czech / Slovak / English)
//
//  The display and the web need DIFFERENT Czech/Slovak. The built-in GFX font is 
//  7-bit ASCII, so anything drawn on the panel has to be written without diacritics
//  ("Predpoved"); a browser has no such problem and gets the real thing
//  ("Predpoveď" with the accents). Keeping both in one table means a string can
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
#define LANG_SK 2

// X(id, czech DISPLAY (ASCII), czech WEB (UTF-8), slovak DISPLAY (ASCII), slovak WEB (UTF-8), english)
//
// One list, five columns - the enum and every table below are generated from
// it, so they cannot drift apart.
#define LANG_STRINGS(X) \
  X(S_WIFI_WAIT,     "Cekam na WiFi",      "Čekám na WiFi",         "Cakam na WiFi",         "Čakám na WiFi",         "Waiting for WiFi") \
  X(S_DOWNLOADING,   "Stahuji...",         "Stahuji...",            "Stahujem...",           "Sťahujem...",           "Downloading...") \
  X(S_LOADING,       "Nacitam...",         "Načítám...",            "Nacitavam...",          "Načítavam...",          "Loading...") \
  X(S_ERROR,         "Chyba",              "Chyba",                 "Chyba",                 "Chyba",                 "Error") \
  X(S_OK,            "OK",                 "OK",                    "OK",                    "OK",                    "OK") \
  X(S_NO_LOCATION,   "Nastav polohu",      "Nastavte polohu",       "Nastav polohu",         "Nastavte polohu",       "Set your location") \
  X(S_KM,            "km",                 "km",                    "km",                    "km",                    "km") \
  X(S_SETTINGS,      "Nastaveni",          "Nastavení",             "Nastavenia",            "Nastavenia",            "Settings") \
  X(S_BRIGHTNESS,    "Jas",                "Jas",                   "Jas",                   "Jas",                   "Brightness") \
  X(S_NOT_CONNECTED, "nepripojeno",        "nepřipojeno",           "nepripojene",           "nepripojené",           "not connected") \
  X(S_LOCATION,      "Poloha:",            "Poloha:",               "Poloha:",               "Poloha:",               "Location:") \
  X(S_TOP,           "Nahore",             "Nahoře",                "Hore",                  "Hore",                  "Top") \
  X(S_UNITS_AVIA,    "Jednotky: letecke",  "Jednotky: letecké",     "Jednotky: letecke",     "Jednotky: letecké",     "Units: aviation") \
  X(S_UNITS_METRIC,  "Jednotky: metricke", "Jednotky: metrické",    "Jednotky: metricke",    "Jednotky: metrické",    "Units: metric") \
  X(S_WIFI_LOC,      "WiFi / poloha",      "WiFi / poloha",         "WiFi / poloha",         "WiFi / poloha",         "WiFi / location") \
  X(S_FW_UPDATE,     "Aktualizace FW",     "Aktualizace FW",        "Aktualizacia FW",       "Aktualizácia FW",       "Firmware update") \
  X(S_WEB_HINT,      "Nastaveni v prohlizeci:", "Nastavení v prohlížeči:", "Nastavenia v prehliadaci:", "Nastavenia v prehliadači:", "Settings in a browser:") \
  X(S_AIRCRAFT,      "Letadel",            "Letadel",               "Lietadiel",             "Lietadiel",             "Aircraft") \
  X(S_ALTITUDE,      "Vyska",              "Výška",                 "Vyska",                 "Výška",                 "Altitude") \
  X(S_SPEED,         "Rychlost",           "Rychlost",              "Rychlost",              "Rýchlosť",              "Speed") \
  X(S_TRACK,         "Kurz",               "Kurz",                  "Kurz",                  "Kurz",                  "Track") \
  X(S_CLIMB,         "Stoupani",           "Stoupání",              "Stupanie",              "Stúpanie",              "Climb") \
  X(S_TYPE,          "Typ",                "Typ",                   "Typ",                   "Typ",                   "Type") \
  X(S_FROM,          "Z",                  "Z",                     "Z",                     "Z",                     "From") \
  X(S_TO,            "Do",                 "Do",                    "Do",                    "Do",                    "To") \
  X(S_ROUTE_WAIT,    "zjistuji trasu",     "zjišťuji trasu",        "zistujem trasu",        "zisťujem trasu",        "looking up route") \
  X(S_SIGNAL_LOST,   "signal ztracen",     "signál ztracen",        "signal strateny",       "signál stratený",       "signal lost") \
  X(S_UNKNOWN,       "neznamy",            "neznámý",               "neznamy",               "neznámy",               "unknown") \
  X(S_EMERGENCY,     "NOUZE",              "NOUZE",                 "NUDZA",                 "NÚDZA",                 "EMERGENCY") \
  X(S_HIJACK,        "UNOS",               "ÚNOS",                  "UNOS",                  "ÚNOS",                  "HIJACK") \
  X(S_RADIO_FAIL,    "BEZ RADIA",          "BEZ RÁDIA",             "BEZ RADIA",             "BEZ RÁDIA",             "RADIO FAIL") \
  X(S_METEORADAR,    "Meteoradar",         "Meteoradar",            "Meteoradar",            "Meteoradar",            "Weather radar") \
  X(S_NOW,           "nyni",               "nyní",                  "teraz",                 "teraz",                 "now") \
  X(S_MIN,           "min",                "min",                   "min",                   "min",                   "min") \
  X(S_WHOLE_CZ,      "cela CR",            "celá ČR",               "cela CR",               "celá ČR",               "whole CZ") \
  X(S_LOADING_NEWER, "nacitam novejsi snimky...", "načítám novější snímky...", "nacitavam novsie snimky...", "načítavam novšie snímky...", "loading newer frames...") \
  X(S_OLD_DATA,      "bez spojeni, zobrazena starsi data", "bez spojení, zobrazena starší data", "bez spojenia, zobrazene starsie data", "bez spojenia, zobrazené staršie dáta", "no link, showing older data") \
  X(S_FRAME_WIDE,    "snimek moc siroky",  "snímek moc široký",     "snimka moc siroka",     "snímka moc široká",     "frame too wide") \
  X(S_FORECAST,      "Predpoved",          "Předpověď",             "Predpoved",             "Predpoveď",             "Forecast") \
  X(S_AIR,           "Ovzdusi",            "Ovzduší",               "Ovzdusie",              "Ovzdušie",              "Air quality") \
  X(S_POLLEN,        "Pyl",                "Pyl",                   "Pel",                   "Peľ",                   "Pollen") \
  X(S_TODAY,         "dnes",               "dnes",                  "dnes",                  "dnes",                  "today") \
  X(S_LAT_LABEL,     "Zemepisna sirka",    "Zeměpisná šířka",       "Zemepisna sirka",       "Zemepisná šírka",       "Latitude") \
  X(S_LON_LABEL,     "Zemepisna delka",    "Zeměpisná délka",       "Zemepisna dlzka",       "Zemepisná dĺžka",       "Longitude")

enum StrId : uint16_t {

#define X(id, cz, czw, sk, skw, en) id,
  LANG_STRINGS(X)
#undef X
  STR_COUNT
};

void    Lang_Set(uint8_t lang);     // LANG_CZ / LANG_SK / LANG_EN; anything else = CZ
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
