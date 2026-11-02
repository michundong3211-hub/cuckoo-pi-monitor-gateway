#pragma once

#include <lvgl.h>
#include <gateway_settings_layout.hpp>

// Gravity settings only: target strings are validated to fit their rect.
inline lv_obj_t* create_gravity_settings_compact_label(
    lv_obj_t* parent, const char* text, const GatewaySettingsRect& rect,
    lv_color_t color, const lv_font_t* font) {
  lv_obj_t* label = lv_label_create(parent);
  lv_label_set_text(label, text);
  lv_obj_set_width(label, rect.width);
  lv_obj_set_height(label, LV_SIZE_CONTENT);
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_set_style_text_color(label, color, LV_PART_MAIN);
  if (font) lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
  lv_obj_set_pos(label, rect.x, rect.y + rect.height / 2);
  lv_obj_set_style_translate_y(label, lv_pct(-50), LV_PART_MAIN);
  return label;
}
