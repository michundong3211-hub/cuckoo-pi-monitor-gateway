#include "gmt154_audio_codec.hpp"

#if defined(GMT154_06) && defined(ARDUINO_ARCH_ESP32) && \
    defined(ENABLE_GATEWAY_AUDIO)

#include <Wire.h>

#include "gmt154_audio_profile.hpp"

bool Gmt154AudioCodec::begin() {
  if (_handle != nullptr) return true;

  if (!Wire.begin(gmt154::kAudioSdaPin, gmt154::kAudioSclPin,
                  gmt154::kAudioI2cClockHz)) {
    return false;
  }

  _handle = es8311_create(I2C_NUM_0, gmt154::kAudioCodecI2cAddress);
  if (_handle == nullptr) return false;

  const es8311_clock_config_t clock_config = {
      .mclk_inverted = false,
      .sclk_inverted = false,
      .mclk_from_mclk_pin = true,
      .mclk_frequency =
          static_cast<int>(gmt154::kAudioMclkFrequencyHz),
      .sample_frequency = static_cast<int>(gmt154::kAudioSampleRateHz),
  };
  if (es8311_init(_handle, &clock_config, ES8311_RESOLUTION_16,
                  ES8311_RESOLUTION_16) != ESP_OK) {
    end();
    return false;
  }

  if (es8311_voice_volume_set(_handle, 70, nullptr) != ESP_OK) {
    end();
    return false;
  }
  return true;
}

void Gmt154AudioCodec::end() {
  if (_handle != nullptr) {
    es8311_delete(_handle);
    _handle = nullptr;
  }
}

bool Gmt154AudioCodec::ready() const { return _handle != nullptr; }

#else

bool Gmt154AudioCodec::begin() { return false; }
void Gmt154AudioCodec::end() {}
bool Gmt154AudioCodec::ready() const { return false; }

#endif
