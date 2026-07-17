// =============================================================================
//  MeteoPlaneRadar
//  TCA9554 I/O expander (LCD reset / CS / power control).
//
//  Project: MeteoPlaneRadar - live aircraft radar on a round touchscreen
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
//  Web:     https://chiptron.cz
//  Board:   Waveshare ESP32-S3-Touch-LCD-2.1 (round 480x480 display, ST7701)
// =============================================================================
#include "TCA9554.h"

// Shadow copy of the output register (we flip individual bits).
static uint8_t s_output = 0xFF;

static void writeReg(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(TCA9554_ADDR);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

static uint8_t readReg(uint8_t reg) {
  Wire.beginTransmission(TCA9554_ADDR);
  Wire.write(reg);
  Wire.endTransmission();
  Wire.requestFrom((int)TCA9554_ADDR, 1);
  return Wire.available() ? Wire.read() : 0xFF;
}

void TCA9554_Init() {
  // 0 = output for every pin
  writeReg(TCA9554_CONFIG_REG, 0x00);
  s_output = 0xFF;
  writeReg(TCA9554_OUTPUT_REG, s_output);
}

void TCA9554_SetPin(uint8_t pin, bool high) {
  // pin 1-8 -> bit 0-7
  uint8_t bit = pin - 1;
  if (high) s_output |= (1 << bit);
  else      s_output &= ~(1 << bit);
  writeReg(TCA9554_OUTPUT_REG, s_output);
}

uint8_t TCA9554_ReadOutput() {
  return readReg(TCA9554_OUTPUT_REG);
}
