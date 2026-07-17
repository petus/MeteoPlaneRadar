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

// Close the aircraft detail panel (used by the long-press screen switch).
void ScreenPlanes_CloseDetail();

// Is the aircraft detail open? (main then blocks range change / screen switch)
bool ScreenPlanes_DetailOpen();
