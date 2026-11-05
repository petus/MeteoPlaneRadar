// =============================================================================
//  MeteoPlaneRadar
//  Distance and bearing between two points (0.8.0).
//
//  The flat-earth approximation the radar projection already uses: accurate to
//  well under a per cent at the hundred-odd kilometres anything here cares
//  about, and no trigonometry beyond one cosine and one atan2.
//
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
// =============================================================================
#pragma once
#include <math.h>

// km from (lat0, lon0) to (lat1, lon1), and the compass bearing from the first
// point to the second (0 = north, 90 = east).
static inline void Geo_DistBearing(double lat0, double lon0, double lat1, double lon1,
                                   float* km, float* bearingDeg) {
  const float dx = (float)((lon1 - lon0) * 111.0 * cos(lat0 * 0.017453292519943));
  const float dy = (float)((lat1 - lat0) * 111.0);
  if (km) *km = sqrtf(dx * dx + dy * dy);
  if (bearingDeg) {
    float b = atan2f(dx, dy) * 57.2957795f;
    if (b < 0) b += 360.0f;
    *bearingDeg = b;
  }
}

// Smallest angle between two bearings, 0..180.
static inline float Geo_AngleDiff(float a, float b) {
  float d = fmodf(fabsf(a - b), 360.0f);
  return d > 180.0f ? 360.0f - d : d;
}
