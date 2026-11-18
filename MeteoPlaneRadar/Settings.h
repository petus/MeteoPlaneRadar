// =============================================================================
//  MeteoPlaneRadar
//  Persisted settings - interface.
//
//  0.6.0 note: this grew from six values to about thirty, and the web UI needs
//  all of them in one go. Rather than scatter thirty Preferences keys across
//  the code, everything lives here and can be serialised to JSON in one call -
//  the web page, the export file and the captive portal all read the same
//  source. The NVS keys stay individual (cheap partial writes, and an existing
//  device keeps its settings after the update).
//
//  Project: MeteoPlaneRadar - live aircraft radar on a round touchscreen
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
//  Web:     https://chiptron.cz
// =============================================================================
#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "Config.h"   // DEFAULT_LAT / DEFAULT_LON, SCREEN_*

void   Settings_Begin();

// --- WiFi credentials -------------------------------------------------------
// Stored here rather than left to WiFiManager: the access point has to stay up
// until a network is actually entered, and the captive portal has to speak the
// user's language - both of which need the firmware to own the credentials.
//
// 0.8.0: up to WIFI_SLOTS networks. Slot 0 is the one the old single-network
// keys ("ssid"/"wpass") always held, so an updated device keeps its network.
// The slots can be filled by hand in the web UI even when the network is not in
// range - the point is to set up the office before taking the device there.
const char* Settings_WifiSsid(uint8_t slot = 0);
const char* Settings_WifiPass(uint8_t slot = 0);
bool        Settings_HasWifi();                 // at least one slot filled
// From the captive portal: remember this network. Updates the password if the
// SSID is already known, otherwise puts it first and pushes the rest down.
void        Settings_SetWifi(const char* ssid, const char* pass);
// From the web UI: write one slot exactly. An empty ssid clears the slot; a
// nullptr pass keeps the stored password (the page never sees it).
void        Settings_SetWifiSlot(uint8_t slot, const char* ssid, const char* pass);
void        Settings_ClearWifi();

// --- Location ---------------------------------------------------------------
double Settings_Lat();
double Settings_Lon();
bool   Settings_HasLocation();
void   Settings_SetLocation(double lat, double lon);

// --- Brightness -------------------------------------------------------------
// Two levels now. Settings_Backlight() returns whichever one currently applies,
// so every existing caller keeps working without knowing about night mode.
uint8_t Settings_Backlight();          // the level in force right now
void    Settings_SetBacklight(uint8_t pct);   // sets the ACTIVE level
uint8_t Settings_BrightDay();
void    Settings_SetBrightDay(uint8_t pct);
void    Settings_SetBrightNight(uint8_t pct);
bool    Settings_NightAuto();
void    Settings_SetNightAuto(bool on);
int8_t  Settings_NightOffsetMin();     // +/- minutes around sunrise/sunset
void    Settings_SetNightOffsetMin(int8_t m);

// Which level is in force. Set by NightMode; read by everything that draws.
bool    Settings_IsNight();
void    Settings_SetNight(bool night);

// 0.9.2: with the automatic switch on, night follows either the sun (with the
// offset above) or a clock window. Windows are minutes of the day; one that
// crosses midnight (22:00 - 6:00) is fine, from == to means "never".
uint8_t  Settings_NightSource();               // NIGHT_SRC_*
void     Settings_SetNightSource(uint8_t s);
uint16_t Settings_NightFromMin();
uint16_t Settings_NightToMin();
void     Settings_SetNightWindow(uint16_t from, uint16_t to);
// 0.9.2: the display switched off (backlight at 0) for a window of its own. A
// touch lights it for a while; an alert too, if darkAlerts.
bool     Settings_DarkOn();
void     Settings_SetDarkOn(bool on);
uint16_t Settings_DarkFromMin();
uint16_t Settings_DarkToMin();
void     Settings_SetDarkWindow(uint16_t from, uint16_t to);
bool     Settings_DarkAlerts();
void     Settings_SetDarkAlerts(bool on);
// 0.9.2: the name day on the clock - Czech and Slovak calendars, not English.
bool     Settings_NameDay();
void     Settings_SetNameDay(bool on);
// 0.9.3: clock screen layout, CLOCK_LAYOUT_* (A basic, C sun, D next hours).
uint8_t  Settings_ClockLayout();
void     Settings_SetClockLayout(uint8_t l);

// --- Units, language --------------------------------------------------------
bool    Settings_MetricUnits();
void    Settings_SetMetricUnits(bool metric);
void    Settings_SetLanguage(uint8_t l);

// --- Screens ----------------------------------------------------------------
// idx is SCREEN_CLOCK_I .. SCREEN_FORECAST_I. Settings is always enabled.
bool    Settings_ScreenEnabled(uint8_t idx);
void    Settings_SetScreenEnabled(uint8_t idx, bool on);
// Automatic screen cycling, in SECONDS. 0 = off.
//
// This was minutes up to 0.6.0 and is stored under a NEW key, because
// reinterpreting the old one would silently turn "every 3 minutes" into "every
// 3 seconds" on update. Settings_Begin() converts the old value once.
uint16_t Settings_AutoRotateSec();
void     Settings_SetAutoRotateSec(uint16_t s);

// 0.9.1: the screen dots at the top disappear this many seconds after a switch
// (or a touch) and come back with the next one. 0 = always shown, the old
// behaviour and the default, so an update changes nothing by itself.
uint8_t  Settings_DotsHideSec();
void     Settings_SetDotsHideSec(uint8_t s);

// --- Weather radar ----------------------------------------------------------
uint8_t Settings_RadarSource();            // RADAR_SRC_CHMU / _RAINVIEWER / _CHMU_MASKED
void    Settings_SetRadarSource(uint8_t s);

// --- Storm watch (0.8.0) ----------------------------------------------------
// With StormOnly on, the weather radar is left out of the automatic cycling
// unless an echo of at least StormDbz lies within StormKm of the device.
bool     Settings_StormOnly();
void     Settings_SetStormOnly(bool on);
uint8_t  Settings_StormDbz();
void     Settings_SetStormDbz(uint8_t dbz);
uint16_t Settings_StormKm();
void     Settings_SetStormKm(uint16_t km);

// --- Aircraft nearby (0.8.0) ------------------------------------------------
// With PlaneAlert on, the aircraft screen is left out of the cycling and jumps
// in for PlaneAlertSec seconds whenever an aircraft comes within PlaneAlertKm -
// or flies through the window view, which takes priority even when further.
bool     Settings_PlaneAlert();
void     Settings_SetPlaneAlert(bool on);
uint16_t Settings_PlaneAlertKm();
void     Settings_SetPlaneAlertKm(uint16_t km);
uint16_t Settings_PlaneAlertSec();
void     Settings_SetPlaneAlertSec(uint16_t s);
// 0.9.9: what the alert shows. The detail at once (default, as before 0.9.8),
// the radar first with the aircraft marked and then its detail, or the radar
// only. 0.9.8 had the first and the last as a checkbox ("paDet").
#define PA_MODE_DETAIL       0
#define PA_MODE_RADAR_DETAIL 1
#define PA_MODE_RADAR        2
uint8_t  Settings_PlaneAlertMode();
void     Settings_SetPlaneAlertMode(uint8_t m);
bool     Settings_WindowOn();
void     Settings_SetWindowOn(bool on);
uint16_t Settings_WindowAz();              // = Settings_TopBearing() since 0.8.6
uint16_t Settings_WindowWidth();           // full width of the view, degrees
void     Settings_SetWindowWidth(uint16_t deg);
uint16_t Settings_WindowKm();
void     Settings_SetWindowKm(uint16_t km);

// --- Home Assistant (0.8.0) -------------------------------------------------
bool     Settings_HaEnabled();
void     Settings_SetHaEnabled(bool on);
// The dBZ / mm/h scale down the left of the weather screen. On by default: it
// is what makes the colours mean anything. Off for those who know the scale by
// heart and would rather see the map underneath it.
bool    Settings_MeteoLegend();
void    Settings_SetMeteoLegend(bool on);
// 0.9.1: the "from / to" lines in the aircraft detail. The route comes from a
// public database keyed by callsign, and a callsign reused on another leg gives
// a route that is not true - so it can be switched off. Off also means the
// lookup is not made at all.
bool    Settings_ShowRoute();
void    Settings_SetShowRoute(bool on);

// --- 3D printer watch (0.9.0) -----------------------------------------------
// One printer. The host is an IP address, optionally with :port. The secrets
// (Prusa password, API key) are stored in NVS, which is NOT encrypted. They are
// never part of the config JSON - the page only learns whether one is set - and
// go into the export / come from an import only with Settings_PrintExport().
bool        Settings_PrintOn();
void        Settings_SetPrintOn(bool on);
uint8_t     Settings_PrintType();            // PRINT_TYPE_*
void        Settings_SetPrintType(uint8_t t);
const char* Settings_PrintHost();
void        Settings_SetPrintHost(const char* h);
const char* Settings_PrintUser();            // Prusa digest user (not secret)
void        Settings_SetPrintUser(const char* u);
const char* Settings_PrintKey();             // API key, "" = none
const char* Settings_PrintPass();            // Prusa password, "" = none
void        Settings_SetPrintKey(const char* k);   // "" leaves it unchanged
void        Settings_SetPrintPass(const char* p);  // "" leaves it unchanged
void        Settings_ForgetPrintSecrets();   // RAM and NVS
// Whether the password and API key go into the settings export and are taken
// from an import. They are stored in NVS either way.
bool        Settings_PrintExport();
void        Settings_SetPrintExport(bool on);
bool        Settings_PrintAlertFinish();
bool        Settings_PrintAlertError();
bool        Settings_PrintAlertPause();
void        Settings_SetPrintAlerts(bool finish, bool error, bool pause);
uint8_t     Settings_PrintNight();           // PRINT_NIGHT_*
void        Settings_SetPrintNight(uint8_t m);
// Bumped by every setter above, so the watch task can tell its copy is stale.
uint32_t    Settings_PrintGen();

// --- Energy -----------------------------------------------------------------
//
// What the user pays on top of the exchange price. Everything the device knows
// about their tariff, and deliberately nothing more: there is no national
// "correct" surcharge to default to, so both start at zero and the screen then
// shows the bare spot price and says so. See the note at the top of Energy.h.
//
// The fee is in CZK per MEGAWATT hour, the same unit the price arrives in, so
// it can be added before the single conversion to kWh. Suppliers quote it per
// kWh, so the web page does that multiplication where the user can see it.
uint16_t Settings_PriceFee();
void     Settings_SetPriceFee(uint16_t czkPerMwh);
// VAT in per cent. 0 = do not add any, which is what a business wanting the
// price without it would choose.
uint8_t  Settings_PriceVat();
void     Settings_SetPriceVat(uint8_t pct);

// Which country's generation mix to draw. A lower-case energy-charts code
// ("cz", "sk", "de", ...) or "eu" for the continent as a whole. Anything that
// is not two letters is refused, so a typo cannot turn into a 404 every fifteen
// minutes.
const char* Settings_MixCountry();
void        Settings_SetMixCountry(const char* code);

// Flight route source asked first (0.9.8): the one that last worked. Device
// state, not a setting - not in the web page or the export.
#define ROUTE_SRC_LOL 0
#define ROUTE_SRC_DB  1
uint8_t Settings_RouteSrc();
void    Settings_SetRouteSrc(uint8_t src);

// --- Exchange rates (0.9.6) -------------------------------------------------
// The currencies the device knows: what can be a home currency (one the source
// can answer for) and what can be watched. Codes are ISO 4217, upper case.
#define FX_HOMES   "CZK EUR PLN HUF CHF GBP DKK SEK NOK RON USD"
#define FX_CODES   "EUR USD GBP CHF PLN HUF CZK JPY CNY DKK SEK NOK RON TRY CAD AUD"
// Home currency as stored: "" = automatic, otherwise one of FX_HOMES.
void        Settings_SetFxHome(const char* code);   // "" or unknown -> automatic
// The one actually used: the stored one, or the automatic choice - Czech ->
// CZK, Slovak -> EUR, English -> the currency of the mix country (EUR when it
// has none the sources know).
const char* Settings_FxHomeEff();
// Watched currencies as stored: "EUR,USD,GBP" or "" = the default for the home
// currency. Unknown codes and duplicates are dropped, at most FX_MAX kept.
void        Settings_SetFxList(const char* csv);
// The currencies to show, home currency left out: the stored list, or the
// default for the home currency. Returns how many were written to out.
int         Settings_FxCodesEff(char out[][4], int max);

// --- Cryptocurrencies (0.9.7) -----------------------------------------------
// The coins on offer, by symbol; Settings_CryptoId() gives CoinGecko's id.
#define CR_CODES   "BTC ETH SOL XRP BNB ADA DOGE TRX LTC DOT LINK USDT"
const char* Settings_CryptoId(const char* sym);     // nullptr when unknown
// As stored: "BTC,ETH" or "" = default (BTC, ETH, SOL, XRP).
void        Settings_SetCryptoList(const char* csv);
int         Settings_CryptoCodesEff(char out[][6], int max);
// Optional CoinGecko Demo API key. Never sent back to the page and not part
// of an export - only whether one is set.
const char* Settings_CgKey();
void        Settings_SetCgKey(const char* key);   // "" clears it

// --- Clock appearance -------------------------------------------------------
uint8_t  Settings_SecondsStyle();          // SEC_STYLE_*
void     Settings_SetSecondsStyle(uint8_t s);
uint16_t Settings_ClockColor();            // RGB565
void     Settings_SetClockColor(uint16_t c);
uint16_t Settings_SecondsColor();
void     Settings_SetSecondsColor(uint16_t c);
uint8_t  Settings_ClockFont();             // CLOCK_FONT_*
void     Settings_SetClockFont(uint8_t f);

// --- Altitude colours (0.8.0) -----------------------------------------------
// Three boundaries in METRES, ascending, and four RGB565 colours: band 0 is
// below the first boundary, band 3 above the last.
uint16_t Settings_AltBandM(uint8_t i);     // i = 0..2
void     Settings_SetAltBandsM(uint16_t b0, uint16_t b1, uint16_t b2);
uint16_t Settings_AltColor(uint8_t band);  // band = 0..3
void     Settings_SetAltColor(uint8_t band, uint16_t c);

// --- Aircraft filters and alerts --------------------------------------------
uint16_t Settings_AltMinFt();
uint16_t Settings_AltMaxFt();
void     Settings_SetAltRangeFt(uint16_t lo, uint16_t hi);
bool     Settings_OnlyWithCallsign();
void     Settings_SetOnlyWithCallsign(bool on);
bool     Settings_SquawkAlert();
void     Settings_SetSquawkAlert(bool on);
const char* Settings_WatchCallsign();      // "" = nothing watched
void     Settings_SetWatchCallsign(const char* s);

// --- UI state (debounced writes) --------------------------------------------
uint8_t Settings_PlaneRange();
void    Settings_SetPlaneRange(uint8_t idx);
uint8_t Settings_MeteoRange();
void    Settings_SetMeteoRange(uint8_t idx);
uint8_t Settings_Screen();
void    Settings_SetScreen(uint8_t idx);
uint16_t Settings_TopBearing();
void     Settings_SetTopBearing(uint16_t deg);

// --- Admin password (protects OTA, factory reset and config import) ---------
//
// Stored in the clear, which deserves an explanation: the firmware update page
// authenticates with HTTP Basic, and the library has to be handed the actual
// password to check it against - a digest cannot be used there. Hashing the
// copy in NVS while the same secret sits next to it in plain text would be
// security theatre, so it is stored once, honestly, and never leaves the
// device: it is not in the config JSON, not in the export, and the web page
// only ever learns whether one is set.
//
// This is a protection against the household, not against an attacker on the
// network. Anyone who can read the flash can read the password.
bool Settings_HasAdminPassword();
void Settings_SetAdminPassword(const char* plain);   // "" = protection off
bool Settings_CheckAdminPassword(const char* plain);
const char* Settings_AdminPassword();                // for HTTP Basic auth

// --- Serialisation ----------------------------------------------------------
// Everything except the password digest and the WiFi credentials. Used by the
// web UI, by the backup export and by the import.
void Settings_ToJson(JsonObject out);
// Applies whatever is present; missing keys keep their current value. Returns
// true when at least one value changed.
bool Settings_FromJson(JsonObjectConst in);

void Settings_Tick();       // call once per loop(); debounced NVS flush
void Settings_ClearAll();
