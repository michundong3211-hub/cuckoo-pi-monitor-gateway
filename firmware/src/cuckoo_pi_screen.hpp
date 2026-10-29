#pragma once

#include <cstddef>
#include <cstdint>

#include "gateway_settings_layout.hpp"

struct CuckooPiScreenLayout {
  GatewaySettingsRect screen{};
  GatewaySettingsRect header{};
  GatewaySettingsRect title{};
  GatewaySettingsRect index{};
  GatewaySettingsRect online{};
  GatewaySettingsRect temperature_card{};
  GatewaySettingsRect beer_heading{};
  GatewaySettingsRect mode_card{};
  GatewaySettingsRect mode{};
  GatewaySettingsRect state{};
  GatewaySettingsRect beer{};
  GatewaySettingsRect beer_unit{};
  GatewaySettingsRect beer_set{};
  GatewaySettingsRect fridge{};
  GatewaySettingsRect fridge_set{};
  GatewaySettingsRect chiller{};
  GatewaySettingsRect runtime{};
  GatewaySettingsRect touch{};
  bool show_beer_set = true;
  bool show_fridge_temp = true;
  bool show_chiller = true;
};

struct CuckooPiTextMetrics {
  int16_t width = 0;
  int16_t height = 0;
  int16_t base_line = 0;
};

struct CuckooPiFontMeasurement {
  uint8_t point_size = 0;
  CuckooPiTextMetrics metrics{};
};

struct CuckooPiTemperatureGroup {
  GatewaySettingsRect number{};
  GatewaySettingsRect unit{};
  bool fits = false;
};

CuckooPiScreenLayout calculateCuckooPiScreenLayout(int16_t width,
                                                   int16_t height,
                                                   int mode = 2);
CuckooPiTemperatureGroup layoutCuckooPiTemperatureGroup(
    const GatewaySettingsRect& region, CuckooPiTextMetrics number,
    CuckooPiTextMetrics unit, bool has_unit, int16_t gap);
uint8_t selectCuckooPiFontSize(const CuckooPiFontMeasurement* candidates,
                               size_t candidate_count, uint8_t preferred_size,
                               int16_t max_width, int16_t max_height);
bool formatCuckooPiRuntime(uint32_t seconds, char* output, size_t output_size);
bool formatCuckooPiStateLabel(bool portrait, const char* state, char* output,
                              size_t output_size);
bool formatCuckooPiRuntimeLabel(bool portrait, bool has_runtime,
                                uint32_t seconds, char* output,
                                size_t output_size);
bool formatCuckooPiMetricLabel(const char* label, bool multiline,
                               bool has_value, float value, const char* unit,
                               char* output, size_t output_size);
