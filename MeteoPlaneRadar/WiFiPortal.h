// =============================================================================
//  MeteoPlaneRadar
//  WiFi - connect, or put up an access point until someone tells us a network.
//
//  WiFiManager is gone as of 0.6.0. Two reasons: its portal is English-only and
//  cannot be translated, and it blocks the whole sketch while it runs - which
//  meant suspending the watchdog and freezing the display. The replacement is a
//  plain access point plus the project's own web page, so the portal speaks the
//  same two languages as the rest of the device and the main loop keeps
//  running the whole time.
//
//  On a first run the access point has no timeout: without a network the
//  device cannot fetch anything, so there is nothing to go back to.
//
//  With networks remembered it does not wait forever, though. The access point
//  then usually means the router was not up yet when the device booted (after
//  a power cut or a watchdog restart), so every two minutes the device looks
//  for a remembered network and joins it, and after 30 minutes with nobody on
//  the access point it restarts. See WIFI_AP_RETRY_MS in Config.h.
//
//  Project: MeteoPlaneRadar - live aircraft radar on a round touchscreen
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
//  Web:     https://chiptron.cz
// =============================================================================
#pragma once
#include <Arduino.h>
#include "Config.h"   // AP_SSID / AP_PASSWORD
#define PORTAL_IP "192.168.4.1"

// Connect with the stored credentials, or bring up the access point. Blocks
// only for the initial connection attempt; everything after that is in Loop().
void WiFi_Begin();

// Connection upkeep and the handover from the portal. Call from loop().
void WiFi_Loop();

bool   WiFi_IsConnected();
bool   WiFi_IsAP();
String WiFi_SSID();
String WiFi_IP();

// Forget the network and go back to the access point.
void WiFi_Reset();

// Redraw the access-point instructions (SSID, QR code, address).
void WiFi_DrawApScreen();
