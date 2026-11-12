#pragma once
// Why the chip last restarted. A test sets g_simResetReason to play a crash.
typedef enum { ESP_RST_UNKNOWN, ESP_RST_POWERON, ESP_RST_SW, ESP_RST_PANIC, ESP_RST_TASK_WDT } esp_reset_reason_t;
inline esp_reset_reason_t g_simResetReason = ESP_RST_POWERON;
static inline esp_reset_reason_t esp_reset_reason() { return g_simResetReason; }
