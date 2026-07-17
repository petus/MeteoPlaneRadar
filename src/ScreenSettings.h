// =============================================================================
//  MeteoPlaneRadar
//  Screen 2: settings - interface.
//
//  Project: MeteoPlaneRadar - live aircraft radar on a round touchscreen
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
//  Web:     https://chiptron.cz
//  Board:   Waveshare ESP32-S3-Touch-LCD-2.1 (round 480x480 display, ST7701)
// =============================================================================
#pragma once
#include <Arduino.h>

void ScreenSettings_Enter();
void ScreenSettings_Draw();
bool ScreenSettings_Tick();
bool ScreenSettings_HandleTap(int x, int y);

// Tells main that the user wants to launch the WiFi portal.
bool ScreenSettings_WantsPortal();
void ScreenSettings_ClearPortal();
