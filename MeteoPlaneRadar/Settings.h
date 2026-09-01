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
const char* Settings_WifiSsid();
const char* Settings_WifiPass();
bool        Settings_HasWifi();
void        Settings_SetWifi(const char* ssid, const char* pass);
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
uint8_t Settings_RadarSource();            // RADAR_SRC_CHMU / RADAR_SRC_RAINVIEWER
void    Settings_SetRadarSource(uint8_t s);
// The dBZ / mm/h scale down the left of the weather screen. On by default: it
// is what makes the colours mean anything. Off for those who know the scale by
// heart and would rather see the map underneath it.
bool    Settings_MeteoLegend();
void    Settings_SetMeteoLegend(bool on);

// --- Clock appearance -------------------------------------------------------
uint8_t  Settings_SecondsStyle();          // SEC_STYLE_*
void     Settings_SetSecondsStyle(uint8_t s);
uint16_t Settings_ClockColor();            // RGB565
void     Settings_SetClockColor(uint16_t c);
uint16_t Settings_SecondsColor();
void     Settings_SetSecondsColor(uint16_t c);

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
