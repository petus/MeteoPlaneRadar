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
uint8_t Settings_BrightNight();
void    Settings_SetBrightDay(uint8_t pct);
void    Settings_SetBrightNight(uint8_t pct);
bool    Settings_NightAuto();
void    Settings_SetNightAuto(bool on);
int8_t  Settings_NightOffsetMin();     // +/- minutes around sunrise/sunset
void    Settings_SetNightOffsetMin(int8_t m);

// Which level is in force. Set by NightMode; read by everything that draws.
bool    Settings_IsNight();
void    Settings_SetNight(bool night);

// --- Units, language --------------------------------------------------------
bool    Settings_MetricUnits();
void    Settings_SetMetricUnits(bool metric);
uint8_t Settings_Language();               // LANG_CZ / LANG_EN
void    Settings_SetLanguage(uint8_t l);

// --- Screens ----------------------------------------------------------------
// idx is SCREEN_CLOCK_I .. SCREEN_FORECAST_I. Settings is always enabled.
bool    Settings_ScreenEnabled(uint8_t idx);
void    Settings_SetScreenEnabled(uint8_t idx, bool on);
uint8_t Settings_EnabledCount();           // data screens currently on
// Automatic screen cycling, in SECONDS. 0 = off.
//
// This was minutes up to 0.6.0 and is stored under a NEW key, because
// reinterpreting the old one would silently turn "every 3 minutes" into "every
// 3 seconds" on update. Settings_Begin() converts the old value once.
uint16_t Settings_AutoRotateSec();
void     Settings_SetAutoRotateSec(uint16_t s);

// --- Weather radar ----------------------------------------------------------
uint8_t Settings_RadarSource();            // RADAR_SRC_CHMU / _RAINVIEWER / _CHMU_MASKED
void    Settings_SetRadarSource(uint8_t s);
bool    Settings_RadarIsChmu();            // either of the two CHMU variants

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
bool     Settings_WindowOn();
void     Settings_SetWindowOn(bool on);
uint16_t Settings_WindowAz();              // bearing of the centre, 0..359
void     Settings_SetWindowAz(uint16_t deg);
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
