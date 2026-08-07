// =============================================================================
//  MeteoPlaneRadar
//  Screen 2: settings (brightness, WiFi, location).
//
//  Project: MeteoPlaneRadar - live aircraft radar on a round touchscreen
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
//  Web:     https://chiptron.cz
//  Board:   Waveshare ESP32-S3-Touch-LCD-2.1 (round 480x480 display, ST7701)
// =============================================================================
#include "ScreenSettings.h"
#include "Settings.h"
#include "WiFiPortal.h"
#include "UI.h"
#include "Display_ST7701.h"
#include "Config.h"
#include "Version.h"

#include <WiFi.h>

// Round panel - controls centred in a vertical list. The rows are packed a bit
// tighter than before to make room for the map-rotation row.
#define ROW_BRIGHT   84    // brightness label
#define SL_Y        106    // slider
#define ROW_WIFI    146
#define ROW_LOC     192
#define ROT_Y       236    // map rotation row
#define ROT_H        40
#define AUTO_Y      280    // auto screen-switch interval row
#define AUTO_H       40

// Brightness slider.
#define SL_X  90
#define SL_W  300
#define SL_H  24

// Map rotation controls: [-] [value] [+] on the right of the row.
#define ROT_MINUS_X  240
#define ROT_BTN_W     42
#define ROT_VAL_X    288
#define ROT_VAL_W     56
#define ROT_PLUS_X   350

// Small compass preview on the left edge, next to the rotation row.
#define COMPASS_CX    48
#define COMPASS_CY   256
#define COMPASS_R     24

// Two stacked buttons (WiFi/location + firmware update).
#define BTN_X   90
#define BTN_W   300
#define BTN_H   42
#define BTN1_Y  322        // WiFi / poloha (portal)
#define BTN2_Y  366        // aktualizace FW (OTA)

static bool s_wantsPortal = false;
static bool s_wantsOTA    = false;

// Short compass label for the eight main directions. Anything that is not a
// multiple of 45 deg (possible if MAP_ROT_STEP_DEG is changed) falls back to
// plain degrees, so the row never shows nonsense.
static const char* bearingLabel(uint16_t deg, char* buf, size_t len) {
  static const char* N8[8] = { "S", "SV", "V", "JV", "J", "JZ", "Z", "SZ" };
  if (deg % 45 == 0) return N8[(deg / 45) % 8];
  snprintf(buf, len, "%u", (unsigned)deg);
  return buf;
}

void ScreenSettings_Enter() {}
bool ScreenSettings_Tick() { return false; }

bool ScreenSettings_WantsPortal() { return s_wantsPortal; }
void ScreenSettings_ClearPortal() { s_wantsPortal = false; }

bool ScreenSettings_WantsOTA() { return s_wantsOTA; }
void ScreenSettings_ClearOTA() { s_wantsOTA = false; }

bool ScreenSettings_HandleTap(int x, int y) {
  // Brightness slider (generous touch zone around the track).
  if (y >= SL_Y - 25 && y <= SL_Y + SL_H + 25 && x >= SL_X - 10 && x <= SL_X + SL_W + 10) {
    int pct = (x - SL_X) * 100 / SL_W;
    if (pct < 10) pct = 10;
    if (pct > 100) pct = 100;
    Settings_SetBacklight(pct);
    Set_Backlight(pct);
    return true;
  }
  // Which bearing is at the top: [-] and [+] step through the compass by
  // MAP_ROT_STEP_DEG (must divide 90 so the cardinal directions stay reachable).
  // "+" walks clockwise: S -> SV -> V -> JV ...
  if (y >= ROT_Y && y <= ROT_Y + ROT_H) {
    int top = (int)Settings_TopBearing();
    if (x >= ROT_MINUS_X && x <= ROT_MINUS_X + ROT_BTN_W) {
      Settings_SetTopBearing((uint16_t)((top - MAP_ROT_STEP_DEG + 360) % 360));
      return true;
    }
    if (x >= ROT_PLUS_X && x <= ROT_PLUS_X + ROT_BTN_W) {
      Settings_SetTopBearing((uint16_t)((top + MAP_ROT_STEP_DEG) % 360));
      return true;
    }
  }
  // Auto screen-switch interval: [-]/[+] step the minutes 0..9 (0 = off), clamped
  // at both ends (unlike the compass row, which wraps).
  if (y >= AUTO_Y && y <= AUTO_Y + AUTO_H) {
    int m = (int)Settings_AutoSwitchMin();
    if (x >= ROT_MINUS_X && x <= ROT_MINUS_X + ROT_BTN_W) {
      Settings_SetAutoSwitchMin((uint8_t)(m > 0 ? m - 1 : 0));
      return true;
    }
    if (x >= ROT_PLUS_X && x <= ROT_PLUS_X + ROT_BTN_W) {
      Settings_SetAutoSwitchMin((uint8_t)(m < 9 ? m + 1 : 9));
      return true;
    }
  }
  // "WiFi / poloha" button.
  if (x >= BTN_X && x <= BTN_X + BTN_W && y >= BTN1_Y && y <= BTN1_Y + BTN_H) {
    s_wantsPortal = true;
    return true;
  }
  // "Firmware update (OTA)" button.
  if (x >= BTN_X && x <= BTN_X + BTN_W && y >= BTN2_Y && y <= BTN2_Y + BTN_H) {
    s_wantsOTA = true;
    return true;
  }
  return false;
}

void ScreenSettings_Draw() {
  gfx->fillScreen(C_BLACK);

  UI_TextCentered("Nastaveni", 40, C_WHITE, 3);
  // Firmware version right under the title, so the user always knows what is
  // flashed (useful before/after an OTA update).
  { char v[24]; snprintf(v, sizeof(v), "firmware v%s", FW_VERSION);
    UI_TextCentered(v, 68, C_GRAY, 1); }

  // --- Brightness (slider) ---
  gfx->setTextSize(2); gfx->setTextColor(C_GRAY);
  gfx->setCursor(SL_X, ROW_BRIGHT); gfx->print("Jas");
  uint8_t bl = Settings_Backlight();
  char blbuf[8]; snprintf(blbuf, sizeof(blbuf), "%d%%", bl);
  gfx->setTextColor(C_WHITE);
  gfx->setTextSize(2);
  // value on the right
  int16_t bx1, by1; uint16_t bw, bh;
  gfx->getTextBounds(blbuf, 0, 0, &bx1, &by1, &bw, &bh);
  gfx->setCursor(SL_X + SL_W - bw, ROW_BRIGHT); gfx->print(blbuf);
  // track + fill
  gfx->fillRoundRect(SL_X, SL_Y, SL_W, SL_H, SL_H / 2, C_DKGRAY);
  int fillW = SL_W * bl / 100;
  gfx->fillRoundRect(SL_X, SL_Y, fillW, SL_H, SL_H / 2, C_CYAN);
  gfx->fillCircle(SL_X + fillW, SL_Y + SL_H / 2, 15, C_WHITE);

  // --- WiFi ---
  gfx->setTextSize(2); gfx->setTextColor(C_GRAY);
  gfx->setCursor(SL_X, ROW_WIFI); gfx->print("WiFi: ");
  if (WiFi_IsConnected()) {
    gfx->setTextColor(C_GREEN);
    gfx->print(WiFi_SSID());
    // IP in a larger font on the next line
    gfx->setTextSize(2); gfx->setTextColor(C_WHITE);
    gfx->setCursor(SL_X, ROW_WIFI + 22); gfx->print(WiFi_IP());
  } else {
    gfx->setTextColor(C_YELLOW);
    gfx->print("nepripojeno");
  }

  // --- Location ---
  gfx->setTextSize(2); gfx->setTextColor(C_GRAY);
  gfx->setCursor(SL_X, ROW_LOC); gfx->print("Poloha:");
  char loc[40];
  snprintf(loc, sizeof(loc), "%.3f, %.3f", Settings_Lat(), Settings_Lon());
  gfx->setTextColor(C_WHITE);
  gfx->setCursor(SL_X, ROW_LOC + 22); gfx->print(loc);

  // --- Which bearing is at the top: label + [-] [value] [+] ---
  // The user sets the direction they are looking, so the value reads as a
  // compass point ("SV"), not as an amount to turn by.
  gfx->setTextSize(2); gfx->setTextColor(C_GRAY);
  gfx->setCursor(SL_X, ROT_Y + 12); gfx->print("Nahore");

  uint16_t top = Settings_TopBearing();
  gfx->fillRoundRect(ROT_MINUS_X, ROT_Y, ROT_BTN_W, ROT_H, 8, C_DKGRAY);
  UI_TextCenteredIn("-", ROT_MINUS_X, ROT_BTN_W, ROT_Y + 12, C_WHITE, 2);
  gfx->fillRoundRect(ROT_PLUS_X, ROT_Y, ROT_BTN_W, ROT_H, 8, C_DKGRAY);
  UI_TextCenteredIn("+", ROT_PLUS_X, ROT_BTN_W, ROT_Y + 12, C_WHITE, 2);
  { char rb[8];
    UI_TextCenteredIn(bearingLabel(top, rb, sizeof(rb)),
                      ROT_VAL_X, ROT_VAL_W, ROT_Y + 12, C_YELLOW, 2); }

  // Small compass preview on the left edge: a ring, a needle and "S" for north.
  // North sits at screen angle (0 - top), the same rule the radar uses.
  {
    gfx->drawCircle(COMPASS_CX, COMPASS_CY, COMPASS_R, C_DKGRAY);
    float a = -(float)top * 0.0174532925f;
    int nx = COMPASS_CX + (int)((COMPASS_R - 6) * sinf(a));
    int ny = COMPASS_CY - (int)((COMPASS_R - 6) * cosf(a));
    gfx->drawLine(COMPASS_CX, COMPASS_CY, nx, ny, C_WHITE);
    gfx->fillCircle(COMPASS_CX, COMPASS_CY, 2, C_GRAY);
    int lx = COMPASS_CX + (int)(COMPASS_R * sinf(a)) - 2;
    int ly = COMPASS_CY - (int)(COMPASS_R * cosf(a)) - 3;
    gfx->setTextSize(1); gfx->setTextColor(C_WHITE);
    gfx->setCursor(lx, ly); gfx->print("S");
  }

  // --- Auto screen switching: label + [-] [value] [+] (same pattern as above) ---
  // Value is the interval in minutes; "Vyp" (off) means the screens never auto-cycle.
  gfx->setTextSize(2); gfx->setTextColor(C_GRAY);
  gfx->setCursor(SL_X, AUTO_Y + 12); gfx->print("Auto min");

  uint8_t am = Settings_AutoSwitchMin();
  gfx->fillRoundRect(ROT_MINUS_X, AUTO_Y, ROT_BTN_W, ROT_H, 8, C_DKGRAY);
  UI_TextCenteredIn("-", ROT_MINUS_X, ROT_BTN_W, AUTO_Y + 12, C_WHITE, 2);
  gfx->fillRoundRect(ROT_PLUS_X, AUTO_Y, ROT_BTN_W, ROT_H, 8, C_DKGRAY);
  UI_TextCenteredIn("+", ROT_PLUS_X, ROT_BTN_W, AUTO_Y + 12, C_WHITE, 2);
  { char ab[8];
    if (am == 0) strcpy(ab, "Vyp"); else snprintf(ab, sizeof(ab), "%u", am);
    UI_TextCenteredIn(ab, ROT_VAL_X, ROT_VAL_W, AUTO_Y + 12, C_YELLOW, 2); }

  // --- Button 1: change WiFi / location (portal) ---
  gfx->fillRoundRect(BTN_X, BTN1_Y, BTN_W, BTN_H, 12, C_CYAN);
  UI_TextCentered("WiFi / poloha", BTN1_Y + BTN_H / 2 - 8, C_BLACK, 2);

  // --- Button 2: firmware update over WiFi (OTA) ---
  gfx->fillRoundRect(BTN_X, BTN2_Y, BTN_W, BTN_H, 12, C_ORANGE);
  UI_TextCentered("Aktualizace FW", BTN2_Y + BTN_H / 2 - 8, C_BLACK, 2);

  // Signature at the bottom.
  UI_TextCentered("chiptron.cz", 414, C_GREEN, 2);
}
