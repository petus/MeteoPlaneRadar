// =============================================================================
//  MeteoPlaneRadar
//  Screen: spot electricity price. See ScreenPrice.h.
//
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
//  Board:   Waveshare ESP32-S3-Touch-LCD-2.1 (round 480x480 display, ST7701)
// =============================================================================
#include "AAFont.h"   // CZT(): accents with the smooth fonts
#include "ScreenPrice.h"
#include "Energy.h"
#include "Settings.h"
#include "Outside.h"
#include "Layout.h"
#include "Lang.h"
#include "UI.h"
#include "Display_ST7701.h"
#include "Config.h"

#include <WiFi.h>
#include <time.h>
#include <math.h>

#define CX (LCD_WIDTH / 2)
#define CY (LCD_HEIGHT / 2)

// The ring. Deliberately well inside the rim: the screen dots at the top and
// the clock line under them are drawn by code that knows nothing about this
// screen, so the ring has to stay out of their way rather than be drawn over.
// R_OUT 188 puts the top of the ring at y=52, just below the clock line.
#define R_OUT 188
#define R_IN  138

// Hour labels sit inside the ring, where there is nothing else.
#define R_LABEL 116

// The middle. Read top to bottom: which day, the price, its unit, the min and
// max hour of that day, what the number includes, and what a gesture will do.
//
// EVERYTHING lives inside the ring. It used to put the last two lines on
// LY_FOOTER like the other screens do, which on a screen whose ring reaches
// y=428 meant drawing them straight onto the coloured sectors - the text was
// there, it just could not be seen. On this screen the rim belongs to the dial
// and the only free canvas is the disc in the middle, so the stack is packed
// to fit inside R_IN and nothing is drawn outside it.
#define DAY_Y     132
#define BIG_Y     162
#define UNIT_Y    208
#define LO_Y      250
#define HI_Y      276
#define HINT_Y    306
#define NOTE_Y    332

// How long a tapped hour stays in the middle before the display goes back to
// showing the current price. Long enough to read and compare a second hour,
// short enough that a device left alone returns to being useful by itself.
#define SEL_HOLD_MS 12000UL

static uint8_t       s_day = 0;          // 0 = today, 1 = tomorrow
static int           s_sel = -1;         // tapped hour, -1 = none
static unsigned long s_selAt = 0;
static int           s_lastSlot = -1;
static bool          s_lastValid = false;

void ScreenPrice_Enter() {
  s_day = 0;
  s_sel = -1;
  s_lastSlot = -1;
}

// Tomorrow can only be shown once it exists - the day-ahead auction clears
// around 14:00 and there is genuinely nothing to draw before that.
static void setDay(uint8_t d) {
  if (d == 1 && !Energy_TomorrowValid()) d = 0;
  if (d == s_day) return;
  s_day = d;
  s_sel = -1;
}

void ScreenPrice_ChangeRange(int dir) {
  (void)dir;                       // two states - either direction toggles
  setDay(s_day ? 0 : 1);
}

void ScreenPrice_RangeText(char* out, size_t cap) {
  if (!out || !cap) return;
  snprintf(out, cap, "%s", s_day ? T(S_TOMORROW) : T(S_TODAY));
}

bool ScreenPrice_Tick() {
  const bool valid = Energy_PriceValid();
  bool want = false;

  if (valid != s_lastValid) { s_lastValid = valid; want = true; }

  // The selection expiring is a change on screen, so it has to ask for a redraw
  // rather than waiting for the next hour to tick over.
  if (s_sel >= 0 && millis() - s_selAt >= SEL_HOLD_MS) { s_sel = -1; want = true; }

  // Redraw on the QUARTER hour: that is when the needle moves to a new block
  // and, more to the point, when the price in the middle actually changes.
  if (Outside_TimeValid()) {
    time_t now = time(nullptr);
    struct tm lt; localtime_r(&now, &lt);
    const int slot = Price_SlotOf(lt.tm_hour, lt.tm_min);
    if (slot != s_lastSlot) { s_lastSlot = slot; want = true; }
  }

  // ...but the clock at the top of the screen moves every minute, and without
  // this it would only be redrawn on the quarter hour.
  if (UI_StatusLineChanged()) want = true;

  return want;
}

// --- Colour -----------------------------------------------------------------
// Blend two RGB565 colours. t is 0..1 and is clamped by the caller.
static uint16_t mix565(uint16_t a, uint16_t b, float t) {
  const int ar = (a >> 11) & 0x1F, ag = (a >> 5) & 0x3F, ab = a & 0x1F;
  const int br = (b >> 11) & 0x1F, bg = (b >> 5) & 0x3F, bb = b & 0x1F;
  const int r = ar + (int)((br - ar) * t + 0.5f);
  const int g = ag + (int)((bg - ag) * t + 0.5f);
  const int bl = ab + (int)((bb - ab) * t + 0.5f);
  return (uint16_t)((r << 11) | (g << 5) | bl);
}

// Green -> yellow -> orange -> red across the day's own range.
//
// Two decisions worth writing down. The span is never allowed below a floor:
// on a flat day the difference between the cheapest and dearest hour can be a
// few haléře, and stretching THAT across the full palette paints a dramatic
// picture of nothing happening. And the top of the scale is pinned once the
// price passes PRICE_SCALE_MAX, so an expensive day reads as expensive instead
// of merely "red at its own top end".
static uint16_t priceColor(float czkMwh, float lo, float hi) {
  if (isnan(czkMwh)) return C_DKGRAY;
  if (czkMwh < PRICE_NEGATIVE_BELOW) return C_CYAN;   // paid to consume

  float span = hi - lo;
  if (span < 400.0f) span = 400.0f;
  float t = (czkMwh - lo) / span;
  if (czkMwh >= PRICE_SCALE_MAX) t = 1.0f;
  if (t < 0) t = 0;
  if (t > 1) t = 1;

  if (t < 0.5f) return mix565(C_GREEN,  C_YELLOW, t * 2.0f);
  return               mix565(C_YELLOW, C_RED,    (t - 0.5f) * 2.0f);
}

// --- Geometry ---------------------------------------------------------------
// Half the width of the DISC inside the ring at height y - the counterpart of
// Layout_ChordHalf(), which measures the panel. Everything in the middle of
// this screen has to fit inside the ring, not merely inside the glass.
static int innerHalf(int y) {
  const long dy = (long)y - CY;
  const long d2 = (long)(R_IN - 6) * (R_IN - 6) - dy * dy;
  if (d2 <= 0) return 0;
  return (int)sqrtf((float)d2);
}

// Midnight at the top, running clockwise, so the dial is read exactly like the
// clock screen next to it. The unit is a QUARTER HOUR, so a full turn is 96
// steps of 3.75 degrees - about twelve pixels of arc each at this radius, which
// is still a distinguishable block.
static inline float slotAngle(float slot) {
  return (slot * (360.0f / PRICE_SLOTS) - 90.0f) * 0.0174532925f;
}

// Sectors of this screen's dial, which is always centred on the panel.
static inline void fillSector(float a0, float a1, int rIn, int rOut, uint16_t col) {
  UI_FillRing(CX, CY, rIn, rOut, a0, a1, col);
}

// --- Drawing ----------------------------------------------------------------
static void drawRing() {
  float lo = 0, hi = 0;
  if (!Energy_DayRange(s_day, &lo, &hi)) return;

  // No gap between the quarter hours: at 3.75 degrees apiece a separator would
  // eat a third of every block, and the price through a day is a curve rather
  // than ninety-six unrelated numbers. The structure comes from the hour ticks
  // below instead.
  for (int i = 0; i < PRICE_SLOTS; i++) {
    const float p = s_day ? Energy_PriceTomorrow(i) : Energy_PriceToday(i);
    if (isnan(p)) continue;
    fillSector(slotAngle(i), slotAngle(i + 1), R_IN, R_OUT, priceColor(p, lo, hi));
  }

  // A short black notch on the outer edge at every full hour. This is what
  // stops the ring reading as a smear: you can count round to the hour you
  // care about without the notches cutting the colour band in half.
  for (int h = 0; h < 24; h++) {
    const float a = slotAngle(h * PRICE_PER_HOUR);
    const float c = cosf(a), s = sinf(a);
    gfx->drawLine(CX + (int)((R_OUT - 7) * c), CY + (int)((R_OUT - 7) * s),
                  CX + (int)((R_OUT + 1) * c), CY + (int)((R_OUT + 1) * s), C_BLACK);
  }

  // Quarter marks, drawn over the sectors so they survive whatever colour is
  // underneath. These are what turn a ring of colour into a readable dial.
  for (int h = 0; h < 24; h += 6) {
    const float a = slotAngle(h * PRICE_PER_HOUR);
    const float c = cosf(a), s = sinf(a);
    gfx->drawLine(CX + (int)((R_IN - 4) * c), CY + (int)((R_IN - 4) * s),
                  CX + (int)((R_OUT + 4) * c), CY + (int)((R_OUT + 4) * s), C_BLACK);
    char lbl[4];
    snprintf(lbl, sizeof(lbl), "%d", h);
    const int w = Layout_TextW(lbl, 2);
    const int lx = CX + (int)(R_LABEL * c) - w / 2;
    const int ly = CY + (int)(R_LABEL * s) - 8;
    if (Layout_Claim(lx - 3, ly - 3, w + 6, LY_CHAR_H(2) + 6)) {
      gfx->setTextSize(2);
      gfx->setTextColor(C_GRAY);
      gfx->setCursor(lx, ly);
      gfx->print(lbl);
    }
  }
}

// The needle. Only on today's dial - on tomorrow's it would point at an hour
// that has not happened, which is worse than no marker at all.
static void drawNeedle() {
  if (s_day != 0 || !Outside_TimeValid()) return;
  time_t now = time(nullptr);
  struct tm lt; localtime_r(&now, &lt);
  const float a = slotAngle((lt.tm_hour + lt.tm_min / 60.0f) * PRICE_PER_HOUR);

  // Three lines a fraction of a degree apart rather than one: a single-pixel
  // needle over a bright sector is nearly invisible at arm's length.
  for (int k = -1; k <= 1; k++)
    fillSector(a + k * 0.004f, a + k * 0.004f, R_IN - 12, R_OUT + 12, C_WHITE);

  const float c = cosf(a), s = sinf(a);
  gfx->fillCircle(CX + (int)((R_OUT + 12) * c), CY + (int)((R_OUT + 12) * s), 4, C_WHITE);
}

// The tapped hour, marked so it is obvious which sector the middle is showing.
static void drawSelection() {
  if (s_sel < 0) return;
  const float a0 = slotAngle(s_sel), a1 = slotAngle(s_sel + 1);
  fillSector(a0, a0, R_IN - 10, R_OUT + 10, C_WHITE);
  fillSector(a1, a1, R_IN - 10, R_OUT + 10, C_WHITE);
}

// One "label value" line in the middle, centred as a pair so the two lines
// under the price line up with each other.
static void infoRow(int y, const char* label, const char* value, uint16_t valCol) {
  const int lw = Layout_TextW(label, 2);
  const int vw = Layout_TextW(value, 2);
  const int gap = 10;
  const int x0 = CX - (lw + gap + vw) / 2;
  if (!Layout_Claim(x0 - 4, y - 3, lw + gap + vw + 8, LY_CHAR_H(2) + 6)) return;
  gfx->setTextSize(2);
  gfx->setTextColor(C_GRAY);
  gfx->setCursor(x0, y);
  gfx->print(label);
  gfx->setTextColor(valCol);
  gfx->setCursor(x0 + lw + gap, y);
  gfx->print(value);
}

// CZK/kWh with a comma, because that is how the number is written in Czech and
// this string is never parsed by anything.
static void fmtKwh(float czkMwh, char* out, size_t cap) {
  if (isnan(czkMwh)) { snprintf(out, cap, "--"); return; }
  const float v = Energy_FinalCzkKwh(czkMwh);
  snprintf(out, cap, "%.2f", v);
  if (Lang_Get() != LANG_EN) for (char* p = out; *p; p++) if (*p == '.') *p = ',';
}

static void drawCentre() {
  char buf[40], val[24];

  // --- which day ---
  const char* dayLbl = s_day ? T(S_TOMORROW) : T(S_TODAY);
  Layout_ReserveTextCentered(dayLbl, 2, CX, DAY_Y);
  UI_TextCentered(dayLbl, DAY_Y, C_GRAY, 2);

  // --- the number in the middle ---
  // Either the hour that was tapped, or - on today's dial - the hour we are in.
  // On tomorrow's dial with nothing tapped there is no "now", so the cheapest
  // hour takes the middle: it is the reason anyone looks at tomorrow at all.
  int showSlot = s_sel;
  bool isNow = false;
  if (showSlot < 0) {
    if (s_day == 0 && Outside_TimeValid()) {
      time_t now = time(nullptr);
      struct tm lt; localtime_r(&now, &lt);
      showSlot = Price_SlotOf(lt.tm_hour, lt.tm_min);
      isNow = true;
    } else {
      showSlot = Energy_CheapestSlot(s_day);
    }
  }

  const float p = s_day ? Energy_PriceTomorrow(showSlot) : Energy_PriceToday(showSlot);
  fmtKwh(p, val, sizeof(val));

  float lo = 0, hi = 0;
  const bool haveRange = Energy_DayRange(s_day, &lo, &hi);
  const uint16_t col = haveRange ? priceColor(p, lo, hi) : C_WHITE;

  Layout_ReserveTextCentered(val, 5, CX, BIG_Y);
  UI_TextCentered(val, BIG_Y, col, 5);

  // The unit, and - when the middle is NOT showing the current hour - which
  // hour it is showing. A big number with no hour next to it would be read as
  // the price right now, which on tomorrow's dial is simply wrong.
  if (isNow || showSlot < 0) snprintf(buf, sizeof(buf), "%s", CZT("Kc/kWh", "Kč/kWh"));
  else snprintf(buf, sizeof(buf), "%s  %02d:%02d", CZT("Kc/kWh", "Kč/kWh"),
                Price_SlotHour(showSlot), Price_SlotMinute(showSlot));
  Layout_ReserveTextCentered(buf, 1, CX, UNIT_Y);
  UI_TextCentered(buf, UNIT_Y, C_GRAY, 1);

  // --- cheapest and dearest hour of this day ---
  const int cheap = Energy_CheapestSlot(s_day);
  const int dear  = Energy_PriceHighestSlot(s_day);

  // "min" and "max" rather than spelled-out superlatives. They are the same
  // word in both languages, they are what a chart would call them, and the
  // dozen pixels they give back go to the hour and the price beside them.
  //
  // The time is Price_SlotHour/Minute, NOT the slot number: since the switch
  // to quarter-hour trading a slot runs 0..95, so a bare "%02d:00" printed
  // things like "63:00". The clock has to be rebuilt from the slot every time.
  if (cheap >= 0) {
    const float cp = s_day ? Energy_PriceTomorrow(cheap) : Energy_PriceToday(cheap);
    fmtKwh(cp, val, sizeof(val));
    snprintf(buf, sizeof(buf), "%02d:%02d  %s",
             Price_SlotHour(cheap), Price_SlotMinute(cheap), val);
    infoRow(LO_Y, "min", buf, C_GREEN);
  }
  if (dear >= 0 && dear != cheap) {
    const float dp = s_day ? Energy_PriceTomorrow(dear) : Energy_PriceToday(dear);
    fmtKwh(dp, val, sizeof(val));
    snprintf(buf, sizeof(buf), "%02d:%02d  %s",
             Price_SlotHour(dear), Price_SlotMinute(dear), val);
    infoRow(HI_Y, "max", buf, C_ORANGE);
  }

  // --- what the number actually is ---
  // The spot price alone and the price with the user's own tariff on top are
  // very different numbers, and which one is on screen has to be unambiguous -
  // otherwise someone compares it with their bill and concludes the device is
  // broken.
  //
  // Grey at size 2, not dark grey at size 1. It said "burza" in 0x2124 on
  // black, which on this panel is a shade off invisible - and a caveat nobody
  // can read is not a caveat. This line is the whole difference between the
  // exchange price and a bill, so it gets read or it does not belong here.
  {
    char note[40];
    if (Settings_PriceFee() == 0 && Settings_PriceVat() == 0)
      snprintf(note, sizeof(note), "%s", T(S_SPOT));
    else
      snprintf(note, sizeof(note), "+%u Kc/MWh%s", (unsigned)Settings_PriceFee(),
               Settings_PriceVat() ? " +DPH" : "");
    // A four-digit surcharge with VAT is wider than the disc is at this
    // height. Drop a size rather than let it run under the ring - the same
    // thing the clock screen does with a long Czech month name.
    uint8_t sz = 2;
    if (Layout_TextW(note, 2) > 2 * innerHalf(HINT_Y + LY_CHAR_H(2)) - 8) sz = 1;
    UI_TextCentered(note, HINT_Y, C_GRAY, sz);
  }
}

void ScreenPrice_Draw() {
  gfx->fillScreen(C_BLACK);
  Layout_Begin();

  Layout_ReserveBand(LY_DOTS - 6, 12);
  Layout_ReserveBand(LY_STATUS - 3, 22);
  UI_DrawStatusLine(LY_STATUS);

  // Outside Czechia there is nothing to load and nothing will ever arrive, so
  // say that instead of spinning on "Nacitam..." for ever. The fetcher is quiet
  // too - see Energy_Tick().
  if (!Energy_PriceInArea()) {
    UI_TextCentered(T(S_PRICE), CY - 30, C_WHITE, 2);
    UI_TextCentered(T(S_PRICE_CZ_ONLY), CY, C_YELLOW, 2);
    UI_TextCentered(T(S_OUT_OF_AREA), CY + 26, C_GRAY, 1);
    return;
  }

  if (!Energy_PriceValid()) {
    UI_TextCentered(T(S_PRICE), CY - 20, C_WHITE, 2);
    UI_TextCentered(WiFi.status() == WL_CONNECTED ? T(S_LOADING) : T(S_WIFI_WAIT),
                    CY + 10, C_YELLOW, 2);
    return;
  }

  // Asked for tomorrow, and then the day rolled over or the data went away.
  if (s_day == 1 && !Energy_TomorrowValid()) s_day = 0;

  drawRing();
  drawNeedle();
  drawSelection();
  drawCentre();

  // The bottom line of the middle: either what a tap will switch to, or why
  // there is nothing to switch to. Inside the disc, for the reason given above
  // the layout constants.
  {
    const char* note = nullptr;
    if (Energy_TomorrowValid())  note = s_day ? T(S_TODAY) : T(S_TOMORROW);
    else if (s_day == 0)         note = T(S_NO_TOMORROW);
    if (note && Layout_TextW(note, 1) <= 2 * innerHalf(NOTE_Y + LY_CHAR_H(1)) - 6)
      UI_TextCentered(note, NOTE_Y, C_GRAY, 1);
  }
}

bool ScreenPrice_HandleTap(int x, int y) {
  if (!Energy_PriceValid()) return false;

  const float dx = (float)(x - CX), dy = (float)(y - CY);
  const float r = sqrtf(dx * dx + dy * dy);

  // Inside the ring: the middle is the day switch. A generous target - it is
  // most of the screen - because that is the gesture people find first.
  if (r < R_IN - 6) {
    if (!Energy_TomorrowValid()) {
      // Nothing to switch to. Clearing a selection is still a sensible thing
      // for a tap in the middle to do.
      if (s_sel < 0) return false;
      s_sel = -1;
      return true;
    }
    setDay(s_day ? 0 : 1);
    return true;
  }

  // On the ring: read that hour out. atan2f gives -pi..pi measured from the
  // +x axis, and the dial starts at the top, so a quarter turn is added back
  // before it is divided into hours.
  if (r <= R_OUT + 14) {
    float a = atan2f(dy, dx) * 57.2957795f + 90.0f;
    while (a < 0)    a += 360.0f;
    while (a >= 360) a -= 360.0f;
    const int i = (int)(a / (360.0f / PRICE_SLOTS)) % PRICE_SLOTS;
    const float p = s_day ? Energy_PriceTomorrow(i) : Energy_PriceToday(i);
    if (isnan(p)) return false;
    s_sel = (s_sel == i) ? -1 : i;      // tapping the same block again clears it
    s_selAt = millis();
    return true;
  }
  return false;
}
