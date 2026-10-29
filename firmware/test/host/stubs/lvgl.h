#pragma once

#include <stddef.h>
#include <stdint.h>

struct lv_area_t {
  int32_t x1;
  int32_t y1;
  int32_t x2;
  int32_t y2;
};

enum lv_color_format_t { LV_COLOR_FORMAT_RGB565 };
enum lv_display_render_mode_t { LV_DISPLAY_RENDER_MODE_PARTIAL };

struct lv_display_t;
using lv_display_flush_cb_t = void (*)(lv_display_t*, const lv_area_t*,
                                       uint8_t*);

struct lv_display_t {
  int32_t width;
  int32_t height;
  lv_color_format_t color_format = LV_COLOR_FORMAT_RGB565;
  void* buffer1 = nullptr;
  void* buffer2 = nullptr;
  size_t buffer_bytes = 0;
  lv_display_render_mode_t render_mode = LV_DISPLAY_RENDER_MODE_PARTIAL;
  void* user_data = nullptr;
  lv_display_flush_cb_t flush_cb = nullptr;
  int flush_ready_calls = 0;
};

struct FakeLvglState {
  bool display_create_fails = false;
  int display_create_calls = 0;
  int display_delete_calls = 0;
};

inline FakeLvglState g_fake_lvgl;

inline void fakeLvglReset() { g_fake_lvgl = FakeLvglState{}; }

inline lv_display_t* lv_display_create(int32_t width, int32_t height) {
  ++g_fake_lvgl.display_create_calls;
  if (g_fake_lvgl.display_create_fails) return nullptr;
  return new lv_display_t{width, height};
}

inline void lv_display_delete(lv_display_t* display) {
  ++g_fake_lvgl.display_delete_calls;
  delete display;
}

inline void lv_display_set_color_format(lv_display_t* display,
                                        lv_color_format_t format) {
  display->color_format = format;
}

inline void lv_display_set_buffers(lv_display_t* display, void* buffer1,
                                   void* buffer2, size_t bytes,
                                   lv_display_render_mode_t mode) {
  display->buffer1 = buffer1;
  display->buffer2 = buffer2;
  display->buffer_bytes = bytes;
  display->render_mode = mode;
}

inline void lv_display_set_user_data(lv_display_t* display, void* user_data) {
  display->user_data = user_data;
}

inline void* lv_display_get_user_data(lv_display_t* display) {
  return display->user_data;
}

inline void lv_display_set_flush_cb(lv_display_t* display,
                                    lv_display_flush_cb_t callback) {
  display->flush_cb = callback;
}

inline void lv_display_flush_ready(lv_display_t* display) {
  ++display->flush_ready_calls;
}
