// =============================================================================
//  MeteoPlaneRadar
//  Vector weather icons drawn from a WMO code.
//
//  Vector rather than bitmaps on purpose: an icon set at three sizes would be
//  a few hundred kilobytes of flash and would still look wrong at a fourth
//  size. These are a handful of circles and lines, scale to any radius, and
//  cost nothing to store.
//
//  Project: MeteoPlaneRadar - live aircraft radar on a round touchscreen
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
//  Web:     https://chiptron.cz
// =============================================================================
#pragma once
#include <Arduino.h>

// Coarse groups - everything the drawing code actually distinguishes.
enum WxKind : uint8_t {
  WX_CLEAR = 0, WX_PARTLY, WX_CLOUDY, WX_FOG,
  WX_DRIZZLE, WX_RAIN, WX_SNOW, WX_SHOWER, WX_STORM
};


// Draw centred on (cx, cy) fitting inside a box of 2*r. night = draw a moon
// instead of a sun where the icon has one.
void WxIcon_Draw(int cx, int cy, int r, int wmoCode, bool night);

// 0.9.7: the Moon as it looks tonight - phase 0 new, 0.25 first quarter,
// 0.5 full, 0.75 last quarter. south = southern hemisphere, where the lit
// side is the other one.
void WxIcon_Moon(int cx, int cy, int r, double phase, bool south);
// A small up (rise) or down (set) triangle, w px wide, centred on cy.
void WxIcon_RiseSet(int x, int cy, int w, bool up);

// A colour that suits the condition - used for the temperature next to it.
uint16_t WxIcon_Color(int wmoCode);
