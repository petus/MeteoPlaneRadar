// =============================================================================
//  MeteoPlaneRadar
//  Aircraft icons by type (0.8.0).
//
//  The radar used to draw every aircraft as the same winged arrow. Now the
//  icon follows the airframe, after the scheme adsb-radar.com uses
//  (https://adsb-radar.com/help/icons.html): a helicopter looks like a
//  helicopter, an A380 is visibly bigger than a Cessna, a glider has its long
//  thin wings.
//
//  How the kind is picked, first match wins:
//    1. the ICAO type code from adsb.fi ("t", e.g. A20N, B738, EC35) against the
//       type lists from that page - with its few obvious copy errors fixed
//       (the B767 tile listing 737s, A350 among the four-engined, the MD11
//       tile repeating the ERJ list, MD90 among the helicopters);
//    2. a few prefixes for types the lists do not name (B73x, A32x, E1xx...);
//    3. the ADS-B emitter category (A1 light ... A7 rotorcraft, B1 glider...),
//       which every transponder sends even when the airframe database has
//       never heard of the aircraft;
//    4. the old arrow.
//
//  The result is cached in Aircraft::icon at parse time, so drawing a hundred
//  aircraft twelve times a second costs no string compares.
//
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
// =============================================================================
#pragma once
#include <Arduino.h>

enum PlaneIconKind : uint8_t {
  PI_GENERIC = 0,   // the original arrow
  PI_LIGHT,         // Cessna, Piper, ultralights - small, straight high wing
  PI_TWINPROP,      // Dash 8, ATR, King Air, L410
  PI_BIZJET,        // Learjet, Citation, Falcon, Gulfstream - rear engines
  PI_REGIONAL,      // CRJ, ERJ, E-Jets, Fokker 100, BAe 146
  PI_NARROW,        // A320 family, B737, B757
  PI_WIDE,          // A330, A350, B767, B777, B787, MD-11
  PI_QUAD,          // A340, A380, B747
  PI_MILTRANS,      // C-130, C-160, A400M - high wing, four props
  PI_FREIGHTER,     // An-124, C-5, C-17, Il-76 - heavy, high wing jets
  PI_FIGHTER,       // delta / swept fighters and jet trainers
  PI_HELI,          // rotorcraft
  PI_TILTROTOR,     // V-22, AW609
  PI_GLIDER,        // sailplanes, motor gliders
  PI_BALLOON,       // lighter than air
  PI_DRONE,         // UAV
  PI_KIND_N
};

// type = ICAO type designator ("" if unknown), cat = emitter category ("" if
// unknown). Case does not matter.
uint8_t PlaneIcon_Classify(const char* type, const char* cat);

// Draw the icon centred on (x, y), nose along trackDeg (0 = up, clockwise,
// already corrected for the map rotation). hasTrack = false draws the
// "direction unknown" circle as before, whatever the kind.
void PlaneIcon_Draw(int x, int y, float trackDeg, bool hasTrack, uint8_t kind, uint16_t col);

// Short name of the kind for the detail panel / Home Assistant ("helicopter").
const char* PlaneIcon_Name(uint8_t kind);
