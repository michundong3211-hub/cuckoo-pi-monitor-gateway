#include "cuckoo_pi_screen.hpp"

#include <algorithm>
#include <cstdio>

namespace {
GatewaySettingsRect rect(int16_t x, int16_t y, int16_t width, int16_t height) {
  return GatewaySettingsRect{x, y, width, height};
}

CuckooPiScreenLayout fridgeConstantLayout(CuckooPiScreenLayout layout,
                                          int16_t width, int16_t height,
                                          int mode) {
  if (mode != 1) return layout;
  layout.show_beer_set = false;
  layout.show_fridge_temp = false;
  layout.show_chiller = false;
  layout.beer_set = rect(0, 0, 0, 0);
  layout.fridge = rect(0, 0, 0, 0);
  layout.chiller = rect(0, 0, 0, 0);
  if (width == 240 && height == 320) {
    layout.beer = rect(16, 62, 208, 66);
    layout.fridge_set = rect(8, 144, 224, 52);
    layout.mode_card = rect(8, 200, 108, 52);
    layout.mode = layout.mode_card;
    layout.state = rect(124, 200, 108, 52);
    layout.runtime = rect(8, 256, 224, 52);
  } else if (width == 320 && height == 240) {
    layout.fridge_set = rect(180, 40, 132, 86);
    layout.mode_card = rect(8, 136, 148, 46);
    layout.mode = layout.mode_card;
    layout.state = rect(164, 136, 148, 46);
    layout.runtime = rect(8, 186, 304, 46);
  } else if (height > width) {
    layout.temperature_card = rect(8, 48, width - 16, 164);
    layout.beer_heading = rect(16, 56, width - 32, 24);
    layout.beer = rect(16, 86, width - 32, 92);
    layout.beer_unit = rect(width - 70, 120, 34, 30);
    layout.fridge_set = rect(8, 220, width - 16, 56);
    layout.mode_card = rect(8, 284, (width - 24) / 2, 58);
    layout.mode = layout.mode_card;
    layout.state = rect(16 + (width - 24) / 2, 284,
                        (width - 24) / 2, 58);
    layout.runtime = rect(8, 350, width - 16, 42);
  } else {
    layout.fridge_set = rect(236, 48, 236, 42);
    layout.mode_card = rect(236, 144, 112, 54);
    layout.mode = layout.mode_card;
    layout.state = rect(360, 144, 112, 54);
    layout.runtime = rect(8, 234, width - 16, 74);
  }
  return layout;
}
}

CuckooPiScreenLayout calculateCuckooPiScreenLayout(int16_t width,
                                                   int16_t height, int mode) {
  CuckooPiScreenLayout layout{};
  layout.screen = rect(0, 0, width, height);
  if ((width == 320 && height == 480) || (width == 480 && height == 320)) {
    const bool portrait = height > width;
    layout = calculateCuckooPiScreenLayout(portrait ? 240 : 320,
                                            portrait ? 320 : 240, mode);
    const int16_t source_width = portrait ? 240 : 320;
    const int16_t source_height = portrait ? 320 : 240;
    GatewaySettingsRect* regions[] = {&layout.header, &layout.title,
      &layout.index, &layout.online, &layout.temperature_card,
      &layout.beer_heading, &layout.mode_card, &layout.mode, &layout.state,
      &layout.beer, &layout.beer_unit, &layout.beer_set, &layout.fridge,
      &layout.fridge_set, &layout.chiller, &layout.runtime, &layout.touch};
    for (GatewaySettingsRect* r : regions) {
      if (r->width <= 0 || r->height <= 0) continue;
      const int16_t x2 = (r->x + r->width) * width / source_width;
      const int16_t y2 = (r->y + r->height) * height / source_height;
      r->x = r->x * width / source_width;
      r->y = r->y * height / source_height;
      r->width = x2 - r->x;
      r->height = y2 - r->y;
    }
    layout.screen = rect(0, 0, width, height);
    return layout;
  }
  if (width == 240 && height == 240) {
    // GMT154-06 maps the 464px active SVG region to the 240px LCD at
    // 240/464. Keep this square layout independent from the Waveshare
    // portrait and landscape arrangements below.
    layout.header = rect(0, 0, 240, 32);
    layout.title = rect(12, 5, 144, 22);
    layout.online = rect(178, 5, 58, 22);
    layout.temperature_card = rect(0, 32, 240, 149);
    layout.beer_heading = rect(0, 43, 240, 18);
    layout.beer = rect(16, 58, 208, 64);
    layout.beer_unit = rect(206, 90, 30, 20);
    layout.beer_set = rect(8, 122, 112, 58);
    layout.fridge_set = rect(120, 122, 112, 58);
    layout.mode_card = rect(8, 180, 112, 60);
    layout.mode = rect(8, 180, 112, 60);
    layout.state = rect(120, 180, 112, 60);
    layout.touch = rect(0, 32, 240, 208);
    return layout;
  }
  if (width == 240 && height == 320) {
    // Waveshare 2.8-inch portrait: mirror the GMT154 information order while
    // keeping the compact typography and retaining all secondary readings.
    layout.header = rect(0, 0, 240, 32);
    layout.title = rect(8, 4, 108, 24);
    layout.index = rect(116, 4, 30, 24);
    layout.online = rect(154, 4, 78, 24);
    layout.temperature_card = rect(8, 38, 224, 102);
    layout.beer_heading = rect(16, 42, 208, 18);
    layout.beer = rect(16, 62, 208, 66);
    layout.beer_unit = rect(190, 76, 26, 20);
    layout.beer_set = rect(8, 144, 108, 40);
    layout.fridge_set = rect(124, 144, 108, 40);
    layout.mode_card = rect(8, 188, 108, 42);
    layout.mode = rect(8, 188, 108, 42);
    layout.state = rect(124, 188, 108, 42);
    layout.runtime = rect(8, 234, 224, 22);
    layout.fridge = rect(8, 260, 108, 52);
    layout.chiller = rect(124, 260, 108, 52);
    layout.touch = rect(0, 32, 240, 288);
    return fridgeConstantLayout(layout, width, height, mode);
  }
  if (width == 320 && height == 240) {
    // Waveshare 2.8-inch landscape panel. Keep the same reading order while
    // using the width for setpoints and compact secondary metrics.
    layout.header = rect(0, 0, 320, 28);
    layout.title = rect(8, 3, 126, 22);
    layout.index = rect(136, 3, 34, 22);
    layout.online = rect(232, 3, 80, 22);
    layout.temperature_card = rect(8, 36, 164, 94);
    layout.beer_heading = rect(14, 40, 152, 16);
    layout.beer = rect(14, 59, 152, 52);
    layout.beer_unit = rect(139, 77, 26, 20);
    layout.beer_set = rect(180, 40, 132, 40);
    layout.fridge_set = rect(180, 86, 132, 40);
    layout.mode_card = rect(8, 136, 148, 40);
    layout.mode = rect(8, 136, 148, 40);
    layout.state = rect(164, 136, 148, 40);
    layout.runtime = rect(8, 180, 304, 18);
    layout.fridge = rect(8, 202, 148, 36);
    layout.chiller = rect(164, 202, 148, 36);
    layout.touch = rect(0, 28, 320, 212);
    return fridgeConstantLayout(layout, width, height, mode);
  }
  const bool portrait = height > width;
  if (portrait) {
    layout.header = rect(0, 0, width, 44);
    layout.title = rect(8, 4, 180, 34);
    layout.index = rect(190, 4, 36, 32);
    layout.online = rect(width - 88, 4, 72, 32);
    layout.temperature_card = rect(8, 48, width - 16, 142);
    layout.beer_heading = rect(16, 54, width - 32, 24);
    layout.beer = rect(16, 82, width - 32, 76);
    layout.beer_unit = rect(width - 70, 112, 34, 30);
    layout.beer_set = rect(8, 198, (width - 24) / 2, 50);
    layout.fridge_set = rect(16 + (width - 24) / 2, 198,
                             (width - 24) / 2, 50);
    layout.mode_card = rect(8, 256, (width - 24) / 2, 54);
    layout.mode = layout.mode_card;
    layout.state = rect(16 + (width - 24) / 2, 256,
                        (width - 24) / 2, 54);
    layout.runtime = rect(8, 318, width - 16, 30);
    layout.fridge = rect(8, 356, (width - 24) / 2, height - 364);
    layout.chiller = rect(16 + (width - 24) / 2, 356,
                          (width - 24) / 2, height - 364);
    layout.touch = rect(0, 44, width, height - 44);
  } else {
    layout.header = rect(0, 0, width, 36);
    layout.title = rect(8, 3, 180, 30);
    layout.index = rect(194, 3, width == 320 ? 36 : 42, 30);
    layout.online = rect(width == 320 ? 236 : width - 94, 3,
                         width == 320 ? 76 : 86, 30);
    layout.temperature_card = rect(8, 44, 216, 180);
    layout.beer_heading = rect(18, 52, 196, 24);
    layout.beer = rect(18, 82, 196, 100);
    layout.beer_unit = rect(184, 121, 30, 30);
    layout.beer_set = rect(236, 48, 236, 42);
    layout.fridge_set = rect(236, 96, 236, 42);
    layout.mode_card = rect(236, 144, 112, 54);
    layout.mode = layout.mode_card;
    layout.state = rect(360, 144, 112, 54);
    layout.runtime = rect(8, 234, 148, 74);
    layout.fridge = rect(166, 234, 148, 74);
    layout.chiller = rect(324, 234, 148, 74);
    layout.touch = rect(0, 36, width, height - 36);
  }
  return fridgeConstantLayout(layout, width, height, mode);
}

CuckooPiTemperatureGroup layoutCuckooPiTemperatureGroup(
    const GatewaySettingsRect& region, CuckooPiTextMetrics number,
    CuckooPiTextMetrics unit, bool has_unit, int16_t gap) {
  CuckooPiTemperatureGroup group{};
  if (region.width <= 0 || region.height <= 0) return group;

  const int16_t safe_gap = has_unit ? std::max<int16_t>(0, gap) : 0;
  const int16_t measured_unit_width =
      has_unit ? std::max<int16_t>(1, unit.width) : 0;
  if (has_unit && measured_unit_width + safe_gap >= region.width) {
    return group;
  }
  const int16_t unit_width =
      std::min<int16_t>(region.width, measured_unit_width);
  const int16_t available_number_width =
      std::max<int16_t>(1, region.width - safe_gap - unit_width);
  const int16_t number_width = std::min<int16_t>(
      available_number_width, std::max<int16_t>(1, number.width));
  const int16_t number_height = std::min<int16_t>(
      region.height, std::max<int16_t>(1, number.height));
  const int16_t unit_height =
      has_unit ? std::min<int16_t>(region.height,
                                   std::max<int16_t>(1, unit.height))
               : 0;
  const int16_t total_width = number_width + safe_gap + unit_width;
  const int16_t left = region.x + (region.width - total_width) / 2;

  group.number.width = number_width;
  group.number.height = number_height;
  group.number.x = left;
  group.unit.width = unit_width;
  group.unit.height = unit_height;
  group.unit.x = left + number_width + safe_gap;

  const int16_t number_base = std::min<int16_t>(
      number_height, std::max<int16_t>(0, number.base_line));
  const int16_t unit_base = std::min<int16_t>(
      unit_height, std::max<int16_t>(0, unit.base_line));
  const int16_t number_low = number_height - number_base;
  const int16_t unit_low = unit_height - unit_base;
  const int16_t baseline_low = has_unit ? std::max(number_low, unit_low)
                                        : number_low;
  const int16_t number_high = region.height - number_base;
  const int16_t unit_high = region.height - unit_base;
  const int16_t baseline_high = has_unit ? std::min(number_high, unit_high)
                                         : number_high;
  const int16_t baseline = baseline_low <= baseline_high
                               ? static_cast<int16_t>((baseline_low +
                                                       baseline_high) /
                                                      2)
                               : static_cast<int16_t>(region.height / 2);
  group.number.y = region.y + std::clamp<int16_t>(
      baseline - (number_height - number_base), 0,
      static_cast<int16_t>(region.height - number_height));
  if (has_unit) {
    group.unit.y = region.y + std::clamp<int16_t>(
        baseline - (unit_height - unit_base), 0,
        static_cast<int16_t>(region.height - unit_height));
  }

  group.fits = number.width > 0 && number.height > 0 &&
               number.width + safe_gap + measured_unit_width <= region.width &&
               number.height <= region.height &&
               (!has_unit || (unit.width > 0 && unit.height > 0 &&
                              unit.width <= region.width &&
                              unit.height <= region.height &&
                              std::max<int16_t>(number_height - number_base,
                                                unit_height - unit_base) +
                                      std::max<int16_t>(number_base, unit_base) <=
                                  region.height));
  return group;
}

uint8_t selectCuckooPiFontSize(const CuckooPiFontMeasurement* candidates,
                               size_t candidate_count, uint8_t preferred_size,
                               int16_t max_width, int16_t max_height) {
  if (!candidates || candidate_count == 0) return 0;
  uint8_t fallback = 0;
  uint8_t selected = 0;
  for (size_t i = 0; i < candidate_count; ++i) {
    const CuckooPiFontMeasurement& candidate = candidates[i];
    if (candidate.point_size == 0 || candidate.point_size > preferred_size)
      continue;
    if (fallback == 0 || candidate.point_size < fallback)
      fallback = candidate.point_size;
    if (candidate.metrics.width <= max_width &&
        candidate.metrics.height <= max_height &&
        (selected == 0 || candidate.point_size > selected)) {
      selected = candidate.point_size;
    }
  }
  return selected != 0 ? selected : fallback;
}

bool formatCuckooPiRuntime(uint32_t seconds, char* output, size_t output_size) {
  if (!output || output_size == 0) return false;
  const uint32_t days = seconds / 86400U;
  const uint32_t hours = (seconds % 86400U) / 3600U;
  const uint32_t minutes = (seconds % 3600U) / 60U;
  const uint32_t remaining = seconds % 60U;
  int written = 0;
  if (days > 0) {
    written = std::snprintf(output, output_size, "%lud %luh",
                            static_cast<unsigned long>(days),
                            static_cast<unsigned long>(hours));
  } else if (hours > 0) {
    written = std::snprintf(output, output_size, "%luh %lum",
                            static_cast<unsigned long>(hours),
                            static_cast<unsigned long>(minutes));
  } else if (minutes > 0) {
    written = std::snprintf(output, output_size, "%lum %lus",
                            static_cast<unsigned long>(minutes),
                            static_cast<unsigned long>(remaining));
  } else {
    written = std::snprintf(output, output_size, "%lus",
                            static_cast<unsigned long>(remaining));
  }
  return written >= 0 && static_cast<size_t>(written) < output_size;
}

bool formatCuckooPiStateLabel(bool portrait, const char* state, char* output,
                              size_t output_size) {
  if (!output || output_size == 0) return false;
  const char* value = state && state[0] ? state : "--";
  const int written = std::snprintf(
      output, output_size,
      portrait ? "STATE\n%s" : "STATE %s", value);
  return written >= 0 && static_cast<size_t>(written) < output_size;
}

bool formatCuckooPiRuntimeLabel(bool portrait, bool has_runtime,
                                uint32_t seconds, char* output,
                                size_t output_size) {
  if (!output || output_size == 0) return false;
  char runtime[24] = "--";
  if (has_runtime &&
      !formatCuckooPiRuntime(seconds, runtime, sizeof(runtime))) {
    return false;
  }
  int written = 0;
  if (portrait) {
    written = std::snprintf(output, output_size, "RUN TIME\n%s", runtime);
  } else if (has_runtime) {
    written = std::snprintf(output, output_size, "RUN TIME %s", runtime);
  } else {
    written = std::snprintf(output, output_size, "RUN TIME --");
  }
  return written >= 0 && static_cast<size_t>(written) < output_size;
}

bool formatCuckooPiMetricLabel(const char* label, bool multiline,
                               bool has_value, float value, const char* unit,
                               char* output, size_t output_size) {
  if (!label || !output || output_size == 0) return false;
  if (!has_value) {
    const int written = std::snprintf(output, output_size,
                                      "%s%s--", label,
                                      multiline ? "\n" : "  ");
    return written >= 0 && static_cast<size_t>(written) < output_size;
  }
  const int written = std::snprintf(
      output, output_size, "%s%s%.2f %s", label,
      multiline ? "\n" : "  ", static_cast<double>(value), unit ? unit : "");
  return written >= 0 && static_cast<size_t>(written) < output_size;
}
