// =============================================================================
//  MeteoPlaneRadar
//  Map underlay: European country outlines and cities.
//
//  Project: MeteoPlaneRadar - live aircraft radar on a round touchscreen
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
//  Web:     https://chiptron.cz
//  Board:   Waveshare ESP32-S3-Touch-LCD-2.1 (round 480x480 display, ST7701)
// =============================================================================
#include "EuBorder.h"
#include "EuMapData.h"
#include "CzCitiesData.h"   // Czech cities - they replace the European ones
#include "UI.h"
#include "Layout.h"
#include "Canvas16.h"   // framebuffer read-back for the label contrast
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Decode a fixed-point vertex back into degrees.
static inline float ptLon(uint16_t v) { return EU_LON_ORIGIN + v * EU_COORD_SCALE; }
static inline float ptLat(uint16_t v) { return EU_LAT_ORIGIN + v * EU_COORD_SCALE; }

void EuBorder_Draw(ProjectFn project, uint16_t color,
                   float lat0, float lat1, float lon0, float lon1) {
  const int nRings = EU_RING_COUNT;

  for (int r = 0; r < nRings; r++) {
    uint16_t start = EU_RING_OFFSETS[r];
    uint16_t end   = EU_RING_OFFSETS[r + 1];   // exclusive
    if (end - start < 2) continue;

    // Ring-level rejection: if the ring's bounding box misses the window
    // entirely, skip the whole country in one go. This is what keeps the
    // draw cheap - at 25 km over Prague, nearly every ring dies right here.
    uint16_t bxmin = 0xFFFF, bxmax = 0, bymin = 0xFFFF, bymax = 0;
    for (uint16_t i = start; i < end; i++) {
      uint16_t x = EU_BORDER_PTS[i][0], y = EU_BORDER_PTS[i][1];
      if (x < bxmin) bxmin = x;
      if (x > bxmax) bxmax = x;
      if (y < bymin) bymin = y;
      if (y > bymax) bymax = y;
    }
    if (ptLon(bxmax) < lon0 || ptLon(bxmin) > lon1) continue;
    if (ptLat(bymax) < lat0 || ptLat(bymin) > lat1) continue;

    // Segment level: draw the segment i-1 -> i whenever *either* endpoint lies
    // in the window. Requiring both would chop off the border exactly where it
    // crosses the edge of the view, leaving visible gaps at the screen rim.
    //
    // Every vertex of a surviving ring is projected once and cached in prevX/
    // prevY, so a point sitting just outside the window is still available as
    // the far end of the segment that reaches into it.
    int   prevX = 0, prevY = 0;
    bool  prevIn = false;
    bool  first = true;

    for (uint16_t i = start; i < end; i++) {
      float lon = ptLon(EU_BORDER_PTS[i][0]);
      float lat = ptLat(EU_BORDER_PTS[i][1]);
      bool  in  = (lat >= lat0 && lat <= lat1 && lon >= lon0 && lon <= lon1);

      int sx, sy;
      project(lat, lon, &sx, &sy);

      if (!first && (in || prevIn)) {
        gfx->drawLine(prevX, prevY, sx, sy, color);
      }

      prevX = sx; prevY = sy;
      prevIn = in;
      first = false;
    }
  }
}

// Do we have this place in our own Czech list? Matched on POSITION, not on the
// name: our list writes some of them differently ("Usti n. L." against the
// European "Usti nad Labem") and a name comparison would let those through
// twice. Verified against the real data - all 21 overlaps pair up one to one,
// and no two distinct towns fall within the tolerance of each other.
static bool haveCzechCity(float lat, float lon) {
  const float TOL = 0.05f;                 // about 5 km
  for (int i = 0; i < CZ_CITY_COUNT; i++) {
    if (fabsf(CZ_CITIES[i].lat - lat) <= TOL &&
        fabsf(CZ_CITIES[i].lon - lon) <= TOL) return true;
  }
  return false;
}

// --- Label collision ---------------------------------------------------------
// At the widest ranges the labels start sitting on top of each other and the
// map turns into mush. Every label that gets drawn claims a rectangle; a later
// one that would overlap it is dropped entirely, dot and all - half a name
// under another name is worse than no name.
//
// Since 0.6.0 the rectangles are held by the shared Layout module rather than
// by a private array here. That is what stops a city label from landing on the
// clock or the altitude legend: the screen reserves its chrome before the map
// is drawn, so those rectangles are already taken by the time the first city
// asks for space.
//
// Cities are drawn tier by tier, so the bigger place always wins the spot and
// it is the small town next to it that disappears.

// --- Label colour against what is underneath (0.8.1) ------------------------
// On the weather radar the labels sit on top of the precipitation, and yellow
// rain or the pale shades of the masked composite are as bright as the white
// text - the name simply vanished. So before a label is drawn, the pixels it
// is about to cover are read back from the framebuffer: if too many of them are
// close in brightness to the intended colour, the label (and its dot) is drawn
// black instead. On the aircraft radar the map is black and nothing changes.
#define LABEL_MIN_CONTRAST 90      // luma difference that still reads, 0..255
#define LABEL_BAD_SHARE    25      // per cent of low-contrast pixels -> go black

static int luma565(uint16_t c) {
  const int r = ((c >> 11) & 0x1F) * 255 / 31;
  const int g = ((c >> 5) & 0x3F) * 255 / 63;
  const int b = (c & 0x1F) * 255 / 31;
  return (r * 77 + g * 150 + b * 29) >> 8;
}

static uint16_t contrastColour(int x, int y, int w, int h, uint16_t want) {
  uint16_t* fb = static_cast<Canvas16*>(gfx)->getFramebuffer();
  if (!fb) return want;
  const int W = gfx->width(), H = gfx->height();
  const int lw = luma565(want);
  int total = 0, bad = 0;
  for (int j = y; j < y + h; j++) {
    if (j < 0 || j >= H) continue;
    for (int i = x; i < x + w; i++) {
      if (i < 0 || i >= W) continue;
      total++;
      if (abs(luma565(fb[(int32_t)j * W + i]) - lw) < LABEL_MIN_CONTRAST) bad++;
    }
  }
  if (total && bad * 100 >= total * LABEL_BAD_SHARE) return C_BLACK;
  return want;
}

// One city. Split out of the loop below so the European set and the Czech
// list can share it.
static void drawOneCity(const EuCity& c, ProjectFn project, int cx, int cy,
                        long r2, uint16_t dotColor, uint16_t textColor,
                        bool showFull, uint8_t tier,
                        float lat0, float lat1, float lon0, float lon1);

void EuBorder_DrawCities(ProjectFn project, int cx, int cy, int radius,
                         uint16_t dotColor, uint16_t textColor,
                         bool showFull, uint8_t maxTier,
                         float lat0, float lat1, float lon0, float lon1) {
  long r2 = (long)radius * radius;

  // Tier by tier, biggest first - whoever claims the space first keeps it.
  for (uint8_t tier = 1; tier <= maxTier; tier++) {
    for (int i = 0; i < EU_CITY_COUNT; i++) {
      const EuCity& c = EU_CITIES[i];
      // Czech cities come from our own list instead - same places, but with the
      // abbreviations people actually recognise (PHA, not PRAH). Only places we
      // really do have are skipped, so Dresden or Wroclaw, which fall inside the
      // same rectangle, still get drawn from the European data.
      if (c.lat >= CZ_BOX_LAT0 && c.lat <= CZ_BOX_LAT1 &&
          c.lon >= CZ_BOX_LON0 && c.lon <= CZ_BOX_LON1 &&
          haveCzechCity(c.lat, c.lon)) continue;
      drawOneCity(c, project, cx, cy, r2, dotColor, textColor,
                  showFull, tier, lat0, lat1, lon0, lon1);
    }
    for (int i = 0; i < CZ_CITY_COUNT; i++)
      drawOneCity(CZ_CITIES[i], project, cx, cy, r2, dotColor, textColor,
                  showFull, tier, lat0, lat1, lon0, lon1);
  }
}

static void drawOneCity(const EuCity& c, ProjectFn project, int cx, int cy,
                        long r2, uint16_t dotColor, uint16_t textColor,
                        bool showFull, uint8_t tier,
                        float lat0, float lat1, float lon0, float lon1) {
  {

    // This pass draws exactly one tier - see the loop in the caller.
    if (c.tier != tier) return;

    // Geographic cull before projecting.
    if (c.lat < lat0 || c.lat > lat1 || c.lon < lon0 || c.lon > lon1) return;

    int sx, sy;
    project(c.lat, c.lon, &sx, &sy);

    // Skip anything outside the display circle.
    long dx = sx - cx, dy = sy - cy;
    if (dx * dx + dy * dy > r2) return;

    // Where the label goes: to the right of the dot, or to the left when that
    // would spill out of the circle.
    const char* label = showFull ? c.name : c.abbr;
    int tw = Layout_TextW(label, 1);
    int ty = sy - 4;
    int tx = sx + 9;
    if ((long)(tx + tw - cx) * (tx + tw - cx) + dy * dy > r2) tx = sx - 9 - tw;

    // Claim the space, dot included, with two pixels of air around it. If it is
    // taken, try the other side of the dot before giving up.
    // Claim the space, dot included, with two pixels of air around it. If it is
    // taken, try the other side of the dot before giving up.
    int bx0 = (tx < sx) ? (tx - 2) : (sx - 5);
    int bx1 = (tx < sx) ? (sx + 5) : (tx + tw + 2);
    if (!Layout_Claim(bx0, ty - 2, bx1 - bx0 + 1, 12)) {
      int alt = (tx > sx) ? (sx - 9 - tw) : (sx + 9);
      int ax0 = (alt > sx) ? (sx - 5) : (alt - 2);
      int ax1 = (alt > sx) ? (alt + tw + 2) : (sx + 5);
      if (!Layout_Claim(ax0, ty - 2, ax1 - ax0 + 1, 12)) return;   // both sides busy
      tx = alt;
    }

    // Decide the colours BEFORE drawing anything of this city, so the dot does
    // not count as background for its own label.
    const uint16_t dc = contrastColour(sx - 3, sy - 3, 7, 7, dotColor);
    const uint16_t tc = contrastColour(tx, ty, tw, 8, textColor);
    // Black goes on with a thin rim of the original colour: a label that is
    // only partly over bright rain must stay readable over the dark part too.
    if (dc != dotColor) gfx->fillCircle(sx, sy, 4, dotColor);
    gfx->fillCircle(sx, sy, 3, dc);
    gfx->setTextSize(1);
    if (tc != textColor) {
      gfx->setTextColor(textColor);
      static const int8_t OFF[4][2] = { {-1, 0}, {1, 0}, {0, -1}, {0, 1} };
      for (const auto& o : OFF) { gfx->setCursor(tx + o[0], ty + o[1]); gfx->print(label); }
    }
    gfx->setTextColor(tc);
    gfx->setCursor(tx, ty);
    gfx->print(label);
  }
}
