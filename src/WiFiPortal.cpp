// =============================================================================
//  MeteoPlaneRadar
//  WiFi connection + configuration AP portal with QR code.
//
//  Project: MeteoPlaneRadar - live aircraft radar on a round touchscreen
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
//  Web:     https://chiptron.cz
//  Board:   Waveshare ESP32-S3-Touch-LCD-2.1 (round 480x480 display, ST7701)
// =============================================================================
#include "WiFiPortal.h"
#include "Settings.h"
#include "UI.h"
#include "Display_ST7701.h"

#include <WiFi.h>
#include <WiFiManager.h>

static unsigned long s_lastReconnect = 0;

// Draw the AP join instructions on the display (round panel!).
// QR centred, short text above and below - all inside the safe circle.
static void drawApScreen() {
  gfx->fillScreen(C_BLACK);

  // Title at the top (inside the circle).
  UI_TextCentered("MeteoPlaneRadar", 34, C_CYAN, 2);
  UI_TextCentered("chiptron.cz", 58, C_GRAY, 1);
  UI_TextCentered("Scan with your phone:", 78, C_GRAY, 1);

  // QR centred. Round panel - the QR must be smaller so it fits entirely.
  int qrSize = 190;
  int qrX = (LCD_WIDTH - qrSize) / 2;
  int qrY = 98;
  UI_DrawWifiQR(AP_SSID, AP_PASSWORD, /*open=*/true, qrX, qrY, qrSize);

  // Below the QR: SSID + password (centred).
  UI_TextCentered("MeteoPlaneRadar", 300, C_WHITE, 1);
  UI_TextCentered("no password  |  then 192.168.4.1", 322, C_GRAY, 1);
  UI_TextCentered("Back: Exit button in the portal (or wait 3 min)", 344, C_GRAY, 1);

  extern Arduino_GFX* gfx;
  gfx->flush();   // canvas -> panel
}

static const char* apPass() {
  return (strlen(AP_PASSWORD) == 0) ? nullptr : AP_PASSWORD;
}

// WiFiManager callback fired when the AP starts.
static void onAP(WiFiManager* wm) {
  drawApScreen();
}

static void saveParams(WiFiManagerParameter& pLat, WiFiManagerParameter& pLon) {
  double lat = atof(pLat.getValue());
  double lon = atof(pLon.getValue());
  if (lat >= -90 && lat <= 90 && lon >= -180 && lon <= 180 && (lat != 0 || lon != 0)) {
    Settings_SetLocation(lat, lon);
  }
}

bool WiFi_ConnectOrPortal() {
  gfx->fillScreen(C_BLACK);
  UI_TextCentered("Connecting to WiFi...", LCD_HEIGHT / 2, C_WHITE, 2);
  { extern Arduino_GFX* gfx; gfx->flush(); }

  char latBuf[24], lonBuf[24];
  snprintf(latBuf, sizeof(latBuf), "%.5f", Settings_Lat());
  snprintf(lonBuf, sizeof(lonBuf), "%.5f", Settings_Lon());

  WiFiManager wm;
  wm.setConfigPortalTimeout(180);
  wm.setConnectTimeout(20);
  wm.setAPCallback(onAP);

  WiFiManagerParameter pLat("lat", "Latitude (lat)", latBuf, 24);
  WiFiManagerParameter pLon("lon", "Longitude (lon)", lonBuf, 24);
  wm.addParameter(&pLat);
  wm.addParameter(&pLon);
  wm.setSaveParamsCallback([&] { saveParams(pLat, pLon); });

  bool ok = wm.autoConnect(AP_SSID, apPass());
  if (ok) {
    saveParams(pLat, pLon);
    Serial.printf("WiFi ok, IP %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("WiFi not connected");
  }
  return ok;
}

void WiFi_StartPortal() {
  char latBuf[24], lonBuf[24];
  snprintf(latBuf, sizeof(latBuf), "%.5f", Settings_Lat());
  snprintf(lonBuf, sizeof(lonBuf), "%.5f", Settings_Lon());

  WiFiManager wm;
  wm.setConfigPortalTimeout(180);
  wm.setAPCallback(onAP);
  // Menu with an "Exit" button - lets you leave the portal without connecting/resetting.
  const char* menu[] = {"wifi", "info", "exit"};
  wm.setMenu(menu, 3);
  WiFiManagerParameter pLat("lat", "Latitude (lat)", latBuf, 24);
  WiFiManagerParameter pLon("lon", "Longitude (lon)", lonBuf, 24);
  wm.addParameter(&pLat);
  wm.addParameter(&pLon);
  wm.setSaveParamsCallback([&] { saveParams(pLat, pLon); });
  wm.startConfigPortal(AP_SSID, apPass());
  saveParams(pLat, pLon);
}

void WiFi_Loop() {
  if (WiFi.status() == WL_CONNECTED) return;
  unsigned long now = millis();
  if (now - s_lastReconnect < 15000) return;
  s_lastReconnect = now;
  WiFi.reconnect();
}

bool   WiFi_IsConnected() { return WiFi.status() == WL_CONNECTED; }
String WiFi_SSID() { return WiFi_IsConnected() ? WiFi.SSID() : String("(not connected)"); }
String WiFi_IP()   { return WiFi_IsConnected() ? WiFi.localIP().toString() : String("-"); }
void   WiFi_Reset() { WiFiManager wm; wm.resetSettings(); }
