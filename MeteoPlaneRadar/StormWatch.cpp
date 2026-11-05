// =============================================================================
//  MeteoPlaneRadar
//  Storm watch - see StormWatch.h.
//
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
// =============================================================================
#include "StormWatch.h"
#include "CHMU.h"
#include "Settings.h"
#include "Config.h"
#include "Status.h"
#include "Geo.h"

#include <WiFi.h>
#include <PNGdec.h>
#include <esp_heap_caps.h>
#include <math.h>
#include <new>

// -----------------------------------------------------------------------------
//  The CHMU colour scale, 4 dBZ per step (scl-dbzmmh.png on opendata.chmi.cz).
//  A pixel is read back to the step whose colour it is closest to - but only if
//  it is genuinely close. Anything else (the black background, the paler
//  "does not reach the ground" shades of the masked composite, compression
//  noise at an edge) is not precipitation as far as the watch is concerned.
// -----------------------------------------------------------------------------
struct DbzColour { uint8_t r, g, b; uint8_t dbz; };
static const DbzColour SCALE[] = {
  {  56,   0, 112,  4 }, {  48,   0, 168,  8 }, {   0,   0, 252, 12 },
  {   0, 108, 192, 16 }, {   0, 160,   0, 20 }, {   0, 188,   0, 24 },
  {  52, 216,   0, 28 }, { 156, 220,   0, 32 }, { 224, 220,   0, 36 },
  { 252, 176,   0, 40 }, { 252, 132,   0, 44 }, { 252,  88,   0, 48 },
  { 252,   0,   0, 52 }, { 160,   0,   0, 56 }, { 252, 252, 252, 60 },
};
static const int SCALE_N = sizeof(SCALE) / sizeof(SCALE[0]);
static const int MATCH_MAX_D2 = 40 * 40;      // squared RGB distance

// RGB565 -> dBZ, or -1. Cached: a frame has a few dozen distinct colours and
// tens of thousands of pixels.
static uint32_t s_cacheKey[64];
static int8_t  s_cacheVal[64];
static int toDbz(uint16_t c) {
  if (c == 0) return -1;
  const int slot = (c ^ (c >> 6)) & 63;
  if (s_cacheKey[slot] == c) return s_cacheVal[slot];
  const int r = ((c >> 11) & 0x1F) * 255 / 31;
  const int g = ((c >> 5) & 0x3F) * 255 / 63;
  const int b = (c & 0x1F) * 255 / 31;
  int best = -1, bestD = MATCH_MAX_D2;
  for (int i = 0; i < SCALE_N; i++) {
    const int dr = r - SCALE[i].r, dg = g - SCALE[i].g, db = b - SCALE[i].b;
    const int d = dr * dr + dg * dg + db * db;
    if (d < bestD) { bestD = d; best = SCALE[i].dbz; }
  }
  s_cacheKey[slot] = c;
  s_cacheVal[slot] = (int8_t)best;
  return best;
}

// -----------------------------------------------------------------------------
//  State
// -----------------------------------------------------------------------------
static bool   s_active = false;
static bool   s_valid  = false;
static int    s_clear  = 0;
static float  s_maxDbz = -1;
static float  s_nearKm = -1;
static int    s_pixels = 0;
static String s_lastName;
static String s_frameTime;
static unsigned long s_nextCheck = 20000;     // let the boot settle first

bool   StormWatch_Active()    { return s_active; }
bool   StormWatch_Valid()     { return s_valid; }
float  StormWatch_MaxDbz()    { return s_maxDbz; }
float  StormWatch_NearestKm() { return s_nearKm; }
int    StormWatch_Pixels()    { return s_pixels; }
String StormWatch_FrameTime() { return s_frameTime; }

bool StormWatch_InCoverage() {
  const double la = Settings_Lat(), lo = Settings_Lon();
  return lo >= CHMU_LON_LEFT && lo <= CHMU_LON_DATA_RIGHT &&
         la >= CHMU_LAT_BOTTOM && la <= CHMU_LAT_DATA_TOP;
}

// -----------------------------------------------------------------------------
//  Decoding
//
//  The PNG decoder object is ~60 kB. The weather screen's own lives in .bss;
//  a second one there would come straight out of the internal RAM every TLS
//  handshake needs, so this one is placed in PSRAM.
// -----------------------------------------------------------------------------
static PNG*      s_png = nullptr;
static uint8_t*  s_buf = nullptr;
static uint16_t* s_line = nullptr;
static int       s_lineCap = 0;

// Analysis window, in image pixels, and the image geometry.
static int s_imgW = 0, s_imgH = 0;
static int s_x0, s_x1, s_y0, s_y1;
static int s_dataX1, s_dataY0;
static int s_thr = STORM_DBZ_DEFAULT;
static float s_radKm = STORM_KM_DEFAULT;

static float mercY(float latDeg) { float r = latDeg * 0.0174532925f; return logf(tanf(0.7853981634f + r * 0.5f)); }
static int lonToX(float lon) { return lroundf((lon - CHMU_LON_LEFT) * (s_imgW - 1) / (CHMU_LON_RIGHT - CHMU_LON_LEFT)); }
static int latToY(float lat) {
  const float yt = mercY(CHMU_LAT_TOP), yb = mercY(CHMU_LAT_BOTTOM);
  return lroundf((yt - mercY(lat)) * (s_imgH - 1) / (yt - yb));
}
static float xToLon(int x) { return CHMU_LON_LEFT + (float)x * (CHMU_LON_RIGHT - CHMU_LON_LEFT) / (s_imgW - 1); }
static float yToLat(int y) {
  const float yt = mercY(CHMU_LAT_TOP), yb = mercY(CHMU_LAT_BOTTOM);
  const float m = yt - (float)y * (yt - yb) / (s_imgH - 1);
  return (2.0f * atanf(expf(m)) - 1.5707963268f) * 57.29577951f;
}

static int drawCb(PNGDRAW* d) {
  const int y = d->y;
  if (y > s_y1) return 0;                 // past the circle - stop decoding
  if (y < s_y0 || y < s_dataY0) return 1;
  // Blend against black: a semi-transparent pixel then comes out darker than
  // any colour of the scale and is ignored, which is exactly right for the
  // paler shades of the masked composite.
  s_png->getLineAsRGB565(d, s_line, PNG_RGB565_LITTLE_ENDIAN, 0x00000000);
  const float lat = yToLat(y);
  const double myLat = Settings_Lat(), myLon = Settings_Lon();
  for (int x = s_x0; x <= s_x1; x++) {
    if (x < 0 || x >= s_imgW || x > s_dataX1) continue;
    const int dbz = toDbz(s_line[x]);
    if (dbz < 0) continue;
    float km;
    Geo_DistBearing(myLat, myLon, lat, xToLon(x), &km, nullptr);
    if (km > s_radKm) continue;
    if (dbz > s_maxDbz) s_maxDbz = (float)dbz;
    if (dbz >= s_thr) {
      s_pixels++;
      if (s_nearKm < 0 || km < s_nearKm) s_nearKm = km;
    }
  }
  return 1;
}

static bool ensureBuffers() {
  if (!s_png) {
    void* mem = heap_caps_malloc(sizeof(PNG), MALLOC_CAP_SPIRAM);
    if (!mem) return false;
    s_png = new (mem) PNG();
  }
  if (!s_buf) s_buf = (uint8_t*)heap_caps_malloc(CHMU_MAX_PNG, MALLOC_CAP_SPIRAM);
  return s_buf != nullptr;
}

static bool analyse(uint8_t* data, size_t size) {
  if (s_png->openRAM(data, (int)size, drawCb) != PNG_SUCCESS) return false;
  s_imgW = s_png->getWidth();
  s_imgH = s_png->getHeight();
  if (s_imgW <= 0 || s_imgH <= 0) { s_png->close(); return false; }
  if (s_imgW > s_lineCap) {
    if (s_line) heap_caps_free(s_line);
    s_line = (uint16_t*)heap_caps_malloc((size_t)s_imgW * 2, MALLOC_CAP_SPIRAM);
    s_lineCap = s_line ? s_imgW : 0;
    if (!s_line) { s_png->close(); return false; }
  }

  s_thr   = Settings_StormDbz();
  s_radKm = (float)Settings_StormKm();
  const double la = Settings_Lat(), lo = Settings_Lon();
  const float dLat = s_radKm / 111.0f;
  const float dLon = s_radKm / (111.0f * cosf(la * 0.0174532925f));
  s_x0 = lonToX(lo - dLon) - 1; s_x1 = lonToX(lo + dLon) + 1;
  s_y0 = latToY(la + dLat) - 1; s_y1 = latToY(la - dLat) + 1;
  s_dataX1 = lonToX(CHMU_LON_DATA_RIGHT);
  s_dataY0 = latToY(CHMU_LAT_DATA_TOP);

  s_maxDbz = -1; s_nearKm = -1; s_pixels = 0;
  memset(s_cacheKey, 0xFF, sizeof(s_cacheKey));   // 0xFFFFFFFF = empty slot
  s_png->decode(nullptr, 0);
  s_png->close();
  return true;
}

void StormWatch_Tick() {
  const bool wanted = Settings_StormOnly() || Settings_HaEnabled();
  if (!wanted) { s_active = false; return; }
  const unsigned long now = millis();
  if ((long)(now - s_nextCheck) < 0) return;
  if (WiFi.status() != WL_CONNECTED) { s_nextCheck = now + STORM_RETRY_MS; return; }

  if (!StormWatch_InCoverage()) {
    s_active = false; s_valid = false;
    Status_Set(ST_STORM, "mimo pokryti CHMU");
    s_nextCheck = now + STORM_PERIOD_MS;
    return;
  }
  if (!ensureBuffers()) {
    Status_Set(ST_STORM, "malo pameti");
    s_nextCheck = now + STORM_RETRY_MS;
    return;
  }

  const bool masked = (Settings_RadarSource() == RADAR_SRC_CHMU_MASKED);
  uint8_t* data = nullptr;
  size_t size = 0;
  String name;

  // The weather screen may already hold this very frame.
  const int n = CHMU_AnimCount();
  if (n > 0 && CHMU_Masked() == masked && CHMU_AnimName(n - 1) != s_lastName) {
    data = CHMU_AnimData(n - 1);
    size = CHMU_AnimSize(n - 1);
    name = CHMU_AnimName(n - 1);
  }
  if (!data) {
    if (!CHMU_FetchLatest(masked, s_buf, CHMU_MAX_PNG, &size, &name)) {
      Status_Set(ST_STORM, "chyba stahovani");
      s_nextCheck = millis() + STORM_RETRY_MS;
      return;
    }
    data = s_buf;
  }

  if (!analyse(data, size)) {
    Status_Set(ST_STORM, "chyba dekodovani");
    s_nextCheck = millis() + STORM_RETRY_MS;
    return;
  }
  s_lastName  = name;
  s_frameTime = CHMU_TimeTextFromName(name);
  s_valid     = true;

  const bool storm = s_pixels >= STORM_MIN_PIXELS;
  if (storm) { s_active = true; s_clear = 0; }
  else if (s_active && ++s_clear >= STORM_CLEAR_CHECKS) { s_active = false; s_clear = 0; }

  if (s_pixels > 0) {
    Status_Set(ST_STORM, "%s %s: max %d dBZ, %.0f km", s_active ? "BOURKA" : "klid",
               s_frameTime.c_str(), (int)s_maxDbz, s_nearKm);
  } else {
    Status_Set(ST_STORM, "%s %s: max %d dBZ", s_active ? "BOURKA" : "klid",
               s_frameTime.c_str(), (int)s_maxDbz);
  }
  Serial.printf("Bourka: %s, max %.0f dBZ, %d px >= %d dBZ do %u km\n",
                s_active ? "ANO" : "ne", s_maxDbz, s_pixels, s_thr, (unsigned)Settings_StormKm());
  s_nextCheck = millis() + STORM_PERIOD_MS;
}
