// =============================================================================
//  MeteoPlaneRadar
//  Route geometry (0.9.7): does a route fit where the aircraft actually is?
//
//  adsb.lol answered that itself ("plausible"). adsbdb, the fallback, only
//  knows what a callsign usually flies - and callsigns get reused, so the
//  check has to happen here: an aircraft on a route is never far off the way
//  between the two airports. The test is an ellipse with the airports as its
//  foci: the detour origin -> aircraft -> destination may be at most 25 %
//  longer than the direct distance, plus 150 km for the climb-out, holding
//  and approach near the airports.
//
//      Bucharest -> Liege, aircraft over Prague: 1080 + 650 km against 1650
//      direct - fits.  Athens -> Istanbul over Prague: 1530 + 1250 against
//      560 - rejected, which is the mistake this check exists for.
//
//  Header only, no Arduino - tools can test it on a PC.
//
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
// =============================================================================
#pragma once
#include <math.h>

#define ROUTE_DETOUR_FACTOR 1.25f
#define ROUTE_DETOUR_KM     150.0f

// Great-circle distance in km. Only compared with itself, so the exact Earth
// radius does not matter.
static inline float RouteGeo_Km(float lat1, float lon1, float lat2, float lon2) {
  const float R = 6371.0f, D = 0.017453293f;
  const float dLat = (lat2 - lat1) * D, dLon = (lon2 - lon1) * D;
  float a = sinf(dLat * 0.5f) * sinf(dLat * 0.5f) +
            cosf(lat1 * D) * cosf(lat2 * D) * sinf(dLon * 0.5f) * sinf(dLon * 0.5f);
  if (a < 0) a = 0; else if (a > 1) a = 1;
  return 2.0f * R * asinf(sqrtf(a));
}

static inline bool RouteGeo_Plausible(float pLat, float pLon,
                                      float oLat, float oLon, float dLat, float dLon) {
  const float direct = RouteGeo_Km(oLat, oLon, dLat, dLon);
  const float via = RouteGeo_Km(oLat, oLon, pLat, pLon) + RouteGeo_Km(pLat, pLon, dLat, dLon);
  return via <= direct * ROUTE_DETOUR_FACTOR + ROUTE_DETOUR_KM;
}
