// =============================================================================
//  MeteoPlaneRadar
//  CST820 capacitive touch - interface.
//
//  Project: MeteoPlaneRadar - live aircraft radar on a round touchscreen
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
//  Web:     https://chiptron.cz
//  Board:   Waveshare ESP32-S3-Touch-LCD-2.1 (round 480x480 display, ST7701)
// =============================================================================
#pragma once
#include <Arduino.h>

#define CST820_ADDR     0x15
#define CST820_INT_PIN  16

struct TouchData {
  uint16_t x = 0;
  uint16_t y = 0;
  uint8_t  points = 0;   // 0 = no touch
};

bool Touch_Init();          // reset via EXIO2 + wake-up
void Touch_Read(TouchData* out);
