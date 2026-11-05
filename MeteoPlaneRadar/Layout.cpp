// =============================================================================
//  MeteoPlaneRadar
//  Layout - reservations and collision checking. See Layout.h for the why.
//
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
// =============================================================================
#include "Layout.h"
#include "AAFont.h"
#include "Config.h"
#include <string.h>
#include <math.h>

// 100 aircraft callsigns + ~60 city labels + chrome. Overflowing simply means
// later elements stop being collision-checked, never a crash - but the count is
// sized so that does not happen in practice.
#define LAYOUT_MAX 200

struct Box { int16_t x0, y0, x1, y1; };   // inclusive
static Box s_box[LAYOUT_MAX];
static int s_n = 0;

void Layout_Begin() { s_n = 0; }

static inline bool overlaps(const Box& a, const Box& b) {
  return a.x0 <= b.x1 && a.x1 >= b.x0 && a.y0 <= b.y1 && a.y1 >= b.y0;
}

static Box makeBox(int x, int y, int w, int h) {
  Box b;
  b.x0 = (int16_t)x;
  b.y0 = (int16_t)y;
  b.x1 = (int16_t)(x + w - 1);
  b.y1 = (int16_t)(y + h - 1);
  return b;
}

static bool Layout_IsFree(int x, int y, int w, int h) {
  if (w <= 0 || h <= 0) return true;
  Box b = makeBox(x, y, w, h);
  for (int i = 0; i < s_n; i++) if (overlaps(b, s_box[i])) return false;
  return true;
}

void Layout_Reserve(int x, int y, int w, int h) {
  if (w <= 0 || h <= 0) return;
  if (s_n >= LAYOUT_MAX) return;
  s_box[s_n++] = makeBox(x, y, w, h);
}

bool Layout_Claim(int x, int y, int w, int h) {
  if (w <= 0 || h <= 0) return true;
  if (!Layout_IsFree(x, y, w, h)) return false;
  if (s_n >= LAYOUT_MAX) return false;   // out of slots - refuse rather than risk
  s_box[s_n++] = makeBox(x, y, w, h);
  return true;
}

int Layout_ChordHalf(int y) {
  const int R = LCD_WIDTH / 2 - 2;
  long dy = (long)y - LCD_HEIGHT / 2;
  long d2 = (long)R * R - dy * dy;
  if (d2 <= 0) return 0;
  return (int)sqrtf((float)d2);
}

void Layout_ReserveBand(int y, int h) {
  // Measure the chord at whichever edge of the band is narrower - the far side
  // of the screen centre is the one that pinches.
  int yTest = (y + h / 2 < LCD_HEIGHT / 2) ? y : (y + h - 1);
  int half = Layout_ChordHalf(yTest);
  if (half <= 0) return;
  Layout_Reserve(LCD_WIDTH / 2 - half, y, 2 * half, h);
}

int Layout_TextW(const char* s, uint8_t size) {
  if (!s) return 0;
  // With a smooth typeface the real width - never more than the built-in
  // font's, see AAFont.h - so centring and claims follow what is drawn.
  if (AAText_Enabled()) return AAText_Width((const uint8_t*)s, strlen(s), size);
  return (int)strlen(s) * LY_CHAR_W(size);
}

void Layout_ReserveTextCentered(const char* s, uint8_t size, int cx, int y) {
  int w = Layout_TextW(s, size);
  if (w <= 0) return;
  Layout_Reserve(cx - w / 2 - 4, y - 2, w + 8, LY_CHAR_H(size) + 4);
}

// --- Self-test --------------------------------------------------------------
void Layout_SelfTest() {
#if LAYOUT_DEBUG
  struct Band { const char* name; int y; int h; };
  // Heights are what the band actually draws: size-2 text is 16 px, size-1 is
  // 8, the dot rows are twice their radius.
  static const Band B[] = {
    { "LY_DOTS",       LY_DOTS - 5,      10 },
    { "LY_STATUS",     LY_STATUS - 3,    22 },
    { "LY_SUB",        LY_SUB,            8 },
    { "LY_LEGEND",     LY_LEGEND,        16 },
    { "LY_NOTE",       LY_NOTE,          10 },
    { "LY_RANGE",      LY_RANGE,         16 },
    { "LY_RANGE_DOTS", LY_RANGE_DOTS - 5, 10 },
    { "LY_FOOTER",     LY_FOOTER,        16 },
  };
  const int n = sizeof(B) / sizeof(B[0]);
  int clashes = 0;
  Serial.println("LAYOUT: kontrola pasu");
  for (int i = 0; i < n; i++) {
    // Vertical overlap between bands - they all span the width, so touching in
    // y is enough to be a real collision.
    for (int j = i + 1; j < n; j++) {
      int a0 = B[i].y, a1 = B[i].y + B[i].h - 1;
      int b0 = B[j].y, b1 = B[j].y + B[j].h - 1;
      if (a0 <= b1 && a1 >= b0) {
        Serial.printf("  KOLIZE: %s (%d..%d) x %s (%d..%d)\n",
                      B[i].name, a0, a1, B[j].name, b0, b1);
        clashes++;
      }
    }
    // And each band has to fit inside the circle at its own height.
    if (Layout_ChordHalf(B[i].y) < 40 || Layout_ChordHalf(B[i].y + B[i].h - 1) < 40) {
      Serial.printf("  UZKE: %s je moc blizko okraje kruhu\n", B[i].name);
      clashes++;
    }
  }
  Serial.printf("LAYOUT: %s (%d pasu)\n", clashes ? "NALEZENY KOLIZE" : "OK", n);
#endif
}
