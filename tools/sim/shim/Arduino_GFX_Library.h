// Host stand-in for the GFX library: the drawing entry points the cards screen
// uses, on top of the same virtual writers Canvas16 overrides. The built-in
// 5x7 font is NOT here - with a smooth typeface selected nothing reaches it,
// and the harness reports if anything does.
//
// writeEllipseHelper() and writeFillEllipseHelper() below are taken from
// Arduino_GFX.cpp of "GFX Library for Arduino" v1.6.9, so circles come out
// pixel for pixel as on the device. That code is under this licence:
//
//   Software License Agreement (BSD License)
//
//   Copyright (c) 2012 Adafruit Industries.  All rights reserved.
//
//   Redistribution and use in source and binary forms, with or without
//   modification, are permitted provided that the following conditions are met:
//
//   - Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//   - Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//
//   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
//   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
//   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
//   ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
//   LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
//   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
//   SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
//   INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
//   CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
//   ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
//   POSSIBILITY OF SUCH DAMAGE.
//
// Everything else in this file is part of MeteoPlaneRadar's tools (MIT).
#pragma once
#include <Arduino.h>
#define GFX_NOT_DEFINED -1
extern int g_builtinGlyphs;

class Arduino_GFX : public Print {
 public:
  Arduino_GFX(int16_t w, int16_t h) : WIDTH(w), HEIGHT(h) {}
  virtual bool begin(int32_t speed = GFX_NOT_DEFINED) = 0;
  virtual void flush(bool force_flush = false) { (void)force_flush; }
  virtual void writePixelPreclipped(int16_t x, int16_t y, uint16_t color) = 0;
  virtual void writeFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) { for (int16_t j = 0; j < h; j++) writePixel(x, y + j, color); }
  virtual void writeFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) { for (int16_t i = 0; i < w; i++) writePixel(x + i, y, color); }
  virtual void writeFillRectPreclipped(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) { for (int16_t j = 0; j < h; j++) writeFastHLine(x, y + j, w, color); }

  void startWrite() {}
  void endWrite() {}
  void writePixel(int16_t x, int16_t y, uint16_t color) { if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) writePixelPreclipped(x, y, color); }
  void drawPixel(int16_t x, int16_t y, uint16_t color) { writePixel(x, y, color); }
  void writeFillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    if (w <= 0 || h <= 0) return;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > WIDTH) w = WIDTH - x;
    if (y + h > HEIGHT) h = HEIGHT - y;
    if (w > 0 && h > 0) writeFillRectPreclipped(x, y, w, h, color);
  }
  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) { writeFillRect(x, y, w, h, color); }
  void fillScreen(uint16_t color) { fillRect(0, 0, WIDTH, HEIGHT, color); }
  void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
    const bool steep = abs(y1 - y0) > abs(x1 - x0);
    if (steep) { int16_t t = x0; x0 = y0; y0 = t; t = x1; x1 = y1; y1 = t; }
    if (x0 > x1) { int16_t t = x0; x0 = x1; x1 = t; t = y0; y0 = y1; y1 = t; }
    const int16_t dx = x1 - x0, dy = abs(y1 - y0), ystep = (y0 < y1) ? 1 : -1;
    int16_t err = dx / 2;
    for (; x0 <= x1; x0++) {
      if (steep) writePixel(y0, x0, color); else writePixel(x0, y0, color);
      err -= dy;
      if (err < 0) { y0 += ystep; err += dx; }
    }
  }
  void drawCircle(int16_t x, int16_t y, int16_t r, uint16_t color) { writeEllipseHelper(x, y, r, r, 0xf, color); }
  void fillCircle(int16_t x, int16_t y, int16_t r, uint16_t color) { writeFillEllipseHelper(x, y, r, r, 3, 0, color); }

  int16_t width() const { return WIDTH; }
  int16_t height() const { return HEIGHT; }
  void setTextSize(uint8_t s) { textsize_x = textsize_y = s; }
  void setTextColor(uint16_t c) { textcolor = textbgcolor = c; }
  void setTextColor(uint16_t c, uint16_t bg) { textcolor = c; textbgcolor = bg; }
  void setCursor(int16_t x, int16_t y) { cursor_x = x; cursor_y = y; }

  using Print::write;
  size_t write(uint8_t c) override { (void)c; g_builtinGlyphs++; cursor_x += 6 * textsize_x; return 1; }

 protected:
  // From Arduino_GFX.cpp v1.6.9 (BSD licence, see the top of this file).
  void writeEllipseHelper(int32_t x, int32_t y, int32_t rx, int32_t ry, uint8_t cornername, uint16_t color) {
    if (rx < 0 || ry < 0 || ((rx == 0) && (ry == 0))) return;
    if (ry == 0) { writeFastHLine(x - rx, y, (ry << 2) + 1, color); return; }
    if (rx == 0) { writeFastVLine(x, y - ry, (rx << 2) + 1, color); return; }
    int32_t xt, yt, s, i;
    int32_t rx2 = rx * rx, ry2 = ry * ry;
    i = -1; xt = 0; yt = ry;
    s = (ry2 << 1) + rx2 * (1 - (ry << 1));
    do {
      while (s < 0) s += ry2 * ((++xt << 2) + 2);
      if (cornername & 0x1) writeFastHLine(x - xt, y - yt, xt - i, color);
      if (cornername & 0x2) writeFastHLine(x + i + 1, y - yt, xt - i, color);
      if (cornername & 0x4) writeFastHLine(x + i + 1, y + yt, xt - i, color);
      if (cornername & 0x8) writeFastHLine(x - xt, y + yt, xt - i, color);
      i = xt;
      s -= (--yt) * rx2 << 2;
    } while (ry2 * xt <= rx2 * yt);
    i = -1; yt = 0; xt = rx;
    s = (rx2 << 1) + ry2 * (1 - (rx << 1));
    do {
      while (s < 0) s += rx2 * ((++yt << 2) + 2);
      if (cornername & 0x1) writeFastVLine(x - xt, y - yt, yt - i, color);
      if (cornername & 0x2) writeFastVLine(x + xt, y - yt, yt - i, color);
      if (cornername & 0x4) writeFastVLine(x + xt, y + i + 1, yt - i, color);
      if (cornername & 0x8) writeFastVLine(x - xt, y + i + 1, yt - i, color);
      i = yt;
      s -= (--xt) * ry2 << 2;
    } while (rx2 * yt <= ry2 * xt);
  }
  void writeFillEllipseHelper(int32_t x, int32_t y, int32_t rx, int32_t ry, uint8_t corners, int16_t delta, uint16_t color) {
    if (rx < 0 || ry < 0 || ((rx == 0) && (ry == 0))) return;
    if (ry == 0) { writeFastHLine(x - rx, y, (ry << 2) + 1, color); return; }
    if (rx == 0) { writeFastVLine(x, y - ry, (rx << 2) + 1, color); return; }
    int32_t xt, yt, i;
    int32_t rx2 = (int32_t)rx * rx, ry2 = (int32_t)ry * ry;
    int32_t s;
    writeFastHLine(x - rx, y, (rx << 1) + 1, color);
    i = 0; yt = 0; xt = rx;
    s = (rx2 << 1) + ry2 * (1 - (rx << 1));
    do {
      while (s < 0) s += rx2 * ((++yt << 2) + 2);
      if (corners & 1) writeFillRect(x - xt, y - yt, (xt << 1) + 1 + delta, yt - i, color);
      if (corners & 2) writeFillRect(x - xt, y + i + 1, (xt << 1) + 1 + delta, yt - i, color);
      i = yt;
      s -= (--xt) * ry2 << 2;
    } while (rx2 * yt <= ry2 * xt);
    xt = 0; yt = ry;
    s = (ry2 << 1) + rx2 * (1 - (ry << 1));
    do {
      while (s < 0) s += ry2 * ((++xt << 2) + 2);
      if (corners & 1) writeFastHLine(x - xt, y - yt, (xt << 1) + 1 + delta, color);
      if (corners & 2) writeFastHLine(x - xt, y + yt, (xt << 1) + 1 + delta, color);
      s -= (--yt) * rx2 << 2;
    } while (ry2 * xt <= rx2 * yt);
  }

  int16_t WIDTH, HEIGHT;
  int16_t cursor_x = 0, cursor_y = 0;
  uint16_t textcolor = 0xFFFF, textbgcolor = 0xFFFF;
  uint8_t textsize_x = 1, textsize_y = 1;
};
