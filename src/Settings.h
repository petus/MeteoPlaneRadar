// =============================================================================
//  MeteoPlaneRadar
//  Persisting settings - interface.
//
//  Project: MeteoPlaneRadar - live aircraft radar on a round touchscreen
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
//           Ondra OK1CDJ / apps.ok1cdj.com
//  Web:     https://chiptron.cz
//  Board:   Waveshare ESP32-S3-Touch-LCD-2.1 (round 480x480 display, ST7701)
// =============================================================================
#pragma once
#include <Arduino.h>
#include "Config.h"   // DEFAULT_LAT / DEFAULT_LON

void   Settings_Begin();

double Settings_Lat();
double Settings_Lon();
bool   Settings_HasLocation();
void   Settings_SetLocation(double lat, double lon);

uint8_t Settings_Backlight();
void    Settings_SetBacklight(uint8_t pct);

// Units in the aircraft detail: false = aviation (ft/kt), true = metric (m/kmh).
bool    Settings_MetricUnits();
void    Settings_SetMetricUnits(bool metric);

// UI state persisted across restarts: range index per screen + active screen.
// Written debounced (see Settings_Tick) so swiping does not hammer the flash.
uint8_t Settings_PlaneRange();
void    Settings_SetPlaneRange(uint8_t idx);
uint8_t Settings_MeteoRange();
void    Settings_SetMeteoRange(uint8_t idx);
uint8_t Settings_AprsRange();
void    Settings_SetAprsRange(uint8_t idx);
uint8_t Settings_Screen();
void    Settings_SetScreen(uint8_t idx);

// APRS station to display + aprs.fi API key. Entered in the WiFi portal, written
// immediately (they change rarely). Empty callsign = the APRS screen is idle.
const char* Settings_AprsCall();
void        Settings_SetAprsCall(const char* call);
const char* Settings_AprsKey();
void        Settings_SetAprsKey(const char* key);

// Which compass bearing is shown at the TOP of the aircraft radar, in degrees
// (0..359, multiples of MAP_ROT_STEP_DEG). 0 = north up, 90 = looking east.
// You set the direction you are actually looking, not an amount to turn by.
uint16_t Settings_TopBearing();
void     Settings_SetTopBearing(uint16_t deg);

// Call once per loop(); flushes pending UI-state changes to NVS after a short
// idle delay (so a swipe does not trigger a flash write every time).
void    Settings_Tick();

void   Settings_ClearAll();
