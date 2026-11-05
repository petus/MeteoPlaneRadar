// =============================================================================
//  MeteoPlaneRadar
//  Screen 1: aircraft radar - interface.
//
//  Project: MeteoPlaneRadar - live aircraft radar on a round touchscreen
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
//  Web:     https://chiptron.cz
//  Board:   Waveshare ESP32-S3-Touch-LCD-2.1 (round 480x480 display, ST7701)
// =============================================================================
#pragma once
#include <Arduino.h>

void ScreenPlanes_Enter();
void ScreenPlanes_Draw();
bool ScreenPlanes_Tick();                    // true = needs a redraw

// Short tap - select an aircraft / close the detail panel.
bool ScreenPlanes_HandleTap(int x, int y);

// Swipe - change the range (dir = +1 / -1).
void ScreenPlanes_ChangeRange(int dir);

// The current range as text ("25 km"), for the web UI.
void ScreenPlanes_RangeText(char* out, size_t cap);

// Close the aircraft detail panel (used by the long-press screen switch).
void ScreenPlanes_CloseDetail();

// Is the aircraft detail open? (main then blocks range change / screen switch)
bool ScreenPlanes_DetailOpen();

// --- Aircraft-nearby alert (0.8.0) ------------------------------------------
// Called right after ScreenPlanes_Enter() when the screen is brought up by the
// alert: opens the detail of that aircraft and picks a range that fits it,
// without remembering the range. AlertEnd() closes the detail and puts the
// user's own range back. A swipe during the alert ends alert mode by itself.
void ScreenPlanes_AlertBegin(const char* hex, float distKm, bool inWindow);
void ScreenPlanes_AlertEnd();
bool ScreenPlanes_AlertActive();
