// =============================================================================
//  MeteoPlaneRadar
//  GT911 capacitive touch controller (ESP32-S3-Touch-LCD-2.8C).
//
//  Project: MeteoPlaneRadar - live aircraft radar on a round touchscreen
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
//  Web:     https://chiptron.cz
//  Board:   Waveshare ESP32-S3-Touch-LCD-2.8C (round 480x480 display, ST7701)
//
//  Deliberately shaped like Touch_CST820.cpp: same INT-driven polling, same
//  "silence is not a fault" rule, same sanity checks on the decoded sample. The
//  two chips are nothing alike on the wire - 16-bit register addresses here, a
//  buffer-ready flag that has to be cleared by hand, five points instead of one
//  - but the failure modes they produce are the same, so the defences are too.
// =============================================================================
#include "Touch_GT911.h"
#include "TCA9554.h"
#include "Display_ST7701.h"   // LCD_WIDTH / LCD_HEIGHT
#include "Config.h"           // TOUCH_USE_INT, TOUCH_IDLE_POLL_MS
#include <Wire.h>

static uint8_t s_addr = 0;    // 0 = we have not found the chip yet

// --- I2C, 16-bit register addresses -----------------------------------------
static bool readAt(uint8_t addr, uint16_t reg, uint8_t* buf, size_t len) {
  Wire.beginTransmission(addr);
  Wire.write((uint8_t)(reg >> 8));
  Wire.write((uint8_t)(reg & 0xFF));
  if (Wire.endTransmission(false) != 0) return false;
  uint8_t got = Wire.requestFrom((int)addr, (int)len);
  if (got != len) { while (Wire.available()) Wire.read(); return false; }
  for (size_t i = 0; i < len; i++) buf[i] = Wire.read();
  return true;
}

static bool readRegs(uint16_t reg, uint8_t* buf, size_t len) {
  if (!s_addr) return false;
  return readAt(s_addr, reg, buf, len);
}

static bool writeReg(uint16_t reg, uint8_t val) {
  if (!s_addr) return false;
  Wire.beginTransmission(s_addr);
  Wire.write((uint8_t)(reg >> 8));
  Wire.write((uint8_t)(reg & 0xFF));
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

uint8_t GT911_Address() { return s_addr; }

// --- Reset with the address pinned ------------------------------------------
// The GT911 samples INT as it comes out of reset and takes 0x5D if it was low.
// Hence INT is briefly an output here - the one moment in the whole firmware
// when the ESP32 drives that wire. It is released the moment the chip has
// latched, and from then on it is the GT911's output again, as it should be.
bool GT911_Reset() {
  pinMode(GT911_INT_PIN, OUTPUT);
  digitalWrite(GT911_INT_PIN, LOW);
  delay(10);

  bool ok = TCA9554_SetPin(EXIO_TOUCH_RST, false);
  delay(10);
  ok = TCA9554_SetPin(EXIO_TOUCH_RST, true) && ok;

  // The datasheet wants INT held for >5 ms after reset is released; Waveshare's
  // own board support waits 200 ms and there is no reason to be braver.
  delay(200);
  digitalWrite(GT911_INT_PIN, HIGH);   // release cleanly, not through a glitch
  delay(1);
  pinMode(GT911_INT_PIN, INPUT_PULLUP);
  delay(50);                            // chip finishes booting
  return ok;
}

// --- Who is out there? ------------------------------------------------------
// A successful read at either candidate address is taken as a yes, and the ID
// is printed rather than insisted upon. Nothing else on this board lives at
// 0x5D or 0x14 (the expander is at 0x20, the CST820 at 0x15), so an answer
// there is already conclusive - and refusing a panel because a clone spells its
// product ID differently would cost the user the whole display, not just the
// touch, since the panel init is chosen from this answer.
bool GT911_Probe() {
  const uint8_t cand[2] = { GT911_ADDR_MAIN, GT911_ADDR_ALT };
  for (uint8_t i = 0; i < 2; i++) {
    uint8_t id[4] = {};
    if (!readAt(cand[i], GT911_REG_PRODUCT_ID, id, 4)) continue;
    s_addr = cand[i];
    Serial.printf("GT911 na 0x%02X, ID: %c%c%c%c\n", s_addr,
                  isprint(id[0]) ? id[0] : '.', isprint(id[1]) ? id[1] : '.',
                  isprint(id[2]) ? id[2] : '.', isprint(id[3]) ? id[3] : '.');
    return true;
  }
  return false;
}

// --- INT line ---------------------------------------------------------------
static volatile bool s_intFlag = false;
static bool          s_fingerDown = false;
static unsigned long s_lastPoll = 0;
static unsigned long s_lastReport = 0;

static void IRAM_ATTR onTouchInt() { s_intFlag = true; }

static bool s_intAttached = false;

bool GT911_Init() {
  // Board_Detect() has normally done both of these already; repeating them is
  // harmless and keeps this callable on its own (Touch_Init may be called again
  // at runtime).
  if (!s_addr) {
    GT911_Reset();
    if (!GT911_Probe()) {
      Serial.println("GT911 nereaguje na I2C");
      return false;
    }
  }

  pinMode(GT911_INT_PIN, INPUT_PULLUP);
#if TOUCH_USE_INT
  // Only detach what we actually attached - Touch_Init() may be called again at
  // runtime, but on the first call there is no handler yet and asking the GPIO
  // driver to remove one it never installed just prints a complaint.
  if (s_intAttached) detachInterrupt(digitalPinToInterrupt(GT911_INT_PIN));
  attachInterrupt(digitalPinToInterrupt(GT911_INT_PIN), onTouchInt, FALLING);
  s_intAttached = true;
  s_intFlag = false;
  s_fingerDown = false;
#endif
  return true;
}

void GT911_Read(TouchData* out) {
  out->points = 0;
  if (!s_addr) return;

#if TOUCH_USE_INT
  unsigned long nowMs = millis();
  bool due = s_intFlag || s_fingerDown || (nowMs - s_lastPoll >= TOUCH_IDLE_POLL_MS);
  if (!due) return;
  s_intFlag = false;
  s_lastPoll = nowMs;

  // A finger that is down but has stopped producing reports would otherwise
  // keep us doing an I2C transfer every 5 ms for ever - the GT911 only raises
  // the ready flag when it has something new, so one lost report is enough.
  // The gesture itself has long since ended upstairs (TOUCH_RELEASE_MS).
  if (s_fingerDown && nowMs - s_lastReport > 300) s_fingerDown = false;
#endif

  // Bit 7 of the status register means "a report is waiting"; the low nibble is
  // how many points it holds. Reading the coordinates at any other time gives
  // whatever was left in the buffer from last time.
  uint8_t st = 0;
  if (!readRegs(GT911_REG_STATUS, &st, 1)) {
#if TOUCH_DEBUG
    static unsigned long tErr = 0;
    if (millis() - tErr > 1000) { tErr = millis(); Serial.println("GT911: cteni 0x814E selhalo"); }
#endif
    return;                                          // standby - not a fault
  }
#if TOUCH_DEBUG
  { static unsigned long tTick = 0; static uint32_t nPoll = 0; nPoll++;
    if (millis() - tTick > 1000) {
      tTick = millis();
      Serial.printf("GT911: %lu cteni/s, posledni 0x814E=0x%02X, INT=%d\n",
                    (unsigned long)nPoll, st, digitalRead(GT911_INT_PIN));
      nPoll = 0;
    } }
#endif
  if (!(st & 0x80)) return;

  uint8_t n = st & 0x0F;
  uint8_t buf[6] = {};
  bool got = (n >= 1 && n <= 5) && readRegs(GT911_REG_POINT1, buf, sizeof(buf));

  // Clear the flag whatever happened, including for a nonsense count. Leave it
  // set and the controller stops refilling the buffer, which reads from up here
  // as a touch that died mid-gesture.
  writeReg(GT911_REG_STATUS, 0x00);

  s_lastReport = millis();
#if TOUCH_DEBUG
  Serial.printf("GT911: st=0x%02X n=%u  raw=%02X %02X %02X %02X %02X %02X\n",
                st, n, buf[0], buf[1], buf[2], buf[3], buf[4], buf[5]);
#endif
  if (n == 0) { s_fingerDown = false; return; }      // finger lifted
  if (!got) return;

  // The GT911 reports up to five points; the UI is built around one. The first
  // point is the one the user started the gesture with, so a second finger
  // landing on the glass no longer cancels a swipe - it is simply ignored.
  uint16_t x = (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
  uint16_t y = (uint16_t)buf[2] | ((uint16_t)buf[3] << 8);
  if (x >= LCD_WIDTH || y >= LCD_HEIGHT) {
#if TOUCH_DEBUG
    Serial.printf("GT911: bod (%u,%u) je mimo panel %dx%d - zahozeno\n",
                  x, y, LCD_WIDTH, LCD_HEIGHT);
#endif
    return;                                          // outside the panel
  }

  s_fingerDown = true;
  out->points = 1;
  out->x = x;
  out->y = y;
}
