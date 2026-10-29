#pragma once

#include <stdint.h>

#include "Wire.h"
#include "../../src/gateway_orientation.hpp"

class Qmi8658 {
 public:
  bool begin(TwoWire& wire = Wire);
  bool beginOnInitializedBus(TwoWire& wire);
  bool readAcceleration(GatewayAccelerationSample* sample);

  static GatewayAccelerationSample decodeAcceleration(const uint8_t raw[6]);

 private:
  bool writeRegister(uint8_t reg, uint8_t value);
  bool readRegisters(uint8_t start_reg, uint8_t* buffer, uint8_t length);
  bool probeAddress(uint8_t address);

  TwoWire* wire_ = nullptr;
  uint8_t address_ = 0;
  bool initialized_ = false;
};
