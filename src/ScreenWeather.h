// =============================================================================
//  MeteoPlaneRadar
//  Screen: CHMU precipitation radar (meteoradar) with animation - interface.
//
//  Full-screen Web-Mercator crop of the CHMU precipitation composite, masked to
//  the round display, with the CR outline + cities as an underlay, a dBZ/mm/h
//  legend and a 6-frame animation with a per-frame time indicator.
//
//  Project: MeteoPlaneRadar - live aircraft radar on a round touchscreen
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
//  Board:   Waveshare ESP32-S3-Touch-LCD-2.1 (round 480x480 display, ST7701)
// =============================================================================
#pragma once
#include <Arduino.h>

void ScreenWeather_Enter();
bool ScreenWeather_Tick();          // downloads/animates; true = needs redraw
void ScreenWeather_Draw();
void ScreenWeather_ChangeRange(int dir);   // swipe: change the range
