#include <stdint.h>

#include <Arduino_GFX_Library.h>
#include <Wire.h>
#include <lvgl.h>

#include "../../src/gateway35_display.hpp"

TwoWire Wire;

namespace {
int failures = 0;

void expect(bool condition) {
  if (!condition) ++failures;
}

void queueStartupReads() {
  const uint8_t config[] = {0xFB};
  const uint8_t output[] = {0xA4};
  Wire.queueReadResponse(config, sizeof(config), 1);
  Wire.queueReadResponse(output, sizeof(output), 1);
}

bool tcaFinishedBeforeGfx() { return Wire.transmissions.size() == 7; }

bool blackFilledBeforeBacklight() {
  return g_fake_arduino_gfx.fill_screen_calls == 1;
}

void resetFakes() {
  Wire = TwoWire();
  fakeArduinoGfxReset();
  fakeLvglReset();
}

void testProbeNackDoesNotBlockPanelAndLvgl() {
  resetFakes();
  queueStartupReads();
  std::vector<uint8_t> results(7, 0);
  results[6] = 1;
  Wire.setEndTransmissionResults(results.data(), results.size());
  gateway35::Gateway35DisplayAdapter adapter;
  expect(adapter.begin());
  expect(Wire.transmissions.size() == 7);
  expect(Wire.transmissions[6].address == 0x38);
  expect(g_fake_arduino_gfx.gfx_begin_calls == 1);
  expect(g_fake_lvgl.display_create_calls == 0);
  expect(adapter.createLvglDisplay() != nullptr);
  expect(g_fake_lvgl.display_create_calls == 1);
}

void testStartupAndDisplayCreation() {
  resetFakes();
  queueStartupReads();
  g_fake_arduino_gfx.before_gfx_begin = &tcaFinishedBeforeGfx;
  g_fake_arduino_gfx.before_backlight_on = &blackFilledBeforeBacklight;
  gateway35::Gateway35DisplayAdapter adapter;
  expect(adapter.begin());
  expect(g_fake_arduino_gfx.ordering_ok);
  expect(g_fake_arduino_gfx.gfx_begin_calls == 1);
  expect(g_fake_arduino_gfx.fill_screen_calls == 1);
  expect(g_fake_arduino_gfx.esp32spi_construct_calls == 1);
  expect(g_fake_arduino_gfx.esp32spi_dc == 3);
  expect(g_fake_arduino_gfx.esp32spi_cs == -1);
  expect(g_fake_arduino_gfx.esp32spi_sclk == 5);
  expect(g_fake_arduino_gfx.esp32spi_mosi == 1);
  expect(g_fake_arduino_gfx.esp32spi_miso == 2);
  expect(g_fake_arduino_gfx.st7796_construct_calls == 1);
  expect(g_fake_arduino_gfx.st7796_reset == -1);
  expect(g_fake_arduino_gfx.st7796_rotation == 0);
  expect(g_fake_arduino_gfx.st7796_ips);
  expect(g_fake_arduino_gfx.st7796_width == 320);
  expect(g_fake_arduino_gfx.st7796_height == 480);
  expect(g_fake_arduino_gfx.digital_values.size() == 2);
  expect(g_fake_arduino_gfx.digital_values.size() == 2 &&
         g_fake_arduino_gfx.digital_values[0] == LOW &&
         g_fake_arduino_gfx.digital_values[1] == HIGH);

  lv_display_t* first = adapter.createLvglDisplay();
  expect(first != nullptr);
  expect(g_fake_arduino_gfx.ps_malloc_calls == 1);
  expect(g_fake_lvgl.display_create_calls == 1);
  expect(first != nullptr && first->width == 320 && first->height == 480);
  expect(first != nullptr && first->color_format == LV_COLOR_FORMAT_RGB565);
  expect(first != nullptr && first->buffer_bytes == 320u * 480u / 10u * 2u);
  expect(first != nullptr &&
         first->render_mode == LV_DISPLAY_RENDER_MODE_PARTIAL);
  expect(adapter.createLvglDisplay() == first);
  expect(g_fake_arduino_gfx.ps_malloc_calls == 1);
  expect(g_fake_lvgl.display_create_calls == 1);
}

void testPrintLineClearsOnlyItsOwnLine() {
  resetFakes();
  queueStartupReads();
  gateway35::Gateway35DisplayAdapter adapter;
  expect(adapter.begin());
  const int startup_screen_clears = g_fake_arduino_gfx.fill_screen_calls;
  adapter.printLine(2, "LEFT", false);
  expect(g_fake_arduino_gfx.fill_screen_calls == startup_screen_clears);
  expect(g_fake_arduino_gfx.fill_rect_calls == 1);
  expect(g_fake_arduino_gfx.fill_rects.size() == 1);
  expect(g_fake_arduino_gfx.fill_rects.size() == 1 &&
         g_fake_arduino_gfx.fill_rects[0].x == 0 &&
         g_fake_arduino_gfx.fill_rects[0].y == 48 &&
         g_fake_arduino_gfx.fill_rects[0].width == 320 &&
         g_fake_arduino_gfx.fill_rects[0].height == 24);
  expect(g_fake_arduino_gfx.last_cursor_x == 0);
  expect(g_fake_arduino_gfx.last_cursor_y == 48);
  expect(g_fake_arduino_gfx.last_printed_text == "LEFT");

  g_fake_arduino_gfx.text_bounds_width = 60;
  adapter.printLine(3, "CENTER", true);
  expect(g_fake_arduino_gfx.fill_screen_calls == startup_screen_clears);
  expect(g_fake_arduino_gfx.fill_rect_calls == 2);
  expect(g_fake_arduino_gfx.fill_rects.size() == 2 &&
         g_fake_arduino_gfx.fill_rects[1].x == 0 &&
         g_fake_arduino_gfx.fill_rects[1].y == 72 &&
         g_fake_arduino_gfx.fill_rects[1].width == 320 &&
         g_fake_arduino_gfx.fill_rects[1].height == 24);
  expect(g_fake_arduino_gfx.get_text_bounds_calls == 1);
  expect(g_fake_arduino_gfx.last_cursor_x == 130);
  expect(g_fake_arduino_gfx.last_cursor_y == 72);
  expect(g_fake_arduino_gfx.last_printed_text == "CENTER");
}

void testPanelRotationUsesArduinoGfxHardwareRotation() {
  resetFakes();
  queueStartupReads();
  gateway35::Gateway35DisplayAdapter adapter;
  expect(adapter.begin());
  expect(adapter.setRotation(1));
  expect(g_fake_arduino_gfx.set_rotation_calls == 1);
  expect(g_fake_arduino_gfx.last_rotation == 1);
  expect(adapter.setRotation(0));
  expect(g_fake_arduino_gfx.set_rotation_calls == 2);
  expect(g_fake_arduino_gfx.last_rotation == 0);
}

void testFlushCompletesEveryBranch() {
  resetFakes();
  queueStartupReads();
  gateway35::Gateway35DisplayAdapter adapter;
  expect(adapter.begin());
  lv_display_t* display = adapter.createLvglDisplay();
  uint16_t pixels[] = {1, 2, 3, 4, 5, 6};
  const lv_area_t normal = {7, 11, 8, 13};
  gateway35::Gateway35DisplayAdapter::flush(
      display, &normal, reinterpret_cast<uint8_t*>(pixels));
  expect(display->flush_ready_calls == 1);
  expect(g_fake_arduino_gfx.draw_calls == 1);
  expect(g_fake_arduino_gfx.last_draw_x == 7);
  expect(g_fake_arduino_gfx.last_draw_y == 11);
  expect(g_fake_arduino_gfx.last_draw_width == 2);
  expect(g_fake_arduino_gfx.last_draw_height == 3);
  expect(g_fake_arduino_gfx.last_draw_pixels == pixels);

  const lv_area_t empty = {4, 4, 3, 4};
  gateway35::Gateway35DisplayAdapter::flush(
      display, &empty, reinterpret_cast<uint8_t*>(pixels));
  expect(display->flush_ready_calls == 2);
  expect(g_fake_arduino_gfx.draw_calls == 1);

  lv_display_t missing_user_data{};
  gateway35::Gateway35DisplayAdapter::flush(
      &missing_user_data, &normal, reinterpret_cast<uint8_t*>(pixels));
  expect(missing_user_data.flush_ready_calls == 1);
  expect(g_fake_arduino_gfx.draw_calls == 1);
}

void testCreationFailuresReleaseOwnedBuffer() {
  resetFakes();
  queueStartupReads();
  gateway35::Gateway35DisplayAdapter allocation_failure;
  expect(allocation_failure.begin());
  g_fake_arduino_gfx.ps_malloc_fails = true;
  expect(allocation_failure.createLvglDisplay() == nullptr);
  expect(g_fake_arduino_gfx.ps_malloc_calls == 1);
  expect(g_fake_arduino_gfx.free_calls == 0);
  expect(g_fake_lvgl.display_create_calls == 0);

  resetFakes();
  queueStartupReads();
  gateway35::Gateway35DisplayAdapter display_failure;
  expect(display_failure.begin());
  g_fake_lvgl.display_create_fails = true;
  expect(display_failure.createLvglDisplay() == nullptr);
  expect(g_fake_arduino_gfx.ps_malloc_calls == 1);
  expect(g_fake_arduino_gfx.free_calls == 1);
  expect(g_fake_lvgl.display_create_calls == 1);
}

void testSuccessfulAdapterDestructionReleasesDisplayAndBuffer() {
  resetFakes();
  queueStartupReads();
  {
    gateway35::Gateway35DisplayAdapter adapter;
    expect(adapter.begin());
    expect(adapter.createLvglDisplay() != nullptr);
    expect(g_fake_lvgl.display_delete_calls == 0);
    expect(g_fake_arduino_gfx.free_calls == 0);
  }
  expect(g_fake_lvgl.display_delete_calls == 1);
  expect(g_fake_arduino_gfx.free_calls == 1);
}
}  // namespace

int main() {
  testProbeNackDoesNotBlockPanelAndLvgl();
  testStartupAndDisplayCreation();
  testPrintLineClearsOnlyItsOwnLine();
  testPanelRotationUsesArduinoGfxHardwareRotation();
  testFlushCompletesEveryBranch();
  testCreationFailuresReleaseOwnedBuffer();
  testSuccessfulAdapterDestructionReleasesDisplayAndBuffer();
  return failures == 0 ? 0 : 1;
}
