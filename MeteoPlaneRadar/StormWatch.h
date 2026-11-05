// =============================================================================
//  MeteoPlaneRadar
//  Storm watch (0.8.0) - is there rain or a storm near the device right now?
//
//  Every five minutes the newest CHMU radar frame is fetched (one PNG of a few
//  tens of kB) and the pixels within the configured distance of the device are
//  read back into dBZ through the CHMU colour scale. If enough of them reach
//  the configured threshold, the storm is "on" and the weather radar joins the
//  automatic screen cycling; it drops out again after a couple of clear frames.
//
//  It runs only when something wants the answer: the "radar only when a storm
//  is coming" option, or the Home Assistant endpoint. With both off it never
//  makes a request.
//
//  When the weather screen is showing CHMU frames anyway, the newest of those is
//  analysed instead of downloading the same picture a second time.
//
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
// =============================================================================
#pragma once
#include <Arduino.h>

void  StormWatch_Tick();          // call from loop(); does a fetch when due
bool  StormWatch_Active();        // storm on: the radar belongs in the cycling
bool  StormWatch_Valid();         // at least one frame has been analysed
float StormWatch_MaxDbz();        // strongest echo within the radius, -1 = none
float StormWatch_NearestKm();     // nearest echo at/above the threshold, -1 = none
int   StormWatch_Pixels();        // how many km2 at/above the threshold
bool  StormWatch_InCoverage();    // the device lies inside the CHMU picture
String StormWatch_FrameTime();    // HH:MM of the analysed frame, "" if none
