#pragma once

#include <stddef.h>
#include <stdint.h>

#include <Wire.h>

namespace gateway35 {

struct TouchPoint {
  uint16_t x;
  uint16_t y;
};

enum class TouchResult : uint8_t {
  kValid,
  kNoTouch,
  kIoError,
  kInvalidReport,
};

bool probeTouch(TwoWire& wire);
TouchResult parseTouchReport(const uint8_t* report, size_t length,
                             TouchPoint* point);
TouchResult readTouch(TwoWire& wire, TouchPoint* point);

}  // namespace gateway35
