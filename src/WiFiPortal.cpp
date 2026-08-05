// =============================================================================
//  MeteoPlaneRadar
//  WiFi connection + configuration AP portal with QR code.
//
//  Project: MeteoPlaneRadar - live aircraft radar on a round touchscreen
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
//           Ondra OK1CDJ / apps.ok1cdj.com
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
  UI_TextCentered("Naskenuj mobilem:", 78, C_GRAY, 1);

  // QR centred. Round panel - the QR must be smaller so it fits entirely.
  int qrSize = 190;
  int qrX = (LCD_WIDTH - qrSize) / 2;
  int qrY = 98;
  UI_DrawWifiQR(AP_SSID, AP_PASSWORD, /*open=*/true, qrX, qrY, qrSize);

  // Below the QR: SSID + password (centred).
  UI_TextCentered("MeteoPlaneRadar", 300, C_WHITE, 1);
  UI_TextCentered("bez hesla  |  pak 192.168.4.1", 322, C_GRAY, 1);
  UI_TextCentered("Zpet: tlacitko Exit v portalu (nebo pockej 3 min)", 344, C_GRAY, 1);

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

static void saveParams(WiFiManagerParameter& pLat, WiFiManagerParameter& pLon,
                       WiFiManagerParameter& pCall, WiFiManagerParameter& pKey) {
  double lat = atof(pLat.getValue());
  double lon = atof(pLon.getValue());
  if (lat >= -90 && lat <= 90 && lon >= -180 && lon <= 180 && (lat != 0 || lon != 0)) {
    Settings_SetLocation(lat, lon);
  }
  // APRS station + aprs.fi API key (only overwrite when the field is non-empty,
  // so leaving them blank in the form does not wipe a stored value).
  if (pCall.getValue()[0]) Settings_SetAprsCall(pCall.getValue());
  if (pKey.getValue()[0])  Settings_SetAprsKey(pKey.getValue());
}

bool WiFi_ConnectOrPortal() {
  gfx->fillScreen(C_BLACK);
  UI_TextCentered("Pripojuji k WiFi...", LCD_HEIGHT / 2, C_WHITE, 2);
  { extern Arduino_GFX* gfx; gfx->flush(); }

  char latBuf[24], lonBuf[24];
  snprintf(latBuf, sizeof(latBuf), "%.5f", Settings_Lat());
  snprintf(lonBuf, sizeof(lonBuf), "%.5f", Settings_Lon());

  WiFiManager wm;
  wm.setConfigPortalTimeout(PORTAL_TIMEOUT_S);
  wm.setConnectTimeout(20);
  wm.setAPCallback(onAP);

  WiFiManagerParameter pLat("lat", "Zeměpisná šířka (lat)", latBuf, 24);
  WiFiManagerParameter pLon("lon", "Zeměpisná délka (lon)", lonBuf, 24);
  WiFiManagerParameter pCall("aprscall", "APRS volací znak stanice", Settings_AprsCall(), 15);
  WiFiManagerParameter pKey("aprskey", "aprs.fi API klíč", Settings_AprsKey(), 23);
  wm.addParameter(&pLat);
  wm.addParameter(&pLon);
  wm.addParameter(&pCall);
  wm.addParameter(&pKey);
  wm.setSaveParamsCallback([&] { saveParams(pLat, pLon, pCall, pKey); });

  bool ok = wm.autoConnect(AP_SSID, apPass());
  if (ok) {
    saveParams(pLat, pLon, pCall, pKey);
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
  wm.setConfigPortalTimeout(PORTAL_TIMEOUT_S);
  wm.setAPCallback(onAP);
  // Menu with an "Exit" button - lets you leave the portal without connecting/resetting.
  const char* menu[] = {"wifi", "info", "exit"};
  wm.setMenu(menu, 3);
  WiFiManagerParameter pLat("lat", "Zeměpisná šířka (lat)", latBuf, 24);
  WiFiManagerParameter pLon("lon", "Zeměpisná délka (lon)", lonBuf, 24);
  WiFiManagerParameter pCall("aprscall", "APRS volací znak stanice", Settings_AprsCall(), 15);
  WiFiManagerParameter pKey("aprskey", "aprs.fi API klíč", Settings_AprsKey(), 23);
  wm.addParameter(&pLat);
  wm.addParameter(&pLon);
  wm.addParameter(&pCall);
  wm.addParameter(&pKey);
  wm.setSaveParamsCallback([&] { saveParams(pLat, pLon, pCall, pKey); });
  wm.startConfigPortal(AP_SSID, apPass());
  saveParams(pLat, pLon, pCall, pKey);
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
