#pragma once

#include <cstdint>

// Indices 0..6 are the persisted user-selectable colors. Index 7 is the
// compatibility/default option that matches the BLE accent color.
constexpr uint8_t RING_COLOR_COUNT = 7;
constexpr uint8_t RING_COLOR_DEFAULT_INDEX = RING_COLOR_COUNT;
constexpr uint8_t RING_COLOR_OPTION_COUNT = RING_COLOR_COUNT + 1;
constexpr uint32_t RING_COLOR_DEFAULT_HEX = 0x45D9B2;
constexpr uint32_t RING_COLOR_HEX[RING_COLOR_COUNT] = {
    0xFFD700, 0x61026F, 0xFF4500, 0x07A73E,
    0x1E90FF, 0xD5D1CF, 0x000000};

constexpr uint8_t normalize_ring_color_index(int index) {
  return index >= 0 && index < RING_COLOR_COUNT
             ? static_cast<uint8_t>(index)
             : RING_COLOR_DEFAULT_INDEX;
}

constexpr uint32_t ring_color_hex(uint8_t index) {
  return index < RING_COLOR_COUNT ? RING_COLOR_HEX[index]
                                  : RING_COLOR_DEFAULT_HEX;
}
