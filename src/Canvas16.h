// =============================================================================
//  MeteoPlaneRadar
//  Canvas16 - off-screen 16-bit (RGB565) canvas whose framebuffer lives in
//  PSRAM. The whole UI is drawn into it with normal Arduino_GFX calls and then
//  pushed to the panel in ONE shot via flush() -> a single
//  esp_lcd_panel_draw_bitmap of the screen.
//
//  Why not Arduino_Canvas? Its flush() calls draw16bitRGBBitmap on the output,
//  which on our esp_lcd RGB panel falls back to pixel-by-pixel drawing - 230k
//  tiny writes into the panel framebuffer while the RGB DMA is reading it, i.e.
//  exactly the PSRAM double-write/read contention that makes pixels flicker.
//  Canvas16 keeps the buffer in PSRAM and flushes it as one bulk transfer, so
//  the DMA sees a single read burst. Verified fix ported from the SatRadar
//  project. Only rotation 0 is implemented (we never rotate).
//
//  Project: MeteoPlaneRadar - live aircraft radar on a round touchscreen
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
//  Board:   Waveshare ESP32-S3-Touch-LCD-2.1 (round 480x480 display, ST7701)
// =============================================================================
#pragma once
#include <Arduino_GFX_Library.h>
#include "esp_heap_caps.h"
#include "Display_ST7701.h"

class Canvas16 : public Arduino_GFX {
 public:
  Canvas16(int16_t w, int16_t h) : Arduino_GFX(w, h) {}

  ~Canvas16() {
    if (_fb) heap_caps_free(_fb);
  }

  bool begin(int32_t speed = GFX_NOT_DEFINED) override {
    (void)speed;
    if (!_fb) {
      size_t bytes = (size_t)WIDTH * HEIGHT * 2;
      _fb = (uint16_t*)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
    return _fb != nullptr;
  }

  uint16_t* getFramebuffer() { return _fb; }

  // Single controlled flush: whole canvas -> panel, one draw_bitmap.
  // Signature must match Arduino_GFX::flush(bool) exactly so that a call through
  // an Arduino_GFX* (as in the .ino) is dispatched here and not to the base.
  void flush(bool force_flush = false) override {
    (void)force_flush;
    if (_fb) LCD_Flush(_fb);
  }

  // --- Core writers (rotation 0 only). All are bounds-checked so a stray
  //     coordinate (e.g. a city label projected off-screen) can never write
  //     past the framebuffer. ---
  void writePixelPreclipped(int16_t x, int16_t y, uint16_t color) override {
    if (!_fb) return;
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) return;
    _fb[(int32_t)y * WIDTH + x] = color;
  }

  void writeFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) override {
    if (!_fb || h == 0) return;
    if (h < 0) { y += h + 1; h = -h; }
    if (x < 0 || x > (WIDTH - 1)) return;
    if (y > (HEIGHT - 1)) return;
    int16_t y2 = y + h - 1;
    if (y2 < 0) return;
    if (y < 0) { h += y; y = 0; }
    if (y2 > (HEIGHT - 1)) h = (HEIGHT - 1) - y + 1;
    uint16_t* p = _fb + (int32_t)y * WIDTH + x;
    while (h--) { *p = color; p += WIDTH; }
  }

  void writeFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) override {
    if (!_fb || w == 0) return;
    if (w < 0) { x += w + 1; w = -w; }
    if (y < 0 || y > (HEIGHT - 1)) return;
    if (x > (WIDTH - 1)) return;
    int16_t x2 = x + w - 1;
    if (x2 < 0) return;
    if (x < 0) { w += x; x = 0; }
    if (x2 > (WIDTH - 1)) w = (WIDTH - 1) - x + 1;
    uint16_t* p = _fb + (int32_t)y * WIDTH + x;
    while (w--) *p++ = color;
  }

  void writeFillRectPreclipped(int16_t x, int16_t y, int16_t w, int16_t h,
                               uint16_t color) override {
    if (!_fb || w <= 0 || h <= 0) return;
    // Clip to the framebuffer.
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > WIDTH)  w = WIDTH  - x;
    if (y + h > HEIGHT) h = HEIGHT - y;
    if (w <= 0 || h <= 0) return;
    uint16_t* row = _fb + (int32_t)y * WIDTH + x;
    for (int16_t j = 0; j < h; j++) {
      for (int16_t i = 0; i < w; i++) row[i] = color;
      row += WIDTH;
    }
  }

 private:
  uint16_t* _fb = nullptr;
};
