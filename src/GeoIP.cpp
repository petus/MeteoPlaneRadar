// =============================================================================
//  MeteoPlaneRadar
//  Automatic location detection from the public IP address.
//
//  Project: MeteoPlaneRadar - live aircraft radar on a round touchscreen
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
//  Web:     https://chiptron.cz
//  Board:   Waveshare ESP32-S3-Touch-LCD-2.1 (round 480x480 display, ST7701)
// =============================================================================
#include "GeoIP.h"
#include "Settings.h"

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// Free IP geolocation, no API key. Returns lat/lon based on the public IP.
#define GEOIP_URL "http://ip-api.com/json/?fields=status,lat,lon,city"

bool GeoIP_DetectIfNeeded() {
  // A user-entered location wins - never overwrite it.
  if (Settings_HasLocation()) return false;
  if (WiFi.status() != WL_CONNECTED) return false;

  HTTPClient http;
  http.setTimeout(6000);
  if (!http.begin(GEOIP_URL)) return false;
  int code = http.GET();
  if (code != HTTP_CODE_OK) { http.end(); Serial.printf("GeoIP: HTTP %d\n", code); return false; }

  String payload = http.getString();
  http.end();

  JsonDocument doc;
  if (deserializeJson(doc, payload)) { Serial.println("GeoIP: JSON error"); return false; }
  if (String(doc["status"] | "") != "success") { Serial.println("GeoIP: failed"); return false; }

  double lat = doc["lat"] | 0.0;
  double lon = doc["lon"] | 0.0;
  // Sanity check (Europe-ish) so a one-off glitch cannot throw the radar off.
  if (lat < 35.0 || lat > 72.0 || lon < -25.0 || lon > 45.0) {
    Serial.println("GeoIP: outside the expected range, ignoring");
    return false;
  }

  Settings_SetLocation(lat, lon);
  Serial.printf("GeoIP: location from IP: %.4f, %.4f (%s)\n",
                lat, lon, (const char*)(doc["city"] | "?"));
  return true;
}
