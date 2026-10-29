#include "gateway35_board_io.hpp"

#include "waveshare_35_profile.hpp"

namespace gateway35 {
namespace {

bool readRegister(TwoWire& wire, uint8_t address, uint8_t reg,
                  uint8_t* value) {
  wire.beginTransmission(address);
  wire.write(reg);
  if (wire.endTransmission() != 0) return false;
  if (wire.requestFrom(address, static_cast<size_t>(1)) != 1) return false;
  if (wire.available() < 1) return false;
  const int read_value = wire.read();
  if (read_value < 0) return false;
  *value = static_cast<uint8_t>(read_value);
  return true;
}

bool writeRegister(TwoWire& wire, uint8_t address, uint8_t reg,
                   uint8_t value) {
  wire.beginTransmission(address);
  wire.write(reg);
  wire.write(value);
  return wire.endTransmission() == 0;
}

}  // namespace

bool initializePanelHardware(TwoWire& wire, const StartupHooks& hooks) {
  if (hooks.probeTouch == nullptr || hooks.beginDisplay == nullptr ||
      hooks.fillBlack == nullptr || hooks.setBacklight == nullptr ||
      hooks.delayMs == nullptr) {
    return false;
  }

  hooks.setBacklight(hooks.context, false);
  if (!wire.begin(kI2cSdaPin, kI2cSclPin, kI2cClockHz)) return false;

  uint8_t config = 0;
  if (!readRegister(wire, kTca9554Address, kTca9554ConfigRegister, &config)) {
    return false;
  }
  config &= static_cast<uint8_t>(~kTca9554Exio1Mask);
  if (!writeRegister(wire, kTca9554Address, kTca9554ConfigRegister, config)) {
    return false;
  }

  uint8_t output = 0;
  if (!readRegister(wire, kTca9554Address, kTca9554OutputRegister, &output)) {
    return false;
  }
  const uint8_t reset_high = output | kTca9554Exio1Mask;
  const uint8_t reset_low = output & static_cast<uint8_t>(~kTca9554Exio1Mask);
  if (!writeRegister(wire, kTca9554Address, kTca9554OutputRegister,
                     reset_high)) {
    return false;
  }
  hooks.delayMs(hooks.context, 10);
  if (!writeRegister(wire, kTca9554Address, kTca9554OutputRegister,
                     reset_low)) {
    return false;
  }
  hooks.delayMs(hooks.context, 10);
  if (!writeRegister(wire, kTca9554Address, kTca9554OutputRegister,
                     reset_high)) {
    return false;
  }
  hooks.delayMs(hooks.context, 200);

  // Touch is optional for bringing up the LCD.  A missing or late-responding
  // FT6336 must not prevent the panel reset, ST7796 initialization, and
  // backlight from being enabled.
  (void)hooks.probeTouch(hooks.context, wire);
  if (!hooks.beginDisplay(hooks.context)) return false;
  hooks.fillBlack(hooks.context);
  hooks.setBacklight(hooks.context, true);
  return true;
}

}  // namespace gateway35
