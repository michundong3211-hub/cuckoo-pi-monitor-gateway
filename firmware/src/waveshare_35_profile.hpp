#pragma once

#include <stdint.h>

namespace gateway35 {

inline constexpr char kSku[] = "30733";

inline constexpr uint16_t kDisplayWidth = 320;
inline constexpr uint16_t kDisplayHeight = 480;

inline constexpr int8_t kLcdBacklightPin = 6;
inline constexpr int8_t kSpiMisoPin = 2;
inline constexpr int8_t kSpiMosiPin = 1;
inline constexpr int8_t kSpiSclkPin = 5;
inline constexpr int8_t kLcdDcPin = 3;
inline constexpr int8_t kLcdCsPin = -1;
inline constexpr int8_t kLcdResetPin = -1;

inline constexpr int8_t kI2cSdaPin = 8;
inline constexpr int8_t kI2cSclPin = 7;
inline constexpr uint32_t kI2cClockHz = 100000;

inline constexpr uint8_t kAudioCodecI2cAddress = 0x18;
inline constexpr int8_t kAudioSdaPin = 8;
inline constexpr int8_t kAudioSclPin = 7;
inline constexpr int8_t kAudioMclkPin = 12;
inline constexpr int8_t kAudioBclkPin = 13;
inline constexpr int8_t kAudioLrckPin = 15;
inline constexpr int8_t kAudioDoutPin = 16;
inline constexpr uint32_t kAudioSampleRateHz = 16000;
inline constexpr uint32_t kAudioMclkFrequencyHz = 4096000;

inline constexpr uint8_t kFt6336Address = 0x38;
inline constexpr uint8_t kTca9554Address = 0x20;
inline constexpr uint8_t kTca9554OutputRegister = 0x01;
inline constexpr uint8_t kTca9554ConfigRegister = 0x03;
inline constexpr uint8_t kPowerAmplifierEnableBit = 0x80;
inline constexpr uint8_t kTca9554Exio1Mask = 0x02;  // EXIO1/P1 LCD reset

}  // namespace gateway35
