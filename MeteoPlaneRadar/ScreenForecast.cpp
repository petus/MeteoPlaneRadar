// =============================================================================
//  MeteoPlaneRadar
//  Screen: weather forecast (Open-Meteo).
//
//  0.7.0 rewrite. It used to be six consecutive hours and three days, which
//  answered "what is the next six hours doing" and nothing else. It now reads
//  the way people actually ask:
//
//      now / 12-15h / 15-18h  - the rest of your afternoon, by the clock
//      today                  - the one day nobody counts to
//      St 16.9., Ct 17.9. ... - as many as the circle has room for
//
//  How many weekday rows appear is worked out while drawing rather than fixed
//  in a constant: the air-quality line at the foot owns its band, the rows fill
//  what is left, and on a future panel with different proportions the same code
//  fits a different number of days instead of overrunning.
//
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
//  Board:   Waveshare ESP32-S3-Touch-LCD-2.1 (round 480x480 display, ST7701)
// =============================================================================
#include "ScreenForecast.h"
#include "Forecast.h"
#include "Settings.h"
#include "WxIcon.h"
#include "Layout.h"
#include "Lang.h"
#include "SrcHealth.h"
#include "UI.h"
#include "Display_ST7701.h"
#include "Config.h"

#include <WiFi.h>
#include <time.h>
#include <math.h>

#define CX (LCD_WIDTH / 2)

// --- Vertical stack ---------------------------------------------------------
// The rows live between y=69 and y=411. That is not a taste decision: the
// widest row reaches 165 px either side of the centre, and the chord of a
// 480 px circle is narrower than that outside those two lines. Anything put
// above or below them would have its outer columns silently dropped by
// Layout_Claim, which looks like a bug rather than a margin.
#define HOUR_Y0    72
#define HOUR_H     30
#define HOUR_ROWS   3
#define SEP_Y      (HOUR_Y0 + HOUR_ROWS * HOUR_H + 6)
#define DAY_Y0     (SEP_Y + 8)
#define DAY_H      31
#define AQ_Y      404     // the air-quality line, which the days must not reach

// --- Columns, measured from the centre --------------------------------------
// The label column holds "St 31.12." at size 2 - nine characters, 108 px, the
// widest date this screen can ever produce. That is what sets everything else:
// with the icon, both temperatures, the rain and the wind behind it the row
// comes to 344 px, which needs a half-chord of 172 and therefore fits between
// y=76 and y=404. The rows below are laid out inside that window.
//
// The gaps between columns are 6 px rather than 8 for the same reason. Eight
// made the row 352 px, which fits only between y=80 and y=400 - and that is a
// row short of what the circle will hold.
#define COL_LABEL  (CX - 172)
#define COL_ICON   (CX -  45)
#define COL_TEMP   (CX -  26)
#define COL_PRECIP (CX +  64)
#define COL_WIND   (CX + 121)

// Wind below this is not news anywhere, and the column is better spent on
// nothing at all - the same rule precipitation has always followed here.
#define WIND_SHOW_KMH 12.0f

static unsigned long s_lastSeen = 0;

void ScreenForecast_Enter() {
  s_lastSeen = 0;
}

bool ScreenForecast_Tick() {
  static bool lastValid = false;
  static int  lastHour  = -1;
  bool want = false;

  bool valid = Forecast_Valid();
  time_t now = time(nullptr);
  struct tm lt; localtime_r(&now, &lt);
  if (valid != lastValid || lt.tm_hour != lastHour) {
    lastValid = valid;
    lastHour = lt.tm_hour;
    want = true;
  }

  // The rows only change on the hour. The clock at the top changes every
  // minute, and it is drawn as part of the same redraw.
  if (UI_StatusLineChanged()) want = true;

  return want;
}

// One text field, claimed before it is drawn so it can never sit on top of a
// neighbour that turned out wider than expected.
static void field(const char* s, int x, int y, uint8_t size, uint16_t col) {
  if (!s || !*s) return;
  int w = Layout_TextW(s, size);
  if (!Layout_Claim(x - 2, y - 2, w + 4, LY_CHAR_H(size) + 4)) return;
  gfx->setTextSize(size);
  gfx->setTextColor(col);
  gfx->setCursor(x, y);
  gfx->print(s);
}

// A number with its unit beside it. The pair claims its space together, so a
// unit can never be drawn without the number it belongs to.
static void valueWithUnit(const char* val, const char* unit,
                          int x, int y, uint16_t col) {
  if (!val || !*val) return;
  const int vw  = Layout_TextW(val, 2);
  const int uw  = (unit && *unit) ? Layout_TextW(unit, 1) : 0;
  const int gap = uw ? 3 : 0;
  const int tot = vw + gap + uw;

  if (!Layout_Claim(x - 2, y - 2, tot + 4, LY_CHAR_H(2) + 4)) return;

  gfx->setTextSize(2);
  gfx->setTextColor(col);
  gfx->setCursor(x, y);
  gfx->print(val);

  if (uw) {
    gfx->setTextSize(1);
    gfx->setTextColor(C_GRAY);
    gfx->setCursor(x + vw + gap, y + 8);   // sits on the baseline of the digits
    gfx->print(unit);
  }
}

// The row label. One string, one size: the labels are now self-explanatory
// ("12-15h", "Ct"), so the small grey qualifier that used to sit beside them -
// the clock hour, the date - was repeating what the label already said.
static void rowLabel(const char* s, int y, uint16_t col) {
  field(s, COL_LABEL, y, 2, col);
}

// Shared tail of both row kinds: icon, temperature, rain, wind.
static void drawValues(int y, int code, const char* temp, uint16_t tempCol,
                       float precip, float wind, int iconR) {
  char buf[16];

  if (Layout_Claim(COL_ICON - iconR, y - iconR + 8, 2 * iconR, 2 * iconR))
    WxIcon_Draw(COL_ICON, y + 8, iconR, code, false);

  field(temp, COL_TEMP, y, 2, tempCol);

  // Precipitation is only interesting when there is some - a column of "0.0"
  // is noise, and the space is better spent on nothing at all.
  if (precip >= 0.05f) {
    snprintf(buf, sizeof(buf), "%.1f", precip);
    valueWithUnit(buf, "mm", COL_PRECIP, y, C_CYAN);
  }

  if (wind >= WIND_SHOW_KMH) {
    snprintf(buf, sizeof(buf), "%d", (int)lroundf(wind));
    valueWithUnit(buf, "km/h", COL_WIND, y, C_GRAY);
  }
}

// --- The "now" row ----------------------------------------------------------
// Prefers the API's own current block over hourly[0]: the hourly value is that
// whole hour's model figure, so at 14:55 it describes an hour that is nearly
// over. The current block is measured much closer to now.
static void drawNowRow(int y) {
  const FcHour* hrs = Forecast_Hours();
  if (Forecast_HourCount() < 1 && !Forecast_CurrentValid()) return;

  float temp, precip, wind;
  int   code;

  if (Forecast_CurrentValid()) {
    temp   = Forecast_CurrentTemp();
    precip = Forecast_CurrentPrecip();
    wind   = Forecast_CurrentWind();
    code   = Forecast_CurrentCode();
  } else {
    temp   = hrs[0].temp;
    precip = hrs[0].precip;
    wind   = hrs[0].wind;
    code   = hrs[0].code;
  }

  rowLabel(T(S_NOW), y + 7, C_WHITE);

  char tbuf[12];
  snprintf(tbuf, sizeof(tbuf), "%d %s", (int)lroundf(temp), OUTSIDE_DEG_TEXT);
  drawValues(y + 7, code, tbuf, WxIcon_Color(code), precip, wind, 11);
}

// --- Window rows ------------------------------------------------------------
// A block of hours labelled by the clock - "12-15h" - rather than by how far
// away it is. "In 3 hours" makes you do the arithmetic; "12-15h" is already
// the answer, and it is how people talk about an afternoon.
//
// Each value is aggregated the way that value is actually used:
//   temperature - the mean, which is what "it will be about this" means,
//   rain        - the SUM, because what matters is how much falls in total,
//   wind        - the MAXIMUM, because a gusty hour ruins the whole window,
//   icon        - the highest WMO code, which is very nearly "the worst of
//                 them": the scale runs clear, cloud, fog, drizzle, rain,
//                 snow, showers, thunderstorm in ascending order.
static void drawWindowRow(int i0, int y) {
  const FcHour* hrs = Forecast_Hours();
  const int n = Forecast_HourCount();
  const int i1 = i0 + FORECAST_WIN_H - 1;
  if (i1 >= n) return;

  float sumT = 0, sumP = 0, maxW = 0;
  int   code = 0;
  for (int i = i0; i <= i1; i++) {
    sumT += hrs[i].temp;
    sumP += hrs[i].precip;
    if (hrs[i].wind > maxW) maxW = hrs[i].wind;
    if (hrs[i].code > code) code = hrs[i].code;
  }
  const float temp = sumT / (float)FORECAST_WIN_H;

  // The label spans from the start of the first hour to the END of the last,
  // so a three-hour window really does read as three hours: 12-15, not 12-14.
  struct tm a, b;
  localtime_r(&hrs[i0].t, &a);
  time_t endT = hrs[i1].t + 3600;
  localtime_r(&endT, &b);

  // A window ending at midnight reads as 21-24, not 21-00: the end of the day
  // is the 24th hour of it, and "21-00" looks like a typo.
  int endHour = b.tm_hour;
  if (endHour == 0) endHour = 24;

  char lbl[12], tbuf[12];
  snprintf(lbl, sizeof(lbl), "%02d-%02dh", a.tm_hour, endHour);
  rowLabel(lbl, y + 7, C_GRAY);

  snprintf(tbuf, sizeof(tbuf), "%d %s", (int)lroundf(temp), OUTSIDE_DEG_TEXT);
  drawValues(y + 7, code, tbuf, WxIcon_Color(code), sumP, maxW, 11);
}

// --- Daily rows -------------------------------------------------------------
// "dnes" keeps its name because it is the one day nobody counts to and it needs
// no date - it is today. Every other row is the weekday and the date at the
// same size: "St 16.9.". The weekday is what you read, the date is what you
// check against a calendar, and a week out you want both.
//
// The longest this can ever be is "St 31.12." - nine characters, which is what
// the label column was sized for.
static void drawDayRow(const FcDay& d, int index, int y) {
  char big[16], buf[24];
  struct tm lt; localtime_r(&d.t, &lt);

  if (index == 0) {
    snprintf(big, sizeof(big), "%s", T(S_TODAY));
  } else if (Lang_Get() == LANG_EN) {
    // English weekdays are three letters, not two, so "Wed 31.12." would be
    // ten characters and one too wide for the column. Day/month with a slash
    // keeps the same order and gets it back to nine.
    snprintf(big, sizeof(big), "%s %d/%d", Lang_WeekdayShort(lt.tm_wday),
             lt.tm_mday, lt.tm_mon + 1);
  } else {
    snprintf(big, sizeof(big), "%s %d.%d.", Lang_WeekdayShort(lt.tm_wday),
             lt.tm_mday, lt.tm_mon + 1);
  }

  rowLabel(big, y + 9, index == 0 ? C_WHITE : C_GRAY);

  // Maximum and minimum share the temperature column. No unit: "-12/-19" plus a
  // suffix would run into the precipitation column, and the hourly rows above
  // already label what this column is.
  snprintf(buf, sizeof(buf), "%d/%d", (int)lroundf(d.tmax), (int)lroundf(d.tmin));
  drawValues(y + 9, d.code, buf, WxIcon_Color(d.code), d.precip, d.wind, 13);
}

// --- Air quality ------------------------------------------------------------
// European AQI bands, coloured the way the index itself is published.
static uint16_t aqiColor(int aqi) {
  if (aqi < 0)  return C_GRAY;
  if (aqi <= 20) return C_GREEN;
  if (aqi <= 40) return 0x87E0;    // yellow-green
  if (aqi <= 60) return C_YELLOW;
  if (aqi <= 80) return C_ORANGE;
  return C_RED;
}

// Particulates, on the WHO daily guidance rather than the AQI bands - the two
// do not line up, and PM2.5 is the number people actually recognise.
static uint16_t pmColor(float pm25) {
  if (pm25 <= 15) return C_GREEN;
  if (pm25 <= 25) return C_YELLOW;
  if (pm25 <= 50) return C_ORANGE;
  return C_RED;
}

// Pollen counts are grains per cubic metre. The thresholds differ per species,
// so this is a deliberately coarse "is it worth knowing about" scale.
static uint16_t pollenColor(float p) {
  if (p < 10)  return C_GRAY;
  if (p < 50)  return C_GREEN;
  if (p < 100) return C_YELLOW;
  return C_ORANGE;
}

// Air quality, on ONE line now that the days have taken the vertical space it
// used to have. Built as up to three label/value pairs, measured, and then
// dropped from the right until what is left fits inside the circle - so a
// narrow day loses the pollen count rather than pushing AQI off the rim.
static void drawAirQuality() {
  if (!AirQuality_Valid()) return;

  struct Pair { char label[16]; char value[16]; uint16_t col; };
  Pair p[3];
  int n = 0;

  const int aqi = AirQuality_Aqi();
  if (aqi >= 0) {
    snprintf(p[n].label, sizeof(p[n].label), "AQI");
    snprintf(p[n].value, sizeof(p[n].value), "%d", aqi);
    p[n].col = aqiColor(aqi);
    n++;
  }
  const float pm = AirQuality_Pm25();
  if (pm > 0) {
    snprintf(p[n].label, sizeof(p[n].label), "PM2.5");
    snprintf(p[n].value, sizeof(p[n].value), "%.0f", pm);
    p[n].col = pmColor(pm);
    n++;
  }
  const float pollen = AirQuality_PollenMax();
  if (pollen >= 0) {
    // The species that is worst right now, so the number means something -
    // "pollen 120" without knowing it is birch is not actionable.
    const char* worst = AirQuality_PollenWorst();
    if (worst && *worst) snprintf(p[n].label, sizeof(p[n].label), "%s", worst);
    else                 snprintf(p[n].label, sizeof(p[n].label), "%s", T(S_POLLEN));
    snprintf(p[n].value, sizeof(p[n].value), "%.0f", pollen);
    p[n].col = pollenColor(pollen);
    n++;
  }
  if (n == 0) return;

  const int room = 2 * Layout_ChordHalf(AQ_Y + 16) - 12;
  const int PAIRGAP = 6, COLGAP = 18;

  int width = 0;
  while (n > 0) {
    width = 0;
    for (int i = 0; i < n; i++) {
      width += Layout_TextW(p[i].label, 2) + PAIRGAP + Layout_TextW(p[i].value, 2);
      if (i) width += COLGAP;
    }
    if (width <= room) break;
    n--;                         // drop the rightmost pair and measure again
  }
  if (n == 0) return;

  // No Layout_Claim here. This line is CHROME: ScreenForecast_Draw reserved its
  // band before anything else was drawn, precisely so the day rows could not
  // creep into it. Claiming the same rectangle afterwards collided with that
  // reservation, the claim failed, and the whole air-quality line silently
  // stopped being drawn. Chrome is positioned by design and draws; only
  // data-positioned elements ask permission.
  int x = CX - width / 2;

  for (int i = 0; i < n; i++) {
    gfx->setTextSize(2);
    gfx->setTextColor(C_GRAY);
    gfx->setCursor(x, AQ_Y);
    gfx->print(p[i].label);
    x += Layout_TextW(p[i].label, 2) + PAIRGAP;
    gfx->setTextColor(p[i].col);
    gfx->setCursor(x, AQ_Y);
    gfx->print(p[i].value);
    x += Layout_TextW(p[i].value, 2) + COLGAP;
  }
}

// -----------------------------------------------------------------------------
void ScreenForecast_Draw() {
  gfx->fillScreen(C_BLACK);
  Layout_Begin();

  // Chrome first: the screen dots, the status line and the air-quality band own
  // their space, so a row below can never creep into them.
  Layout_ReserveBand(LY_DOTS - 6, 12);
  Layout_ReserveBand(LY_STATUS - 3, 22);
  Layout_ReserveBand(AQ_Y - 3, 22);
  UI_DrawStatusLine(LY_STATUS);

  if (!Settings_HasLocation()) {
    UI_TextCentered(T(S_NO_LOCATION), LCD_HEIGHT / 2, C_YELLOW, 2);
    return;
  }
  if (!Forecast_Valid()) {
    UI_NoData(T(S_FORECAST), Forecast_Health());   // 0.9.7: not "Loading..." for ever
    return;
  }

  // --- now, then two windows of FORECAST_WIN_H hours each ---
  drawNowRow(HOUR_Y0);
  drawWindowRow(1,                   HOUR_Y0 + HOUR_H);
  drawWindowRow(1 + FORECAST_WIN_H,  HOUR_Y0 + 2 * HOUR_H);

  // --- separator ---
  {
    int half = Layout_ChordHalf(SEP_Y) - 30;
    if (half > 20) {
      Layout_Reserve(CX - half, SEP_Y - 1, 2 * half, 3);
      gfx->drawFastHLine(CX - half, SEP_Y, 2 * half, C_DKGRAY);
    }
  }

  // --- today, tomorrow, the day after, then as many weekdays as fit ---
  // The row count comes from the space between the separator and the
  // air-quality band, not from a constant, so the layout stays honest if either
  // of those moves.
  const FcDay* dys = Forecast_Days();
  int dn = Forecast_DayCount();
  const int fits = (AQ_Y - 6 - DAY_Y0) / DAY_H;
  if (dn > fits) dn = fits;
  if (dn > FORECAST_DAYS) dn = FORECAST_DAYS;
  for (int i = 0; i < dn; i++) drawDayRow(dys[i], i, DAY_Y0 + i * DAY_H);

  drawAirQuality();
}
