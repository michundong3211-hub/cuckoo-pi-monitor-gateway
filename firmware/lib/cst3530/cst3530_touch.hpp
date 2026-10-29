#pragma once

#include <stddef.h>
#include <stdint.h>

struct Cst3530Point {
  uint16_t x;
  uint16_t y;
  uint16_t strength;
};

bool cst3530ParseFirstPoint(const uint8_t* report, size_t length,
                            Cst3530Point* point);
