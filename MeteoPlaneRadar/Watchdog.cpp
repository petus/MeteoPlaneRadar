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
void Watchdog_LoopStart();
const char* Watchdog_StuckWhat();

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
  Watchdog_LoopStart();
  if (const char* w = Watchdog_StuckWhat())
    Serial.printf("WATCHDOG: minuly restart po zaseknuti (%s)\n", w);
}

void Watchdog_Feed() {
  if (s_subscribed) esp_task_wdt_reset();
}

// --- 0.9.2: stuck-loop guard ------------------------------------------------------
#include "esp_attr.h"
#include "esp_system.h"

// RTC memory is not cleared by a watchdog restart, so the note survives it.
#define STUCK_MAGIC 0x5754434BUL   // "WTCK"
static RTC_NOINIT_ATTR uint32_t s_stuckMagic;
static RTC_NOINIT_ATTR char     s_stuckWhat[24];

static unsigned long s_passStart = 0;
static bool          s_stuckLogged = false;
static char          s_lastStuck[24] = "";

void Watchdog_LoopStart() {
  s_passStart = millis();
  s_stuckLogged = false;
}

void Watchdog_PollFeed(const char* what) {
  if (millis() - s_passStart < WDT_LOOP_MAX_MS) { Watchdog_Feed(); return; }
  if (s_stuckLogged) return;
  s_stuckLogged = true;
  strncpy(s_stuckWhat, (what && *what) ? what : "?", sizeof(s_stuckWhat) - 1);
  s_stuckWhat[sizeof(s_stuckWhat) - 1] = '\0';
  s_stuckMagic = STUCK_MAGIC;
  Serial.printf("WATCHDOG: smycka visi %lu s (%s), nechavam restartovat\n",
                (millis() - s_passStart) / 1000UL, s_stuckWhat);
}

// Read once and cleared, so a later, ordinary restart does not repeat it.
// Watchdog_Begin() calls it early, before anything can set a new note.
const char* Watchdog_StuckWhat() {
  static bool read = false;
  if (!read) {
    read = true;
    const esp_reset_reason_t r = esp_reset_reason();
    const bool wdt = (r == ESP_RST_TASK_WDT || r == ESP_RST_INT_WDT || r == ESP_RST_WDT);
    if (s_stuckMagic == STUCK_MAGIC && wdt) {
      strncpy(s_lastStuck, s_stuckWhat, sizeof(s_lastStuck) - 1);
      s_lastStuck[sizeof(s_lastStuck) - 1] = '\0';
    }
    s_stuckMagic = 0;                     // one report per restart
  }
  return s_lastStuck[0] ? s_lastStuck : nullptr;
}
