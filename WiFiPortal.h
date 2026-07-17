// =============================================================================
//  MeteoPlaneRadar
//  WiFi portal - interface and AP settings.
//
//  Project: MeteoPlaneRadar - live aircraft radar on a round touchscreen
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
//  Web:     https://chiptron.cz
//  Board:   Waveshare ESP32-S3-Touch-LCD-2.1 (round 480x480 display, ST7701)
// =============================================================================
#pragma once
#include <Arduino.h>

#define AP_SSID     "MeteoPlaneRadar"
#define AP_PASSWORD ""            // "" = open network
#define PORTAL_IP   "192.168.4.1"

// Try the stored WiFi; on failure start the AP portal (draws instructions + QR).
bool WiFi_ConnectOrPortal();

// Force-start the portal from the settings screen.
void WiFi_StartPortal();

// Connection upkeep - call from loop().
void WiFi_Loop();

bool   WiFi_IsConnected();
String WiFi_SSID();
String WiFi_IP();
void   WiFi_Reset();
