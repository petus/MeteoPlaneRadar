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

bool Layout_InsideCircle(int x, int y, int w, int h) {
  const int half1 = Layout_ChordHalf(y);
  const int half2 = Layout_ChordHalf(y + h - 1);
  const int half  = (half1 < half2) ? half1 : half2;
  if (half <= 0) return false;
  return (x >= LCD_WIDTH / 2 - half) && (x + w <= LCD_WIDTH / 2 + half);
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
  // 0.8.6: the rim counts as taken too. A callsign or a city label next to the
  // edge used to be drawn half off the glass; now it is left out like any
  // other label that has no room.
  if (!Layout_InsideCircle(x, y, w, h)) return false;
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

// --- Text collision check (0.8.6) -------------------------------------------
// See TEXT_CHECK in Config.h. Every string that reaches the display is noted
// with the rectangle of its text cell; on flush the rectangles are compared.
#if TEXT_CHECK
struct TextRec { Box b; char s[40]; };
#define TEXT_MAX 160
static TextRec s_txt[TEXT_MAX];
static int s_nt = 0;

void Layout_NoteText(int x, int y, int w, int h, const char* s, size_t n) {
  if (w <= 0 || h <= 0 || !s || !n) return;
  // A blank string or one made only of spaces covers nothing worth checking.
  bool ink = false;
  for (size_t i = 0; i < n; i++) if (s[i] != ' ') { ink = true; break; }
  if (!ink) return;
  if (s_nt >= TEXT_MAX) return;
  TextRec& t = s_txt[s_nt++];
  t.b = makeBox(x, y, w, h);
  const size_t m = n < sizeof(t.s) - 1 ? n : sizeof(t.s) - 1;
  memcpy(t.s, s, m);
  t.s[m] = '\0';
}

int Layout_TextCheckFrame() {
  int problems = 0;
  const int cx = LCD_WIDTH / 2, cy = LCD_HEIGHT / 2, r = LCD_WIDTH / 2;
  for (int i = 0; i < s_nt; i++) {
    const Box& a = s_txt[i].b;
    // Inside the circle: all four corners. One pixel of slack - an accent
    // touching the rim is not worth a report.
    const int xs[2] = { a.x0, a.x1 }, ys[2] = { a.y0, a.y1 };
    bool out = false;
    for (int xi = 0; xi < 2; xi++)
      for (int yi = 0; yi < 2; yi++) {
        const long dx = xs[xi] - cx, dy = ys[yi] - cy;
        if (dx * dx + dy * dy > (long)(r + 1) * (r + 1)) out = true;
      }
    // Report a string drawn twice at the same spot (an outline, a redraw) once.
    for (int k = 0; k < i && out; k++)
      if (s_txt[k].b.x0 == a.x0 && s_txt[k].b.y0 == a.y0 && !strcmp(s_txt[k].s, s_txt[i].s)) out = false;
    if (out) {
      Serial.printf("TEXT: \"%s\" (%d,%d %dx%d) presahuje okraj displeje\n",
                    s_txt[i].s, a.x0, a.y0, a.x1 - a.x0 + 1, a.y1 - a.y0 + 1);
      problems++;
    }
    for (int j = i + 1; j < s_nt; j++) {
      const Box& b = s_txt[j].b;
      // The same string at the same place is a redraw, not a collision.
      if (a.x0 == b.x0 && a.y0 == b.y0 && !strcmp(s_txt[i].s, s_txt[j].s)) continue;
      // One pixel of slack on every side: cells that merely touch are fine.
      if (a.x0 + 1 <= b.x1 && a.x1 - 1 >= b.x0 && a.y0 + 1 <= b.y1 && a.y1 - 1 >= b.y0) {
        Serial.printf("TEXT: \"%s\" (%d,%d %dx%d) prekryva \"%s\" (%d,%d %dx%d)\n",
                      s_txt[i].s, a.x0, a.y0, a.x1 - a.x0 + 1, a.y1 - a.y0 + 1,
                      s_txt[j].s, b.x0, b.y0, b.x1 - b.x0 + 1, b.y1 - b.y0 + 1);
        problems++;
      }
    }
  }
  s_nt = 0;
  return problems;
}
#else
void Layout_NoteText(int, int, int, int, const char*, size_t) {}
int  Layout_TextCheckFrame() { return 0; }
#endif
