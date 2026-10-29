#include "Qmi8658.hpp"

namespace {
constexpr int kQmiSdaPin = 11;
constexpr int kQmiSclPin = 10;
constexpr uint32_t kQmiFrequencyHz = 400000;

constexpr uint8_t kQmiAddressPrimary = 0x6A;
constexpr uint8_t kQmiAddressSecondary = 0x6B;

constexpr uint8_t kRegisterCtrl1 = 0x02;
constexpr uint8_t kRegisterCtrl2 = 0x03;
constexpr uint8_t kRegisterCtrl7 = 0x08;
constexpr uint8_t kRegisterAxL = 0x35;

constexpr uint8_t kCtrl1AutoIncrement = 0x40;
constexpr uint8_t kCtrl2Accel4g120Hz = 0x16;
constexpr uint8_t kCtrl7AccelOnly = 0x41;

constexpr float kAccelerationScale = 8192.0f;

int16_t decodeAxis(const uint8_t low, const uint8_t high) {
  return static_cast<int16_t>((static_cast<uint16_t>(high) << 8) | low);
}
}  // namespace

bool Qmi8658::begin(TwoWire& wire) {
  wire.begin(kQmiSdaPin, kQmiSclPin, kQmiFrequencyHz);

  return beginOnInitializedBus(wire);
}

bool Qmi8658::beginOnInitializedBus(TwoWire& wire) {

  wire_ = &wire;
  initialized_ = false;
  address_ = 0;

  if (probeAddress(kQmiAddressPrimary)) {
    address_ = kQmiAddressPrimary;
  } else if (probeAddress(kQmiAddressSecondary)) {
    address_ = kQmiAddressSecondary;
  } else {
    wire_ = nullptr;
    return false;
  }

  if (!writeRegister(kRegisterCtrl1, kCtrl1AutoIncrement) ||
      !writeRegister(kRegisterCtrl2, kCtrl2Accel4g120Hz) ||
      !writeRegister(kRegisterCtrl7, kCtrl7AccelOnly)) {
    wire_ = nullptr;
    address_ = 0;
    return false;
  }

  initialized_ = true;
  return true;
}

bool Qmi8658::readAcceleration(GatewayAccelerationSample* sample) {
  if (!initialized_ || wire_ == nullptr || sample == nullptr) {
    return false;
  }

  uint8_t raw[6] = {};
  if (!readRegisters(kRegisterAxL, raw, sizeof(raw))) {
    return false;
  }

  *sample = decodeAcceleration(raw);
  return true;
}

GatewayAccelerationSample Qmi8658::decodeAcceleration(const uint8_t raw[6]) {
  GatewayAccelerationSample sample{};
  sample.x_g = static_cast<float>(decodeAxis(raw[0], raw[1])) / kAccelerationScale;
  sample.y_g = static_cast<float>(decodeAxis(raw[2], raw[3])) / kAccelerationScale;
  sample.z_g = static_cast<float>(decodeAxis(raw[4], raw[5])) / kAccelerationScale;
  return sample;
}

bool Qmi8658::writeRegister(uint8_t reg, uint8_t value) {
  if (wire_ == nullptr || address_ == 0) {
    return false;
  }

  wire_->beginTransmission(address_);
  wire_->write(reg);
  wire_->write(value);
  return wire_->endTransmission() == 0;
}

bool Qmi8658::readRegisters(uint8_t start_reg, uint8_t* buffer, uint8_t length) {
  if (wire_ == nullptr || address_ == 0 || buffer == nullptr || length == 0) {
    return false;
  }

  wire_->beginTransmission(address_);
  wire_->write(start_reg);
  if (wire_->endTransmission(false) != 0) {
    return false;
  }

  if (wire_->requestFrom(address_, length) != length) {
    return false;
  }

  for (uint8_t i = 0; i < length; ++i) {
    if (wire_->available() <= 0) {
      return false;
    }

    const int value = wire_->read();
    if (value < 0) {
      return false;
    }
    buffer[i] = static_cast<uint8_t>(value);
  }

  return true;
}

bool Qmi8658::probeAddress(uint8_t address) {
  if (wire_ == nullptr) {
    return false;
  }

  wire_->beginTransmission(address);
  return wire_->endTransmission() == 0;
}
