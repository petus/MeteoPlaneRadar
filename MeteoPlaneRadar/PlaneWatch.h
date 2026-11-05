// =============================================================================
//  MeteoPlaneRadar
//  Aircraft-nearby watch (0.8.0).
//
//  While the aircraft screen is not showing, nobody asks adsb.fi for anything -
//  so nobody would notice a plane flying right over the house. This keeps a
//  small, cheap poll going (only the alert radius or the window distance) and
//  picks the aircraft worth interrupting the cycling for:
//
//    1. one in the WINDOW VIEW - within the window's distance and inside its
//       angle - even if it is further away than another one;
//    2. otherwise the nearest one within the alert radius.
//
//  "In view" is about where you can actually look, which is why it outranks
//  plain distance: a plane 3 km behind the house is closer than one 9 km out in
//  front of the window, and you will only ever see the second.
//
//  Each aircraft is announced once (remembered for PLANE_ALERT_FORGET_MS). The
//  altitude and callsign filters from the web UI apply - if you filtered
//  something off the map, it should not interrupt you either.
//
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
// =============================================================================
#pragma once
#include <Arduino.h>

// planesShowing = the aircraft screen is on the display right now; it does its
// own fetching then, so the watch stays quiet.
void PlaneWatch_Tick(bool planesShowing);

// A new aircraft worth an alert? Fills hex/km/inWindow and remembers it as
// announced. false = nothing new.
bool PlaneWatch_Take(char* hex, size_t hexCap, float* km, bool* inWindow);

// For Home Assistant: the aircraft the alert logic would pick right now, even
// if it was already announced. Index into ADSB_List(), or -1.
int  PlaneWatch_Best(float* km, float* bearing, bool* inWindow);
