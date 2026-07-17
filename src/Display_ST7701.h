// =============================================================================
//  MeteoPlaneRadar
//  ST7701 display driver - pins, timing, interface.
//
//  Project: MeteoPlaneRadar - live aircraft radar on a round touchscreen
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
//  Web:     https://chiptron.cz
//  Board:   Waveshare ESP32-S3-Touch-LCD-2.1 (round 480x480 display, ST7701)
// =============================================================================
#pragma once
#include <Arduino.h>
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"

// --- SPI pins for the ST7701 command interface (init sequence) ---
#define LCD_CLK_PIN  2
#define LCD_MOSI_PIN 1

// --- Backlight ---
#define LCD_BL_PIN       6
#define BL_PWM_FREQ      20000
#define BL_PWM_RES       10       // 10 bit -> 0..1023
#define BL_MAX           100

// --- Dimensions ---
#define LCD_WIDTH   480
#define LCD_HEIGHT  480

// --- RGB timing (from the Waveshare datasheet) ---
// Pixel clock 8 MHz (NOT 16). This halves the DMA bandwidth demand on the PSRAM
// bus so the display survives contention from network buffers / canvas flush
// without random-pixel flicker or the image creeping upward. Verified fix taken
// from the SatRadar project - do not raise it back to 16 MHz.
#define RGB_FREQ_HZ  (8 * 1000 * 1000)
#define RGB_HPW  8
#define RGB_HBP  10
#define RGB_HFP  50
#define RGB_VPW  3
#define RGB_VBP  8
#define RGB_VFP  8

// --- RGB data pins (B0..B4, G0..G5, R0..R4) ---
#define RGB_HSYNC 38
#define RGB_VSYNC 39
#define RGB_DE    40
#define RGB_PCLK  41
#define RGB_D0    5
#define RGB_D1    45
#define RGB_D2    48
#define RGB_D3    47
#define RGB_D4    21
#define RGB_D5    14
#define RGB_D6    13
#define RGB_D7    12
#define RGB_D8    11
#define RGB_D9    10
#define RGB_D10   9
#define RGB_D11   46
#define RGB_D12   3
#define RGB_D13   8
#define RGB_D14   18
#define RGB_D15   17

extern esp_lcd_panel_handle_t panel_handle;

// Initialise the ST7701 (reset, SPI init sequence, RGB panel). Call after TCA9554_Init.
void ST7701_Init();

// Blit a colour rectangle straight to the panel (x2/y2 inclusive).
void LCD_DrawBitmap(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t* color);

// Push a full 480x480 canvas to the panel in ONE draw_bitmap call. This is THE
// flush - the only place a whole-screen buffer is handed to the driver. Keeping
// it single and rare (instead of pixel-by-pixel) is the core anti-flicker fix.
void LCD_Flush(const uint16_t* fb);

// Backlight, 0-100.
void Backlight_Init();
void Set_Backlight(uint8_t light);
