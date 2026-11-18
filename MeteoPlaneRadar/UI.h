// =============================================================================
//  MeteoPlaneRadar
//  Shared UI helpers - colours, global gfx, interface.
//
//  Project: MeteoPlaneRadar - live aircraft radar on a round touchscreen
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
//  Web:     https://chiptron.cz
//  Board:   Waveshare ESP32-S3-Touch-LCD-2.1 (round 480x480 display, ST7701)
// =============================================================================
#pragma once
#include <Arduino_GFX_Library.h>

// Colours (RGB565)
#define C_BLACK  0x0000
#define C_RED    0xF800
#define C_GREEN  0x07E0
#define C_WHITE  0xFFFF
#define C_YELLOW 0xFFE0
#define C_GRAY   0x8410
#define C_DKGRAY 0x2124
#define C_CYAN   0x05FF
#define C_ORANGE 0xFC00   // altitude band 2-6 km

// Global display (defined in the .ino).
extern Arduino_GFX* gfx;

// Draw a WiFi QR code (for joining the AP). open=true -> open network.
void UI_DrawWifiQR(const char* ssid, const char* password, bool open,
                   int x, int y, int size_px);

// Horizontally centred text (size 1-4).
void UI_TextCentered(const char* text, int cy, uint16_t color, uint8_t size);

// Text centred inside the rectangle [x, x+w) - used for labels above the map.
void UI_TextCenteredIn(const char* text, int x, int w, int cy,
                       uint16_t color, uint8_t size);

// A filled sector of an annulus - the building block of both energy dials.
// Angles in RADIANS, zero at three o'clock, increasing clockwise (screen y
// grows downwards), a0 <= a1. Pass a0 == a1 for a single radial line.
//
// Radial lines rather than a scanline fill: the GFX build here has no arc
// primitive, and the colour changes from one sector to the next, so a
// span-based fill would need a separate pass per sector anyway. The step is
// fine enough that the outer edge has no gaps at the radii these screens use.
void UI_FillRing(int cx, int cy, int rIn, int rOut, float a0, float a1,
                 uint16_t color);

// Half the width of the display circle at height y - i.e. how much room a line
// of text actually has there. On a round panel the usable width shrinks fast
// towards the top, so anything near the edge has to be measured, not assumed.

// The clock + outside temperature line, centred under the screen dots. Draws
// nothing when neither is known yet, and refuses to draw text that would not
// fit inside the circle rather than letting it run off the edge.
void UI_DrawStatusLine(int cy);

// 0.9.7: what a data screen shows while it has nothing to show. Waiting for
// WiFi or for the first answer: "Loading...". The last attempt failed:
// "Source unavailable" and when the next try is - never a "Loading..." that
// stays up for ever because the server is down.
struct SrcHealth;
void UI_NoData(const char* title, const SrcHealth& h);
// "next try in 4 min" / "retrying now", for the line above.
void UI_RetryText(const SrcHealth& h, char* out, size_t cap);

// Has that line changed since the last time anyone asked?
//
// The screens only redraw when their own Tick() says something happened, and
// the clock at the top is drawn as part of that redraw. On a screen whose data
// changes rarely - the mix arrives every quarter of an hour, the forecast twice
// an hour - the clock therefore sat still between updates and looked frozen.
// Those screens call this from their Tick() so the minute keeps moving.
//
// Compares the whole line rather than just the minute, so the outside
// temperature changing is caught too. One shared record of the last text is
// enough because only the active screen is ticked, and switching screens
// redraws everything anyway.
bool UI_StatusLineChanged();
