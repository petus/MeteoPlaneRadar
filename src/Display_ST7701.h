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
bool ST7701_Init();   // false = panel could not be brought up (see serial log)

// Blit a colour rectangle straight to the panel (x2/y2 inclusive).
void LCD_DrawBitmap(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t* color);

// --- Double buffering (zero copy) -------------------------------------------
// The panel owns TWO framebuffers in PSRAM. We draw straight into the one that
// is NOT being displayed and then hand that pointer to the driver, which only
// switches the DMA over at the next VSYNC - no 450 kB copy at all.
//
// Why this matters: copying a whole frame took ~28 ms while one frame lasts
// ~34 ms, so the write pointer and the panel's scan-out crawled along together
// and kept crossing each other, tearing a band of rows across the middle.
//
// Returns the framebuffer to draw into (idx 0 or 1), or nullptr if the driver
// did not give us one - the caller then falls back to its own buffer + copy.
uint16_t* LCD_FrameBuffer(int idx);

// Show the given buffer. If it is one of the panel's own framebuffers this is a
// pointer switch (zero copy); any other pointer is copied in as before.
void LCD_Flush(const uint16_t* fb);

// Ask the RGB driver to re-align its DMA to VSYNC at the next frame. On some
// cold starts the panel latches the DMA half a frame off and the picture comes
// up split into two halves; a restart clears that "permanent shift" (the fix
// esp_lcd_rgb_panel_restart() is documented for). Safe to call any time.
void LCD_Restart();

// Frames scanned out since boot, counted in the panel's VSYNC interrupt. Used
// by the display watchdog in the main sketch: at ~29 fps this must keep rising,
// and if it stops while the sketch is still running, the RGB peripheral has
// died and the screen is black with the backlight still on.
uint32_t LCD_VsyncCount();

// Backlight, 0-100.
void Backlight_Init();
void Set_Backlight(uint8_t light);
