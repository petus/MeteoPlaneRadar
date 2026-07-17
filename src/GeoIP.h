// =============================================================================
//  MeteoPlaneRadar
//  Automatic location detection - interface.
//
//  Project: MeteoPlaneRadar - live aircraft radar on a round touchscreen
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
//  Web:     https://chiptron.cz
//  Board:   Waveshare ESP32-S3-Touch-LCD-2.1 (round 480x480 display, ST7701)
// =============================================================================
#pragma once
#include <Arduino.h>

// If no location is stored yet, try to determine it from the IP and save it.
// Called after connecting to WiFi. Returns true if a location was filled in.
bool GeoIP_DetectIfNeeded();
