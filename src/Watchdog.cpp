// =============================================================================
//  MeteoPlaneRadar
//  Hardware watchdog for 24/7 operation.
//
//  Project: MeteoPlaneRadar - live aircraft radar on a round touchscreen
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
//  Web:     https://chiptron.cz
//  Board:   Waveshare ESP32-S3-Touch-LCD-2.1 (round 480x480 display, ST7701)
// =============================================================================
#include "Watchdog.h"
#include "esp_task_wdt.h"

static bool s_subscribed = false;

void Watchdog_Begin() {
  // ESP32 core 3.x uses a config struct.
  esp_task_wdt_config_t cfg = {
    .timeout_ms = WDT_TIMEOUT_S * 1000,
    .idle_core_mask = 0,       // watch only our loop, not the idle tasks
    .trigger_panic = true,     // reboot on timeout
  };
  // Arduino already initialised the TWDT - just reconfigure it; if not, init.
  if (esp_task_wdt_reconfigure(&cfg) != ESP_OK) {
    esp_task_wdt_init(&cfg);
  }
  if (esp_task_wdt_add(NULL) == ESP_OK) s_subscribed = true;
}

void Watchdog_Feed() {
  if (s_subscribed) esp_task_wdt_reset();
}

void Watchdog_Suspend() {
  if (s_subscribed) { esp_task_wdt_delete(NULL); s_subscribed = false; }
}

void Watchdog_Resume() {
  if (!s_subscribed && esp_task_wdt_add(NULL) == ESP_OK) {
    s_subscribed = true;
    esp_task_wdt_reset();
  }
}
