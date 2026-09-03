// =============================================================================
//  MeteoPlaneRadar
//  GT911 capacitive touch (ESP32-S3-Touch-LCD-2.8C) - interface.
//
//  Project: MeteoPlaneRadar - live aircraft radar on a round touchscreen
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
//  Web:     https://chiptron.cz
//  Board:   Waveshare ESP32-S3-Touch-LCD-2.8C (round 480x480 display, ST7701)
// =============================================================================
#pragma once
#include <Arduino.h>
#include "Touch.h"

// The GT911 has no fixed address: it reads the INT pin as it leaves reset and
// settles on 0x5D if INT was low, 0x14 if it was high. GT911_Reset() aims for
// 0x5D; the alternate is probed anyway so an unexpected board revision is not
// simply "no touch".
#define GT911_ADDR_MAIN 0x5D
#define GT911_ADDR_ALT  0x14

// INT shares the pin with the CST820 on the 2.1 - the boards are wired alike.
#define GT911_INT_PIN   16

// 16-bit register addresses (the GT911 addresses its map big-endian).
#define GT911_REG_PRODUCT_ID 0x8140   // "911\0"
#define GT911_REG_STATUS     0x814E   // bit7 = buffer ready, bits0-3 = points
// Point 1 starts at 0x8150 with the X low byte. The track id is the byte
// BEFORE it, at 0x814F - reading from the id and treating the first byte as X
// shifts every coordinate by eight bits, which decodes a tap at (344, 270) as
// (3585, 5633) and gets it thrown away as off-panel. That is a touch that does
// nothing at all, with the controller answering perfectly the whole time.
#define GT911_REG_POINT1     0x8150   // xL, xH, yL, yH, sizeL, sizeH

// Pulse the reset line with INT held low, so the chip comes up on 0x5D.
// Returns false only if the I/O expander refused the write.
bool GT911_Reset();

// Is a GT911 answering? Reads the product ID and remembers which of the two
// addresses replied. Call after GT911_Reset().
bool GT911_Probe();

// The address GT911_Probe() found (0 before it has succeeded).
uint8_t GT911_Address();

bool GT911_Init();
void GT911_Read(TouchData* out);
