#include "Touch_CST3530.h"

#include <Wire.h>

#include "cst3530_touch.hpp"

namespace {

constexpr uint32_t CST3530_END_READ_REG = 0xD00002AB;
constexpr uint32_t CST3530_DATA_REG = 0xD0070000;
constexpr uint32_t CST3530_COORD_NEXT_REG = 0xD0070900;

struct TouchState {
  uint8_t points = 0;
  Cst3530Point point[CST3530_LCD_TOUCH_MAX_POINTS];
};

TouchState touchState;

bool i2cProbe(uint8_t address) {
  Wire1.beginTransmission(address);
  return Wire1.endTransmission(true) == 0;
}

bool i2cRead(uint32_t reg, uint8_t* data, uint32_t length) {
  Wire1.beginTransmission(CST3530_ADDR);
  Wire1.write(static_cast<uint8_t>(reg >> 24));
  Wire1.write(static_cast<uint8_t>(reg >> 16));
  Wire1.write(static_cast<uint8_t>(reg >> 8));
  Wire1.write(static_cast<uint8_t>(reg));
  if (Wire1.endTransmission(false) != 0) return false;

  if (Wire1.requestFrom(CST3530_ADDR, length) != length) return false;
  for (uint32_t i = 0; i < length; ++i) data[i] = Wire1.read();
  return true;
}

void endRead() {
  Wire1.beginTransmission(CST3530_ADDR);
  Wire1.write(static_cast<uint8_t>(CST3530_END_READ_REG >> 24));
  Wire1.write(static_cast<uint8_t>(CST3530_END_READ_REG >> 16));
  Wire1.write(static_cast<uint8_t>(CST3530_END_READ_REG >> 8));
  Wire1.write(static_cast<uint8_t>(CST3530_END_READ_REG));
  Wire1.endTransmission(true);
}

void resetController() {
  digitalWrite(CST3530_RST_PIN, LOW);
  delay(100);
  digitalWrite(CST3530_RST_PIN, HIGH);
  delay(500);
}

}  // namespace

bool TOUCH2_Init() {
  Wire1.begin(CST3530_SDA_PIN, CST3530_SCL_PIN, CST3530_I2C_FREQ_HZ);
  pinMode(CST3530_INT_PIN, INPUT);
  pinMode(CST3530_RST_PIN, OUTPUT);
  resetController();

  if (!i2cProbe(CST3530_ADDR)) {
    Serial.printf("CST3530 not found at I2C address 0x%02X\r\n", CST3530_ADDR);
    return false;
  }

  Serial.println("CST3530 touch controller initialized");
  return true;
}

bool Touch2_Read_Data() {
  uint8_t report[9] = {0};
  touchState.points = 0;
  if (!i2cRead(CST3530_DATA_REG, report, sizeof(report))) return false;

  Cst3530Point point{};
  if (!cst3530ParseFirstPoint(report, sizeof(report), &point)) {
    endRead();
    return true;
  }

  const uint8_t count = report[3] & 0x0F;
  if (count > CST3530_LCD_TOUCH_MAX_POINTS) {
    endRead();
    return false;
  }

  touchState.point[0] = point;
  touchState.points = count;
  if (count > 1) {
    uint8_t extra[(CST3530_LCD_TOUCH_MAX_POINTS - 1) * 5] = {0};
    if (!i2cRead(CST3530_COORD_NEXT_REG, extra, (count - 1) * 5)) {
      touchState.points = 0;
      endRead();
      return false;
    }
    for (uint8_t i = 1; i < count; ++i) {
      const uint8_t* data = &extra[(i - 1) * 5];
      touchState.point[i].x =
          static_cast<uint16_t>(((data[3] & 0x0F) << 8) | data[0]);
      touchState.point[i].y =
          static_cast<uint16_t>(((data[3] & 0xF0) << 4) | data[1]);
      touchState.point[i].strength = data[2];
    }
  }

  endRead();
  return true;
}

bool Touch2_Get_XY(uint16_t* x, uint16_t* y, uint16_t* strength,
                   uint8_t* pointNum, uint8_t maxPointNum) {
  if (!x || !y || !pointNum || maxPointNum == 0) return false;

  const uint8_t count = min(touchState.points, maxPointNum);
  for (uint8_t i = 0; i < count; ++i) {
    x[i] = touchState.point[i].x;
    y[i] = touchState.point[i].y;
    if (strength) strength[i] = touchState.point[i].strength;
  }
  *pointNum = count;
  touchState.points = 0;
  return count > 0;
}
