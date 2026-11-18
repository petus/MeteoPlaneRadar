// =============================================================================
//  MeteoPlaneRadar
//  Weather forecast, sunrise/sunset and air quality - all from Open-Meteo.
//  Free, no key, no registration.
//
//  ONE request serves three features, which is why they share a module:
//    - the forecast screen (next hours + next days),
//    - the current temperature and precipitation on the clock screen,
//    - sunrise and sunset, which drive the automatic night mode.
//
//  Times are requested as plain UTC epochs (timezone=UTC, timeformat=unixtime)
//  rather than local ones. Our own clock is a UTC epoch too (seeded from the
//  HTTP Date header), so everything can be compared directly and localtime_r()
//  does the conversion for display exactly once. Asking Open-Meteo for local
//  times as well is how you end up an hour out twice a year.
//
//  Project: MeteoPlaneRadar - live aircraft radar on a round touchscreen
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
//  Web:     https://chiptron.cz
// =============================================================================
#pragma once
#include <Arduino.h>
#include <time.h>
#include "Config.h"

struct FcHour {
  time_t t     = 0;
  float  temp  = 0;    // degC
  float  precip = 0;   // mm
  float  wind  = 0;    // km/h
  int    code  = 0;    // WMO weather code
};

struct FcDay {
  time_t t      = 0;
  float  tmax   = 0;
  float  tmin   = 0;
  float  precip = 0;
  float  wind   = 0;
  int    code   = 0;
  time_t sunrise = 0;
  time_t sunset  = 0;
};

// Call from loop(). Fetches when due, cheap no-op otherwise.
void Forecast_Tick();

// Ask for a refresh on the next tick (after the location changed).
void Forecast_Invalidate();

bool Forecast_Valid();
struct SrcHealth;
const SrcHealth& Forecast_Health();   // 0.9.7

// Hours from NOW: index 0 is the hour we are in, index 3 is three hours out.
// forecast_hours makes the API line the array up that way, so no searching.
int  Forecast_HourCount();
const FcHour* Forecast_Hours();

// Days from TODAY: index 0 is today, 1 tomorrow, 2 the day after. (Up to
// 0.6.6 index 0 was tomorrow - anything that counts days off this array had to
// be checked when that changed.)
int  Forecast_DayCount();
const FcDay*  Forecast_Days();

// Current conditions (the "current" block of the same response).
bool  Forecast_CurrentValid();
float Forecast_CurrentTemp();
float Forecast_CurrentPrecip();
float Forecast_CurrentWind();     // km/h
int   Forecast_CurrentCode();

// Today's sun times as UTC epochs. False when nothing has been fetched yet, in
// which case night mode has to fall back on the manual setting.
bool Forecast_SunTimes(time_t* rise, time_t* set);

// --- Air quality (separate endpoint, same idea) ------------------------------
bool  AirQuality_Valid();
int   AirQuality_Aqi();        // European AQI, -1 when unknown
float AirQuality_Pm25();
// Highest of the three pollen counts (grains/m3), or -1 outside Europe / out of
// season, where the API simply returns nothing.
float AirQuality_PollenMax();
const char* AirQuality_PollenWorst();   // "brizа"/"birch" etc., "" when unknown
