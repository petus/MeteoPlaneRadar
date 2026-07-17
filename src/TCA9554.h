// =============================================================================
//  MeteoPlaneRadar
//  TCA9554 I/O expander - interface and EXIO pins.
//
//  Project: MeteoPlaneRadar - live aircraft radar on a round touchscreen
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
//  Web:     https://chiptron.cz
//  Board:   Waveshare ESP32-S3-Touch-LCD-2.1 (round 480x480 display, ST7701)
// =============================================================================
#pragma once
#include <Arduino.h>
#include <Wire.h>

#define TCA9554_ADDR        0x20
#define TCA9554_INPUT_REG   0x00
#define TCA9554_OUTPUT_REG  0x01
#define TCA9554_CONFIG_REG  0x03

// EXIO pins (1-8, using Waveshare's numbering)
#define EXIO_LCD_RST   1
#define EXIO_TOUCH_RST 2
#define EXIO_LCD_CS    3
#define EXIO_LCD_PWR   8

void     TCA9554_Init();                       // all EXIO pins as outputs
void     TCA9554_SetPin(uint8_t pin, bool high);
uint8_t  TCA9554_ReadOutput();
