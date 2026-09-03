// =============================================================================
//  MeteoPlaneRadar
//  CST820 capacitive touch (ESP32-S3-Touch-LCD-2.1) - interface.
//
//  Project: MeteoPlaneRadar - live aircraft radar on a round touchscreen
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
//  Web:     https://chiptron.cz
//  Board:   Waveshare ESP32-S3-Touch-LCD-2.1 (round 480x480 display, ST7701)
// =============================================================================
#pragma once
#include <Arduino.h>
#include "Touch.h"

#define CST820_ADDR     0x15
#define CST820_INT_PIN  16

// Is a CST820 answering? Used by Board_Detect() to tell the two boards apart,
// which is why it is a plain register read and nothing else - no reset, and in
// particular nothing done to the INT pin, which on this board is the chip's own
// output. Board.cpp resets the controller before calling this.
bool CST820_Probe();

bool CST820_Init();          // reset via EXIO2 + wake-up
void CST820_Read(TouchData* out);
