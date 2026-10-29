#pragma once

#include <stdint.h>

#include <Wire.h>

namespace gateway35 {

struct StartupHooks {
  void* context;
  bool (*probeTouch)(void* context, TwoWire& wire);
  bool (*beginDisplay)(void* context);
  void (*fillBlack)(void* context);
  void (*setBacklight)(void* context, bool enabled);
  void (*delayMs)(void* context, uint32_t milliseconds);
};

bool initializePanelHardware(TwoWire& wire, const StartupHooks& hooks);

}  // namespace gateway35
