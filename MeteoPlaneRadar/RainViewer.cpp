// =============================================================================
//  MeteoPlaneRadar
//  RainViewer tile radar. See RainViewer.h for the design notes.
//
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
// =============================================================================
#include "RainViewer.h"
#include "Net.h"
#include "Status.h"
#include "Display_ST7701.h"
#include "TimeUtil.h"
#include <ArduinoJson.h>
#include <PNGdec.h>
#include <esp_heap_caps.h>
#include <math.h>
#include <string.h>
#include <time.h>

#define FRAME_PX ((int32_t)LCD_WIDTH * LCD_HEIGHT)

static void (*s_poll)() = nullptr;
void RainViewer_SetPollFn(void (*fn)()) { s_poll = fn; }
static inline void poll() { if (s_poll) s_poll(); }

// --- The view ---------------------------------------------------------------
static double s_lat = 0, s_lon = 0;
static float  s_reqRadius = -1;
static int    s_zoom = RV_MAX_ZOOM;
static int    s_scale = 1;     // display pixels per tile pixel (1, 2, 4 or 8)
static double s_originX = 0, s_originY = 0;   // world pixel of the display's top-left
static float  s_effRadiusKm = 0;

// How much of the world is on screen, in world pixels. With s_scale > 1 the
// display shows FEWER world pixels, each drawn as a block.
static inline double viewWorldW() { return (double)LCD_WIDTH  / s_scale; }
static inline double viewWorldH() { return (double)LCD_HEIGHT / s_scale; }

// --- Frames -----------------------------------------------------------------
struct RvFrame {
  uint16_t* px = nullptr;
  time_t    t  = 0;
  char      path[48] = "";
  bool      ready = false;
};
static RvFrame s_fr[RV_ANIM_MAX];
static int     s_frameN = 0;          // frames we intend to build
static int     s_readyN = 0;          // frames finished, counted from the oldest
static char    s_host[64] = "";

// --- Incremental fetch state ------------------------------------------------
enum RvState : uint8_t { RV_IDLE, RV_NEED_INDEX, RV_TILES, RV_DONE };
static RvState s_state = RV_IDLE;
static int  s_buildOrder[RV_ANIM_MAX];  // which frame to build next
static int  s_buildPos = 0;
static int  s_tilePos  = 0;
static int  s_tileTry  = 0;   // attempts spent on the current tile
static unsigned long s_retryAt = 0;   // do not try again before this
static bool s_failed = false;
static int  s_missed = 0;     // tiles given up on, reported on the status page

// Tile grid for the current view.
static int s_tx0, s_ty0, s_txN, s_tyN;

// One tile's PNG, reused for every download.
static uint8_t* s_png = nullptr;
static size_t   s_pngLen = 0;
static PNG      s_decoder;
static uint16_t* s_lineBuf = nullptr;

// Where the tile being decoded lands on the display.
static int s_dstX = 0, s_dstY = 0;
static uint16_t* s_dstFrame = nullptr;

bool RainViewer_Busy()   { return s_state == RV_NEED_INDEX || s_state == RV_TILES; }
bool RainViewer_Failed() { return s_failed; }
float RainViewer_EffectiveRadiusKm() { return s_effRadiusKm; }

// --- Web Mercator -----------------------------------------------------------
static inline double worldSize(int z) { return 256.0 * (double)(1UL << z); }

static void lonLatToWorld(double lat, double lon, int z, double* wx, double* wy) {
  double s = worldSize(z);
  double la = lat * 0.01745329252;
  if (la >  1.4835) la =  1.4835;      // clamp near the poles, where y explodes
  if (la < -1.4835) la = -1.4835;
  *wx = (lon + 180.0) / 360.0 * s;
  *wy = (1.0 - log(tan(0.78539816339 + la * 0.5)) / M_PI) * 0.5 * s;
}

static void worldToLonLat(double wx, double wy, int z, double* lat, double* lon) {
  double s = worldSize(z);
  *lon = wx / s * 360.0 - 180.0;
  double n = M_PI * (1.0 - 2.0 * wy / s);
  *lat = atan(sinh(n)) * 57.2957795131;
}

void RainViewer_Project(float lat, float lon, int* sx, int* sy) {
  double wx, wy;
  lonLatToWorld(lat, lon, s_zoom, &wx, &wy);
  *sx = (int)lround((wx - s_originX) * s_scale);
  *sy = (int)lround((wy - s_originY) * s_scale);
}

void RainViewer_Window(float* lat0, float* lat1, float* lon0, float* lon1) {
  double laTL, loTL, laBR, loBR;
  worldToLonLat(s_originX, s_originY, s_zoom, &laTL, &loTL);
  worldToLonLat(s_originX + viewWorldW(), s_originY + viewWorldH(), s_zoom,
                &laBR, &loBR);
  if (lat0) *lat0 = (float)laBR;      // south
  if (lat1) *lat1 = (float)laTL;      // north
  if (lon0) *lon0 = (float)loTL;
  if (lon1) *lon1 = (float)loBR;
}

// Pick the zoom and the upscale factor. Nothing above zoom 7 exists, so closer
// views are made by drawing each tile pixel as a block - coarser than CHMU, but
// that is genuinely all the detail RainViewer has.
//
// mustCover forces a view at least as wide as asked for, used by the
// whole-country range where showing too little would be worse than too much.
static void chooseZoomAndScale(double lat, float radiusKm, bool mustCover) {
  const double want = (double)radiusKm * 1000.0 / (LCD_HEIGHT / 2);   // m per px
  const double base = 156543.03392 * cos(lat * 0.01745329252);

  // The zoom we would ask for if there were no cap.
  const int ideal = (int)lround(log(base / want) / log(2.0));

  // Always the HIGHEST zoom available: zoom 4 blown up eight times covers the
  // same ground as zoom 7 with an eighth of the detail. Scaling is what is left
  // over after the cap, never a choice.
  int z = ideal;
  if (z > RV_MAX_ZOOM) z = RV_MAX_ZOOM;
  if (z < 3) z = 3;

  int s = 1;
  if (ideal > z) {
    const int shift = ideal - z;
    s = (shift >= 4) ? RV_MAX_SCALE : (1 << shift);
    if (s > RV_MAX_SCALE) s = RV_MAX_SCALE;
  }

  // The whole-country view must not come back narrower than the country, so
  // back off until it covers what was asked for.
  if (mustCover) {
    while ((base / (double)(1UL << z) / s) < want && (s > 1 || z > 3)) {
      if (s > 1) s >>= 1;
      else       z--;
    }
  }

  s_zoom = z;
  s_scale = s;
  const double mpp = base / (double)(1UL << s_zoom) / s_scale;
  s_effRadiusKm = (float)(mpp * (LCD_HEIGHT / 2) / 1000.0);
}

// --- Buffers ----------------------------------------------------------------
static bool ensureBuffers(int frames) {
  if (!s_png) {
    s_png = (uint8_t*)heap_caps_malloc(RV_MAX_PNG, MALLOC_CAP_SPIRAM);
    if (!s_png) s_png = (uint8_t*)malloc(RV_MAX_PNG);
    if (!s_png) return false;
  }
  if (!s_lineBuf) {
    // One decoded tile row. Internal RAM: PNGdec writes it pixel by pixel and
    // PSRAM would make that crawl.
    s_lineBuf = (uint16_t*)malloc(RV_TILE_SIZE * sizeof(uint16_t));
    if (!s_lineBuf) return false;
  }
  for (int i = 0; i < frames; i++) {
    if (!s_fr[i].px) {
      s_fr[i].px = (uint16_t*)heap_caps_malloc(FRAME_PX * 2, MALLOC_CAP_SPIRAM);
      if (!s_fr[i].px) return false;
    }
  }
  return true;
}

// --- PNG callback -----------------------------------------------------------
// One decoded tile row, expanded by s_scale into the frame. At scale 1 this is
// a straight memcpy; above that each source pixel becomes an s x s block.
// Nearest neighbour on purpose - the tiles are already smoothed server-side
// (RV_SMOOTH), and interpolating radar reflectivity would invent intensities
// that the data does not contain.
static int rvPngDraw(PNGDRAW* d) {
  if (!s_dstFrame || !s_lineBuf) return 0;
  // The line buffer is sized for the tile size we asked for. If the server ever
  // answered with something wider, decoding into it would run off the end of
  // the allocation - refuse instead of corrupting memory.
  if (d->iWidth > RV_TILE_SIZE) return 0;

  const int S = s_scale;
  const int dyTop = s_dstY + d->y * S;
  if (dyTop + S <= 0 || dyTop >= LCD_HEIGHT) return 1;   // whole row off-screen

  // Transparent pixels (no radar coverage) blend to black, which is what the
  // rest of the screen is - so "no data" and "no rain" look the same, exactly
  // as they do on the CHMU composite.
  s_decoder.getLineAsRGB565(d, s_lineBuf, PNG_RGB565_LITTLE_ENDIAN, 0x00000000);

  // Which source pixels can land on screen at all. Working this out once beats
  // clipping inside the inner loop, which at scale 8 runs 2048 times a row.
  int sx0 = 0, sx1 = RV_TILE_SIZE - 1;
  if (s_dstX < 0) sx0 = (-s_dstX + S - 1) / S;
  const int maxSx = (LCD_WIDTH - 1 - s_dstX) / S;
  if (maxSx < sx1) sx1 = maxSx;
  if (sx0 > sx1) return 1;

  if (S == 1) {
    const int dy = dyTop;
    if (dy < 0 || dy >= LCD_HEIGHT) return 1;
    memcpy(s_dstFrame + (int32_t)dy * LCD_WIDTH + (s_dstX + sx0),
           s_lineBuf + sx0, (size_t)(sx1 - sx0 + 1) * sizeof(uint16_t));
    return 1;
  }

  for (int r = 0; r < S; r++) {
    const int dy = dyTop + r;
    if (dy < 0 || dy >= LCD_HEIGHT) continue;
    uint16_t* dst = s_dstFrame + (int32_t)dy * LCD_WIDTH;
    for (int sx = sx0; sx <= sx1; sx++) {
      const uint16_t col = s_lineBuf[sx];
      int dx = s_dstX + sx * S;
      for (int k = 0; k < S; k++, dx++) {
        if (dx < 0 || dx >= LCD_WIDTH) continue;
        dst[dx] = col;
      }
    }
  }
  return 1;
}

// --- Index ------------------------------------------------------------------
static bool fetchIndex(int wantFrames) {
  String body;
  if (!Net_GetString(RV_INDEX_URL, body, "RAINVIEWER")) return false;

  JsonDocument filter;
  filter["host"] = true;
  JsonObject fp = filter["radar"]["past"].add<JsonObject>();
  fp["time"] = true;
  fp["path"] = true;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body,
                                             DeserializationOption::Filter(filter));
  body = String();
  if (err) { Serial.printf("RAINVIEWER: JSON %s\n", err.c_str()); return false; }

  const char* host = doc["host"] | "https://tilecache.rainviewer.com";
  strncpy(s_host, host, sizeof(s_host) - 1);
  s_host[sizeof(s_host) - 1] = '\0';

  JsonArrayConst past = doc["radar"]["past"];
  if (past.isNull() || past.size() == 0) {
    Serial.println("RAINVIEWER: zadne snimky v indexu");
    return false;
  }

  int have = (int)past.size();
  int take = wantFrames;
  if (take > RV_ANIM_MAX) take = RV_ANIM_MAX;
  if (take > have) take = have;

  // The newest `take` frames, kept oldest-first to match the CHMU animation.
  int first = have - take;
  for (int i = 0; i < take; i++) {
    JsonObjectConst f = past[first + i];
    s_fr[i].t = (time_t)(f["time"] | 0);
    const char* p = f["path"] | "";
    strncpy(s_fr[i].path, p, sizeof(s_fr[i].path) - 1);
    s_fr[i].path[sizeof(s_fr[i].path) - 1] = '\0';
    s_fr[i].ready = false;
  }
  s_frameN = take;
  s_readyN = 0;

  // Build the newest frame first: one usable picture beats six that arrive
  // together a minute later. The rest fill in behind it.
  for (int i = 0; i < take; i++) s_buildOrder[i] = take - 1 - i;
  s_buildPos = 0;
  s_tilePos = 0;
  s_tileTry = 0;
  return true;
}

// --- Tiles ------------------------------------------------------------------
static void computeGrid() {
  double wx, wy;
  double lat = s_lat, lon = s_lon;
  float  rad = s_reqRadius;
  if (rad <= 0.0f) { lat = CZ_VIEW_LAT; lon = CZ_VIEW_LON; rad = CZ_VIEW_RADIUS_KM; }

  // The whole-country view must not come back narrower than the country.
  chooseZoomAndScale(lat, rad, s_reqRadius <= 0.0f);
  lonLatToWorld(lat, lon, s_zoom, &wx, &wy);
  s_originX = wx - viewWorldW() / 2.0;
  s_originY = wy - viewWorldH() / 2.0;

  s_tx0 = (int)floor(s_originX / RV_TILE_SIZE);
  s_ty0 = (int)floor(s_originY / RV_TILE_SIZE);
  int tx1 = (int)floor((s_originX + viewWorldW() - 1) / RV_TILE_SIZE);
  int ty1 = (int)floor((s_originY + viewWorldH() - 1) / RV_TILE_SIZE);
  s_txN = tx1 - s_tx0 + 1;
  s_tyN = ty1 - s_ty0 + 1;
  if (s_txN < 1) s_txN = 1;
  if (s_tyN < 1) s_tyN = 1;
  Serial.printf("RainViewer: zoom %d, zvetseni %dx, %dx%d dlazdic, polomer %.0f km\n",
                s_zoom, s_scale, s_txN, s_tyN, s_effRadiusKm);
}

static bool fetchOneTile(int frameIdx, int tileIdx) {
  const int tiles = s_txN * s_tyN;
  if (tileIdx >= tiles) return false;

  const int gx = tileIdx % s_txN;
  const int gy = tileIdx / s_txN;
  const int tx = s_tx0 + gx;
  const int ty = s_ty0 + gy;

  const int world = 1 << s_zoom;
  int wrapX = tx % world; if (wrapX < 0) wrapX += world;      // wrap at the date line
  if (ty < 0 || ty >= world) return true;                     // above the pole - nothing there

  char url[192];
  snprintf(url, sizeof(url), "%s%s/%d/%d/%d/%d/%d/%d_%d.png",
           s_host, s_fr[frameIdx].path, RV_TILE_SIZE, s_zoom, wrapX, ty,
           RV_COLOR, RV_SMOOTH, RV_SNOW);

  if (!Net_GetBinary(url, s_png, RV_MAX_PNG, &s_pngLen, "RAINVIEWER")) return false;

  s_dstX = (int)lround((tx * (double)RV_TILE_SIZE - s_originX) * s_scale);
  s_dstY = (int)lround((ty * (double)RV_TILE_SIZE - s_originY) * s_scale);
  s_dstFrame = s_fr[frameIdx].px;

  if (s_decoder.openRAM(s_png, s_pngLen, rvPngDraw) != PNG_SUCCESS) {
    Serial.println("RAINVIEWER: dlazdice se neda dekodovat");
    return false;
  }
  s_decoder.decode(nullptr, 0);
  s_decoder.close();
  poll();
  return true;
}

// --- Public -----------------------------------------------------------------
void RainViewer_Begin(double lat, double lon, float radiusKm, int wantFrames) {
  bool same = (fabs(lat - s_lat) < 1e-6) && (fabs(lon - s_lon) < 1e-6) &&
              (fabsf(radiusKm - s_reqRadius) < 0.01f) && s_state != RV_IDLE;
  if (same) return;

  // A view change abandons whatever was in flight, connection included.
  Net_SessionEnd();

  s_lat = lat; s_lon = lon; s_reqRadius = radiusKm;
  computeGrid();

  int want = wantFrames;
  if (want > RV_ANIM_MAX) want = RV_ANIM_MAX;
  if (!ensureBuffers(want)) {
    Serial.println("RAINVIEWER: nedostatek PSRAM na snimky");
    s_state = RV_DONE;
    s_failed = true;
    return;
  }
  // Start from black: a partially built frame must not show pixels left over
  // from the previous view.
  for (int i = 0; i < want; i++) {
    if (s_fr[i].px) memset(s_fr[i].px, 0, (size_t)FRAME_PX * 2);
    s_fr[i].ready = false;
  }
  s_frameN = want;
  s_readyN = 0;
  s_missed = 0;
  s_retryAt = 0;
  s_failed = false;
  s_state = RV_NEED_INDEX;
}

void RainViewer_Refresh() {
  if (s_state == RV_IDLE) return;      // nothing configured yet
  Net_SessionEnd();
  for (int i = 0; i < s_frameN; i++) {
    if (s_fr[i].px) memset(s_fr[i].px, 0, (size_t)FRAME_PX * 2);
    s_fr[i].ready = false;
  }
  s_readyN = 0;
  s_buildPos = 0;
  s_tilePos = 0;
  s_tileTry = 0;
  s_retryAt = 0;
  s_missed = 0;
  s_failed = false;
  s_state = RV_NEED_INDEX;
}

bool RainViewer_Step() {
  switch (s_state) {
    case RV_IDLE:
    case RV_DONE:
      return false;

    case RV_NEED_INDEX:
      if (!fetchIndex(s_frameN)) {
        s_failed = true;
        s_state = RV_DONE;
        Net_SessionEnd();
        return false;
      }
      computeGrid();                 // the frame count may have shrunk
      // One TLS connection for the whole build. A handshake per tile needs
      // ~45 kB of internal RAM, which the device does not reliably have while
      // the web server is running - the main reason tiles used to go missing.
      Net_SessionBegin();
      s_state = RV_TILES;
      return false;

    case RV_TILES: {
      if (s_buildPos >= s_frameN) { s_state = RV_DONE; return false; }
      const int f = s_buildOrder[s_buildPos];
      const int tiles = s_txN * s_tyN;

      // Wait out the backoff from the previous failure.
      if (s_retryAt && (long)(millis() - s_retryAt) < 0) return false;   // wrap-safe
      s_retryAt = 0;

      bool ok = fetchOneTile(f, s_tilePos);
      if (!ok && s_tileTry < RV_TILE_RETRY) {
        // Do NOT move on: giving up on the first failure leaves a permanent
        // black square. A dropped connection or a moment of low heap is usually
        // over by the next attempt.
        s_tileTry++;
        s_retryAt = millis() + RV_TILE_RETRY_MS;
        // A failure often means the connection is gone. Drop the session so the
        // next attempt builds a fresh one rather than writing into a dead socket.
        Net_SessionEnd();
        Net_SessionBegin();
        return false;
      }
      if (!ok) {
        Serial.printf("RAINVIEWER: dlazdice %d snimku %d se nestahla ani na %d pokusu\n",
                      s_tilePos, f, RV_TILE_RETRY + 1);
        s_missed++;
      }
      s_tileTry = 0;
      s_tilePos++;

      if (s_tilePos >= tiles) {
        s_fr[f].ready = true;
        s_tilePos = 0;
        s_tileTry = 0;
        s_buildPos++;
        // Ready frames are counted from the oldest, because the animation plays
        // in that order and must never show a half-built frame in the middle.
        int n = 0;
        for (int i = 0; i < s_frameN; i++) { if (!s_fr[i].ready) break; n++; }
        // The newest is built first, so until the oldest one lands this stays
        // at zero and RainViewer_Count() shows the newest frame on its own.
        s_readyN = n;
        if (s_buildPos >= s_frameN) {
          s_state = RV_DONE;
          s_readyN = s_frameN;
          Net_SessionEnd();
          Serial.printf("RainViewer: %d snimku, zoom %d, polomer %.0f km, %d dlazdic chybi\n",
                        s_frameN, s_zoom, s_effRadiusKm, s_missed);
          if (s_missed) Status_Set(ST_RADAR, "RainViewer: %d snimku, %d dlazdic chybi",
                                   s_frameN, s_missed);
          else          Status_Set(ST_RADAR, "RainViewer: %d snimku", s_frameN);
        }
        return true;               // a frame appeared - repaint
      }
      return false;
    }
  }
  return false;
}

int RainViewer_Count() {
  if (s_readyN > 0) return s_readyN;
  // Nothing complete in order yet, but the newest frame is built first, so if
  // that one is done it can be shown as a single still image.
  if (s_frameN > 0 && s_fr[s_frameN - 1].ready) return 1;
  return 0;
}

const uint16_t* RainViewer_Frame(int i) {
  if (s_frameN <= 0) return nullptr;
  // In the single-still case index 0 means "the newest".
  if (s_readyN == 0) return s_fr[s_frameN - 1].ready ? s_fr[s_frameN - 1].px : nullptr;
  if (i < 0 || i >= s_readyN) return nullptr;
  return s_fr[i].ready ? s_fr[i].px : nullptr;
}

static time_t frameTime(int i) {
  if (s_frameN <= 0) return 0;
  if (s_readyN == 0) return s_fr[s_frameN - 1].t;
  if (i < 0 || i >= s_readyN) return 0;
  return s_fr[i].t;
}

String RainViewer_TimeText(int i) {
  time_t t = frameTime(i);
  if (!t) return String("");
  struct tm lt;
  localtime_r(&t, &lt);
  char b[8];
  snprintf(b, sizeof(b), "%02d:%02d", lt.tm_hour, lt.tm_min);
  return String(b);
}

int RainViewer_MinutesAgo(int i) {
  time_t t = frameTime(i);
  if (!t) return 0;
  time_t newest = s_fr[s_frameN - 1].t;
  if (!newest || newest < t) return 0;
  return (int)((newest - t) / 60);
}
