#include <assert.h>
#include <stdint.h>

#include "../../lib/cst3530/cst3530_touch.hpp"

int main() {
  const uint8_t pressed[] = {
      0x00, 0x00, 0xFF, 0x01, 0x23, 0xAB, 0x55, 0x01, 0x10,
  };
  Cst3530Point point{};

  assert(cst3530ParseFirstPoint(pressed, sizeof(pressed), &point));
  assert(point.x == 0x123);
  assert(point.y == 0x0AB);
  assert(point.strength == 0x55);

  const uint8_t released[] = {
      0x00, 0x00, 0xFF, 0x01, 0x23, 0xAB, 0x55, 0x01, 0x00,
  };
  assert(!cst3530ParseFirstPoint(released, sizeof(released), &point));
  return 0;
}
