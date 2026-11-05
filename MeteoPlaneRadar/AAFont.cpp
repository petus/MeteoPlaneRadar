// =============================================================================
//  MeteoPlaneRadar
//  Anti-aliased bitmap fonts - see AAFont.h.
//
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
// =============================================================================
#include "AAFont.h"
#include "ClockFonts.h"
#include "Canvas16.h"
#include "Config.h"
#include "UI.h"
#include "Settings.h"

// Next code point from a UTF-8 string. Malformed bytes come back one by one as
// themselves, which the glyph lookup then simply does not find.
static uint32_t nextCp(const char*& s) {
  const uint8_t c = (uint8_t)*s++;
  if (c < 0x80) return c;
  // The built-in font's degree sign is the single byte 0xF8 (OUTSIDE_DEG_TEXT
  // in Config.h). As UTF-8 it is not a valid lead byte, so it cannot be
  // mistaken for anything else - map it to the real degree sign.
  if (c == 0xF8) return 0xB0;
  if ((c & 0xE0) == 0xC0 && (*s & 0xC0) == 0x80) {
    return ((uint32_t)(c & 0x1F) << 6) | ((uint8_t)*s++ & 0x3F);
  }
  if ((c & 0xF0) == 0xE0 && (s[0] & 0xC0) == 0x80 && (s[1] & 0xC0) == 0x80) {
    uint32_t cp = ((uint32_t)(c & 0x0F) << 12) | ((uint32_t)((uint8_t)s[0] & 0x3F) << 6) |
                  ((uint8_t)s[1] & 0x3F);
    s += 2;
    return cp;
  }
  return c;
}

static const AAGlyph* find(const AAFont* f, uint32_t cp) {
  int lo = 0, hi = (int)f->count - 1;
  while (lo <= hi) {
    const int mid = (lo + hi) / 2;
    const uint16_t m = f->glyphs[mid].cp;
    if (m == cp) return &f->glyphs[mid];
    if (m < cp) lo = mid + 1; else hi = mid - 1;
  }
  return nullptr;
}

int AAFont_TextW(const AAFont* f, const char* s) {
  if (!f || !s) return 0;
  int w = 0;
  while (*s) {
    const AAGlyph* g = find(f, nextCp(s));
    if (g) w += g->adv;
  }
  return w;
}

// Blend fg over bg with coverage a = 0..15, per RGB565 channel.
static inline uint16_t blend(uint16_t bg, uint16_t fg, uint8_t a) {
  if (a >= 15) return fg;
  const uint32_t rb = bg >> 11, gb = (bg >> 5) & 0x3F, bb = bg & 0x1F;
  const uint32_t rf = fg >> 11, gf = (fg >> 5) & 0x3F, bf = fg & 0x1F;
  const uint32_t r = (rb * (15 - a) + rf * a) / 15;
  const uint32_t g = (gb * (15 - a) + gf * a) / 15;
  const uint32_t b = (bb * (15 - a) + bf * a) / 15;
  return (uint16_t)((r << 11) | (g << 5) | b);
}

// Draws up to the terminator or `end`. maxW > 0 squeezes the pen positions so
// the whole string stays within maxW pixels (the glyphs themselves keep their
// shape; only the spacing gives).
static int drawRun(const AAFont* f, const char* s, const char* end, int x, int y,
                   uint16_t color, int maxW);

int AAFont_Draw(const AAFont* f, const char* s, int x, int y, uint16_t color) {
  return drawRun(f, s, nullptr, x, y, color, 0);
}

static int runWidth(const AAFont* f, const char* s, const char* end) {
  int w = 0;
  while (*s && (!end || s < end)) {
    const AAGlyph* g = find(f, nextCp(s));
    if (g) w += g->adv;
  }
  return w;
}

static int drawRun(const AAFont* f, const char* s, const char* end, int x, int y,
                   uint16_t color, int maxW) {
  if (!f || !s) return x;
  const int natural = (maxW > 0) ? runWidth(f, s, end) : 0;
  const bool squeeze = (maxW > 0 && natural > maxW);
  const int x0start = x;
  int pen = 0;
  // Blending needs to READ the framebuffer. Canvas16 hands it out; on any other
  // target fall back to treating the background as black, which is what every
  // screen that uses these fonts draws on anyway.
  Canvas16* cv = static_cast<Canvas16*>(gfx);
  uint16_t* fb = cv ? cv->getFramebuffer() : nullptr;
  const int W = gfx->width(), H = gfx->height();

  while (*s && (!end || s < end)) {
    const AAGlyph* g = find(f, nextCp(s));
    if (!g) continue;
    if (squeeze) x = x0start + (int)((long)pen * maxW / natural);
    const uint8_t* p = f->bits + g->off;
    const int x0 = x + g->dx, y0 = y + g->dy;
    uint32_t n = 0;
    for (int j = 0; j < g->h; j++) {
      const int yy = y0 + j;
      for (int i = 0; i < g->w; i++, n++) {
        const uint8_t byte = p[n >> 1];
        const uint8_t a = (n & 1) ? (byte & 0x0F) : (byte >> 4);
        if (!a) continue;
        const int xx = x0 + i;
        if (xx < 0 || xx >= W || yy < 0 || yy >= H) continue;
        if (fb) {
          uint16_t* px = fb + (int32_t)yy * W + xx;
          *px = blend(*px, color, a);
        } else {
          gfx->drawPixel(xx, yy, blend(0, color, a));
        }
      }
    }
    x += g->adv;
    pen += g->adv;
  }
  if (squeeze) x = x0start + maxW;
  return x;
}

void AAFont_DrawCentered(const AAFont* f, const char* s, int cx, int y, uint16_t color) {
  AAFont_Draw(f, s, cx - AAFont_TextW(f, s) / 2, y, color);
}

static const AAFontSet SET_INTER   = { &FONT_INTER_L,   &FONT_INTER_M,   &FONT_INTER_S };
static const AAFontSet SET_POPPINS = { &FONT_POPPINS_L, &FONT_POPPINS_M, &FONT_POPPINS_S };
static const AAFontSet SET_THIN    = { &FONT_THIN_L,    &FONT_THIN_M,    &FONT_THIN_S };

const AAFontSet* AAFont_ClockSet(uint8_t clockFont) {
  switch (clockFont) {
    case CLOCK_FONT_INTER:   return &SET_INTER;
    case CLOCK_FONT_POPPINS: return &SET_POPPINS;
    case CLOCK_FONT_THIN:    return &SET_THIN;
    default:                 return nullptr;
  }
}

// -----------------------------------------------------------------------------
//  Every other screen (0.8.5) - see AAFont.h
// -----------------------------------------------------------------------------
struct TextSet { const AAFont* t[5]; };
static const TextSet TEXT_INTER   = {{ &FONT_INTER_T1,   &FONT_INTER_T2,   &FONT_INTER_T3,   &FONT_INTER_T4,   &FONT_INTER_T5 }};
static const TextSet TEXT_POPPINS = {{ &FONT_POPPINS_T1, &FONT_POPPINS_T2, &FONT_POPPINS_T3, &FONT_POPPINS_T4, &FONT_POPPINS_T5 }};
static const TextSet TEXT_THIN    = {{ &FONT_THIN_T1,    &FONT_THIN_T2,    &FONT_THIN_T3,    &FONT_THIN_T4,    &FONT_THIN_T5 }};

static const AAFont* textFont(uint8_t size) {
  if (size < 1 || size > 5) return nullptr;
  const TextSet* t = nullptr;
  switch (Settings_ClockFont()) {
    case CLOCK_FONT_INTER:   t = &TEXT_INTER;   break;
    case CLOCK_FONT_POPPINS: t = &TEXT_POPPINS; break;
    case CLOCK_FONT_THIN:    t = &TEXT_THIN;    break;
    default: return nullptr;
  }
  return t->t[size - 1];
}

bool AAText_Enabled() { return Settings_ClockFont() != CLOCK_FONT_PIXEL; }

// What the built-in font would take: 6 px per byte and size step. Per BYTE,
// not per character, so a UTF-8 string - which the built-in font would have
// drawn as two glyphs per accented letter - is allowed a little more room,
// never less.
static int monoWidth(size_t n, uint8_t size) { return (int)n * 6 * size; }

int AAText_Width(const uint8_t* s, size_t n, uint8_t size) {
  const AAFont* f = textFont(size);
  if (!f) return monoWidth(n, size);
  const char* p = (const char*)s;
  const int w = runWidth(f, p, p + n);
  const int m = monoWidth(n, size);
  return w < m ? w : m;
}

int AAText_Print(const uint8_t* s, size_t n, int x, int y, uint8_t size,
                 uint16_t fg, uint16_t bg) {
  const AAFont* f = textFont(size);
  if (!f) return 0;
  const char* p = (const char*)s;
  const int w = AAText_Width(s, n, size);
  if (bg != fg) gfx->fillRect(x, y, w, 8 * size, bg);
  // The built-in glyphs stand on row 7 of the 8-row cell.
  drawRun(f, p, p + n, x, y + 7 * size, fg, monoWidth(n, size));
  return w;
}
