#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

// Core Arduino defaults. SKU30733 display-specific wiring belongs to
// src/waveshare_35_profile.hpp so firmware code has one authoritative source.
static const uint8_t SDA = 8;
static const uint8_t SCL = 7;
static const int8_t SS = -1;
static const uint8_t MOSI = 1;
static const uint8_t MISO = 2;
static const uint8_t SCK = 5;

#endif  // Pins_Arduino_h
