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

bool Touch_Init() {
  // Reset via EXIO2.
  TCA9554_SetPin(EXIO_TOUCH_RST, false);
  delay(10);
  TCA9554_SetPin(EXIO_TOUCH_RST, true);
  delay(50);

  pinMode(CST820_INT_PIN, INPUT_PULLUP);

  // Try reading the chip ID (register 0xA7) as a communication test.
  uint8_t id = 0;
  if (readRegs(0xA7, &id, 1)) {
    Serial.printf("CST820 ID: 0x%02X\n", id);
    return true;
  }
  Serial.println("CST820 nereaguje na I2C");
  return false;
}

void Touch_Read(TouchData* out) {
  out->points = 0;
  uint8_t buf[6] = {};
  // Register 0x02 = number of points, followed by the coordinates.
  if (!readRegs(0x02, buf, 6)) return;
  uint8_t points = buf[0] & 0x0F;
  if (points == 0) return;
  out->points = points;
  out->x = ((buf[1] & 0x0F) << 8) | buf[2];
  out->y = ((buf[3] & 0x0F) << 8) | buf[4];
}
