// =============================================================================
//  MeteoPlaneRadar
//  Screen 3: APRS station on the map - interface.
//
//  Project: MeteoPlaneRadar - live aircraft radar on a round touchscreen
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
//           Ondra OK1CDJ / apps.ok1cdj.com
//  Web:     https://chiptron.cz
//  Board:   Waveshare ESP32-S3-Touch-LCD-2.1 (round 480x480 display, ST7701)
// =============================================================================
#pragma once
#include <Arduino.h>

void ScreenAPRS_Enter();
void ScreenAPRS_Draw();
bool ScreenAPRS_Tick();                 // true = needs a redraw

// Swipe - change the range (dir = +1 / -1), same as the radar screen.
void ScreenAPRS_ChangeRange(int dir);
