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

#include <WiFi.h>

// Round panel - controls centred in a vertical list.
#define ROW_BRIGHT  105    // brightness label
#define SL_Y        138    // slider
#define ROW_WIFI    180
#define ROW_LOC     240
#define ROW_PORTAL  345    // button (pushed below the location)

// Brightness slider.
#define SL_X  90
#define SL_W  300
#define SL_H  24

// Portal button.
#define BTN_X  90
#define BTN_Y  (ROW_PORTAL - 26)
#define BTN_W  300
#define BTN_H  52

static bool s_wantsPortal = false;

void ScreenSettings_Enter() {}
bool ScreenSettings_Tick() { return false; }

bool ScreenSettings_WantsPortal() { return s_wantsPortal; }
void ScreenSettings_ClearPortal() { s_wantsPortal = false; }

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
  // "Change WiFi / location" button - exact button zone.
  if (x >= BTN_X && x <= BTN_X + BTN_W && y >= BTN_Y && y <= BTN_Y + BTN_H) {
    s_wantsPortal = true;
    return true;
  }
  return false;
}

void ScreenSettings_Draw() {
  gfx->fillScreen(C_BLACK);

  UI_TextCentered("Settings", 40, C_WHITE, 3);

  // --- Brightness (slider) ---
  gfx->setTextSize(2); gfx->setTextColor(C_GRAY);
  gfx->setCursor(SL_X, ROW_BRIGHT); gfx->print("Brightness");
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
    gfx->setCursor(SL_X, ROW_WIFI + 24); gfx->print(WiFi_IP());
  } else {
    gfx->setTextColor(C_YELLOW);
    gfx->print("not connected");
  }

  // --- Location ---
  gfx->setTextSize(2); gfx->setTextColor(C_GRAY);
  gfx->setCursor(SL_X, ROW_LOC); gfx->print("Location:");
  char loc[40];
  snprintf(loc, sizeof(loc), "%.3f, %.3f", Settings_Lat(), Settings_Lon());
  gfx->setTextColor(C_WHITE);
  gfx->setCursor(SL_X, ROW_LOC + 24); gfx->print(loc);

  // --- Button: change WiFi / location (portal) ---
  gfx->fillRoundRect(BTN_X, BTN_Y, BTN_W, BTN_H, 12, C_CYAN);
  UI_TextCentered("Change WiFi / location", BTN_Y + BTN_H / 2 - 8, C_BLACK, 2);

  // Signature at the bottom - same font as the "WiFi" label (2), same green as the SSID.
  UI_TextCentered("chiptron.cz", 405, C_GREEN, 2);
}
