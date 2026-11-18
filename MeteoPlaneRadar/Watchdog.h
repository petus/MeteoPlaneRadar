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
#include "Config.h"   // WDT_TIMEOUT_S

void Watchdog_Begin();
void Watchdog_Feed();

// --- 0.9.2: stuck-loop guard ------------------------------------------------------
// loop() calls Watchdog_LoopStart() first thing on every pass. Code that waits
// on the network feeds through Watchdog_PollFeed() instead of Watchdog_Feed():
// it feeds only while the current pass is younger than WDT_LOOP_MAX_MS. After
// that it writes down `what` (memory that survives a restart) and stops, so the
// watchdog restarts the board instead of the screen freezing for good.
void Watchdog_LoopStart();
void Watchdog_PollFeed(const char* what);
// After a restart the guard caused: what was hanging ("ADSB", "CHMU" ...).
// nullptr otherwise.
const char* Watchdog_StuckWhat();

