// =============================================================================
//  MeteoPlaneRadar
//  Screen: clock, date, current weather and a seconds ring.
//
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
//  Board:   Waveshare ESP32-S3-Touch-LCD-2.1 (round 480x480 display, ST7701)
// =============================================================================
#include "ScreenClock.h"
#include "Settings.h"
#include "Outside.h"
#include "Forecast.h"
#include "WxIcon.h"
#include "NightMode.h"
#include "Layout.h"
#include "Lang.h"
#include "UI.h"
#include "Display_ST7701.h"
#include "Config.h"
#include "AAFont.h"
#include "NameDay.h"
#include "Moon.h"

#include <time.h>
#include <math.h>
#include <stdio.h>

#define CX (LCD_WIDTH / 2)
#define CY (LCD_HEIGHT / 2)

// Vertical stack, read top to bottom: name day, date, time, conditions, then
// wind (layout A) or the bottom rows of layouts C and D. The positions live in
// ClockLay below (0.9.3).
#define WX_ICON_R   26
// Pixel font: the time at text size 10 (0.9.3, was 8) - 60 x 80 px a character,
// "16:59" 300 px wide, digits 70 px tall.
#define PX_CLK_SIZE 10

static int s_lastMin = -1;
static int s_lastSec = -1;

void ScreenClock_Enter() { s_lastMin = -1; s_lastSec = -1; }

bool ScreenClock_Tick() {
  if (!Outside_TimeValid()) return false;
  time_t now = time(nullptr);
  struct tm lt; localtime_r(&now, &lt);
  // With the ring off there is nothing on this screen that changes faster than
  // once a minute, so do not spend a redraw per second on it.
  if (Settings_SecondsStyle() == SEC_STYLE_OFF) {
    if (lt.tm_min == s_lastMin) return false;
    s_lastMin = lt.tm_min;
    return true;
  }
  if (lt.tm_sec == s_lastSec) return false;
  s_lastSec = lt.tm_sec;
  s_lastMin = lt.tm_min;
  return true;
}

bool ScreenClock_HandleTap(int x, int y) {
  (void)x; (void)y;
  if (Settings_NightAuto()) return false;    // automatic mode owns the decision
  NightMode_Toggle();
  return true;
}

// Scale an RGB565 colour towards black. Used for the comet tail, where a fixed
// second colour would just be a bright block rather than a fading trail.
static uint16_t dim(uint16_t c, uint8_t num, uint8_t den) {
  if (den == 0) return 0;
  uint16_t r = (c >> 11) & 0x1F, g = (c >> 5) & 0x3F, b = c & 0x1F;
  r = (uint16_t)((uint32_t)r * num / den);
  g = (uint16_t)((uint32_t)g * num / den);
  b = (uint16_t)((uint32_t)b * num / den);
  return (uint16_t)((r << 11) | (g << 5) | b);
}

// --- Name day (0.9.2) ------------------------------------------------------------
// One line above the date: "Svátek: Martin" / "Meniny: Martin, Maroš", or a
// public holiday on its own ("Svátek práce"). Nothing in English. It goes above
// the date rather than below the time because that is where the calendar part
// of the screen is, and the bottom already has the day/night hint and the
// "missed print alert" dot.
//
// The chord is narrow up there, so a long list first loses the label and only
// then names from the end, until it fits.
static int s_ndMon = 0, s_ndDay = 0;      // textcheck: a fixed date (0 = today)
void ScreenClock_TestNameDate(int month, int day) { s_ndMon = month; s_ndDay = day; }

#define ND_COLOR  dim(Settings_ClockColor(), 3, 5)   // the clock's colour, quieter

// width(text) in pixels for whichever font draws it; maxW the room on that line.
static bool nameDayLine(const struct tm& lt, bool ascii, char* out, size_t cap,
                        int (*width)(const char*), int maxW) {
  if (!Settings_NameDay()) return false;
  const int mon = s_ndMon ? s_ndMon : lt.tm_mon + 1;
  const int day = s_ndMon ? s_ndDay : lt.tm_mday;
  char names[96];
  bool holiday = false;
  if (!NameDay_Get(Lang_Get(), mon, day, ascii, names, sizeof(names), &holiday)) return false;
  if (holiday) {
    snprintf(out, cap, "%s", names);
    return width(out) <= maxW;
  }
  const char* label = LT("Svatek:", "Svátek:", "", "Meniny:", "Meniny:");
  snprintf(out, cap, "%s %s", label, names);
  if (width(out) <= maxW) return true;
  // Every name matters more than the label: above the date a list of names
  // reads as the name day anyway. Names go only when even that is too long.
  for (;;) {
    snprintf(out, cap, "%s", names);
    if (width(out) <= maxW) return true;
    if (!NameDay_DropLast(names)) return false;
  }
}

static const AAFont* s_ndFont = nullptr;
static int ndWidthAA(const char* s)  { return AAFont_TextW(s_ndFont, s); }
static int ndWidthPix(const char* s) { return Layout_TextW(s, 2); }

// Position of second `s` on the ring. 0 at the top, clockwise.
static void secPos(int s, int r, int* x, int* y) {
  float a = (s * 6.0f - 90.0f) * 0.0174532925f;
  *x = CX + (int)(r * cosf(a));
  *y = CY + (int)(r * sinf(a));
}

static void drawSecondsRing(int sec) {
  const uint8_t style = Settings_SecondsStyle();
  if (style == SEC_STYLE_OFF) return;
  const int R = LY_SEC_RING_R;
  const uint16_t on  = Settings_SecondsColor();
  const uint16_t off = C_DKGRAY;
  // 0.9.2: every fifth second (5, 10 ... 60) is a slightly bigger dot in the
  // same colour, like the five-minute marks on a dial, so the ring reads at a
  // glance. FIVE() is the extra radius.
#define FIVE(i) (((i) % 5 == 0) ? 1 : 0)

  switch (style) {
    case SEC_STYLE_DOTS:
      for (int i = 0; i < 60; i++) {
        int x, y; secPos(i, R, &x, &y);
        if (i <= sec) gfx->fillCircle(x, y, 3 + FIVE(i), on);
        else          gfx->fillCircle(x, y, 2 + FIVE(i), off);
      }
      break;

    case SEC_STYLE_LINE: {
      // A continuous arc. Drawn as 60 short chords rather than with drawArc,
      // which the GFX build here does not have.
      int px = 0, py = 0;
      for (int i = 0; i <= 60; i++) {
        int x, y; secPos(i, R, &x, &y);
        if (i > 0) gfx->drawLine(px, py, x, y, (i <= sec) ? on : off);
        px = x; py = y;
      }
      // Five-second marks on the arc (0.9.2).
      for (int i = 0; i < 60; i += 5) {
        int x, y; secPos(i, R, &x, &y);
        gfx->fillCircle(x, y, 3, (i <= sec) ? on : off);
      }
      // A little weight on the leading end so the arc has a visible head.
      int hx, hy; secPos(sec, R, &hx, &hy);
      gfx->fillCircle(hx, hy, 4, on);
      break;
    }

    case SEC_STYLE_COMET: {
      const int TAIL = 12;
      for (int i = 0; i < 60; i++) {          // faint track underneath
        int x, y; secPos(i, R, &x, &y);
        if (FIVE(i)) gfx->fillCircle(x, y, 2, off);   // five-second marks (0.9.2)
        else         gfx->drawPixel(x, y, off);
      }
      for (int k = TAIL; k >= 0; k--) {
        int i = sec - k;
        if (i < 0) i += 60;
        int x, y; secPos(i, R, &x, &y);
        uint16_t c = dim(on, (uint8_t)(TAIL + 1 - k), (uint8_t)(TAIL + 1));
        gfx->fillCircle(x, y, ((k == 0) ? 4 : (k < 4 ? 3 : 2)) + FIVE(i), c);
      }
      break;
    }
  }
#undef FIVE
}

// -----------------------------------------------------------------------------
//  The smooth typefaces (0.8.0)
//
//  Same stack as the pixel version below - date, time, conditions, wind - but
//  positioned by BASELINE, because that is how outline fonts are measured. The
//  digits are tabular (every one the same width), so the time does not shuffle
//  sideways as the minutes tick over.
//
//  The date keeps its accents here: the fonts carry the Czech letters, so
//  "Ne 25. října" no longer has to be spelled "rijna" like on the other screens.
// -----------------------------------------------------------------------------
#define AA_ND_GAP     12      // name day: gap above the date line (0.9.2)
#define ND_MARGIN     16      // name day: room left at each end of the chord

// --- 0.9.3: three layouts, chosen on the web (Settings_ClockLayout) -------------
//   A (CLOCK_LAYOUT_BASIC)  date, time, weather and wind spread over the face
//   C (CLOCK_LAYOUT_SUN)    + today's max / min and sunrise / sunset at the bottom
//   D (CLOCK_LAYOUT_HOURS)  + the next hours: three cells, +2 h, +4 h, +6 h
// In C and D the wind joins the weather row to make room. The name day stays
// above the date in all of them and can be switched off on its own.
struct ClockLay {
  int aaDate, aaClk, aaWx, aaWind;      // smooth fonts: baselines, row centre
  int pxDate, pxClk, pxWx, pxWind;      // pixel font: tops, row centre
  bool windInRow;                       // wind on the weather row (no own line)
};
static const ClockLay LAY_A = { 158, 290, 338, 398,   126, 168, 304, 362,  false };
static const ClockLay LAY_X = { 146, 272, 316,   0,   106, 148, 278,   0,  true  };
static const ClockLay LAY_D = { 132, 252, 296,   0,   116, 156, 278,   0,  true  };
static const ClockLay& clockLay() {
  const uint8_t l = Settings_ClockLayout();
  return l == CLOCK_LAYOUT_BASIC ? LAY_A : (l == CLOCK_LAYOUT_HOURS ? LAY_D : LAY_X);
}

// Bottom rows of C and D. Kept above the day/night hint (LY_FOOTER).
#define BOT1_AA   344     // D: icon row centre
#define BOT2_AA   377     // D: hour labels baseline
#define BOT3_AA   405     // D: rain baseline
// 0.9.7: C is the Sun and the Moon - the Moon's icon shows its phase; today's
// max / min moved into the weather row.
#define SUNR_AA   372     // C: sunrise / sunset baseline
#define MOONR_AA  400     // C: moonrise / moonset baseline
#define SUNR_PX   328     // pixel: C sun row top
#define MOONR_PX  360     // pixel: C moon row top
#define RS_ARROW  10      // width of the rise / set triangle
#define HR1_PX    332     // pixel D: icon row centre
#define HR2_PX    356     // pixel D: hour labels top
#define HR3_PX    380     // pixel D: rain top
#define HOUR_STEP 114     // D: distance between the cells
#define HOUR_R    13      // D: icon radius

// Is it night at t? From the sunrise / sunset of today and tomorrow; without
// them, the current day/night state.
static bool nightAt(time_t t) {
  const int n = Forecast_DayCount();
  const FcDay* d = Forecast_Days();
  for (int i = 0; i < n && i < 2; i++) {
    if (!d[i].sunrise || !d[i].sunset) continue;
    if (t >= d[i].sunrise && t < d[i].sunset) return false;
  }
  if (n > 0 && d[0].sunrise) return true;
  return Settings_IsNight();
}

static void hhmmOf(char* b, size_t n, time_t t) {
  struct tm x; localtime_r(&t, &x);
  snprintf(b, n, "%d:%02d", x.tm_hour, x.tm_min);
}

// --- The Moon (0.9.7) --------------------------------------------------------
// tools/textcheck walks a whole lunation through this; 0 = now.
static time_t s_moonTest = 0;
void ScreenClock_TestMoonTime(time_t t) { s_moonTest = t; }
static time_t moonNow() { return s_moonTest ? s_moonTest : time(nullptr); }

// Rise and set for the local day of lt at the place the forecast is for,
// worked out on the device (Moon.cpp) - once a day, or after the place changes.
// Either time is "--" on a day the Moon does not rise or does not set.
static bool moonToday(const struct tm& lt, char* rs, char* ss, size_t n, double* phase) {
  if (!Settings_HasLocation()) return false;
  static int    s_ymd = 0;
  static double s_lat = 0, s_lon = 0;
  static time_t s_rise = 0, s_set = 0;
  const int ymd = (lt.tm_year + 1900) * 10000 + (lt.tm_mon + 1) * 100 + lt.tm_mday;
  const double la = Settings_Lat(), lo = Settings_Lon();
  if (ymd != s_ymd || la != s_lat || lo != s_lon) {
    struct tm m = lt;
    m.tm_hour = m.tm_min = m.tm_sec = 0;
    m.tm_isdst = -1;
    Moon_Times(mktime(&m), la, lo, &s_rise, &s_set);
    s_ymd = ymd; s_lat = la; s_lon = lo;
  }
  if (s_rise) hhmmOf(rs, n, s_rise); else snprintf(rs, n, "--");
  if (s_set)  hhmmOf(ss, n, s_set);  else snprintf(ss, n, "--");
  *phase = Moon_Phase(moonNow());
  return true;
}

// The room a row of height h has at y: the chord at its narrower edge.
static int rowRoom(int top, int h) {
  const int a = Layout_ChordHalf(top), b = Layout_ChordHalf(top + h);
  return 2 * (a < b ? a : b) - 24;
}

static void drawSmooth(const AAFontSet* fs, const struct tm& lt) {
  const ClockLay& L = clockLay();
  const uint8_t lay = Settings_ClockLayout();
  const int sa = fs->small->ascent;

  // --- Date ---
  char date[48];
  // Lang_*() hand out the accented spelling whenever a smooth face is on.
  if (Lang_Get() == LANG_EN) {
    snprintf(date, sizeof(date), "%s %d %s",
             Lang_WeekdayShort(lt.tm_wday), lt.tm_mday, Lang_MonthName(lt.tm_mon));
  } else {
    snprintf(date, sizeof(date), "%s %d. %s",
             Lang_WeekdayShort(lt.tm_wday), lt.tm_mday, Lang_MonthName(lt.tm_mon));
  }
  {
    const int w = AAFont_TextW(fs->small, date);
    Layout_Reserve(CX - w / 2 - 4, L.aaDate - sa - 4, w + 8, sa + 12);
    AAFont_DrawCentered(fs->small, date, CX, L.aaDate, C_GRAY);
  }

  // --- Name day, above the date (0.9.2) ---
  {
    const int base = L.aaDate - sa - AA_ND_GAP;
    s_ndFont = fs->small;
    char nd[112];
    if (nameDayLine(lt, false, nd, sizeof(nd), ndWidthAA,
                    2 * Layout_ChordHalf(base - sa) - 2 * ND_MARGIN)) {
      const int w = AAFont_TextW(fs->small, nd);
      Layout_Reserve(CX - w / 2 - 4, base - sa - 4, w + 8, sa + 10);
      AAFont_DrawCentered(fs->small, nd, CX, base, ND_COLOR);
    }
  }

  // --- HH:MM ---
  char hhmm[8];
  snprintf(hhmm, sizeof(hhmm), "%02d:%02d", lt.tm_hour, lt.tm_min);
  {
    const int w = AAFont_TextW(fs->big, hhmm);
    const int a = fs->big->ascent;
    Layout_Reserve(CX - w / 2 - 4, L.aaClk - a - 4, w + 8, a + 8);
    AAFont_DrawCentered(fs->big, hhmm, CX, L.aaClk, Settings_ClockColor());
  }

  if (!Forecast_CurrentValid()) return;

  // --- Icon, temperature, rain (and in C/D the wind) on one row ---
  char tbuf[16], pbuf[16], wbuf[16];
  snprintf(tbuf, sizeof(tbuf), "%d °C", (int)lroundf(Forecast_CurrentTemp()));
  const float p = Forecast_CurrentPrecip();
  const bool hasRain = (p >= 0.05f);
  if (hasRain) snprintf(pbuf, sizeof(pbuf), "%.1f mm", p);
  snprintf(wbuf, sizeof(wbuf), "%d km/h", (int)lroundf(Forecast_CurrentWind()));

  // 0.9.7: in C today's max / min joins this row (its line went to the Moon).
  char mmx[24] = "";
  if (lay == CLOCK_LAYOUT_SUN && Forecast_DayCount() > 0) {
    const FcDay& d0 = Forecast_Days()[0];
    snprintf(mmx, sizeof(mmx), "%d°/%d°", (int)lroundf(d0.tmax), (int)lroundf(d0.tmin));
  }
  const int iconW = 2 * WX_ICON_R;
  const int tw    = AAFont_TextW(fs->mid, tbuf);
  const int mw    = mmx[0] ? AAFont_TextW(fs->small, mmx) : 0;
  const int pw    = hasRain ? AAFont_TextW(fs->small, pbuf) : 0;
  const int ww    = L.windInRow ? AAFont_TextW(fs->small, wbuf) : 0;
  const int gap   = 16;
  auto rowW = [&](bool w) {
    return iconW + gap + tw + (mw ? 16 + mw : 0) + (hasRain ? 18 + pw : 0) + (w ? 18 + ww : 0);
  };
  // Rain goes before wind: a row too wide for the disc loses the wind, not
  // the whole row (which is what a failed claim used to do).
  bool wind = L.windInRow;
  if (wind && rowW(true) > rowRoom(L.aaWx - WX_ICON_R - 3, 2 * WX_ICON_R + 6)) wind = false;
  const int totalW = rowW(wind);
  const int x0 = CX - totalW / 2;
  if (Layout_Claim(x0 - 6, L.aaWx - WX_ICON_R - 3, totalW + 12, 2 * WX_ICON_R + 6)) {
    WxIcon_Draw(x0 + WX_ICON_R, L.aaWx, WX_ICON_R, Forecast_CurrentCode(), Settings_IsNight());
    int x = AAFont_Draw(fs->mid, tbuf, x0 + iconW + gap, L.aaWx + fs->mid->ascent / 2, C_WHITE);
    if (mw) x = AAFont_Draw(fs->small, mmx, x + 16, L.aaWx + sa / 2, C_YELLOW);
    if (hasRain) x = AAFont_Draw(fs->small, pbuf, x + 18, L.aaWx + sa / 2, C_CYAN);
    if (wind) AAFont_Draw(fs->small, wbuf, x + 18, L.aaWx + sa / 2, C_GRAY);
  }

  // --- Wind on its own line (A) ---
  if (!L.windInRow) {
    const int w = AAFont_TextW(fs->small, wbuf);
    if (Layout_Claim(CX - w / 2 - 6, L.aaWind - sa - 4, w + 12, sa + 12))
      AAFont_DrawCentered(fs->small, wbuf, CX, L.aaWind, C_GRAY);
  }

  // --- C: Sun, Moon, the Moon's phase (0.9.7) ---
  // Two rows with the same columns - icon, rise, set - so the times line up.
  if (lay == CLOCK_LAYOUT_SUN) {
    char sr[8] = "", st[8] = "", mr[8] = "", ms[8] = "";
    time_t rise = 0, set = 0;
    const bool sun = Forecast_SunTimes(&rise, &set);
    if (sun) { hhmmOf(sr, sizeof(sr), rise); hhmmOf(st, sizeof(st), set); }
    double ph = 0;
    const bool moon = moonToday(lt, mr, ms, sizeof(mr), &ph);
    const int ir = 9, AG = 5, CG = 22;
    int wR = 0, wS = 0;
    if (sun)  { wR = AAFont_TextW(fs->small, sr); wS = AAFont_TextW(fs->small, st); }
    if (moon) { { const int a = AAFont_TextW(fs->small, mr), b = AAFont_TextW(fs->small, ms); if (a > wR) wR = a; if (b > wS) wS = b; } }
    const int tot = 2 * ir + 14 + RS_ARROW + AG + wR + CG + RS_ARROW + AG + wS;
    auto row = [&](int y, bool isMoon, const char* r, const char* t) {
      int x = CX - tot / 2;
      const int cy = y - sa / 2;
      if (!Layout_Claim(x - 4, y - sa - 2, tot + 8, sa + 7)) return;   // 28 px a row
      if (isMoon) WxIcon_Moon(x + ir, cy, ir, ph, Settings_Lat() < 0);
      else        WxIcon_Draw(x + ir, cy, ir, 0, false);
      x += 2 * ir + 14;
      WxIcon_RiseSet(x, cy, RS_ARROW, true);
      AAFont_Draw(fs->small, r, x + RS_ARROW + AG, y, C_GRAY);
      x += RS_ARROW + AG + wR + CG;
      WxIcon_RiseSet(x, cy, RS_ARROW, false);
      AAFont_Draw(fs->small, t, x + RS_ARROW + AG, y, C_GRAY);
    };
    if (sun)  row(SUNR_AA, false, sr, st);
    if (moon) {
      row(MOONR_AA, true, mr, ms);       // its icon is the phase
    }
  }

  // --- D: the next hours ---
  if (lay == CLOCK_LAYOUT_HOURS && Forecast_Valid()) {
    const FcHour* h = Forecast_Hours();
    const int n = Forecast_HourCount();
    for (int i = 0; i < 3; i++) {
      const int idx = (i + 1) * 2;                        // +2 h, +4 h, +6 h
      if (idx >= n) break;
      const FcHour& e = h[idx];
      const int cx = CX + (i - 1) * HOUR_STEP;
      struct tm x; localtime_r(&e.t, &x);
      char lab[8], tb[8];
      snprintf(lab, sizeof(lab), "%d h", x.tm_hour);
      snprintf(tb, sizeof(tb), "%d°", (int)lroundf(e.temp));
      const bool rain = e.precip >= 0.05f;
      const int bottom = rain ? BOT3_AA : BOT2_AA;
      if (!Layout_Claim(cx - 46, BOT1_AA - HOUR_R - 2, 92, bottom - (BOT1_AA - HOUR_R) + 8)) continue;
      WxIcon_Draw(cx - 16, BOT1_AA, HOUR_R, e.code, nightAt(e.t));
      AAFont_Draw(fs->small, tb, cx + 2, BOT1_AA + sa / 2, C_WHITE);
      AAFont_DrawCentered(fs->small, lab, cx, BOT2_AA, C_GRAY);
      if (rain) {
        // The number in the row's font, "mm" a size smaller (the text face of
        // the same family through print()), so two neighbouring cells do not
        // run into each other.
        char rb[12]; snprintf(rb, sizeof(rb), "%.1f", e.precip);
        const int nw = AAFont_TextW(fs->small, rb);
        const int uw = Layout_TextW("mm", 2);
        const int x = cx - (nw + 3 + uw) / 2;
        AAFont_Draw(fs->small, rb, x, BOT3_AA, C_CYAN);
        gfx->setTextSize(2); gfx->setTextColor(C_CYAN);
        gfx->setCursor(x + nw + 3, BOT3_AA - 14);   // 6x8 cell: baseline at +7 * size
        gfx->print("mm");
      }
    }
  }
}

// 0.9.0: see ScreenClock_SetBadge(). Below the footer line, above the rim.
#define BADGE_Y (LY_FOOTER + 26)
static uint16_t s_badge = 0;

void ScreenClock_Draw() {
  gfx->fillScreen(C_BLACK);
  Layout_Begin();

  // The screen dots at the top belong to the screen manager - reserve their
  // band so nothing here can land on them.
  Layout_ReserveBand(LY_DOTS - 6, 12);

  if (!Outside_TimeValid()) {
    UI_TextCentered(T(S_WIFI_WAIT), CY - 8, C_YELLOW, 2);
    return;
  }

  time_t now = time(nullptr);
  struct tm lt; localtime_r(&now, &lt);

  // --- Seconds ring (outermost, drawn first) ---
  drawSecondsRing(lt.tm_sec);

  const AAFontSet* fs = AAFont_ClockSet(Settings_ClockFont());
  if (fs) {
    drawSmooth(fs, lt);
  } else {
    // --- Weekday and date (above the clock) ---
    char date[40];
    if (Lang_Get() == LANG_EN) {
      snprintf(date, sizeof(date), "%s %d %s",
               Lang_WeekdayShort(lt.tm_wday), lt.tm_mday, Lang_MonthName(lt.tm_mon));
    } else {
      snprintf(date, sizeof(date), "%s %d. %s",
               Lang_WeekdayShort(lt.tm_wday), lt.tm_mday, Lang_MonthName(lt.tm_mon));
    }
    // Long Czech month names ("listopadu") can outgrow the chord at size 2 - drop
    // to size 1 rather than letting it run off the rim.
    uint8_t dsize = 2;
    const ClockLay& L = clockLay();
    const uint8_t lay = Settings_ClockLayout();
    if (Layout_TextW(date, 2) > 2 * Layout_ChordHalf(L.pxDate + 16) - 16) dsize = 1;
    Layout_ReserveTextCentered(date, dsize, CX, L.pxDate);
    UI_TextCentered(date, L.pxDate, C_GRAY, dsize);

    // --- Name day, above the date (0.9.2) ---
    {
      char nd[112];
      const int ndY = L.pxDate - 26;
      if (nameDayLine(lt, true, nd, sizeof(nd), ndWidthPix,
                      2 * Layout_ChordHalf(ndY) - 2 * ND_MARGIN)) {
        Layout_ReserveTextCentered(nd, 2, CX, ndY);
        UI_TextCentered(nd, ndY, ND_COLOR, 2);
      }
    }

    // --- HH:MM ---
    // PX_CLK_SIZE 10 is 60 px per character, so five characters come to
    // 300 px - well inside the chord at this height (about 450 px).
    char hhmm[8];
    snprintf(hhmm, sizeof(hhmm), "%02d:%02d", lt.tm_hour, lt.tm_min);
    Layout_ReserveTextCentered(hhmm, PX_CLK_SIZE, CX, L.pxClk);
    UI_TextCentered(hhmm, L.pxClk, Settings_ClockColor(), PX_CLK_SIZE);

    // --- Current conditions: icon, temperature and rain on one row ---
    // All of it comes from the forecast request, so the row simply stays empty
    // until the first one lands.
    if (Forecast_CurrentValid()) {
      char tbuf[16], pbuf[16];
      // Space before the unit, as everywhere else in the project and as SI asks
      // for: "18 degC" and "18 °C" both want the gap, and "18°C" would be the
      // one spelling that is wrong either way.
      snprintf(tbuf, sizeof(tbuf), "%d %s", (int)lroundf(Forecast_CurrentTemp()),
               OUTSIDE_DEG_TEXT);

      // Precipitation shares the temperature's row: most of the time there is
      // none, and an empty row would pull the stack off centre.
      const float p = Forecast_CurrentPrecip();
      const bool hasRain = (p >= 0.05f);
      if (hasRain) snprintf(pbuf, sizeof(pbuf), "%.1f mm", p);

      char wbuf[16];
      snprintf(wbuf, sizeof(wbuf), "%d km/h", (int)lroundf(Forecast_CurrentWind()));
      // 0.9.7: in C today's max / min joins this row.
      char mmx[24] = "";
      if (lay == CLOCK_LAYOUT_SUN && Forecast_DayCount() > 0) {
        const FcDay& d0 = Forecast_Days()[0];
        snprintf(mmx, sizeof(mmx), "%d\xF8/%d\xF8", (int)lroundf(d0.tmax), (int)lroundf(d0.tmin));
      }
      const int iconW = 2 * WX_ICON_R;
      const int tw    = Layout_TextW(tbuf, 3);
      const int mw    = mmx[0] ? Layout_TextW(mmx, 2) : 0;
      const int pw    = hasRain ? Layout_TextW(pbuf, 2) : 0;
      const int ww    = L.windInRow ? Layout_TextW(wbuf, 2) : 0;
      const int gap   = 14;
      const int WY = L.pxWx;
      auto rowW = [&](bool w) {
        return iconW + gap + tw + (mw ? 14 + mw : 0) + (hasRain ? 18 + pw : 0) + (w ? 18 + ww : 0);
      };
      // Rain goes before wind (see the smooth version).
      bool wind = L.windInRow;
      if (wind && rowW(true) > rowRoom(WY - WX_ICON_R - 3, 2 * WX_ICON_R + 6)) wind = false;
      const int totalW = rowW(wind);
      const int x0 = CX - totalW / 2;

      if (Layout_Claim(x0 - 6, WY - WX_ICON_R - 3, totalW + 12, 2 * WX_ICON_R + 6)) {
        WxIcon_Draw(x0 + WX_ICON_R, WY, WX_ICON_R,
                    Forecast_CurrentCode(), Settings_IsNight());
        int x = x0 + iconW + gap;
        gfx->setTextSize(3);
        gfx->setTextColor(C_WHITE);
        gfx->setCursor(x, WY - 12);
        gfx->print(tbuf);
        x += tw;
        gfx->setTextSize(2);
        if (mw) {
          gfx->setTextColor(C_YELLOW);
          gfx->setCursor(x + 14, WY - 8);
          gfx->print(mmx);
          x += 14 + mw;
        }
        if (hasRain) {
          gfx->setTextColor(C_CYAN);
          gfx->setCursor(x + 18, WY - 8);
          gfx->print(pbuf);
          x += 18 + pw;
        }
        if (wind) {
          gfx->setTextColor(C_GRAY);
          gfx->setCursor(x + 18, WY - 8);
          gfx->print(wbuf);
        }
      }

      // --- Wind, on its own line (A) ---
      if (!L.windInRow) {
        const int w = Layout_TextW(wbuf, 2);
        if (Layout_Claim(CX - w / 2 - 6, L.pxWind - 3, w + 12, LY_CHAR_H(2) + 6)) {
          UI_TextCentered(wbuf, L.pxWind, C_GRAY, 2);
        }
      }

      // --- C: Sun, Moon, the Moon's phase (0.9.7) ---
      if (lay == CLOCK_LAYOUT_SUN) {
        char sr[8] = "", st[8] = "", mr[8] = "", ms[8] = "";
        time_t rise = 0, set = 0;
        const bool sun = Forecast_SunTimes(&rise, &set);
        if (sun) { hhmmOf(sr, sizeof(sr), rise); hhmmOf(st, sizeof(st), set); }
        double ph = 0;
        const bool moon = moonToday(lt, mr, ms, sizeof(mr), &ph);
        const int ir = 8, AG = 5, CG = 20, H = LY_CHAR_H(2);
        int wR = 0, wS = 0;
        if (sun)  { wR = Layout_TextW(sr, 2); wS = Layout_TextW(st, 2); }
        if (moon) { { const int a = Layout_TextW(mr, 2), b = Layout_TextW(ms, 2); if (a > wR) wR = a; if (b > wS) wS = b; } }
        const int tot = 2 * ir + 12 + RS_ARROW + AG + wR + CG + RS_ARROW + AG + wS;
        auto row = [&](int y, bool isMoon, const char* r, const char* t) {
          int x = CX - tot / 2;
          const int cy = y + H / 2;
          if (!Layout_Claim(x - 4, y - 4, tot + 8, H + 8)) return;
          if (isMoon) WxIcon_Moon(x + ir, cy, ir, ph, Settings_Lat() < 0);
          else        WxIcon_Draw(x + ir, cy, ir, 0, false);
          x += 2 * ir + 12;
          gfx->setTextSize(2);
          gfx->setTextColor(C_GRAY);
          WxIcon_RiseSet(x, cy, RS_ARROW, true);
          gfx->setCursor(x + RS_ARROW + AG, y); gfx->print(r);
          x += RS_ARROW + AG + wR + CG;
          WxIcon_RiseSet(x, cy, RS_ARROW, false);
          gfx->setCursor(x + RS_ARROW + AG, y); gfx->print(t);
        };
        if (sun)  row(SUNR_PX, false, sr, st);
        if (moon) {
          row(MOONR_PX, true, mr, ms);   // its icon is the phase
        }
      }

      // --- D: the next hours ---
      if (lay == CLOCK_LAYOUT_HOURS && Forecast_Valid()) {
        const FcHour* h = Forecast_Hours();
        const int n = Forecast_HourCount();
        for (int i = 0; i < 3; i++) {
          const int idx = (i + 1) * 2;
          if (idx >= n) break;
          const FcHour& e = h[idx];
          const int cx = CX + (i - 1) * HOUR_STEP;
          struct tm x; localtime_r(&e.t, &x);
          char lab[8], tb[8];
          snprintf(lab, sizeof(lab), "%dh", x.tm_hour);
          snprintf(tb, sizeof(tb), "%d\xF8", (int)lroundf(e.temp));
          const bool rain = e.precip >= 0.05f;
          const int bottom = (rain ? HR3_PX : HR2_PX) + 16;
          if (!Layout_Claim(cx - 46, HR1_PX - HOUR_R - 2, 92, bottom - (HR1_PX - HOUR_R) + 4)) continue;
          WxIcon_Draw(cx - 18, HR1_PX, HOUR_R, e.code, nightAt(e.t));
          gfx->setTextSize(2); gfx->setTextColor(C_WHITE);
          gfx->setCursor(cx + 2, HR1_PX - 8); gfx->print(tb);
          gfx->setTextColor(C_GRAY);              // centred on the cell, not the screen
          gfx->setCursor(cx - Layout_TextW(lab, 2) / 2, HR2_PX); gfx->print(lab);
          if (rain) {
            // Number at size 2, "mm" at size 1 on the same baseline.
            char rb[12]; snprintf(rb, sizeof(rb), "%.1f", e.precip);
            const int nw = Layout_TextW(rb, 2), uw = Layout_TextW("mm", 1);
            const int x = cx - (nw + 2 + uw) / 2;
            gfx->setTextColor(C_CYAN);
            gfx->setTextSize(2); gfx->setCursor(x, HR3_PX); gfx->print(rb);
            gfx->setTextSize(1); gfx->setCursor(x + nw + 2, HR3_PX + 7); gfx->print("mm");
          }
        }
      }
    }
  }   // pixel font

  // A quiet hint that a tap switches the look, but only when a tap actually
  // does something - with the automatic mode on it would be a lie.
  if (!Settings_NightAuto()) {
    const char* m = Settings_IsNight() ? LT("noc", "noc", "night", "noc", "noc")
                                       : LT("den", "den", "day", "den", "deň");
    UI_TextCentered(m, LY_FOOTER, C_DKGRAY, 1);
  }

  // 0.9.0: a print alert that timed out unanswered leaves a dot here.
  if (s_badge) {
    gfx->fillCircle(CX, BADGE_Y, 7, s_badge);
    gfx->drawCircle(CX, BADGE_Y, 9, C_DKGRAY);
  }
}

void ScreenClock_SetBadge(uint16_t color) { s_badge = color; }
