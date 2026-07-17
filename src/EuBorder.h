// =============================================================================
//  MeteoPlaneRadar
//  Map underlay - interface for drawing European borders and cities.
//
//  Project: MeteoPlaneRadar - live aircraft radar on a round touchscreen
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
//  Web:     https://chiptron.cz
//  Board:   Waveshare ESP32-S3-Touch-LCD-2.1 (round 480x480 display, ST7701)
// =============================================================================
#pragma once
#include <Arduino.h>

// Callback: converts lat/lon to screen coordinates (supplied by ScreenPlanes).
typedef void (*ProjectFn)(float lat, float lon, int* sx, int* sy);

// The map data covers the whole of Europe (~31k border points, 1100 cities), but
// at a 100 km range only a sliver of that is ever on screen. Both calls below
// therefore take the visible geographic window (in degrees) and discard anything
// outside it *before* touching the framebuffer. Without this culling the draw
// would crawl; with it, only a few dozen points survive per frame.
//
// lat0/lat1 and lon0/lon1 bound the visible area, with a small margin so that
// lines running just off-screen still get drawn.

// Draw the country outlines in the given colour.
void EuBorder_Draw(ProjectFn project, uint16_t color,
                   float lat0, float lat1, float lon0, float lon1);

// Draw the cities (dot + name/abbreviation).
// cx, cy, radius = the display circle (cities outside it are skipped).
// showFull = true -> full names, false -> abbreviations.
// maxTier  = the least important tier still drawn (1 = only the largest cities,
//            3 = everything down to 50k). Keeps dense areas legible when zoomed out.
void EuBorder_DrawCities(ProjectFn project, int cx, int cy, int radius,
                         uint16_t dotColor, uint16_t textColor,
                         bool showFull, uint8_t maxTier,
                         float lat0, float lat1, float lon0, float lon1);
