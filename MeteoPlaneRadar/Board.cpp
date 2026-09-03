// =============================================================================
//  MeteoPlaneRadar
//  Board detection (see Board.h for why this is possible at all).
//
//  Project: MeteoPlaneRadar - live aircraft radar on a round touchscreen
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
//  Web:     https://chiptron.cz
// =============================================================================
#include "Board.h"
#include "Config.h"
#include "TCA9554.h"
#include "Touch_CST820.h"
#include "Touch_GT911.h"
#include <Wire.h>
#include "esp_log.h"

static BoardModel s_model    = BOARD_FALLBACK;
static bool       s_detected = false;

BoardModel  Board_Model()    { return s_model; }
bool        Board_Detected() { return s_detected; }

const char* Board_Name() {
  switch (s_model) {
    case BOARD_LCD_2_1:  return "2.1";
    case BOARD_LCD_2_8C: return "2.8C";
    default:             return "?";
  }
}

// A plain reset of whatever touch controller is on the board: hold EXIO2 low,
// let it go, wait for the chip to boot. Both controllers are happy with this;
// what it deliberately does NOT do is touch the INT pin (see below).
static void plainTouchReset() {
  TCA9554_SetPin(EXIO_TOUCH_RST, false);
  delay(10);
  TCA9554_SetPin(EXIO_TOUCH_RST, true);
  delay(60);
}

// Probing means asking addresses that are not there, and on the board that is
// not there the ESP-IDF I2C driver logs three lines of "transaction failed" per
// unanswered probe - eighteen lines of red before the one line that matters. A
// NACK is the answer here, not a fault, so the driver is told to keep quiet for
// the duration and put back afterwards.
struct QuietI2C {
  QuietI2C()  { esp_log_level_set("i2c.master", ESP_LOG_NONE);  }
  ~QuietI2C() { esp_log_level_set("i2c.master", ESP_LOG_ERROR); }
};

BoardModel Board_Detect() {
#if BOARD_FORCE == 21
  s_model = BOARD_LCD_2_1;  s_detected = true;
  Serial.println("Deska: 2.1 (vynuceno BOARD_FORCE)");
  return s_model;
#elif BOARD_FORCE == 28
  s_model = BOARD_LCD_2_8C; s_detected = true;
  Serial.println("Deska: 2.8C (vynuceno BOARD_FORCE)");
  return s_model;
#else

  // --- Step 1: is there a CST820 at 0x15? -----------------------------------
  //
  // The CST820 is asked first, and asked several times, for one reason: the
  // GT911 probe in step 2 has to drive the INT pin low, and INT is an OUTPUT of
  // the CST820. Driving it would put two outputs on one wire. So on a 2.1 board
  // we want to be done before we ever get there, and a single dropped I2C read
  // must not be what sends us into the contending path.
  //
  // Several tries are needed anyway: the chip answers reliably only in the
  // window after a reset, and it puts itself into standby when nothing is
  // happening (which is why Touch_CST820.cpp treats silence as normal, not as a
  // fault). ~300 ms of asking covers the boot of either controller.
  QuietI2C quiet;

  plainTouchReset();
  for (int i = 0; i < 6; i++) {
    if (CST820_Probe()) {
      s_model = BOARD_LCD_2_1;
      s_detected = true;
      Serial.println("Deska: ESP32-S3-Touch-LCD-2.1 (ST7701 + CST820)");
      return s_model;
    }
    delay(50);
  }

  // --- Step 2: a GT911, then? -----------------------------------------------
  //
  // The GT911 picks its own I2C address from the level on INT while it comes
  // out of reset: low -> 0x5D, high -> 0x14. That is why this probe cannot be
  // a plain bus scan - the chip is not at a fixed address until we have put it
  // there. GT911_Reset() drives INT low across the reset and releases it
  // afterwards, which lands the chip on 0x5D (the address Waveshare's own board
  // support uses). 0x14 is tried as well, in case a board revision wires the
  // INT pull-up differently.
  if (GT911_Reset() && GT911_Probe()) {
    s_model = BOARD_LCD_2_8C;
    s_detected = true;
    Serial.printf("Deska: ESP32-S3-Touch-LCD-2.8C (ST7701 + GT911 na 0x%02X)\n",
                  GT911_Address());
    return s_model;
  }

  // --- Nothing answered -----------------------------------------------------
  // Carry on with the fallback so the screens still work; only the touch is
  // lost. Say it loudly, because from the outside a board that shows the wrong
  // panel init looks like a hardware fault rather than a failed probe.
  s_model = BOARD_FALLBACK;
  s_detected = false;
  Serial.printf("VAROVANI: dotykovy radic neodpovedel (ani CST820 na 0x%02X, ani "
                "GT911 na 0x%02X/0x%02X).\n"
                "          Pokracuji jako deska %s - pokud je to spatne, nastavte "
                "BOARD_FORCE v Config.h.\n",
                CST820_ADDR, GT911_ADDR_MAIN, GT911_ADDR_ALT, Board_Name());
  return s_model;
#endif
}
