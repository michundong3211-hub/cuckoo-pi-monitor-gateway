#include "cst3530_touch.hpp"

bool cst3530ParseFirstPoint(const uint8_t* report, size_t length,
                            Cst3530Point* point) {
  if (!report || !point || length < 9 || report[2] != 0xFF ||
      (report[3] & 0x0F) == 0 || (report[8] & 0xF0) == 0) {
    return false;
  }

  point->x = static_cast<uint16_t>(((report[7] & 0x0F) << 8) | report[4]);
  point->y = static_cast<uint16_t>(((report[7] & 0xF0) << 4) | report[5]);
  point->strength = report[6];
  return true;
}
