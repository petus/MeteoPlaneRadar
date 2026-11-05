// =============================================================================
//  MeteoPlaneRadar
//  A one-line health report per data source, for the web status page.
//
//  Diagnosing a device used to mean plugging in a USB cable and watching the
//  serial log. With the web UI running permanently there is somewhere better to
//  put it: each fetcher leaves a short note here, and the status page shows all
//  of them side by side - so "the weather screen is empty" can be answered
//  without touching the hardware.
//
//  Project: MeteoPlaneRadar - live aircraft radar on a round touchscreen
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
//  Web:     https://chiptron.cz
// =============================================================================
#pragma once
#include <Arduino.h>

enum StatusSlot : uint8_t {
  ST_ADSB = 0, ST_RADAR, ST_FORECAST,
  ST_PRICE,        // spot electricity price
  ST_MIX,          // generation mix
  ST_STORM,        // storm watch (0.8.0)
  ST_PLANEWATCH,   // aircraft-nearby watch (0.8.0)
  ST_COUNT
};

// printf-style; the text is truncated to something the page can show.
void Status_Set(StatusSlot slot, const char* fmt, ...);

// "OK, 12 letadel (pred 4 s)" - the note plus how long ago it was left.
void Status_Text(StatusSlot slot, char* out, size_t cap);
