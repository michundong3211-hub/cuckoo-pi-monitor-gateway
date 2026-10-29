#include "gateway35_audio_codec.hpp"

#if defined(GATEWAY_35) && defined(ARDUINO_ARCH_ESP32)

bool Gateway35AudioCodec::begin() {
  if (_handle != nullptr) return true;

  _handle = es8311_create(I2C_NUM_0, gateway35::kAudioCodecI2cAddress);
  if (_handle == nullptr) return false;

  const es8311_clock_config_t clock_config = {
      .mclk_inverted = false,
      .sclk_inverted = false,
      .mclk_from_mclk_pin = true,
      .mclk_frequency =
          static_cast<int>(gateway35::kAudioMclkFrequencyHz),
      .sample_frequency = static_cast<int>(gateway35::kAudioSampleRateHz),
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

void Gateway35AudioCodec::end() {
  if (_handle != nullptr) {
    es8311_delete(_handle);
    _handle = nullptr;
  }
}

bool Gateway35AudioCodec::ready() const {
  return _handle != nullptr;
}

#else

bool Gateway35AudioCodec::begin() { return false; }
void Gateway35AudioCodec::end() {}
bool Gateway35AudioCodec::ready() const { return false; }

#endif
