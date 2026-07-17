// =============================================================================
//  MeteoPlaneRadar
//  Persisting settings - interface.
//
//  Project: MeteoPlaneRadar - live aircraft radar on a round touchscreen
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
//  Web:     https://chiptron.cz
//  Board:   Waveshare ESP32-S3-Touch-LCD-2.1 (round 480x480 display, ST7701)
// =============================================================================
#pragma once
#include <Arduino.h>

// Default location (Prague) - overwritten on first boot by geolocation, or manually.
#define DEFAULT_LAT 50.0755
#define DEFAULT_LON 14.4378

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

void   Settings_ClearAll();
