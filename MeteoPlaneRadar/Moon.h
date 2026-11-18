// =============================================================================
//  MeteoPlaneRadar
//  Moonrise, moonset and the phase of the Moon (0.9.7), worked out on the
//  device - no request, no API. The method is the low-precision one used by
//  the SunCalc library (V. Agafonkin, BSD-2): the Moon's position from the
//  main terms of its orbit, its altitude every hour of the day, and a
//  quadratic through each pair of hours to find where it crosses the horizon.
//  Good to a few minutes, which is what a clock face needs.
//
//  The place is the one the forecast uses (Settings_Lat/Lon): set in the web
//  UI, or found from the IP address.
//
//  No Arduino here, so it runs on a PC too: during development it was checked
//  against the PyEphem ephemeris over a year in Prague, Bratislava, Tromso and
//  Sydney - typically within half a minute, 95 % within 1.6 minutes.
//
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
// =============================================================================
#pragma once
#include <time.h>

// Rise and set during the local day that starts at dayStart (local midnight,
// as mktime() gives it). Either may be 0: on some days the Moon only rises or
// only sets - it comes up about 50 minutes later each day.
void Moon_Times(time_t dayStart, double lat, double lon, time_t* rise, time_t* set);

// Phase at t: 0 new, 0.25 first quarter, 0.5 full, 0.75 last quarter.
double Moon_Phase(time_t t);
