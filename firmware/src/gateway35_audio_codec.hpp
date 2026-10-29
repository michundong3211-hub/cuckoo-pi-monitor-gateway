#pragma once

#include "waveshare_35_profile.hpp"

#if defined(GATEWAY_35) && defined(ARDUINO_ARCH_ESP32)
#include "es8311.h"
#endif

constexpr bool gateway35AudioStartupReady(bool i2sReady, bool codecReady,
                                          bool amplifierReady, bool taskReady) {
  return i2sReady && codecReady && amplifierReady && taskReady;
}

class Gateway35AudioCodec {
 public:
  bool begin();
  void end();
  bool ready() const;

 private:
#if defined(GATEWAY_35) && defined(ARDUINO_ARCH_ESP32)
  es8311_handle_t _handle = nullptr;
#endif
};
