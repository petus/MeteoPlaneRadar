// =============================================================================
//  MeteoPlaneRadar
//  TCA9554 I/O expander - interface and EXIO pins.
//
//  Project: MeteoPlaneRadar - live aircraft radar on a round touchscreen
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
//  Web:     https://chiptron.cz
//  Boards:  Waveshare ESP32-S3-Touch-LCD-2.1 and -2.8C (identical here:
//           same chip, same address, same EXIO assignment on both)
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
bool     TCA9554_SetPin(uint8_t pin, bool high);   // false = the I2C write failed

// Reads the expander back and repairs it if it does not match what we last
// wrote. This chip holds LCD reset, LCD chip-select and LCD POWER, so a single
// corrupted I2C write here blanks the display for good while the sketch happily
// carries on - no watchdog, no crash, nothing in the log. Call it periodically.
// Returns false if a repair was needed (or the expander did not answer).
bool     TCA9554_Verify();
