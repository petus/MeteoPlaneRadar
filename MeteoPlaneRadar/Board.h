// =============================================================================
//  MeteoPlaneRadar
//  Board detection - which Waveshare panel are we actually running on.
//
//  Project: MeteoPlaneRadar - live aircraft radar on a round touchscreen
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
//  Web:     https://chiptron.cz
//  Boards:  Waveshare ESP32-S3-Touch-LCD-2.1  (round 480x480, ST7701 + CST820)
//           Waveshare ESP32-S3-Touch-LCD-2.8C (round 480x480, ST7701 + GT911)
//
//  The two boards are pin-for-pin identical: the same 16 RGB lines, the same
//  HSYNC/VSYNC/DE/PCLK, the same SPI pair for the ST7701 command interface, the
//  same backlight GPIO, the same I2C bus, the same I/O expander at 0x20 with
//  the same EXIO assignment, and the same touch INT pin. Exactly three things
//  differ, and none of them is a pin:
//
//    1. the ST7701 register init sequence (a genuinely different panel),
//    2. the vertical timing (VPW 3/VBP 8 vs VPW 2/VBP 18),
//    3. the touch controller (CST820 at 0x15 vs GT911 at 0x5D).
//
//  Because nothing collides electrically, the board can be worked out at run
//  time and ONE binary serves both - which matters here, since the firmware
//  updates itself over the air and nobody should have to know which file to
//  pick. The touch controller is the discriminator: it is the only part that
//  can be asked who it is, and it can be asked before the display is brought
//  up, which is exactly when we need the answer.
// =============================================================================
#pragma once
#include <Arduino.h>

enum BoardModel : uint8_t {
  BOARD_UNKNOWN = 0,
  BOARD_LCD_2_1,     // ESP32-S3-Touch-LCD-2.1,  ST7701 + CST820
  BOARD_LCD_2_8C,    // ESP32-S3-Touch-LCD-2.8C, ST7701 + GT911
};

// Probe the I2C bus and decide. Call ONCE in setup(), after Wire.begin() and
// TCA9554_Init() and BEFORE ST7701_Init() - the display init sequence and the
// panel timing both depend on the answer.
//
// Never returns BOARD_UNKNOWN: if nothing answers (a dead touch controller, a
// board revision we have not met) it falls back to BOARD_FALLBACK and says so
// on the serial line. A broken touch must not cost you the picture as well.
BoardModel Board_Detect();

// The result of Board_Detect(), cached. Safe to call from anywhere afterwards;
// before detection has run it reports the fallback.
BoardModel Board_Model();

// "2.1" / "2.8C" - for the log, the web status page and bug reports.
const char* Board_Name();

// True if detection actually recognised the chip, false if we are running on
// the fallback. Only used to colour the log line and the web status.
bool Board_Detected();
