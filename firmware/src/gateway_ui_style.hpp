#pragma once

#include <stdint.h>

struct GatewayUiStyle {
  uint8_t dashboard_header_primary_font;
  uint8_t dashboard_header_secondary_font;
  uint8_t dashboard_battery_font;
  uint8_t dashboard_progress_font;
  uint8_t dashboard_measurement_font;
  uint8_t dashboard_metric_font;
  uint8_t dashboard_footer_font;
  uint8_t dashboard_arc_width;
  uint8_t battery_border_width;
  uint8_t divider_min_width;
  uint8_t alert_title_font;
  uint8_t alert_device_font;
  uint8_t alert_button_font;
  uint8_t alert_border_width;
  uint8_t settings_title_font;
  uint8_t settings_device_font;
  uint8_t settings_button_font;
  uint8_t settings_value_font;
  uint8_t settings_hint_font;
  uint8_t settings_summary_font;
  uint8_t settings_error_font;
};

struct GatewayUiRect {
  int16_t x;
  int16_t y;
  int16_t width;
  int16_t height;
};

struct GatewayDashboardHeaderLayout {
  int16_t header_height;
  GatewayUiRect name;
  GatewayUiRect source;
  GatewayUiRect index;
  GatewayUiRect battery_body;
  GatewayUiRect battery_tip;
  GatewayUiRect battery_percentage;
};

struct GatewayHydrometerPageLayout {
  GatewayDashboardHeaderLayout header;
  GatewayUiRect header_divider;
  GatewayUiRect gravity;
  GatewayUiRect metrics_divider;
  GatewayUiRect temperature;
  GatewayUiRect rssi;
  GatewayUiRect ip_panel;
  GatewayUiRect gateway_ip;
};

GatewayUiStyle gatewayUiStyle(bool large_display);
GatewayDashboardHeaderLayout gatewayDashboardHeaderLayout(
    int16_t screen_width, int16_t screen_height, bool portrait);
GatewayHydrometerPageLayout gatewayHydrometerPageLayout240();

// Return a fill width that stays inside the battery body's drawable area.
int16_t gatewayBatteryFillWidth(int16_t body_width, uint8_t border_width,
                                int16_t margin, int16_t percentage);
