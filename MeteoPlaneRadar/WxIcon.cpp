// =============================================================================
//  MeteoPlaneRadar
//  Vector weather icons. See WxIcon.h.
//
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
// =============================================================================
#include "WxIcon.h"
#include "UI.h"
#include <math.h>

static WxKind WxIcon_Kind(int c) {
  switch (c) {
    case 0:               return WX_CLEAR;
    case 1: case 2:       return WX_PARTLY;
    case 3:               return WX_CLOUDY;
    case 45: case 48:     return WX_FOG;
    case 51: case 53: case 55:
    case 56: case 57:     return WX_DRIZZLE;
    case 61: case 63: case 65:
    case 66: case 67:     return WX_RAIN;
    case 71: case 73: case 75: case 77:
    case 85: case 86:     return WX_SNOW;
    case 80: case 81: case 82: return WX_SHOWER;
    case 95: case 96: case 99: return WX_STORM;
    default:              return WX_CLOUDY;
  }
}

uint16_t WxIcon_Color(int c) {
  switch (WxIcon_Kind(c)) {
    case WX_CLEAR:   return C_YELLOW;
    case WX_PARTLY:  return C_YELLOW;
    case WX_CLOUDY:  return C_GRAY;
    case WX_FOG:     return C_GRAY;
    case WX_DRIZZLE: return C_CYAN;
    case WX_RAIN:    return C_CYAN;
    case WX_SNOW:    return C_WHITE;
    case WX_SHOWER:  return C_CYAN;
    case WX_STORM:   return C_ORANGE;
  }
  return C_WHITE;
}

// --- Primitives -------------------------------------------------------------
static void sun(int cx, int cy, int r, uint16_t col) {
  gfx->fillCircle(cx, cy, r / 2, col);
  for (int i = 0; i < 8; i++) {
    float a = i * 0.7853981634f;                 // 45 deg apart
    int x0 = cx + (int)((r * 0.62f) * cosf(a));
    int y0 = cy + (int)((r * 0.62f) * sinf(a));
    int x1 = cx + (int)((r * 0.95f) * cosf(a));
    int y1 = cy + (int)((r * 0.95f) * sinf(a));
    gfx->drawLine(x0, y0, x1, y1, col);
  }
}

// Crescent: a filled disc with a second disc punched out of it in black. Only
// works because everything behind the icon is already black - which it is, the
// screens all start from fillScreen(C_BLACK).
static void moon(int cx, int cy, int r, uint16_t col) {
  gfx->fillCircle(cx, cy, r / 2, col);
  gfx->fillCircle(cx + r / 4, cy - r / 5, r / 2, C_BLACK);
}

static void cloud(int cx, int cy, int r, uint16_t col) {
  int w = r;                                     // half width of the body
  gfx->fillCircle(cx - w / 2, cy, r / 3, col);
  gfx->fillCircle(cx + w / 4, cy - r / 5, r / 2.6f, col);
  gfx->fillCircle(cx + w / 2, cy + r / 12, r / 3.4f, col);
  gfx->fillRect(cx - w / 2, cy, w, r / 3, col);
}

static void drops(int cx, int cy, int r, uint16_t col, int n, bool heavy) {
  for (int i = 0; i < n; i++) {
    int x = cx - r / 2 + i * (r / (n > 1 ? (n - 1) : 1));
    gfx->drawLine(x, cy, x - r / 8, cy + r / 3, col);
    if (heavy) gfx->drawLine(x + 1, cy, x - r / 8 + 1, cy + r / 3, col);
  }
}

static void flakes(int cx, int cy, int r, uint16_t col, int n) {
  for (int i = 0; i < n; i++) {
    int x = cx - r / 2 + i * (r / (n > 1 ? (n - 1) : 1));
    int y = cy + r / 6;
    int s = r / 7; if (s < 2) s = 2;
    gfx->drawLine(x - s, y, x + s, y, col);
    gfx->drawLine(x, y - s, x, y + s, col);
    gfx->drawLine(x - s / 2, y - s / 2, x + s / 2, y + s / 2, col);
    gfx->drawLine(x - s / 2, y + s / 2, x + s / 2, y - s / 2, col);
  }
}

static void bolt(int cx, int cy, int r, uint16_t col) {
  int s = r / 3; if (s < 3) s = 3;
  gfx->fillTriangle(cx, cy - s, cx - s / 2, cy + s / 2, cx + s / 4, cy, col);
  gfx->fillTriangle(cx + s / 4, cy, cx - s / 4, cy + s, cx + s / 2, cy - s / 4, col);
}

// --- Composition ------------------------------------------------------------
void WxIcon_Draw(int cx, int cy, int r, int code, bool night) {
  if (r < 6) r = 6;
  const uint16_t sunCol   = night ? C_WHITE : C_YELLOW;
  const uint16_t cloudCol = C_GRAY;

  switch (WxIcon_Kind(code)) {
    case WX_CLEAR:
      if (night) moon(cx, cy, r, sunCol); else sun(cx, cy, r, sunCol);
      break;

    case WX_PARTLY:
      // Sun peeking out behind the cloud - drawn first so the cloud overlaps it.
      if (night) moon(cx - r / 4, cy - r / 4, (int)(r * 0.8f), sunCol);
      else       sun (cx - r / 4, cy - r / 4, (int)(r * 0.8f), sunCol);
      cloud(cx + r / 6, cy + r / 5, (int)(r * 0.75f), cloudCol);
      break;

    case WX_CLOUDY:
      cloud(cx, cy, r, cloudCol);
      break;

    case WX_FOG:
      cloud(cx, cy - r / 4, (int)(r * 0.8f), cloudCol);
      for (int i = 0; i < 3; i++) {
        int y = cy + r / 3 + i * (r / 5);
        int half = (i == 1) ? r / 2 : (int)(r * 0.4f);   // staggered, like haze
        gfx->drawFastHLine(cx - half, y, 2 * half, C_GRAY);
      }
      break;

    case WX_DRIZZLE:
      cloud(cx, cy - r / 5, (int)(r * 0.85f), cloudCol);
      drops(cx, cy + r / 3, r, C_CYAN, 3, false);
      break;

    case WX_RAIN:
      cloud(cx, cy - r / 5, (int)(r * 0.85f), cloudCol);
      drops(cx, cy + r / 3, r, C_CYAN, 4, true);
      break;

    case WX_SHOWER:
      // Same as rain but with the sun behind it - a shower is by definition
      // intermittent, and that is what tells them apart at a glance.
      if (night) moon(cx - r / 3, cy - r / 3, (int)(r * 0.7f), sunCol);
      else       sun (cx - r / 3, cy - r / 3, (int)(r * 0.7f), sunCol);
      cloud(cx + r / 6, cy - r / 8, (int)(r * 0.75f), cloudCol);
      drops(cx + r / 6, cy + r / 2, (int)(r * 0.8f), C_CYAN, 3, true);
      break;

    case WX_SNOW:
      cloud(cx, cy - r / 5, (int)(r * 0.85f), cloudCol);
      flakes(cx, cy + r / 4, r, C_WHITE, 3);
      break;

    case WX_STORM:
      cloud(cx, cy - r / 5, (int)(r * 0.85f), C_DKGRAY);
      bolt(cx, cy + r / 3, r, C_YELLOW);
      drops(cx - r / 3, cy + r / 3, (int)(r * 0.6f), C_CYAN, 2, true);
      break;
  }
}

// -----------------------------------------------------------------------------
//  0.9.7: Moon phase and rise / set marks
// -----------------------------------------------------------------------------
void WxIcon_Moon(int cx, int cy, int r, double phase, bool south) {
  // The terminator is an ellipse: at height y the lit part starts at k*w(y),
  // with k = cos(2*pi*phase) running from 1 (new) through 0 (quarter) to -1
  // (full). Waxing is lit on the right as seen from the north.
  const float k = cosf(2.0f * 3.14159265f * (float)phase);
  const bool waxing = phase < 0.5;
  for (int y = -r; y <= r; y++) {
    const float w = sqrtf((float)(r * r - y * y));
    for (int x = -r; x <= r; x++) {
      if (x * x + y * y > r * r) continue;
      const int xs = south ? -x : x;
      const bool lit = waxing ? (xs > k * w) : (xs < -k * w);
      gfx->drawPixel(cx + x, cy + y, lit ? 0xE71C : 0x3186);   // light / dark grey
    }
  }
}

void WxIcon_RiseSet(int x, int cy, int w, bool up) {
  const int h = w - 1;
  if (up) gfx->fillTriangle(x, cy + h / 2, x + w, cy + h / 2, x + w / 2, cy - h / 2, 0x07E0);   // green
  else    gfx->fillTriangle(x, cy - h / 2, x + w, cy - h / 2, x + w / 2, cy + h / 2, 0xFC00);   // orange
}
