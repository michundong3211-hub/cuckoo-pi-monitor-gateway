#include <stdint.h>

#include <iostream>

#include "gmt154_display_adapter.hpp"

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << "\n";
    ++failures;
  }
}

void testFlushWritesAreaPixelsAndReady() {
  fakeTftReset();
  lv_display_t display{};
  TFT_eSPI tft;
  display.user_data = &tft;
  uint16_t pixels[] = {1, 2, 3, 4};
  const lv_area_t area = {3, 5, 4, 6};

  gmt154_display::flush(&display, &area,
                         reinterpret_cast<uint8_t*>(pixels));

  expect(g_fake_tft.start_write_calls == 1, "flush starts one write");
  expect(g_fake_tft.set_addr_window_calls == 1,
         "flush sets one address window");
  expect(g_fake_tft.last_x == 3 && g_fake_tft.last_y == 5,
         "flush uses area origin");
  expect(g_fake_tft.last_width == 2 && g_fake_tft.last_height == 2,
         "flush uses inclusive area dimensions");
  expect(g_fake_tft.push_colors_calls == 1,
         "flush pushes one pixel block");
  expect(g_fake_tft.last_pixels == pixels &&
             g_fake_tft.last_pixel_count == 4 && g_fake_tft.last_swap,
         "flush pushes the supplied pixels with byte swapping");
  expect(g_fake_tft.end_write_calls == 1, "flush ends the write");
  expect(display.flush_ready_calls == 1, "flush signals LVGL ready once");
}

void testFlushReadyCoversInvalidInputs() {
  fakeTftReset();
  uint16_t pixels[] = {1};
  const lv_area_t area = {0, 0, 0, 0};

  lv_display_t missing_tft{};
  gmt154_display::flush(&missing_tft, &area,
                         reinterpret_cast<uint8_t*>(pixels));
  expect(missing_tft.flush_ready_calls == 1,
         "missing TFT still signals LVGL ready");

  lv_display_t missing_pixels{};
  TFT_eSPI tft;
  missing_pixels.user_data = &tft;
  gmt154_display::flush(&missing_pixels, &area, nullptr);
  expect(missing_pixels.flush_ready_calls == 1,
         "missing pixels still signals LVGL ready");
  expect(g_fake_tft.start_write_calls == 0,
         "invalid flush input does not write to TFT");
}

void testAllocationPrefersPsram() {
  fakeGmt154HeapReset();
  const gmt154_display::BufferAllocation allocation =
      gmt154_display::allocateDrawBuffer(11520);

  expect(allocation.buffer != nullptr, "PSRAM allocation succeeds");
  expect(allocation.source == gmt154_display::BufferSource::Psram,
         "PSRAM is reported as the buffer source");
  expect(allocation.bytes == 11520,
         "PSRAM allocation reports its actual byte count");
  expect(g_fake_gmt154_heap.psram_calls == 1 &&
             g_fake_gmt154_heap.internal_calls == 0,
         "successful PSRAM avoids internal fallback");
  expect(g_fake_gmt154_heap.last_psram_bytes == 11520,
         "successful PSRAM keeps the full draw buffer size");
  delete[] static_cast<uint8_t*>(allocation.buffer);
}

void testAllocationFallsBackToInternal() {
  fakeGmt154HeapReset();
  g_fake_gmt154_heap.psram_fails = true;
  const gmt154_display::BufferAllocation allocation =
      gmt154_display::allocateDrawBuffer(11520);

  expect(allocation.buffer != nullptr, "internal fallback succeeds");
  expect(allocation.source == gmt154_display::BufferSource::Internal,
         "internal fallback is reported as the buffer source");
  expect(allocation.bytes == 5760,
         "internal fallback reports its actual byte count");
  expect(g_fake_gmt154_heap.psram_calls == 1 &&
             g_fake_gmt154_heap.internal_calls == 1,
         "fallback calls PSRAM then internal allocation");
  expect(g_fake_gmt154_heap.last_internal_caps ==
             (MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
         "fallback requests internal 8-bit memory");
  expect(g_fake_gmt154_heap.last_psram_bytes == 11520,
         "fallback still first requests the full size from PSRAM");
  expect(g_fake_gmt154_heap.last_internal_bytes == 5760,
         "fallback requests a half-size internal draw buffer");
  lv_display_t display{};
  gmt154_display::registerDrawBuffer(&display, allocation);
  expect(display.buffer1 == allocation.buffer,
         "LVGL registers the allocated fallback buffer");
  expect(display.buffer_bytes == 5760,
         "LVGL registers the fallback allocation's actual byte count");
  delete[] static_cast<uint8_t*>(allocation.buffer);
}

void testAllocationReportsBothFailures() {
  fakeGmt154HeapReset();
  g_fake_gmt154_heap.psram_fails = true;
  g_fake_gmt154_heap.internal_fails = true;
  const gmt154_display::BufferAllocation allocation =
      gmt154_display::allocateDrawBuffer(128);

  expect(allocation.buffer == nullptr, "both allocation attempts can fail");
  expect(allocation.source == gmt154_display::BufferSource::None,
         "both failures report no buffer source");
  expect(g_fake_gmt154_heap.psram_calls == 1 &&
             g_fake_gmt154_heap.internal_calls == 1,
         "both allocation attempts are observable");
}

}  // namespace

int main() {
  testFlushWritesAreaPixelsAndReady();
  testFlushReadyCoversInvalidInputs();
  testAllocationPrefersPsram();
  testAllocationFallsBackToInternal();
  testAllocationReportsBothFailures();
  return failures == 0 ? 0 : 1;
}
