#pragma once

#include <stdint.h>

namespace gmt154 {

inline constexpr int8_t kAudioDoutPin = 4;
inline constexpr int8_t kAudioWsPin = 7;
inline constexpr int8_t kAudioBclkPin = 16;
inline constexpr int8_t kAudioMclkPin = 38;

inline constexpr int8_t kAudioSdaPin = 40;
inline constexpr int8_t kAudioSclPin = 39;
inline constexpr uint32_t kAudioI2cClockHz = 100000;
inline constexpr uint8_t kAudioCodecI2cAddress = 0x18;

inline constexpr int8_t kAmplifierControlPin = 8;
inline constexpr int kAmplifierDisabledLevel = 0;
inline constexpr int kAmplifierEnabledLevel = 1;

// 16 kHz is a firmware choice; the board schematic does not specify Fs.
inline constexpr uint32_t kAudioSampleRateHz = 16000;
inline constexpr uint32_t kAudioMclkFrequencyHz = 4096000;
static_assert(kAudioMclkFrequencyHz == kAudioSampleRateHz * 256,
              "GMT154 ES8311 MCLK must be 256 times the sample rate");

}  // namespace gmt154
