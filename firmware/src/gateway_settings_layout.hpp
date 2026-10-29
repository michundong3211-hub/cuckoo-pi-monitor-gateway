#pragma once

#include <cstdint>

struct GatewaySettingsRect {
  int16_t x = 0;
  int16_t y = 0;
  int16_t width = 0;
  int16_t height = 0;
};

struct GatewaySettingsLayout {
  GatewaySettingsRect screen{};
  GatewaySettingsRect header{};
  GatewaySettingsRect header_title{};
  GatewaySettingsRect header_device{};
  GatewaySettingsRect content{};
  GatewaySettingsRect footer{};
  GatewaySettingsRect home_cells[4]{};
  GatewaySettingsRect home_back{};
  GatewaySettingsRect detail_back{};
  GatewaySettingsRect detail_save{};
  GatewaySettingsRect toggle{};
  GatewaySettingsRect minus_buttons[2]{};
  GatewaySettingsRect plus_buttons[2]{};
  GatewaySettingsRect value_fields[2]{};
  GatewaySettingsRect row_hints[2]{};
  GatewaySettingsRect og_hint{};
  GatewaySettingsRect ring_swatches[8]{};
  GatewaySettingsRect ring_summary{};
};

GatewaySettingsLayout calculateGatewaySettingsLayout(int16_t width,
                                                      int16_t height);
