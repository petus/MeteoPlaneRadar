// =============================================================================
//  MeteoPlaneRadar
//  Cards screen. See ScreenCards.h and Cards.h.
//
//  A card does not carry coordinates. It says what to show and this file
//  decides where: the panel is round, so a row that fits in the middle does
//  not fit near the edge, and a card author should not have to know that.
//  Each line is shrunk until it fits the chord it sits on.
// =============================================================================
#include "ScreenCards.h"
#include "Cards.h"
#include "AAFont.h"
#include "Lang.h"
#include "Layout.h"
#include "Route.h"
#include "Script.h"
#include "Settings.h"
#include "UI.h"
#include <WiFi.h>

static int      s_cur = 0;
static uint32_t s_lastRev = 0;
static bool     s_dirty = true;
static unsigned long s_lastDraw = 0;

// Coming back to the screen starts the card's script afresh.
void ScreenCards_Enter() { s_dirty = true; Script_Release(); }

bool ScreenCards_Tick() {
  bool want = s_dirty;
  if (Cards_Revision() != s_lastRev) want = true;
  if (UI_StatusLineChanged()) want = true;
  // A script card may ask to be redrawn on a schedule (a clock hand, a countdown).
  const Card* c = Cards_Get(s_cur);
  if (c && c->hasScript && c->redrawS && millis() - s_lastDraw >= (unsigned long)c->redrawS * 1000UL) want = true;
  return want;
}

void ScreenCards_ChangeRange(int dir) {
  const int n = Cards_Count();
  if (n < 2) return;
  s_cur = (s_cur + (dir >= 0 ? 1 : -1) + n) % n;
  s_dirty = true;
}

int ScreenCards_Count() {
  const int n = Cards_Count();
  return n > 0 ? n : 1;
}

int ScreenCards_Index() {
  const int n = Cards_Count();
  return (s_cur >= 0 && s_cur < n) ? s_cur : 0;
}

bool ScreenCards_Step(int dir) {
  const int n = Cards_Count();
  const int to = ScreenCards_Index() + (dir >= 0 ? 1 : -1);
  if (to < 0 || to >= n) return false;
  s_cur = to;
  s_dirty = true;
  return true;
}

void ScreenCards_Select(int which) {
  const int n = Cards_Count();
  s_cur = (which < 0 && n > 0) ? n - 1 : 0;
  s_dirty = true;
}

void ScreenCards_RangeText(char* out, size_t cap) {
  if (!out || !cap) return;
  const int n = Cards_Count();
  if (n < 2) { out[0] = '\0'; return; }
  snprintf(out, cap, "%d/%d", (s_cur < n ? s_cur : 0) + 1, n);
}

// Card text is whatever its author typed, accents included. A smooth face
// draws them; the built-in font cannot, so for it they are folded to ASCII the
// same way airport names are.
static const char* panelText(char* buf, size_t cap, const char* utf8) {
  if (AAText_Enabled()) return utf8;
  Route_ToAscii(buf, cap, utf8);
  return buf;
}

// Largest size <= want at which the text fits the round panel. y is the TOP of
// the text, as everywhere in UI.h; a smaller size is centred in the height the
// wanted one would have taken, so a shrunk line does not ride up.
static void centered(const char* s, int y, uint16_t color, uint8_t want) {
  uint8_t size = want;
  int top = y;
  for (; size > 1; size--) {
    top = y + (LY_CHAR_H(want) - LY_CHAR_H(size)) / 2;
    const int a = Layout_ChordHalf(top), b = Layout_ChordHalf(top + LY_CHAR_H(size));
    if (Layout_TextW(s, size) <= 2 * (a < b ? a : b) - 24) break;
  }
  if (size == 1) top = y + (LY_CHAR_H(want) - LY_CHAR_H(1)) / 2;
  UI_TextCentered(s, top, color, size);
}

void ScreenCards_Label(const char* utf8, int y, uint16_t color, uint8_t size) {
  char buf[96];
  centered(panelText(buf, sizeof(buf), utf8), y, color, size);
}

void ScreenCards_Draw() {
  s_dirty = false;
  s_lastRev = Cards_Revision();
  s_lastDraw = millis();

  gfx->fillScreen(C_BLACK);
  Layout_Begin();
  Layout_ReserveBand(LY_DOTS - 6, 12);
  Layout_ReserveBand(LY_STATUS - 3, 22);
  UI_DrawStatusLine(LY_STATUS);

  const int n = Cards_Count();
  if (n == 0) {
    UI_TextCentered(T(S_CARDS), LCD_HEIGHT / 2 - 34, C_WHITE, 3);
    centered(T(S_CARDS_NONE), LCD_HEIGHT / 2 + 6, C_GRAY, 2);
    return;
  }
  if (s_cur >= n) s_cur = 0;
  const Card* c = Cards_Get(s_cur);

  char buf[64];

  // A script card draws the whole panel itself. The status line goes on top
  // afterwards (and the screen dots after that, in the .ino), so they are
  // always there whatever it drew.
  if (c->hasScript) {
    char why[72];
    if (!Script_Draw(c, why, sizeof(why))) {
      gfx->fillScreen(C_BLACK);                    // drop whatever it got to draw
      centered(panelText(buf, sizeof(buf), c->title), 84, C_CYAN, 3);
      centered(T(S_SCRIPT_STOPPED), LCD_HEIGHT / 2 - 24, C_YELLOW, 2);
      centered(why, LCD_HEIGHT / 2 + 6, C_GRAY, 2);
    }
    UI_DrawStatusLine(LY_STATUS);
    return;
  }

  centered(panelText(buf, sizeof(buf), c->title), 84, C_CYAN, 3);

  if (!c->valid) {
    const char* msg = (WiFi.status() != WL_CONNECTED) ? T(S_WIFI_WAIT)
                    : (c->stale ? T(S_CARD_NO_DATA) : T(S_LOADING));
    UI_TextCentered(msg, LCD_HEIGHT / 2 - 8, C_YELLOW, 2);
    return;
  }
  if (c->stale) UI_TextCentered(T(S_OLD_DATA), 116, C_YELLOW, 1);

  // Fewer rows get bigger digits. The block of rows is centred on the middle
  // of the panel, where the chord is widest.
  const uint8_t valSize = (c->nRows <= 2) ? 5 : (c->nRows == 3 ? 4 : 3);
  const int labelH = LY_CHAR_H(2), valH = LY_CHAR_H(valSize);
  const int rowH = labelH + 6 + valH;
  const int gap = (c->nRows <= 2) ? 34 : (c->nRows == 3 ? 20 : 14);
  const int total = c->nRows * rowH + (c->nRows - 1) * gap;
  int y = LCD_HEIGHT / 2 + 10 - total / 2;

  const bool comma = (Lang_Get() != LANG_EN);
  for (uint8_t i = 0; i < c->nRows; i++, y += rowH + gap) {
    const CardRow& r = c->rows[i];
    if (r.label[0])
      centered(panelText(buf, sizeof(buf), r.label), y, C_GRAY, 2);

    const int vy = y + labelH + 6;
    if (!r.value[0]) { UI_TextCentered("--", vy, C_GRAY, valSize); continue; }

    char line[48], shown[48];
    snprintf(line, sizeof(line), "%s%s%s", r.value, r.unit[0] ? " " : "", r.unit);
    // Czech writes the decimal point as a comma. Only for values the card
    // formatted as numbers - a date or a version string keeps its dots.
    if (comma && r.numeric)
      for (char* p = line; *p; p++) if (*p == '.') *p = ',';
    centered(panelText(shown, sizeof(shown), line), vy, C_WHITE, valSize);
  }

  if (c->note[0]) {
    char line[56], shown[56];
    snprintf(line, sizeof(line), "%s%s%s", c->noteLabel, c->noteLabel[0] ? " " : "", c->note);
    centered(panelText(shown, sizeof(shown), line), LY_RANGE, C_GRAY, 2);
  }
}
