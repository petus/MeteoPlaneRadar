// =============================================================================
//  MeteoPlaneRadar
//  Hardware watchdog - interface.
//
//  Project: MeteoPlaneRadar - live aircraft radar on a round touchscreen
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
//  Web:     https://chiptron.cz
//  Board:   Waveshare ESP32-S3-Touch-LCD-2.1 (round 480x480 display, ST7701)
// =============================================================================
#pragma once
#include <Arduino.h>

#define WDT_TIMEOUT_S 20   // reboot after 20 s of being stuck

void Watchdog_Begin();
void Watchdog_Feed();
void Watchdog_Suspend();   // before a blocking operation (WiFi portal)
void Watchdog_Resume();
