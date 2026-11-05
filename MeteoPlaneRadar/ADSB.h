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
#include "Config.h"   // ADSB_MAX

#define ADSB_API_BASE "https://opendata.adsb.fi/api/v3/lat/"

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
  // Registration ("OK-TVU"), from the "r" field of the same adsb.fi answer.
  // It rides along for free in data we download anyway, which is why the
  // separate airframe lookup - a second request per aircraft, and a second API
  // to depend on - could be dropped entirely. The longest real registrations
  // are ten characters, so eleven plus the terminator is plenty.
  char  reg[12] = "";
  // Transponder code as four octal digits. Kept as text, not a number: 7700 is
  // an octal code, and "0021" must not turn into 21.
  char  squawk[6] = "";
  // ADS-B emitter category ("A3" airliner, "A7" helicopter, "B1" glider...).
  // Only a fallback for the icon when the type code is unknown - see
  // PlaneIcon.cpp. Empty when the transponder does not send it.
  char  cat[3] = "";
  uint8_t icon = 0;           // PlaneIconKind, worked out once per fetch
  bool  onGround = false;
  bool  hasTrack = false;     // false = track unknown (drawn differently)
};

void   ADSB_SetPollFn(void (*fn)());

// Fetch aircraft within radiusKm of the location. Returns true on success.
bool   ADSB_Fetch(double lat, double lon, float radiusKm);

// --- Overload protection (0.8.4) - see the note in Config.h ----------------
// ms since the last successful fetch; ULONG_MAX-ish when there never was one.
unsigned long ADSB_DataAgeMs();
bool   ADSB_DataStale();              // older than ADSB_STALE_MS
bool   ADSB_DataHidden();             // older than ADSB_HIDE_MS (or none yet)
// > 0 while the gate is closed: ms until the next request is allowed.
unsigned long ADSB_PauseLeftMs();
int    ADSB_LastHttpCode();           // last HTTP status (0 = no answer at all)

int    ADSB_Count();
const Aircraft* ADSB_List();

// Look up an aircraft by its ICAO hex address. Returns the current index in the
// list, or -1 if that aircraft is not in the latest data (it left the area, or
// fell outside the ADSB_MAX cap). Use this instead of holding on to an index
// across fetches - the list is rebuilt every time and the order is not stable.
int    ADSB_FindByHex(const char* hex);

// Is this aircraft squawking an emergency code?
//   7500 unlawful interference (hijack), 7600 radio failure, 7700 general
// Returns the code, or nullptr when it is not one of them.
const char* ADSB_EmergencyCode(const Aircraft& a);
