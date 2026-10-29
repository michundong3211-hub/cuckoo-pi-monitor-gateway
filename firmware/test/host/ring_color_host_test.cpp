#include <cassert>

#include "../../src/gateway_ring_color.hpp"

int main() {
  assert(RING_COLOR_COUNT == 7);
  assert(RING_COLOR_OPTION_COUNT == 8);
  assert(RING_COLOR_DEFAULT_INDEX == 7);

  assert(normalize_ring_color_index(-1) == RING_COLOR_DEFAULT_INDEX);
  assert(normalize_ring_color_index(7) == RING_COLOR_DEFAULT_INDEX);
  assert(normalize_ring_color_index(99) == RING_COLOR_DEFAULT_INDEX);

  for (uint8_t index = 0; index < RING_COLOR_COUNT; ++index) {
    assert(normalize_ring_color_index(index) == index);
    assert(ring_color_hex(index) == RING_COLOR_HEX[index]);
  }
  assert(ring_color_hex(RING_COLOR_DEFAULT_INDEX) == RING_COLOR_DEFAULT_HEX);
  return 0;
}
