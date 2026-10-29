#include <assert.h>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

#if defined(_WIN32)
#include <windows.h>
#endif

#include "../../src/gateway_ui_style.hpp"

namespace {

std::string read_file(const char* path) {
  std::ifstream input(path);
  std::ostringstream contents;
  contents << input.rdbuf();
  return contents.str();
}

std::string read_utf8_file(const char* path) {
#if defined(_WIN32)
  const int wide_length = MultiByteToWideChar(
      CP_UTF8, 0, path, -1, nullptr, 0);
  assert(wide_length > 0);
  std::wstring wide_path(static_cast<std::size_t>(wide_length - 1), L'\0');
  assert(MultiByteToWideChar(CP_UTF8, 0, path, -1, wide_path.data(),
                            wide_length) == wide_length);
  FILE* input = _wfopen(wide_path.c_str(), L"rb");
  assert(input != nullptr);
  assert(fseek(input, 0, SEEK_END) == 0);
  const long size = ftell(input);
  assert(size >= 0);
  assert(fseek(input, 0, SEEK_SET) == 0);
  std::string contents(static_cast<std::size_t>(size), '\0');
  assert(contents.empty() ||
         fread(contents.data(), 1, contents.size(), input) == contents.size());
  fclose(input);
  return contents;
#else
  return read_file(path);
#endif
}

std::string function_body(const std::string& source,
                          const std::string& signature) {
  const std::size_t start = source.find(signature);
  assert(start != std::string::npos);
  const std::size_t open = source.find('{', start);
  assert(open != std::string::npos);
  int depth = 0;
  for (std::size_t i = open; i < source.size(); ++i) {
    if (source[i] == '{') {
      ++depth;
    } else if (source[i] == '}') {
      --depth;
      if (depth == 0) return source.substr(start, i - start + 1);
    }
  }
  assert(false && "unterminated function body");
  return {};
}

std::string ini_section(const std::string& source, const std::string& header) {
  const std::size_t start = source.find(header);
  assert(start != std::string::npos);
  const std::size_t next = source.find("\n[", start + header.size());
  return source.substr(start, next == std::string::npos ? std::string::npos
                                                          : next - start);
}

std::size_t count_occurrences(const std::string& source,
                              const std::string& needle) {
  assert(!needle.empty());
  std::size_t count = 0;
  std::size_t offset = 0;
  while ((offset = source.find(needle, offset)) != std::string::npos) {
    ++count;
    offset += needle.size();
  }
  return count;
}

bool rectangles_overlap(const GatewayUiRect& first,
                        const GatewayUiRect& second) {
  return first.x < second.x + second.width &&
         second.x < first.x + first.width &&
         first.y < second.y + second.height &&
         second.y < first.y + first.height;
}

void testLegacyStylePreserves28UiProportions() {
  const GatewayUiStyle style = gatewayUiStyle(false);
  assert(style.dashboard_header_primary_font == 20);
  assert(style.dashboard_header_secondary_font == 14);
  assert(style.dashboard_battery_font == 12);
  assert(style.dashboard_progress_font == 40);
  assert(style.dashboard_measurement_font == 40);
  assert(style.dashboard_metric_font == 14);
  assert(style.dashboard_footer_font == 12);
  assert(style.dashboard_arc_width == 12);
  assert(style.battery_border_width == 2);
  assert(style.divider_min_width == 1);
  assert(style.alert_title_font == 20);
  assert(style.alert_device_font == 16);
  assert(style.alert_button_font == 14);
  assert(style.alert_border_width == 2);
  assert(style.settings_title_font == 14);
  assert(style.settings_device_font == 12);
  assert(style.settings_button_font == 14);
  assert(style.settings_value_font == 20);
  assert(style.settings_hint_font == 12);
  assert(style.settings_summary_font == 14);
  assert(style.settings_error_font == 12);
}

void testWaveshare35StyleUsesDashboardVisualHierarchy() {
  const GatewayUiStyle style = gatewayUiStyle(true);
  assert(style.dashboard_header_primary_font == 20);
  assert(style.dashboard_header_secondary_font == 16);
  assert(style.dashboard_battery_font == 14);
  assert(style.dashboard_progress_font == 48);
  assert(style.dashboard_measurement_font == 72);
  assert(style.dashboard_metric_font == 20);
  assert(style.dashboard_footer_font == 16);
  assert(style.dashboard_arc_width == 20);
  assert(style.battery_border_width == 3);
  assert(style.divider_min_width == 2);
  assert(style.alert_title_font == 28);
  assert(style.alert_device_font == 20);
  assert(style.alert_button_font == 20);
  assert(style.alert_border_width == 3);
  assert(style.settings_title_font == 20);
  assert(style.settings_device_font == 16);
  assert(style.settings_button_font == 16);
  assert(style.settings_value_font == 28);
  assert(style.settings_hint_font == 16);
  assert(style.settings_summary_font == 20);
  assert(style.settings_error_font == 14);
}

void testDashboardLayoutGuardsLargeDisplayGeometry() {
  const std::string source = read_file("src/ui_gravitymon_gateway.cpp");
  const std::string platform = read_file("platformio.ini");

  assert(platform.find("build_flags_lvgl =") != std::string::npos);
  assert(platform.find("build_flags_lvgl_gateway35 =") != std::string::npos);
  assert(platform.find("LV_FONT_MONTSERRAT_48=1") != std::string::npos);
  assert(platform.find("LV_FONT_MONTSERRAT_56=1") == std::string::npos);
  assert(platform.find("LV_FONT_MONTSERRAT_48=1\n\t-D LV_FONT_MONTSERRAT_48=1") ==
         std::string::npos);
  assert(source.find("gatewayBatteryFillWidth") != std::string::npos);
  assert(source.find("create_layout_2_dashboard_arc(scr, 24, 40, 120)") !=
         std::string::npos);
  assert(source.find("#if defined(LV_FONT_MONTSERRAT_48)") !=
         std::string::npos);
  assert(source.find("LV_LABEL_LONG_CLIP") != std::string::npos);
  assert(source.find("wid_dashboard_arc_outline") == std::string::npos);
  assert(source.find("dashboard_arc_outline_width") == std::string::npos);
  assert(source.find("dashboard_measurement_font") != std::string::npos);
  assert(source.find("dashboard_progress_font") != std::string::npos);
  assert(source.find("lv_obj_set_style_transform_scale") == std::string::npos);
  assert(source.find("LV_FONT_DECLARE(lv_font_montserrat_72_digits)") !=
         std::string::npos);
  assert(source.find("return &lv_font_montserrat_72_digits") !=
         std::string::npos);

  const std::string measurement_font =
      read_file("src/lv_font_montserrat_72_digits.c");
  assert(measurement_font.find(
             "const lv_font_t lv_font_montserrat_72_digits") !=
         std::string::npos);
  assert(measurement_font.find(".bitmap_format = 0") !=
         std::string::npos);

  const std::string cleanup = read_file("src/ui_gravitymon_gateway.cpp");
  assert(cleanup.find("wid_dashboard_arc_outline") == std::string::npos);
}

void testBatteryFillCannotCrossBodyFrame() {
  assert(gatewayBatteryFillWidth(42, 3, 3, 100) == 30);
  assert(gatewayBatteryFillWidth(42, 3, 3, 69) <= 30);
  assert(gatewayBatteryFillWidth(42, 3, 3, 0) == 0);
}

void testPortraitHeaderUsesEvenGapsWithoutBatteryOverlap() {
  const GatewayDashboardHeaderLayout layout =
      gatewayDashboardHeaderLayout(320, 480, true);

  assert(layout.header_height == 51);
  assert(layout.name.x == 21);
  assert(layout.name.width == 104);
  assert(layout.source.x == 131);
  assert(layout.source.width == 53);
  assert(layout.index.x == 189);
  assert(layout.index.width == 45);

  assert(layout.battery_body.x == 240);
  assert(layout.battery_body.y == 16);
  assert(layout.battery_body.width == 37);
  assert(layout.battery_body.height == 19);
  assert(layout.battery_tip.x == 277);
  assert(layout.battery_tip.y == 21);
  assert(layout.battery_tip.width == 3);
  assert(layout.battery_tip.height == 8);
  assert(layout.battery_percentage.x == 285);
  assert(layout.battery_percentage.width == 35);

  assert(layout.source.x - (layout.name.x + layout.name.width) >= 5);
  assert(layout.index.x - (layout.source.x + layout.source.width) >= 5);
  assert(layout.battery_body.x - (layout.index.x + layout.index.width) >= 5);
  assert(layout.battery_percentage.x -
             (layout.battery_tip.x + layout.battery_tip.width) >=
         5);
  assert(layout.battery_body.width == layout.battery_body.height * 2 - 1);
  assert(layout.battery_body.y * 2 + layout.battery_body.height ==
         layout.header_height);
}

void testHydrometerPageHasDedicatedSquareLayout() {
  const GatewayHydrometerPageLayout layout =
      gatewayHydrometerPageLayout240();
  assert(layout.header.header_height == 29);
  assert(layout.header.name.x == 12);
  assert(layout.header.index.x == 105);
  assert(layout.header.battery_body.x == 181);
  assert(layout.gravity.x == 12 && layout.gravity.y == 38);
  assert(layout.gravity.width == 216 && layout.gravity.height == 100);
  assert(layout.metrics_divider.x == 18 && layout.metrics_divider.y == 146);
  assert(layout.metrics_divider.width == 204 &&
         layout.metrics_divider.height == 1);
  assert(layout.temperature.x == 22 && layout.temperature.y == 153);
  assert(layout.temperature.width == 105 && layout.temperature.height == 35);
  assert(layout.rssi.x == 139 && layout.rssi.y == 153);
  assert(layout.rssi.width == 90 && layout.rssi.height == 35);
  assert(layout.ip_panel.x == 10 && layout.ip_panel.y == 194);
  assert(layout.ip_panel.width == 220 && layout.ip_panel.height == 35);
  assert(layout.gateway_ip.x == 16 && layout.gateway_ip.y == 199);
  assert(layout.gateway_ip.width == 208);
  assert(layout.gateway_ip.height == 25);
  const GatewayUiRect* regions[] = {
      &layout.header.name, &layout.header.index, &layout.header.battery_body,
      &layout.gravity, &layout.temperature, &layout.rssi,
      &layout.ip_panel, &layout.gateway_ip};
  for (const GatewayUiRect* region : regions) {
    assert(region->x >= 0 && region->y >= 0);
    assert(region->width > 0 && region->height > 0);
    assert(region->x + region->width <= 240);
    assert(region->y + region->height <= 240);
  }
  assert(layout.gateway_ip.x >= layout.ip_panel.x);
  assert(layout.gateway_ip.y >= layout.ip_panel.y);
  assert(layout.gateway_ip.x + layout.gateway_ip.width <=
         layout.ip_panel.x + layout.ip_panel.width);
  assert(layout.gateway_ip.y + layout.gateway_ip.height <=
         layout.ip_panel.y + layout.ip_panel.height);
  const GatewayUiRect* non_overlapping[] = {
      &layout.gravity, &layout.metrics_divider, &layout.temperature,
      &layout.rssi, &layout.ip_panel};
  for (std::size_t first = 0;
       first < sizeof(non_overlapping) / sizeof(non_overlapping[0]); ++first) {
    for (std::size_t second = first + 1;
         second < sizeof(non_overlapping) / sizeof(non_overlapping[0]);
         ++second) {
      assert(!rectangles_overlap(*non_overlapping[first],
                                 *non_overlapping[second]));
    }
  }
}

void testGmt154Layout2UsesSharedBackgroundAndMetricFontCentering() {
  const std::string source = read_file("src/ui_gravitymon_gateway.cpp");
  const std::string setup =
      function_body(source, "static void gravitymon_gateway_setup_layout_2(");
  const std::string dashboard = function_body(
      source, "static void create_gmt154_hydrometer_dashboard(");

  assert(setup.find("const lv_color_t kLayout2Background = "
                    "lv_color_hex(0x080B0E);") != std::string::npos);
  assert(setup.find(
             "lv_obj_set_style_bg_color(scr, kLayout2Background, LV_PART_MAIN)") !=
         std::string::npos);
  assert(setup.find("lv_obj_set_style_bg_color(g_state.gravity_root, "
                    "kLayout2Background") !=
         std::string::npos);
  assert(setup.find("lv_color_hex(0x061015)") == std::string::npos);

  assert(dashboard.find(
             "lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), LV_PART_MAIN)") !=
         std::string::npos);
  assert(dashboard.find(
             "create_dashboard_rect(\n      scr, 0, 0, 240, layout.header.header_height,") !=
         std::string::npos);
  assert(dashboard.find(
             "create_dashboard_rect(\n      scr, 0, 0, 240, layout.header.header_height,\n      lv_color_hex(0x000000))") != std::string::npos);
  assert(dashboard.find(
              "lv_obj_set_style_bg_color(ip_panel, lv_color_hex(0x000000), LV_PART_MAIN)") !=
         std::string::npos);
  assert(dashboard.find("lv_color_hex(0x12313A)") == std::string::npos);
  assert(dashboard.find("lv_color_hex(0x102B32)") == std::string::npos);
  assert(dashboard.find("lv_obj_set_style_bg_opa(ip_panel, LV_OPA_COVER") !=
         std::string::npos);
  assert(count_occurrences(dashboard, "lv_color_hex(0x2F8F78)") == 3);
  assert(dashboard.find("const lv_color_t teal = lv_color_hex(0x41DFB8);") !=
         std::string::npos);
  assert(dashboard.find("lv_color_hex(0xFFBD3F)") != std::string::npos);
  assert(dashboard.find("layout.gateway_ip, white, layout_2_font(16)") !=
         std::string::npos);
  assert(dashboard.find("gmt154_sg_font()") != std::string::npos);
  assert(dashboard.find("layout_2_font(54)") == std::string::npos);
  assert(dashboard.find("font->line_height") != std::string::npos);
  assert(dashboard.find("rect.y + (rect.height - font->line_height) / 2") !=
         std::string::npos);
  assert(dashboard.find("lv_obj_set_size(label, rect.width, "
                         "font->line_height)") != std::string::npos);
}

void testGmt154Enables48FontOnlyInItsPlatformioEnvironment() {
  const std::string platform = read_file("platformio.ini");
  const std::size_t common_flags_start = platform.find("build_flags_lvgl =");
  const std::size_t gateway35_flags_start =
      platform.find("build_flags_lvgl_gateway35 =", common_flags_start);
  assert(common_flags_start != std::string::npos);
  assert(gateway35_flags_start != std::string::npos);
  const std::string common = platform.substr(
      common_flags_start, gateway35_flags_start - common_flags_start);
  const std::string gmt154 = ini_section(platform, "[env:gmt154-06]");
  const std::string gmt154_shared =
      ini_section(platform, "[gmt154_env_data]");

  assert(common.find("LV_FONT_MONTSERRAT_48=1") == std::string::npos);
  assert(gmt154.find("${gmt154_env_data.build_flags}") !=
         std::string::npos);
  assert(gmt154_shared.find("\n\t-D LV_FONT_MONTSERRAT_48=1") !=
         std::string::npos);
  assert(common.find("LV_USE_FONT_COMPRESSED") == std::string::npos);
  assert(gmt154_shared.find("\n\t-D LV_USE_FONT_COMPRESSED=1") !=
         std::string::npos);
}

void testGmt154DashboardHidesFixedTitlesAndMeasurementUnits() {
  const std::string source = read_file("src/ui_gravitymon_gateway.cpp");
  const std::string dashboard = function_body(
      source, "static void create_gmt154_hydrometer_dashboard(");

  const char* forbidden[] = {"SPECIFIC GRAVITY", "TEMPERATURE", "RSSI",
                             "SG", "PSI", "Bar", "kPa",
                             "lbl_measurement_unit"};
  for (const char* text : forbidden) {
    assert(dashboard.find(text) == std::string::npos);
  }
  assert(dashboard.find("gmt154_sg_font()") != std::string::npos);
  assert(dashboard.find("layout_2_font(54)") == std::string::npos);
}

void testGmt154UsesReproducible54pxDigitsFontAndBlackPreview() {
  const std::string source = read_file("src/ui_gravitymon_gateway.cpp");
  const std::string font = read_file("src/lv_font_montserrat_54_digits.c");
  const std::string preview =
      read_utf8_file("../../文档/UI预览/gmt154-06-gateway-screen-preview.html");

  assert(source.find("LV_FONT_DECLARE(lv_font_montserrat_54_digits)") !=
         std::string::npos);
  assert(source.find("return &lv_font_montserrat_54_digits") !=
         std::string::npos);
  assert(font.find("Size: 54 px") != std::string::npos);
  assert(font.find("Bpp: 4") != std::string::npos);
  assert(font.find("Montserrat-Medium.ttf") != std::string::npos);
  assert(font.find("--symbols .-0123456789") != std::string::npos);
  assert(font.find("--no-kerning") != std::string::npos);
  assert(font.find("const lv_font_t lv_font_montserrat_54_digits") !=
         std::string::npos);
  assert(preview.find("background: #000000") != std::string::npos);
  assert(preview.find(".header") != std::string::npos);
  assert(preview.find(".header { position: absolute; inset: 0 0 auto; height: 29px; background: #000000; }") !=
         std::string::npos);
  assert(preview.find("background: #12313a") == std::string::npos);
  assert(preview.find(".ip-panel { position: absolute; left: 10px; top: 194px; width: 220px; height: 35px; border: 1px solid #2f8f78; border-radius: 5px; background: #000000; }") !=
         std::string::npos);
  assert(preview.find("background: #102b32") == std::string::npos);
  assert(preview.find("#2f8f78") != std::string::npos);
  assert(preview.find("font-size: 54px") != std::string::npos);
  assert(preview.find("font-size: 16px") != std::string::npos);
  assert(preview.find("IP 255.255.255.255") != std::string::npos);
}

void testGmt154KeepsTemperatureUnitsAndRssiDbm() {
  const std::string source = read_file("src/ui_gravitymon_gateway.cpp");
  const std::string temperature =
      function_body(source, "void gravitymon_gateway_set_temp(");
  assert(temperature.find("%.1f°C") != std::string::npos);
  assert(temperature.find("%.1f°F") != std::string::npos);
  assert(temperature.find("%.1f/%.1f°C") != std::string::npos);
  assert(temperature.find("%.1f/%.1f°F") != std::string::npos);

  const std::string rssi =
      function_body(source, "void gravitymon_gateway_set_rssi(");
  assert(rssi.find("\"--\"") != std::string::npos);
  assert(rssi.find("%d dBm") != std::string::npos);

  const std::string layout_loop =
      function_body(source, "static void gravitymon_gateway_loop_layout_2(");
  assert(layout_loop.find("#if defined(GMT154_06)") != std::string::npos);
  assert(layout_loop.find("g_state.data_rssi") != std::string::npos);
  assert(layout_loop.find("layout_2_copy_rssi_value") != std::string::npos);
}

}  // namespace

int main() {
  testLegacyStylePreserves28UiProportions();
  testWaveshare35StyleUsesDashboardVisualHierarchy();
  testDashboardLayoutGuardsLargeDisplayGeometry();
  testBatteryFillCannotCrossBodyFrame();
  testPortraitHeaderUsesEvenGapsWithoutBatteryOverlap();
  testHydrometerPageHasDedicatedSquareLayout();
  testGmt154Layout2UsesSharedBackgroundAndMetricFontCentering();
  testGmt154Enables48FontOnlyInItsPlatformioEnvironment();
  testGmt154DashboardHidesFixedTitlesAndMeasurementUnits();
  testGmt154UsesReproducible54pxDigitsFontAndBlackPreview();
  testGmt154KeepsTemperatureUnitsAndRssiDbm();
  return 0;
}
