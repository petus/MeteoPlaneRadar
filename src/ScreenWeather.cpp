// =============================================================================
//  MeteoPlaneRadar
//  Screen: CHMU precipitation radar (meteoradar) with a 6-frame animation.
//  Adapted for MeteoPlaneRadar (chiptron.cz) and reworked to the ROUND 480x480 display:
//    - isotropic crop (aspect 1:1 instead of the rectangular 1.5:1),
//    - the decoded image is masked to the display circle (nothing is drawn
//      outside it),
//    - all overlays (legend, animation indicator, range) repositioned so they
//      stay inside the circle.
//
//  Coordinates: Web Mercator, square crop (isotropic scale).
//  Animation: up to 6 frames (5-min step), ~2 fps, ~5 s pause between cycles.
//  New frames are fetched only while the last frame is shown (in the pause) so
//  the animation never tears. "Nacitam animaci..." is shown while downloading.
//
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
//  Board:   Waveshare ESP32-S3-Touch-LCD-2.1 (round 480x480 display, ST7701)
// =============================================================================
#include "ScreenWeather.h"
#include "CHMU.h"
#include "Settings.h"
#include "UI.h"
#include "Display_ST7701.h"
#include "CzBorder.h"

#include <WiFi.h>
#include <PNGdec.h>
#include <math.h>
#include <esp_heap_caps.h>

#define ANIM_FRAMES  CHMU_ANIM_MAX   // 6

// --- Round-display geometry ---
static const int CX     = LCD_WIDTH  / 2;   // 240
static const int CY     = LCD_HEIGHT / 2;   // 240
static const int DISP_R = LCD_WIDTH  / 2 - 2;  // 238 - pixels beyond this are masked off

// Ranges (radius in km, mapped onto the display height).
static const float RANGES_KM[] = {25.0f, 50.0f, 100.0f, 200.0f};
static const int   RANGE_COUNT = sizeof(RANGES_KM) / sizeof(RANGES_KM[0]);
static int s_rangeIdx = 1;
static float currentRange() { return RANGES_KM[s_rangeIdx]; }

static PNG png;
static int s_imgW = 600, s_imgH = 480;
static int s_dataX1 = 100000, s_dataY0 = -1;   // data bounds (outside = title/scale in PNG)
static String s_status = "Start...";
static bool s_wide = false;                    // frame too wide for PNGdec

struct Crop { int x1, y1, x2, y2; };
static Crop s_crop;
static uint16_t* s_lineBuf = nullptr;
static int       s_lineCap = 0;

// Decoded frame crops (RGB565) in PSRAM.
static uint16_t* s_frame565[ANIM_FRAMES] = {0};
static int       s_frameCap[ANIM_FRAMES] = {0};
static uint16_t* s_crop565 = nullptr;    // current target (pngDraw) / source (blit)
static int       s_frameCount = 0;

// Animation state.
static int  s_curFrame = 0;
static bool s_gap = false;
static bool s_needRebuild = false;
static unsigned long s_lastStep = 0, s_gapStart = 0, s_lastFetch = 0;

static int clampI(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

// --- Web Mercator ---
static float mercY(float latDeg) { float r = latDeg * 0.0174532925f; return logf(tanf(0.7853981634f + r * 0.5f)); }
static float mercTop()    { return mercY(CHMU_LAT_TOP); }
static float mercBottom() { return mercY(CHMU_LAT_BOTTOM); }
static int lonToX(float lon) { return lroundf((lon - CHMU_LON_LEFT) * (s_imgW - 1) / (CHMU_LON_RIGHT - CHMU_LON_LEFT)); }
static int latToY(float lat) { float yt = mercTop(), yb = mercBottom(); return lroundf((yt - mercY(lat)) * (s_imgH - 1) / (yt - yb)); }

// Round display -> isotropic (square) crop.
static const float ASPECT = (float)LCD_WIDTH / (float)LCD_HEIGHT;   // 1.0

static void makeCrop(double lat, double lon, float radiusKm) {
  float degLat = radiusKm / 111.32f;
  float degLon = radiusKm * ASPECT / (111.32f * cosf(lat * 0.0174532925));
  int x1 = lonToX(lon - degLon), x2 = lonToX(lon + degLon);
  int y1 = latToY(lat + degLat), y2 = latToY(lat - degLat);
  if (x2 < x1) { int t = x1; x1 = x2; x2 = t; }
  if (y2 < y1) { int t = y1; y1 = y2; y2 = t; }
  s_crop.x1 = clampI(x1, 0, s_imgW - 1);
  s_crop.x2 = clampI(x2, 0, s_imgW - 1);
  s_crop.y1 = clampI(y1, 0, s_imgH - 1);
  s_crop.y2 = clampI(y2, 0, s_imgH - 1);
}
static int cropW() { return s_crop.x2 - s_crop.x1 + 1; }
static int cropH() { return s_crop.y2 - s_crop.y1 + 1; }

// Projection callback for the CR outline + cities (same crop as the image).
static void borderProject(float lat, float lon, int* sx, int* sy) {
  int srcX = lonToX(lon), srcY = latToY(lat);
  *sx = (int)((int64_t)(srcX - s_crop.x1) * LCD_WIDTH  / cropW());
  *sy = (int)((int64_t)(srcY - s_crop.y1) * LCD_HEIGHT / cropH());
}

static int pngDraw(PNGDRAW* d) {
  int srcY = d->y;
  if (srcY < s_crop.y1 || srcY > s_crop.y2) return 1;
  if (!s_crop565 || !s_lineBuf) return 1;
  png.getLineAsRGB565(d, s_lineBuf, PNG_RGB565_LITTLE_ENDIAN, 0xffffffff);
  const int cw = cropW();
  uint16_t* row = s_crop565 + (int64_t)(srcY - s_crop.y1) * cw;
  for (int i = 0; i < cw; i++) {
    int srcX = clampI(s_crop.x1 + i, 0, s_imgW - 1);
    // Outside the data area (title on top, scale on the right in the PNG) = black.
    row[i] = (srcX > s_dataX1 || srcY < s_dataY0) ? 0x0000 : s_lineBuf[srcX];
  }
  return 1;
}

// Stretch the current crop (s_crop565) over the display, masked to the circle
// (nearest neighbour). Pixels outside the round area are left black.
static void blitCrop() {
  if (!s_crop565) return;
  const int cw = cropW(), ch = cropH();
  const long R2 = (long)DISP_R * DISP_R;
  for (int dy = 0; dy < LCD_HEIGHT; dy++) {
    long ddy = dy - CY;
    int srcRow = clampI((int)((int64_t)dy * ch / LCD_HEIGHT), 0, ch - 1);
    const uint16_t* row = s_crop565 + (int64_t)srcRow * cw;
    for (int dx = 0; dx < LCD_WIDTH; dx++) {
      long ddx = dx - CX;
      if (ddx * ddx + ddy * ddy > R2) continue;   // outside the round display
      int srcCol = clampI((int)((int64_t)dx * cw / LCD_WIDTH), 0, cw - 1);
      gfx->drawPixel(dx, dy, row[srcCol]);
    }
  }
}

// -----------------------------------------------------------------------------
//  Build the crop of every frame (decode PNG -> s_frame565[f]).
// -----------------------------------------------------------------------------
static bool rebuildCrops() {
  int cnt = CHMU_AnimCount();
  if (cnt <= 0) { s_frameCount = 0; return false; }
  if (cnt > ANIM_FRAMES) cnt = ANIM_FRAMES;

  // Dimensions + width sanity check from the first frame.
  if (png.openRAM(CHMU_AnimData(0), CHMU_AnimSize(0), pngDraw) != PNG_SUCCESS) { s_frameCount = 0; return false; }
  s_imgW = png.getWidth(); s_imgH = png.getHeight();
  s_dataX1 = lonToX(CHMU_LON_DATA_RIGHT);   // right of this x = scale/edge
  s_dataY0 = latToY(CHMU_LAT_DATA_TOP);     // above this y = title
  int bufSize = png.getBufferSize();
  int pitch = (s_imgH > 0) ? bufSize / s_imgH : bufSize;
  png.close();
  if (2 * (pitch + 1) > (int)PNG_MAX_BUFFERED_PIXELS) { s_wide = true; s_frameCount = 0; return false; }
  s_wide = false;

  if (s_imgW > s_lineCap) {
    if (s_lineBuf) free(s_lineBuf);
    s_lineBuf = (uint16_t*)malloc((size_t)s_imgW * sizeof(uint16_t));
    s_lineCap = s_lineBuf ? s_imgW : 0;
  }
  if (!s_lineBuf) { s_frameCount = 0; return false; }

  makeCrop(Settings_Lat(), Settings_Lon(), currentRange());
  int need = cropW() * cropH();

  int okc = 0;
  for (int f = 0; f < cnt; f++) {
    if (s_frameCap[f] < need) {
      if (s_frame565[f]) free(s_frame565[f]);
      s_frame565[f] = (uint16_t*)heap_caps_malloc((size_t)need * 2, MALLOC_CAP_SPIRAM);
      if (!s_frame565[f]) s_frame565[f] = (uint16_t*)malloc((size_t)need * 2);
      s_frameCap[f] = s_frame565[f] ? need : 0;
    }
    if (!s_frame565[f]) break;
    s_crop565 = s_frame565[f];
    if (png.openRAM(CHMU_AnimData(f), CHMU_AnimSize(f), pngDraw) != PNG_SUCCESS) { png.close(); break; }
    png.decode(nullptr, 0);
    png.close();
    okc++;
  }
  s_frameCount = okc;
  return okc > 0;
}

// Download frames + build crops (blocking). A message is shown meanwhile.
static void loadAndBuild() {
  gfx->fillScreen(C_BLACK);
  UI_TextCentered("Nacitam animaci...", LCD_HEIGHT / 2, C_WHITE, 2);
  gfx->flush();
  int n = CHMU_FetchAnim(ANIM_FRAMES);
  if (n > 0) rebuildCrops(); else s_frameCount = 0;
  s_status = (s_frameCount > 0) ? "OK" : (s_wide ? "snimek moc siroky" : "Chyba stahovani");
}

// -----------------------------------------------------------------------------
//  Overlays (round layout).
// -----------------------------------------------------------------------------
static void drawOverlay() {
  // Crosshair on the user's position (centre).
  gfx->drawFastHLine(CX - 8, CY, 16, C_WHITE);
  gfx->drawFastVLine(CX, CY - 8, 16, C_WHITE);

  // Precipitation-intensity legend - dBZ / mm/h. Placed on the left, vertically
  // centred, so it stays inside the circle. Colours taken from the real CHMU
  // palette so they match the map.
  {
    static const uint16_t COL[6] = { 0xA000, 0xF800, 0xFC20, 0xE6E0, 0x05E0, 0x001F };
    static const char*    LBL[6] = { ">56 / >100", "52 / 65", "46 / 27", "40 / 12", "32 / 3.6", "20 / <1" };
    const int lx = 26, ly = 150, boxW = 96, boxH = 12 + 6 * 13 + 2;
    gfx->fillRect(lx - 2, ly - 2, boxW, boxH, C_BLACK);   // readability backing
    gfx->setTextSize(1); gfx->setTextColor(C_GRAY);
    gfx->setCursor(lx, ly); gfx->print("dBZ / mm/h");
    for (int i = 0; i < 6; i++) {
      int ry = ly + 12 + i * 13;
      gfx->fillRect(lx, ry, 9, 9, COL[i]);
      gfx->drawRect(lx, ry, 9, 9, C_DKGRAY);
      gfx->setTextColor(C_WHITE);
      gfx->setCursor(lx + 13, ry + 1); gfx->print(LBL[i]);
    }
  }

  // Animation indicator - top centre: dots + the frame's time label. Sits below
  // the screen-selector dots (which the main sketch draws at y=18).
  {
    int n = s_frameCount > 0 ? s_frameCount : 1;
    int minAgo = (n - 1 - s_curFrame) * 5;
    char lbl[20];
    String hhmm = (s_frameCount > 0) ? CHMU_AnimTimeText(s_curFrame) : String("");
    if (minAgo <= 0) snprintf(lbl, sizeof(lbl), "nyni  %s", hhmm.c_str());
    else             snprintf(lbl, sizeof(lbl), "-%d min  %s", minAgo, hhmm.c_str());
    gfx->fillRect(CX - 70, 34, 140, 34, C_BLACK);   // backing
    int gap = 16, totalW = (n - 1) * gap, sx0 = CX - totalW / 2, dy = 44;
    for (int i = 0; i < n; i++) {
      int x = sx0 + i * gap;
      if (i == s_curFrame) gfx->fillCircle(x, dy, 4, C_YELLOW);
      else                 gfx->drawCircle(x, dy, 4, C_GRAY);
    }
    UI_TextCenteredIn(lbl, 0, LCD_WIDTH, 56, C_CYAN, 1);
  }

  // Range + indicator dots (bottom centre) with a backing.
  char rbuf[16];
  snprintf(rbuf, sizeof(rbuf), "%.0f km", currentRange());
  gfx->fillRect(CX - 46, LCD_HEIGHT - 74, 92, 40, C_BLACK);
  UI_TextCenteredIn(rbuf, 0, LCD_WIDTH, LCD_HEIGHT - 70, C_YELLOW, 2);
  int dotGap = 16, dotY = LCD_HEIGHT - 42, totalW = (RANGE_COUNT - 1) * dotGap;
  int startX = CX - totalW / 2;
  for (int i = 0; i < RANGE_COUNT; i++) {
    int x = startX + i * dotGap;
    if (i == s_rangeIdx) gfx->fillCircle(x, dotY, 3, C_YELLOW);
    else                 gfx->drawCircle(x, dotY, 3, C_GRAY);
  }
}

void ScreenWeather_Enter() {
  s_lastStep = millis();
  s_gap = false;
  s_curFrame = 0;
}

void ScreenWeather_ChangeRange(int dir) {
  s_rangeIdx = (s_rangeIdx + dir + RANGE_COUNT) % RANGE_COUNT;
  s_needRebuild = true;   // re-crop the frames on the next tick
}

bool ScreenWeather_Tick() {
  if (WiFi.status() != WL_CONNECTED) { s_status = "Ceka na WiFi"; return false; }
  unsigned long now = millis();

  // Range changed -> just re-crop (frames already downloaded).
  if (s_needRebuild && CHMU_AnimCount() > 0) {
    rebuildCrops();
    s_needRebuild = false; s_curFrame = 0; s_gap = false; s_lastStep = now;
    return true;
  }

  // First load.
  if (s_frameCount == 0) {
    loadAndBuild();
    s_lastFetch = now; s_curFrame = 0; s_gap = false; s_lastStep = now;
    return true;
  }

  // Periodic refresh - only during the pause (last frame shown).
  if (s_gap && (now - s_lastFetch >= 300000UL)) {
    loadAndBuild();
    s_lastFetch = now; s_curFrame = 0; s_gap = false; s_lastStep = now;
    return true;
  }

  // Animation step.
  if (s_gap) {
    if (now - s_gapStart >= 5000UL) { s_gap = false; s_curFrame = 0; s_lastStep = now; return true; }
    return false;
  }
  if (now - s_lastStep >= 500UL) {
    s_lastStep = now;
    s_curFrame++;
    if (s_curFrame >= s_frameCount) { s_curFrame = s_frameCount - 1; s_gap = true; s_gapStart = now; }
    return true;
  }
  return false;
}

void ScreenWeather_Draw() {
  gfx->fillScreen(C_BLACK);

  if (s_frameCount == 0) {
    UI_TextCentered("Meteoradar CHMU", LCD_HEIGHT / 2 - 20, C_WHITE, 2);
    UI_TextCentered(s_wide ? "snimek moc siroky" : s_status.c_str(), LCD_HEIGHT / 2 + 6, C_YELLOW, 2);
    return;
  }

  int f = clampI(s_curFrame, 0, s_frameCount - 1);
  s_crop565 = s_frame565[f];
  blitCrop();

  CzBorder_Draw(borderProject, C_GRAY);
  {
    float rng = currentRange();
    bool showFull  = (rng <= 50.0f);
    bool showSmall = (rng <= 100.0f);
    CzBorder_DrawCities(borderProject, CX, CY, DISP_R,
                        C_WHITE, C_CYAN, showFull, showSmall);
  }

  // Thin ring marking the edge of the round display.
  gfx->drawCircle(CX, CY, DISP_R, C_DKGRAY);

  drawOverlay();
}
