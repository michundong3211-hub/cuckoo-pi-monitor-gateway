#pragma once

#if defined(GMT154_06) && defined(ARDUINO_ARCH_ESP32) && \
    defined(ENABLE_GATEWAY_AUDIO)
#include "es8311.h"
#endif

class Gmt154AudioCodec {
 public:
  bool begin();
  void end();
  bool ready() const;

 private:
#if defined(GMT154_06) && defined(ARDUINO_ARCH_ESP32) && \
    defined(ENABLE_GATEWAY_AUDIO)
  es8311_handle_t _handle = nullptr;
#endif
};
