// =============================================================================
//  MeteoPlaneRadar
//  ADS-B client - interface (Aircraft struct, fetching).
//
//  Project: MeteoPlaneRadar - live aircraft radar on a round touchscreen
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
//  Web:     https://chiptron.cz
//  Board:   Waveshare ESP32-S3-Touch-LCD-2.1 (round 480x480 display, ST7701)
// =============================================================================
#pragma once
#include <Arduino.h>

#define ADSB_API_BASE "https://opendata.adsb.fi/api/v3/lat/"
#define ADSB_MAX 40           // cap on aircraft to draw

struct Aircraft {
  float lat = 0;
  float lon = 0;
  float track = 0;            // ground track (degrees)
  float altFt = 0;            // altitude in feet (barometric)
  float gsKt = 0;             // ground speed (knots)
  float baroRate = 0;         // climb/descent rate (ft/min)
  char  callsign[10] = "";
  // ICAO 24-bit address, e.g. "49d0d1". This is the aircraft's *identity*: it
  // is tied to the airframe and does not change between fetches, unlike the
  // position in the list, which adsb.fi may reorder at will. The selection in
  // ScreenPlanes is therefore keyed on this, never on an array index.
  // 8 bytes because adsb.fi prefixes non-ICAO targets (TIS-B/ADS-R) with '~',
  // giving 7 characters plus the terminator.
  char  hex[8] = "";
  char  type[10] = "";        // aircraft type (e.g. A320), if available
  bool  onGround = false;
  bool  hasTrack = false;     // false = track unknown (drawn differently)
};

void   ADSB_SetPollFn(void (*fn)());

// Fetch aircraft within radiusKm of the location. Returns true on success.
bool   ADSB_Fetch(double lat, double lon, float radiusKm);

int    ADSB_Count();
const Aircraft* ADSB_List();

// Look up an aircraft by its ICAO hex address. Returns the current index in the
// list, or -1 if that aircraft is not in the latest data (it left the area, or
// fell outside the ADSB_MAX cap). Use this instead of holding on to an index
// across fetches - the list is rebuilt every time and the order is not stable.
int    ADSB_FindByHex(const char* hex);
