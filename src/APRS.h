// =============================================================================
//  MeteoPlaneRadar
//  APRS client - interface (one station's position from aprs.fi).
//
//  Project: MeteoPlaneRadar - live aircraft radar on a round touchscreen
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
//           Ondra OK1CDJ / apps.ok1cdj.com
//  Web:     https://chiptron.cz
//  Board:   Waveshare ESP32-S3-Touch-LCD-2.1 (round 480x480 display, ST7701)
// =============================================================================
#pragma once
#include <Arduino.h>
#include "Config.h"   // APRS_API_BASE

struct AprsStation {
  double lat = 0;
  double lon = 0;
  char   name[16] = "";
  char   comment[48] = "";
  float  course = 0;        // course over ground (deg), if the station is moving
  float  speedKmh = 0;      // speed (km/h) - aprs.fi already reports metric
  long   lastTimeUtc = 0;   // "lasttime" epoch seconds (kept, but not displayed:
                            // there is no wall clock in this firmware, see .ino)
  bool   hasPos = false;
};

void   APRS_SetPollFn(void (*fn)());

// Fetch the given callsign via aprs.fi. Returns true when the HTTP round-trip
// succeeded (even if the station was "not found" - inspect APRS_HasFix() /
// APRS_Status() for that); false only on a network/transport error, so the
// caller can keep the previous snapshot and back off.
bool   APRS_Fetch(const char* call, const char* apikey);

const AprsStation* APRS_Get();    // last snapshot (check hasPos / APRS_HasFix)
bool   APRS_HasFix();             // true = we have a real position to draw
const char* APRS_Status();        // short human message when there is no fix
