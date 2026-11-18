// =============================================================================
//  MeteoPlaneRadar
//  Shared UI helpers (text, QR code).
//
//  Project: MeteoPlaneRadar - live aircraft radar on a round touchscreen
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
//  Web:     https://chiptron.cz
//  Board:   Waveshare ESP32-S3-Touch-LCD-2.1 (round 480x480 display, ST7701)
// =============================================================================
#include "UI.h"
#include "Layout.h"
#include "Outside.h"
#include "Display_ST7701.h"
#include <math.h>
#include <string.h>
#include "qrcode.h"
#include "Lang.h"
#include "SrcHealth.h"
#include <WiFi.h>
#include "Display_ST7701.h"

void UI_TextCenteredIn(const char* text, int x, int w, int cy,
                       uint16_t color, uint8_t size) {
  // Measured the same way print() will draw it (smooth or built-in font).
  const int tw = Layout_TextW(text, size);
  gfx->setTextSize(size);
  gfx->setTextColor(color);
  gfx->setCursor(x + (w - (int)tw) / 2, cy);
  gfx->print(text);
}

void UI_TextCentered(const char* text, int cy, uint16_t color, uint8_t size) {
  UI_TextCenteredIn(text, 0, LCD_WIDTH, cy, color, size);
}

// atan2 to within ~0.005 rad (a third of a pixel at the edge of the largest
// ring) for a fraction of the cost of atan2f - it runs for every pixel of a
// ring. Same range as atan2f: -pi..pi, angle measured clockwise on screen
// (y grows downwards), 0 = to the right.
static inline float fastAtan2(float y, float x) {
  const float ax = fabsf(x), ay = fabsf(y);
  const float mx = ax > ay ? ax : ay;
  if (mx == 0.0f) return 0.0f;
  const float a = (ax < ay ? ax : ay) / mx;
  const float s = a * a;
  float r = ((-0.0464964749f * s + 0.15931422f) * s - 0.327622764f) * s * a + a;
  if (ay > ax) r = 1.57079637f - r;
  if (x < 0)   r = 3.14159274f - r;
  if (y < 0)   r = -r;
  return r;
}

void UI_FillRing(int cx, int cy, int rIn, int rOut, float a0, float a1,
                 uint16_t color) {
  if (rOut <= 0 || rIn > rOut) return;
  if (a1 < a0) { const float t = a0; a0 = a1; a1 = t; }
  const float sweep = a1 - a0;

  // A sector thinner than a pixel at its outer edge is a LINE - the price
  // needle and the selection marks are drawn that way on purpose.
  if (sweep * rOut < 1.5f) {
    const float c = cosf(a0), s = sinf(a0);
    gfx->drawLine(cx + (int)lroundf(rIn * c), cy + (int)lroundf(rIn * s),
                  cx + (int)lroundf(rOut * c), cy + (int)lroundf(rOut * s), color);
    return;
  }

  // 0.9.4: pixel by pixel. Until now a sector was a fan of radial lines, and
  // two neighbouring lines do not quite touch everywhere on the pixel grid -
  // the holes between them were the thin dark streaks through the colour of
  // the price and mix rings. Now every pixel of the annulus whose angle lies in
  // [a0, a1] is painted, in horizontal runs, and only the sector's bounding
  // box is scanned.
  const float TWO_PI_F = 6.2831853f;
  float bx0 = 1e9f, by0 = 1e9f, bx1 = -1e9f, by1 = -1e9f;
  auto add = [&](float r, float ang) {
    const float x = cx + r * cosf(ang), y = cy + r * sinf(ang);
    if (x < bx0) bx0 = x;
    if (x > bx1) bx1 = x;
    if (y < by0) by0 = y;
    if (y > by1) by1 = y;
  };
  add(rIn, a0); add(rOut, a0); add(rIn, a1); add(rOut, a1);
  for (int k = -8; k <= 8; k++) {               // the axes the sector sweeps past
    const float ax = k * 1.57079637f;
    if (ax > a0 && ax < a1) add(rOut, ax);
  }
  int x0 = (int)floorf(bx0) - 1, x1 = (int)ceilf(bx1) + 1;
  int y0 = (int)floorf(by0) - 1, y1 = (int)ceilf(by1) + 1;
  if (x0 < 0) x0 = 0;
  if (y0 < 0) y0 = 0;
  if (x1 > LCD_WIDTH - 1)  x1 = LCD_WIDTH - 1;
  if (y1 > LCD_HEIGHT - 1) y1 = LCD_HEIGHT - 1;

  const long ri2 = (long)rIn * rIn, ro2 = (long)rOut * rOut;
  for (int y = y0; y <= y1; y++) {
    const long dy = y - cy;
    int run = -1;
    for (int x = x0; x <= x1 + 1; x++) {
      bool in = false;
      if (x <= x1) {
        const long dx = x - cx, d2 = dx * dx + dy * dy;
        if (d2 >= ri2 && d2 <= ro2) {
          float t = fastAtan2((float)dy, (float)dx) - a0;
          t = fmodf(t, TWO_PI_F);
          if (t < 0) t += TWO_PI_F;
          in = (t <= sweep);
        }
      }
      if (in) { if (run < 0) run = x; }
      else if (run >= 0) { gfx->drawFastHLine(run, y, x - run, color); run = -1; }
    }
  }
}

static int UI_ChordHalfWidth(int y) {
  const int R = LCD_WIDTH / 2 - 2;          // same margin as the screens use
  long dy = (long)y - LCD_HEIGHT / 2;
  long d2 = (long)R * R - dy * dy;
  if (d2 <= 0) return 0;
  return (int)sqrtf((float)d2);
}

bool UI_StatusLineChanged() {
  static char last[OUTSIDE_TEXT_MAX] = "";
  char txt[OUTSIDE_TEXT_MAX];
  Outside_StatusText(txt, sizeof(txt));
  if (strcmp(txt, last) == 0) return false;
  strncpy(last, txt, sizeof(last) - 1);
  last[sizeof(last) - 1] = '\0';
  return true;
}

void UI_DrawStatusLine(int cy) {
  char txt[OUTSIDE_TEXT_MAX];
  Outside_StatusText(txt, sizeof(txt));
  if (!txt[0]) return;                      // nothing known yet - leave it empty

  // Measure against the circle at the LOWER edge of the text, which is the
  // narrower end. The longest string this can produce is "23:59   -12 °C"
  // (~168 px at size 2) and the chord here is around 265 px, so it fits - but
  // check anyway: a wider font or a lower line would silently overflow.
  const int tw = Layout_TextW(txt, 2);
  int room = 2 * UI_ChordHalfWidth(cy + 16) - 8;
  if ((int)tw > room) return;

  // Dark backing - on the weather screen this sits straight on top of the radar
  // image, where white on yellow rain would be unreadable.
  gfx->fillRect(LCD_WIDTH / 2 - (int)tw / 2 - 6, cy - 3, (int)tw + 12, 22, C_BLACK);
  UI_TextCentered(txt, cy, C_WHITE, 2);
}

void UI_DrawWifiQR(const char* ssid, const char* password, bool open,
                   int x, int y, int size_px) {
  // WiFi QR payload: WIFI:T:nopass;S:<ssid>;; or ...WPA;...P:<password>;;
  String payload = "WIFI:T:";
  payload += open ? "nopass" : "WPA";
  payload += ";S:"; payload += ssid; payload += ";";
  if (!open) { payload += "P:"; payload += password; payload += ";"; }
  payload += ";";

  uint8_t version = 3;
  if (payload.length() > 60) version = 5;
  if (payload.length() > 100) version = 7;

  QRCode qr;
  uint8_t buf[qrcode_getBufferSize(7)];
  if (qrcode_initText(&qr, buf, version, ECC_MEDIUM, payload.c_str()) != 0) return;

  int modules = qr.size;
  int scale = size_px / (modules + 2);
  if (scale < 1) return;
  int qrPix = (modules + 2) * scale;

  // White background including the quiet zone.
  gfx->fillRect(x, y, qrPix, qrPix, C_WHITE);
  int off = x + scale, offY = y + scale;
  for (int my = 0; my < modules; my++) {
    for (int mx = 0; mx < modules; mx++) {
      if (qrcode_getModule(&qr, mx, my)) {
        gfx->fillRect(off + mx * scale, offY + my * scale, scale, scale, C_BLACK);
      }
    }
  }
}

// -----------------------------------------------------------------------------
//  No data yet (0.9.7)
// -----------------------------------------------------------------------------
void UI_RetryText(const SrcHealth& h, char* out, size_t cap) {
  const unsigned long ms = h.nextIn();
  if (ms < 30000UL) {
    snprintf(out, cap, "%s", LT("zkousim znovu", "zkouším znovu", "retrying now",
                                "skusam znova", "skúšam znova"));
    return;
  }
  const unsigned long min = (ms + 59999UL) / 60000UL;
  snprintf(out, cap, LT("dalsi pokus za %lu min", "další pokus za %lu min", "next try in %lu min",
                        "dalsi pokus o %lu min", "ďalší pokus o %lu min"), min);
}

void UI_NoData(const char* title, const SrcHealth& h) {
  const int CY = LCD_HEIGHT / 2;
  if (WiFi.status() != WL_CONNECTED || !h.down()) {
    UI_TextCentered(title, CY - 20, C_WHITE, 2);
    UI_TextCentered(WiFi.status() == WL_CONNECTED ? T(S_LOADING) : T(S_WIFI_WAIT),
                    CY + 10, C_YELLOW, 2);
    return;
  }
  char b[48];
  UI_RetryText(h, b, sizeof(b));
  UI_TextCentered(title, CY - 36, C_WHITE, 2);
  UI_TextCentered(T(S_SRC_DOWN), CY - 6, C_ORANGE, 2);
  UI_TextCentered(b, CY + 22, C_GRAY, 2);
}
