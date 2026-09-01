// =============================================================================
//  MeteoPlaneRadar
//  CST820 capacitive touch controller.
//
//  Project: MeteoPlaneRadar - live aircraft radar on a round touchscreen
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
//  Web:     https://chiptron.cz
//  Board:   Waveshare ESP32-S3-Touch-LCD-2.1 (round 480x480 display, ST7701)
// =============================================================================
#include "Touch_CST820.h"
#include "TCA9554.h"
#include "Display_ST7701.h"   // LCD_WIDTH / LCD_HEIGHT
#include "Config.h"           // TOUCH_DEBUG
#include <Wire.h>

static bool readRegs(uint8_t reg, uint8_t* buf, size_t len) {
  Wire.beginTransmission(CST820_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  uint8_t got = Wire.requestFrom((int)CST820_ADDR, (int)len);
  if (got != len) { while (Wire.available()) Wire.read(); return false; }
  for (size_t i = 0; i < len; i++) buf[i] = Wire.read();
  return true;
}

// Register 0xA7 is the chip ID. Reading it is the whole probe: on this bus only
// the CST820 lives at 0x15, so an answer there settles which board we are on.
bool CST820_Probe() {
  uint8_t id = 0;
  return readRegs(0xA7, &id, 1);
}

// --- INT line --------------------------------------------------------------
// The controller pulls INT low when it has a touch report ready. Reading the
// registers at any other time gives stale or garbage data - and once the chip
// drops into standby it may not answer at all, which our old unconditional
// polling counted as an error. An interrupt is used rather than a level check
// because the pulse is short and would be missed while a frame is being drawn.
static volatile bool s_intFlag = false;
static bool          s_fingerDown = false;
static unsigned long s_lastPoll = 0;

static void IRAM_ATTR onTouchInt() { s_intFlag = true; }

static bool s_intAttached = false;

bool CST820_Init() {
  // Reset via EXIO2.
  TCA9554_SetPin(EXIO_TOUCH_RST, false);
  delay(10);
  TCA9554_SetPin(EXIO_TOUCH_RST, true);
  delay(50);

  pinMode(CST820_INT_PIN, INPUT_PULLUP);
#if TOUCH_USE_INT
  // Only detach what we actually attached - Touch_Init() may be called again at
  // runtime, but on the first call there is no handler yet and asking the GPIO
  // driver to remove one it never installed just prints a complaint.
  if (s_intAttached) detachInterrupt(digitalPinToInterrupt(CST820_INT_PIN));
  attachInterrupt(digitalPinToInterrupt(CST820_INT_PIN), onTouchInt, FALLING);
  s_intAttached = true;
  s_intFlag = false;
  s_fingerDown = false;
#endif

  // Try reading the chip ID (register 0xA7) as a communication test.
  uint8_t id = 0;
  if (readRegs(0xA7, &id, 1)) {
    Serial.printf("CST820 ID: 0x%02X\n", id);
    return true;
  }
  Serial.println("CST820 nereaguje na I2C");
  return false;
}

// The three checks in Touch_Read below are all that is left of a much larger
// apparatus that used to count bad samples and reset the controller when there
// were too many. That machinery is gone: it existed because we polled the chip
// unconditionally and mistook "the controller is asleep and not answering" for
// "the controller is broken". Reading only when INT says so removed the cause,
// and with it the only code that wrote to the I/O expander at runtime - which
// is what could switch the display off for good.

void CST820_Read(TouchData* out) {
  out->points = 0;

#if TOUCH_USE_INT
  // Read when: the chip raised an event, a finger is already down (we need the
  // movement samples and the release), or the fallback timer expired.
  unsigned long nowMs = millis();
  bool due = s_intFlag || s_fingerDown || (nowMs - s_lastPoll >= TOUCH_IDLE_POLL_MS);
  if (!due) return;
  s_intFlag = false;
  s_lastPoll = nowMs;
#endif

  uint8_t buf[6] = {};
  // Register 0x02 = number of points, followed by the coordinates.
  // A read that fails is NOT treated as a fault: a controller in standby simply
  // does not answer, which is normal behaviour, not a broken chip. And it must
  // NOT clear s_fingerDown - one dropped read in the middle of a drag would
  // stop the per-pass polling and the rest of the gesture would be lost.
  if (!readRegs(0x02, buf, 6)) return;

  // --- Sanity checks on the data itself -------------------------------------
  // The I2C transfer can succeed (the chip ACKs) and still hand back garbage,
  // typically all 0xFF. Decoded naively that is "15 points at (4095, 4095)",
  // which the UI then treats as a real tap somewhere off the map - and that is
  // exactly what kept closing the aircraft detail panel on its own.
  if (buf[0] == 0xFF && buf[1] == 0xFF && buf[2] == 0xFF) return;

  uint8_t points = buf[0] & 0x0F;
  if (points == 0) { s_fingerDown = false; return; }   // finger lifted
  if (points > 1) return;               // CST820 is a single-touch controller

  uint16_t x = ((buf[1] & 0x0F) << 8) | buf[2];
  uint16_t y = ((buf[3] & 0x0F) << 8) | buf[4];
  if (x >= LCD_WIDTH || y >= LCD_HEIGHT) return;   // outside the panel

  s_fingerDown = true;   // keep reading every pass until the finger lifts
  out->points = points;
  out->x = x;
  out->y = y;
}
