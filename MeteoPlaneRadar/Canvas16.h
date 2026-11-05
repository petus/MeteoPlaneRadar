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
#include "AAFont.h"     // smooth text for every print() - 0.8.5

class Canvas16 : public Arduino_GFX {
 public:
  Canvas16(int16_t w, int16_t h) : Arduino_GFX(w, h) {}

  ~Canvas16() {
    if (_owned && _fb) heap_caps_free(_fb);
  }

  // Draw into one of the panel's own framebuffers (zero-copy double buffering).
  // idx says which one it is, so flush() knows which to switch to next.
  void useExternalBuffer(uint16_t* fb, int idx) {
    if (_owned && _fb) heap_caps_free(_fb);
    _fb = fb;
    _fbIndex = idx;
    _owned = false;
  }

  bool begin(int32_t speed = GFX_NOT_DEFINED) override {
    (void)speed;
    if (!_fb) {
      size_t bytes = (size_t)WIDTH * HEIGHT * 2;
      _fb = (uint16_t*)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
      _owned = true;
    }
    return _fb != nullptr;
  }

  uint16_t* getFramebuffer() { return _fb; }

  // --- Text (0.8.5) ----------------------------------------------------------
  // Every print()/printf() ends up in one of these two. With a smooth typeface
  // selected, whole strings go to AAText_Print() instead of being drawn one
  // built-in 5x7 glyph at a time - see AAFont.h. Sizes it has no face for
  // (the clock's old size 8) and the pixel setting take the original path.
  size_t write(const uint8_t* buf, size_t n) override {
    if (n && AAText_Enabled() && textsize_x >= 1 && textsize_x <= 5 && textsize_x == textsize_y) {
      size_t start = 0;
      for (size_t i = 0; i <= n; i++) {
        if (i == n || buf[i] == '\n') {
          if (i > start)
            cursor_x += AAText_Print(buf + start, i - start, cursor_x, cursor_y,
                                     textsize_x, textcolor, textbgcolor);
          if (i < n) { cursor_x = 0; cursor_y += 8 * textsize_y; }
          start = i + 1;
        }
      }
      return n;
    }
    return Print::write(buf, n);
  }
  size_t write(uint8_t c) override {
    if (AAText_Enabled() && textsize_x >= 1 && textsize_x <= 5 && textsize_x == textsize_y)
      return write(&c, 1);
    return Arduino_GFX::write(c);
  }

  // Single controlled flush. Signature must match Arduino_GFX::flush(bool)
  // exactly so a call through an Arduino_GFX* is dispatched here, not to the base.
  //
  // In zero-copy mode this hands the finished framebuffer to the panel and then
  // moves drawing to the other one, so the next frame is never drawn into the
  // buffer currently on screen. Doing the switch HERE means every gfx->flush()
  // in the project (radar, weather, WiFi portal, OTA) gets it automatically.
  void flush(bool force_flush = false) override {
    (void)force_flush;
    if (!_fb) return;
    LCD_Flush(_fb);
    if (!_owned) {
      int next = _fbIndex ^ 1;
      uint16_t* nb = LCD_FrameBuffer(next);
      if (nb) { _fb = nb; _fbIndex = next; }
    }
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
  int       _fbIndex = 0;     // which panel framebuffer _fb points at
  bool      _owned = false;   // false = the buffer belongs to the panel driver
};
