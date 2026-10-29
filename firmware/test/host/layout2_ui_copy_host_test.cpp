#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {
int failures = 0;

std::string readFile(const char* path) {
  std::ifstream input(path);
  if (!input.good()) {
    std::cerr << "cannot open " << path << "\n";
    ++failures;
    return {};
  }
  std::ostringstream contents;
  contents << input.rdbuf();
  return contents.str();
}

void require(const std::string& source, const char* token) {
  if (source.find(token) == std::string::npos) {
    std::cerr << "missing requirement: " << token << "\n";
    ++failures;
  }
}

void reject(const std::string& source, const char* token) {
  if (source.find(token) != std::string::npos) {
    std::cerr << "forbidden legacy token remains: " << token << "\n";
    ++failures;
  }
}

std::string functionBody(const std::string& source, const char* signature) {
  const std::string::size_type signature_pos = source.find(signature);
  if (signature_pos == std::string::npos) return {};
  const std::string::size_type body_start = source.find('{', signature_pos);
  if (body_start == std::string::npos) return {};
  int depth = 0;
  for (std::string::size_type i = body_start; i < source.size(); ++i) {
    if (source[i] == '{') ++depth;
    if (source[i] == '}' && --depth == 0) {
      return source.substr(body_start, i - body_start + 1);
    }
  }
  return {};
}

void requireLvglLoopOrder(const std::string& display) {
  const std::string body = functionBody(display, "void lvgl_loop_handler(");
  if (body.empty()) {
    std::cerr << "cannot parse lvgl_loop_handler body\n";
    ++failures;
    return;
  }
  const auto take = body.find("xSemaphoreTake(");
  const auto lvgl = body.find("lv_task_handler()");
  const auto give = body.find("xSemaphoreGive(");
  const auto poll = body.find("myDisplay.pollOrientation()");
  if (take == std::string::npos || lvgl == std::string::npos ||
      give == std::string::npos || poll == std::string::npos ||
      !(take < lvgl && lvgl < give && give < poll)) {
    std::cerr << "lvgl_loop_handler must take semaphore, run LVGL, release, "
                 "then poll orientation\n";
    ++failures;
  }
}
}  // namespace

int main() {
  const std::string ui = readFile("src/ui_gravitymon_gateway.cpp");
  const std::string display = readFile("src/display.cpp");
  const std::string display_header = readFile("src/display.hpp");
  const std::string orientation = readFile("src/gateway_orientation.hpp");
  const std::string state = readFile("src/gateway_settings_state.hpp");
  const std::string layout = readFile("src/gateway_settings_layout.cpp");

  require(orientation, "GATEWAY_DEFAULT_ORIENTATION");
  require(orientation, "GatewayOrientation::Portrait");
  require(orientation, "gatewayInitialOrientationForLayout");
  require(orientation, "gatewayAutoRotationEnabledForLayout");
  require(display_header, "_orientation = GATEWAY_DEFAULT_ORIENTATION");
  require(display_header, "{GATEWAY_DEFAULT_ORIENTATION}");
  require(display, "gatewayDisplayRotation(_orientation)");
  require(display,
          "gatewayAutoRotationEnabledForLayout(gravitymon_gateway_get_layout())");
  require(display, "_orientation = gatewayInitialOrientationForLayout(layoutId)");
  require(display, "_orientationDebouncer = GatewayOrientationDebouncer");
  reject(display, "_tft->setRotation(1)");
  reject(display, "lv_display_set_rotation(_display, LV_DISPLAY_ROTATION_90)");
  require(ui, "gravitymon_gateway_init_common(lv_disp_t* disp, bool darkmode,");
  require(ui, "uint8_t layout_id)");
  require(ui, "g_state.orientation = gatewayInitialOrientationForLayout(layout_id)");
  require(ui, "gatewayInitialOrientationForLayout");
  require(ui, "kGatewayLayoutCount");
  require(ui, "layout_mgr.total_layouts = kGatewayLayoutCount");
  require(ui, "layout_id = gatewayNormalizeLayoutId(layout_id);");
  reject(ui, "g_state.orientation = GatewayOrientation::Landscape");
  reject(ui, "320x240 landscape");
  require(ui, "lv_display_get_horizontal_resolution");
  require(ui, "lv_display_get_vertical_resolution");

  require(state, "enum class SettingsPage");
  require(state, "Home");
  require(state, "Og");
  require(state, "TemperatureChange");
  require(state, "RingColor");
  require(state, "OfflineAlarm");
  reject(state, "String");

  require(layout, "const int16_t baseWidth");
  require(layout, "const int16_t baseHeight");
  require(layout, "scaleGatewaySettingsRect(0, 0, 320, 36)");
  require(layout, "scaleGatewaySettingsRect(0, 0, 240, 40)");
  require(layout, "scaleGatewaySettingsRect(4, 38, 154, 75)");
  require(layout, "scaleGatewaySettingsRect(4, 42, 114, 111)");

  require(ui, "#include <gateway_settings_layout.hpp>");
  require(ui, "#include <gateway_settings_state.hpp>");
  require(ui, "#include <gateway_rotation_teardown.hpp>");
  require(ui, "GatewaySettingsUiState settings_ui");
  require(ui, "calculateGatewaySettingsLayout(");
  require(ui, "create_settings_home_cell");
  require(ui, "LV_EVENT_LONG_PRESSED");
  require(ui, "gravitymon_gateway_open_settings();");
  require(ui, "!g_state.data_device_id[0]");
  require(ui, "OG SETTING");
  require(ui, "TEMP CHANGE");
  require(ui, "RING COLOR");
  require(ui, "OFFLINE ALARM");
  require(ui, "NOT SET");
  require(ui, "BLE DEFAULT");
  require(ui, "YELLOW");
  require(ui, "PURPLE");
  require(ui, "RED");
  require(ui, "GREEN");
  require(ui, "BLUE");
  require(ui, "WHITE");
  require(ui, "BLACK");
  require(ui, "lv_color_hex(RING_COLOR_DEFAULT_HEX)");
  reject(ui, "GOLD");
  reject(ui, "ORANGE");
  reject(ui, "SILVER");
  require(ui, "SAVE FAILED");
  require(ui, "SettingsPage::Home");
  require(ui, "SettingsPage::Og");
  require(ui, "SettingsPage::TemperatureChange");
  require(ui, "SettingsPage::RingColor");
  require(ui, "SettingsPage::OfflineAlarm");
  require(ui, "myConfig.saveDeviceSettings(settings)");
  require(ui, "gateway_settings_copy_display_device_id(device_id");
  require(ui, "g_state.data_device_id)");
  require(ui, "g_state.settings_ui.clear()");
  require(ui, "lv_obj_set_scrollbar_mode");
  require(ui, "LV_SCROLLBAR_MODE_OFF");
  require(ui, "lv_indev_reset(nullptr, nullptr)");
  require(ui, "runGatewayRotationTeardown(teardown)");
  reject(ui, "lv_indev_get_act");
  require(ui, "reset_gateway_widget_pointers");
  require(ui, "g_state = gravitymon_state_t{}");

  reject(ui, "memset(&g_state");
  reject(ui, "GRAVITY UNIT");
  reject(ui, "TEMP UNIT");
  reject(ui, "TEMP ALARM HIGH");
  reject(ui, "TEMP ALARM LOW");
  reject(ui, "LOW BATTERY");
  reject(ui, "BUZZER");
  reject(ui, "AUTO SWITCH");
  reject(ui, "settings_draft");
  reject(ui,
         "snprintf(g_state.data_device_id, sizeof(g_state.data_device_id)");
  require(display, "constexpr uint32_t kDashboardSettingsLongPressMs = 1200");
  require(display,
          "lv_indev_set_long_press_time(indev, kDashboardSettingsLongPressMs)");
  reject(display, "lv_indev_set_long_press_time(indev, 3000)");
  require(display, "gatewayRotationPointerPressed()");
  require(display, "gatewayRotationPointerReleased()");
  require(ui, "gatewayRotationPointerLost()");
  require(ui, "LV_EVENT_PRESS_LOST");
  require(ui, "gatewayRotationSettingsOpened()");
  require(ui, "gatewayRotationSettingsClosed()");
  require(ui, "clearDecorativeClickable");
  require(ui, "LV_EVENT_RELEASED");
  require(ui, "nullptr);");
  reject(ui, "GatewayRotationUiContext");
  reject(ui, "&rotation");
  require(display, "GATEWAY_ORIENTATION_DIAGNOSTICS");
  require(display, "raw=");
  require(display, "transformed=");
  require(display, "lvgl_rotation=");
  require(display, "pressed=");
  require(display, "released=");
  requireLvglLoopOrder(display);
  const std::string touch_handler =
      functionBody(display, "void touchScreenHandler(");
  require(touch_handler, "gatewayRotationPointerPressed()");
  require(touch_handler, "LV_INDEV_STATE_PRESSED");

  const std::string set_layout = functionBody(display, "void Display::setLayout(");
  const auto gateway35_branch_start = set_layout.find("#if defined(GATEWAY_35)");
  const auto legacy_branch_start = set_layout.find("#else", gateway35_branch_start);
  if (gateway35_branch_start == std::string::npos ||
      legacy_branch_start == std::string::npos) {
    std::cerr << "setLayout must retain both 3.5 and legacy branches\n";
    ++failures;
  } else {
    const std::string gateway35_branch = set_layout.substr(
        gateway35_branch_start, legacy_branch_start - gateway35_branch_start);
    const std::string legacy_branch = set_layout.substr(legacy_branch_start);
    require(gateway35_branch, "plan.target_orientation");
    require(gateway35_branch, "_orientation = plan.target_orientation");
    require(gateway35_branch, "_layoutId = layoutId");
    require(gateway35_branch, "gravitymon_gateway_set_layout(layoutId)");
    reject(gateway35_branch, "_tft->setRotation(");
    reject(gateway35_branch, "lv_display_set_rotation(");

    require(legacy_branch, "gatewayDisplayRotation(_orientation)");
    require(legacy_branch, "_tft->setRotation(rotation.tft_rotation)");
    require(legacy_branch, "lv_display_set_rotation(");
    require(legacy_branch, "gravitymon_gateway_set_layout(layoutId)");
  }
  require(set_layout, "_orientationDebouncer = GatewayOrientationDebouncer(_orientation)");
  require(set_layout, "if (!gravitymon_gateway_set_layout(layoutId))");
  require(set_layout, "old_orientation");
  require(set_layout, "old_rotation");
  require(ui, "bool gravitymon_gateway_set_layout");
  require(ui, "bool gravitymon_gateway_set_orientation");
  require(ui, "gatewayPlanLayoutTransition");
  require(ui, "gatewayCommitLayoutTransition");
  require(display, "gatewayPlanLayoutTransition");
  require(display, "if (!gravitymon_gateway_set_orientation(accepted))");
  require(display, "gatewayNormalizeLayoutId");
  require(ui, "bool gravitymon_gateway_cleanup_layout");

  const std::string poll_orientation =
      functionBody(display, "void Display::pollOrientation(");
  const auto poll_rotation_apply =
      poll_orientation.find("applyDisplayRotation(accepted)");
  const auto poll_ui_commit =
      poll_orientation.find("gravitymon_gateway_set_orientation(accepted)");
  if (poll_rotation_apply == std::string::npos ||
      poll_ui_commit == std::string::npos ||
      poll_rotation_apply > poll_ui_commit) {
    std::cerr << "pollOrientation must apply rotation before UI commit\n";
    ++failures;
  }
  require(poll_orientation, "old_orientation");
  require(poll_orientation, "applyDisplayRotation(old_orientation)");
  require(poll_orientation,
          "_orientationDebouncer = GatewayOrientationDebouncer(_orientation)");
  if (set_layout.find("gravitymon_gateway_set_orientation(_orientation)") !=
      std::string::npos) {
    std::cerr << "setLayout must not queue a separate UI orientation update\n";
    ++failures;
  }
  const std::string ui_set_layout =
      functionBody(ui, "bool gravitymon_gateway_set_layout(");
  const auto layout_cleanup =
      ui_set_layout.find("gravitymon_gateway_cleanup_layout(");
  const auto settings_clear = ui_set_layout.find("g_state.settings_ui.clear()");
  if (layout_cleanup == std::string::npos || settings_clear == std::string::npos ||
      settings_clear < layout_cleanup) {
    std::cerr << "setLayout must preserve settings state until cleanup succeeds\n";
    ++failures;
  }
  return failures == 0 ? 0 : 1;
}
