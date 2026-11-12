// What the rest of the firmware would provide to the cards code, for the host
// programs in this folder (sim.cpp, scripttest.cpp). Include it once per program.
#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <WiFi.h>
#include "Canvas16.h"
#include "UI.h"
#include "Layout.h"
#include "Lang.h"
#include "Cards.h"
#include "Script.h"
#include "ScreenCards.h"
#include "Settings.h"
#include "Status.h"
#include "Net.h"
#include "Outside.h"
#include "Route.h"
#include "Config.h"
#include <chrono>
#include <fstream>
#include <sstream>
#include <string>

Arduino_GFX* gfx = nullptr;
SerialShim Serial;
int g_builtinGlyphs = 0;

// The clock: real time, plus whatever a test adds to jump ahead. Real time has
// to pass for the script time limit to mean anything.
static unsigned long s_ms = 5000;
unsigned long millis() {
  static const auto t0 = std::chrono::steady_clock::now();
  return s_ms + (unsigned long)std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - t0).count();
}

// The time of day a script sees. SIM_CLOCK=HH:MM pins it (for a picture that
// should not depend on when it was made); otherwise it is the PC's clock.
extern "C" time_t time(time_t* out) {
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  time_t now = ts.tv_sec;
  if (const char* fixed = getenv("SIM_CLOCK")) {
    int h = 0, m = 0;
    if (sscanf(fixed, "%d:%d", &h, &m) == 2) {
      struct tm lt;
      localtime_r(&now, &lt);
      lt.tm_hour = h; lt.tm_min = m; lt.tm_sec = 0;
      now = mktime(&lt);
    }
  }
  if (out) *out = now;
  return now;
}

static std::string slurp(const std::string& p) {
  std::ifstream f(p, std::ios::binary);
  std::stringstream s; s << f.rdbuf();
  return s.str();
}

uint8_t Settings_ClockFont() { return CLOCK_FONT_DEFAULT; }
bool Settings_ScreenEnabled(uint8_t idx) {
  // defaults (clock, planes, meteo, forecast) plus the cards screen switched on
  return idx == SCREEN_CLOCK_I || idx == SCREEN_PLANES_I || idx == SCREEN_METEO_I ||
         idx == SCREEN_FORECAST_I || idx == SCREEN_CARDS_I;
}
static bool s_quietStatus = false;
void Status_Set(StatusSlot slot, const char* fmt, ...) {
  char b[160]; va_list ap; va_start(ap, fmt); vsnprintf(b, sizeof(b), fmt, ap); va_end(ap);
  if (!s_quietStatus) printf("STATUS[%d] %s\n", (int)slot, b);
}
static std::string s_status;
void Outside_StatusText(char* buf, size_t cap) { snprintf(buf, cap, "%s", s_status.c_str()); }
void Route_ToAscii(char* dst, size_t cap, const char* src) {
  printf("UNEXPECTED: Route_ToAscii reached with a smooth typeface\n");
  snprintf(dst, cap, "%s", src);
}
uint16_t* LCD_FrameBuffer(int) { return nullptr; }
void LCD_Flush(const uint16_t*) {}

// The screen selector dots, as drawScreenDots() in MeteoPlaneRadar.ino: one per
// place in the stepping, and the cards screen has one place per installed card.
// Off by default like on the device (Settings_ScreenDots); SIM_DOTS=1 in the
// environment draws them.
static bool simDots() { const char* e = getenv("SIM_DOTS"); return e && *e && *e != '0'; }
static void drawScreenDots(int s_screen) {
  if (!simDots()) return;
  int vis[SCREEN_N]; int nVis = 0;
  for (int i = 0; i < SCREEN_N; i++)
    if (i == SCREEN_SETTINGS_I || Settings_ScreenEnabled(i)) vis[nVis++] = i;
  int n = 0;
  for (int i = 0; i < nVis; i++) n += (vis[i] == SCREEN_CARDS_I) ? ScreenCards_Count() : 1;
  if (n <= 1) return;
  const int room = 160;
  int gap = 20, r = 4;
  if ((n - 1) * gap > room) { gap = room / (n - 1); r = (gap >= 9) ? 3 : 2; }
  const int cx = LCD_WIDTH / 2;
  const int y = LY_DOTS;
  const int startX = cx - (n - 1) * gap / 2;
  int k = 0;
  for (int i = 0; i < nVis; i++) {
    const bool cards = (vis[i] == SCREEN_CARDS_I);
    const int places = cards ? ScreenCards_Count() : 1;
    for (int j = 0; j < places; j++, k++) {
      const int x = startX + k * gap;
      const bool on = (vis[i] == s_screen) && (!cards || j == ScreenCards_Index());
      if (on) gfx->fillCircle(x, y, r, C_WHITE);
      else    gfx->drawCircle(x, y, r, C_GRAY);
    }
  }
}

// Writes the frame as a PPM and says how much of it the round glass would hide.
static int dump(const std::string& path) {
  uint16_t* fb = static_cast<Canvas16*>(gfx)->getFramebuffer();
  FILE* f = fopen(path.c_str(), "wb");
  fprintf(f, "P6\n%d %d\n255\n", LCD_WIDTH, LCD_HEIGHT);
  int outside = 0, lit = 0;
  for (int y = 0; y < LCD_HEIGHT; y++) for (int x = 0; x < LCD_WIDTH; x++) {
    const uint16_t c = fb[y * LCD_WIDTH + x];
    const uint8_t r = (c >> 11) & 0x1F, g = (c >> 5) & 0x3F, b = c & 0x1F;
    const uint8_t px[3] = { (uint8_t)((r << 3) | (r >> 2)), (uint8_t)((g << 2) | (g >> 4)), (uint8_t)((b << 3) | (b >> 2)) };
    fwrite(px, 1, 3, f);
    const double dx = x - 239.5, dy = y - 239.5;
    if (c) lit++;
    if (c && dx * dx + dy * dy > 240.0 * 240.0) outside++;
  }
  fclose(f);
  printf("FRAME %s  lit pixels outside the circle: %d  built-in font glyphs: %d\n", path.c_str(), outside, g_builtinGlyphs);
  return lit;
}
static int frame(const std::string& path) {
  ScreenCards_Enter();
  ScreenCards_Draw();
  drawScreenDots(SCREEN_CARDS_I);
  return dump(path);
}
static void simBegin(const char* lang) {
  gfx = new Canvas16(LCD_WIDTH, LCD_HEIGHT);
  gfx->begin();
  Lang_Set(strcmp(lang, "en") == 0 ? LANG_EN : LANG_CZ);
}
