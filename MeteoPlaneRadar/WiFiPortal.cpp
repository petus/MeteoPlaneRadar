// =============================================================================
//  MeteoPlaneRadar
//  WiFi connection and the configuration access point. See WiFiPortal.h.
//
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
// =============================================================================
#include "AAFont.h"   // CZT(): accents with the smooth fonts
#include "WiFiPortal.h"
#include "WebConfig.h"
#include "Settings.h"
#include "Lang.h"
#include "UI.h"
#include "Display_ST7701.h"
#include "Watchdog.h"

#include <WiFi.h>
#include <string.h>

static bool s_ap = false;
static unsigned long s_lastReconnect = 0;
static unsigned long s_lostSince = 0;      // when the link went down, 0 = up
static int  s_slot = -1;                   // which remembered network we are on
static unsigned long s_apLastClient = 0;   // last time anyone was on the access point
static unsigned long s_apNextRetry = 0;    // next look for a remembered network

bool   WiFi_IsAP()        { return s_ap; }
bool   WiFi_IsConnected() { return WiFi.status() == WL_CONNECTED; }
String WiFi_SSID() {
  if (WiFi_IsConnected()) return WiFi.SSID();
  return String(Settings_WifiSsid(s_slot >= 0 ? (uint8_t)s_slot : 0));
}
String WiFi_IP()   { return WiFi_IsConnected() ? WiFi.localIP().toString()
                                               : (s_ap ? WiFi.softAPIP().toString() : String("-")); }

// --- Screens ----------------------------------------------------------------
void WiFi_DrawApScreen() {
  gfx->fillScreen(C_BLACK);

  UI_TextCentered("MeteoPlaneRadar", 34, C_CYAN, 2);
  UI_TextCentered("chiptron.cz", 58, C_GRAY, 1);
  UI_TextCentered(LT("Naskenujte mobilem:", "Naskenujte mobilem:", "Scan with your phone:",
                     "Naskenujte mobilom:", "Naskenujte mobilom:"), 78, C_GRAY, 1);

  const int qrSize = 190;
  UI_DrawWifiQR(AP_SSID, AP_PASSWORD, /*open=*/true,
                (LCD_WIDTH - qrSize) / 2, 98, qrSize);

  UI_TextCentered(AP_SSID, 300, C_WHITE, 1);
  UI_TextCentered(LT("bez hesla  |  pak otevrete 192.168.4.1", "bez hesla  |  pak otevřete 192.168.4.1",
                     "no password  |  then open 192.168.4.1",
                     "bez hesla  |  potom otvorte 192.168.4.1", "bez hesla  |  potom otvorte 192.168.4.1"),
                  322, C_GRAY, 1);
  UI_TextCentered(LT("Cekam na zadani site...", "Čekám na zadání sítě...", "Waiting for your network...",
                     "Cakam na zadanie siete...", "Čakám na zadanie siete..."), 348, C_YELLOW, 1);
  gfx->flush();
}

static void drawConnecting(const char* ssid) {
  gfx->fillScreen(C_BLACK);
  UI_TextCentered(LT("Pripojuji k WiFi...", "Připojuji k WiFi...", "Connecting to WiFi...",
                     "Pripajam sa k WiFi...", "Pripájam sa k WiFi..."),
                  LCD_HEIGHT / 2 - 20, C_WHITE, 2);
  if (ssid && *ssid) UI_TextCentered(ssid, LCD_HEIGHT / 2 + 12, C_CYAN, 2);
  gfx->flush();
}

// --- Access point -----------------------------------------------------------
static void startAP() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_AP);
  const char* pass = (strlen(AP_PASSWORD) == 0) ? nullptr : AP_PASSWORD;
  WiFi.softAP(AP_SSID, pass);
  delay(200);
  s_ap = true;
  s_apLastClient = millis();
  s_apNextRetry = millis() + WIFI_AP_RETRY_MS;
  Serial.printf("WiFi: pristupovy bod %s, http://%s/\n",
                AP_SSID, WiFi.softAPIP().toString().c_str());
  WebConfig_Begin(true);
  WiFi_DrawApScreen();
}

// One connection attempt to one remembered network. Blocking, but bounded -
// and the watchdog is fed throughout, so a slow router cannot cause a reboot.
// show = paint the "connecting" screen (not when switching in the background,
// where the data screens should stay up).
static bool tryConnectSlot(int slot, uint32_t timeoutMs, bool show) {
  const char* ssid = Settings_WifiSsid((uint8_t)slot);
  if (!ssid || !*ssid) return false;
  if (show) drawConnecting(ssid);

  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.disconnect(false);
  WiFi.begin(ssid, Settings_WifiPass((uint8_t)slot));

  const unsigned long start = millis();
  while (millis() - start < timeoutMs) {
    if (WiFi.status() == WL_CONNECTED) {
      s_ap = false;
      s_slot = slot;
      s_lostSince = 0;
      Serial.printf("WiFi ok (%s), IP %s\n", ssid, WiFi.localIP().toString().c_str());
      WebConfig_Begin(false);
      return true;
    }
    Watchdog_Feed();
    delay(100);
  }
  Serial.printf("WiFi: pripojeni k %s se nepovedlo\n", ssid);
  return false;
}

// Try the remembered networks, the ones actually in range first and the
// strongest of those before the rest. Networks the scan did not see are still
// tried at the end - a hidden SSID never shows up in a scan - unless onlySeen:
// in the background, with the data screens frozen while this blocks, a network
// that is not even there is not worth twelve seconds.
static bool connectAny(bool show, int skipSlot, bool onlySeen) {
  if (!Settings_HasWifi()) return false;
  if (show) drawConnecting("");
  int order[WIFI_SLOTS];
  int rssi[WIFI_SLOTS];
  int n = 0;
  for (int i = 0; i < WIFI_SLOTS; i++) rssi[i] = -1000;

  WiFi.mode(WIFI_STA);
  const int found = WiFi.scanNetworks();
  for (int k = 0; k < found; k++) {
    for (int i = 0; i < WIFI_SLOTS; i++) {
      const char* ss = Settings_WifiSsid((uint8_t)i);
      if (*ss && WiFi.SSID(k) == ss && WiFi.RSSI(k) > rssi[i]) rssi[i] = WiFi.RSSI(k);
    }
  }
  WiFi.scanDelete();
  Watchdog_Feed();

  // In range, strongest first...
  for (int pass = 0; pass < (onlySeen ? 1 : 2); pass++) {
    for (;;) {
      int best = -1;
      for (int i = 0; i < WIFI_SLOTS; i++) {
        if (!*Settings_WifiSsid((uint8_t)i) || i == skipSlot) continue;
        bool used = false;
        for (int j = 0; j < n; j++) if (order[j] == i) used = true;
        if (used) continue;
        const bool seen = rssi[i] > -1000;
        if (pass == 0 && !seen) continue;          // ...then the unseen ones
        if (best < 0 || rssi[i] > rssi[best]) best = i;
      }
      if (best < 0) break;
      order[n++] = best;
    }
  }
  for (int j = 0; j < n; j++) {
    if (tryConnectSlot(order[j], WIFI_TRY_MS, show)) return true;
  }
  return false;
}

// Kept for the portal: connect to whichever network was just entered (it is
// slot 0 - Settings_SetWifi() puts a new one first), then the rest.
static bool tryConnect(uint32_t timeoutMs) {
  if (tryConnectSlot(0, timeoutMs, true)) return true;
  return connectAny(true, 0, false);
}

// Access point up although networks are remembered (see WIFI_AP_RETRY_MS in
// Config.h). Nothing happens while someone is connected to the access point -
// they are probably entering a network right now, and a scan would hop
// channels under them.
static void apUpkeep() {
  if (!Settings_HasWifi()) return;                // first run: wait for the user
  const unsigned long now = millis();
  if (WiFi.softAPgetStationNum() > 0) {
    s_apLastClient = now;
    s_apNextRetry = now + WIFI_AP_RETRY_MS;
    return;
  }
  if (now - s_apLastClient >= WIFI_AP_RESTART_MS) {
    Serial.println("WiFi: pristupovy bod 30 min bez obsluhy, restartuji");
    Serial.flush();
    delay(200);
    ESP.restart();
  }
  if ((long)(now - s_apNextRetry) < 0) return;
  s_apNextRetry = now + WIFI_AP_RETRY_MS;

  // Scan with the access point still up, so the QR code keeps working.
  WiFi.mode(WIFI_AP_STA);
  const int found = WiFi.scanNetworks();
  bool seen = false;
  for (int k = 0; k < found && !seen; k++)
    for (int i = 0; i < WIFI_SLOTS; i++) {
      const char* ss = Settings_WifiSsid((uint8_t)i);
      if (*ss && WiFi.SSID(k) == ss) { seen = true; break; }
    }
  WiFi.scanDelete();
  Watchdog_Feed();
  if (!seen) { WiFi.mode(WIFI_AP); return; }

  Serial.println("WiFi: ulozena sit je zpet v dosahu, pripojuji");
  if (connectAny(true, -1, true)) return;
  startAP();                                      // not this time - QR back up
}

void WiFi_Begin() {
  if (Settings_HasWifi() && connectAny(true, -1, false)) return;
  startAP();
}

void WiFi_Loop() {
  // New credentials arrived from the portal.
  if (WebConfig_WantsWifiConnect()) {
    WebConfig_ClearWifiConnect();
    if (tryConnect(20000)) {
      // Connected - the portal has done its job and the full settings page is
      // now served on the home network instead.
      return;
    }
    // Wrong password, out of range, router asleep: back to the access point so
    // the user can simply try again. Their entry is kept in the form.
    startAP();
    return;
  }

  if (s_ap) { apUpkeep(); return; }

  if (WiFi.status() == WL_CONNECTED) { s_lostSince = 0; return; }
  unsigned long now = millis();
  if (s_lostSince == 0) s_lostSince = now;

  // Down for a while and there are other networks to try: the device was
  // probably carried somewhere else, or this router is not coming back soon.
  if (now - s_lostSince >= WIFI_SWITCH_AFTER_MS) {
    int others = 0;
    for (int i = 0; i < WIFI_SLOTS; i++)
      if (i != s_slot && *Settings_WifiSsid((uint8_t)i)) others++;
    if (others > 0) {
      Serial.println("WiFi: zkousim ostatni ulozene site");
      if (connectAny(false, -1, true)) return;
    }
    s_lostSince = millis();                // another full wait before the next round
    return;
  }

  if (now - s_lastReconnect < 15000) return;
  s_lastReconnect = now;
  Serial.println("WiFi: spojeni ztraceno, zkousim znovu");
  WiFi.reconnect();
}

void WiFi_Reset() {
  Settings_ClearWifi();
  WiFi.disconnect(true, true);
  startAP();
}
