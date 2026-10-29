#include "gateway_ui_style.hpp"

namespace {

int16_t scaleCoordinate(int16_t value, int16_t screen_size,
                        int16_t base_size) {
  return static_cast<int16_t>(
      (static_cast<int32_t>(value) * screen_size + base_size / 2) /
      base_size);
}

GatewayUiRect scaleRect(int16_t x, int16_t y, int16_t width, int16_t height,
                        int16_t screen_width, int16_t screen_height,
                        int16_t base_width, int16_t base_height) {
  return {
      scaleCoordinate(x, screen_width, base_width),
      scaleCoordinate(y, screen_height, base_height),
      scaleCoordinate(width, screen_width, base_width),
      scaleCoordinate(height, screen_height, base_height),
  };
}

}  // namespace

GatewayUiStyle gatewayUiStyle(bool large_display) {
  if (large_display) {
    return {
        20, 16, 14, 48, 72, 20, 16, 20, 3, 2, 28,
        20, 20, 3,  20, 16, 16, 28, 16, 20, 14,
    };
  }

  return {
      20, 14, 12, 40, 40, 14, 12, 12, 2, 1, 20,
      16, 14, 2,  14, 12, 14, 20, 12, 14, 12,
  };
}

GatewayDashboardHeaderLayout gatewayDashboardHeaderLayout(
    int16_t screen_width, int16_t screen_height, bool portrait) {
  const int16_t base_width = portrait ? 240 : 320;
  const int16_t base_height = portrait ? 320 : 240;
  const bool large_display = screen_width > base_width;
  const int16_t battery_x =
      large_display ? (portrait ? 180 : 240) : (portrait ? 184 : 244);
  const int16_t percentage_x = portrait
                                   ? (large_display ? 214 : 211)
                                   : 274;
  const int16_t percentage_width = portrait
                                       ? (large_display ? 26 : 29)
                                       : 40;
  const int16_t header_height =
      scaleCoordinate(34, screen_height, base_height);
  const int16_t battery_width =
      scaleCoordinate(28, screen_width, base_width);
  const int16_t battery_height =
      scaleCoordinate(14, screen_width, base_width);
  const int16_t battery_y = (header_height - battery_height) / 2;
  const int16_t tip_width = scaleCoordinate(2, screen_width, base_width);
  const int16_t tip_height = scaleCoordinate(6, screen_width, base_width);
  const int16_t tip_y =
      battery_y + scaleCoordinate(4, screen_width, base_width);

  GatewayDashboardHeaderLayout layout{};
  layout.header_height = header_height;
  layout.name = scaleRect(16, 0, portrait ? 78 : 100, 34, screen_width,
                          screen_height, base_width, base_height);
  layout.source = scaleRect(portrait ? 98 : 118, 0, portrait ? 40 : 42, 34,
                            screen_width, screen_height, base_width,
                            base_height);
  layout.index = scaleRect(portrait ? 142 : 165, 0, portrait ? 34 : 40, 34,
                           screen_width, screen_height, base_width,
                           base_height);
  layout.battery_body = {
      scaleCoordinate(battery_x, screen_width, base_width), battery_y,
      battery_width, battery_height};
  layout.battery_tip = {
      static_cast<int16_t>(layout.battery_body.x + battery_width), tip_y,
      tip_width, tip_height};
  layout.battery_percentage =
      scaleRect(percentage_x, 0, percentage_width, 34, screen_width,
                screen_height, base_width, base_height);
  return layout;
}

GatewayHydrometerPageLayout gatewayHydrometerPageLayout240() {
  GatewayHydrometerPageLayout layout{};
  layout.header = {
      29,
      {12, 0, 75, 29},
      {0, 0, 0, 0},
      {105, 0, 30, 29},
      {181, 8, 25, 12},
      {206, 11, 3, 6},
      {210, 0, 24, 29},
  };
  layout.header_divider = {10, 29, 220, 1};
  layout.gravity = {12, 38, 216, 100};
  layout.metrics_divider = {18, 146, 204, 1};
  layout.temperature = {22, 153, 105, 35};
  layout.rssi = {139, 153, 90, 35};
  layout.ip_panel = {10, 194, 220, 35};
  layout.gateway_ip = {16, 199, 208, 25};
  return layout;
}

int16_t gatewayBatteryFillWidth(int16_t body_width, uint8_t border_width,
                                int16_t margin, int16_t percentage) {
  if (body_width <= 0 || margin < 0) return 0;
  const int32_t drawable_width = static_cast<int32_t>(body_width) -
                                 (2 * static_cast<int32_t>(border_width)) -
                                 (2 * static_cast<int32_t>(margin));
  if (drawable_width <= 0 || percentage <= 0) return 0;
  const int32_t clamped_percentage = percentage >= 100 ? 100 : percentage;
  return static_cast<int16_t>((drawable_width * clamped_percentage + 50) /
                              100);
}
