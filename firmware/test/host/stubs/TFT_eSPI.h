#pragma once

#include <stddef.h>
#include <stdint.h>

struct FakeTftState {
  int start_write_calls = 0;
  int end_write_calls = 0;
  int set_addr_window_calls = 0;
  int32_t last_x = 0;
  int32_t last_y = 0;
  uint32_t last_width = 0;
  uint32_t last_height = 0;
  int push_colors_calls = 0;
  uint16_t* last_pixels = nullptr;
  uint32_t last_pixel_count = 0;
  bool last_swap = false;
};

inline FakeTftState g_fake_tft;

inline void fakeTftReset() { g_fake_tft = FakeTftState{}; }

class TFT_eSPI {
 public:
  void startWrite() { ++g_fake_tft.start_write_calls; }

  void setAddrWindow(int32_t x, int32_t y, uint32_t width,
                     uint32_t height) {
    ++g_fake_tft.set_addr_window_calls;
    g_fake_tft.last_x = x;
    g_fake_tft.last_y = y;
    g_fake_tft.last_width = width;
    g_fake_tft.last_height = height;
  }

  void pushColors(uint16_t* pixels, uint32_t count, bool swap) {
    ++g_fake_tft.push_colors_calls;
    g_fake_tft.last_pixels = pixels;
    g_fake_tft.last_pixel_count = count;
    g_fake_tft.last_swap = swap;
  }

  void endWrite() { ++g_fake_tft.end_write_calls; }
};
