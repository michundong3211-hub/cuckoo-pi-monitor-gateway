#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "../../src/gateway_orientation.hpp"

#if defined(__has_include)
#if __has_include("../../src/gmt154_cuckoo_temperature_policy.hpp")
#include "../../src/gmt154_cuckoo_temperature_policy.hpp"
#define HAS_GMT154_CUCKOO_TEMPERATURE_POLICY 1
#else
#define HAS_GMT154_CUCKOO_TEMPERATURE_POLICY 0
#endif
#else
#define HAS_GMT154_CUCKOO_TEMPERATURE_POLICY 0
#endif

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << "\n";
    ++failures;
  }
}

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

void requireBefore(const std::string& body, const char* first,
                   const char* later, const char* message) {
  const auto first_pos = body.find(first);
  const auto later_pos = body.find(later);
  expect(first_pos != std::string::npos && later_pos != std::string::npos &&
             first_pos < later_pos,
         message);
}

void testGmt154LayoutPolicy() {
  const uint8_t inputs[] = {0, 1, 2, 255};
  for (const uint8_t input : inputs) {
    expect(gatewayNormalizeLayoutId(input) == 2,
           "GMT154 layout IDs normalize to layout 2");
    expect(gatewayInitialOrientationForLayout(input) ==
               GatewayOrientation::Landscape,
           "GMT154 layout IDs keep logical Landscape");
    expect(!gatewayAutoRotationEnabledForLayout(input),
           "GMT154 layout IDs disable auto rotation");
  }

  const GatewayDisplayRotation landscape_rotation =
      gatewayDisplayRotation(GatewayOrientation::Landscape);
  const GatewayDisplayRotation portrait_rotation =
      gatewayDisplayRotation(GatewayOrientation::Portrait);
  expect(landscape_rotation.tft_rotation == 0 &&
             landscape_rotation.lvgl_rotation_degrees == 0,
         "GMT154 Landscape rotation is 0/0");
  expect(portrait_rotation.tft_rotation == 0 &&
             portrait_rotation.lvgl_rotation_degrees == 0,
         "GMT154 Portrait rotation is 0/0");
}

void testEntrypointsNormalizeBeforeLayoutUse() {
  const std::string display = readFile("src/display.cpp");
  const std::string ui = readFile("src/ui_gravitymon_gateway.cpp");
  const std::string create_ui = functionBody(display, "void Display::createUI(");
  const std::string gateway_init =
      functionBody(ui, "void gravitymon_gateway_init(");

  requireBefore(create_ui, "layoutId = gatewayNormalizeLayoutId(layoutId);",
                "gatewayInitialOrientationForLayout(layoutId)",
                "Display::createUI must normalize before reading layout ID");
  requireBefore(create_ui, "layoutId = gatewayNormalizeLayoutId(layoutId);",
                "gravitymon_gateway_init(",
                "Display::createUI must normalize before UI init");
  requireBefore(
      gateway_init, "layout_id = gatewayNormalizeLayoutId(layout_id);",
      "gatewayRotationReleaseLocks()",
      "gravitymon_gateway_init must normalize at entry");
  expect(gateway_init.find("if (layout_id >= kGatewayLayoutCount)") ==
             std::string::npos,
         "gravitymon_gateway_init must replace the old clamp");
}

void testGmt154CuckooPiObjectsAreNotCreatedOrRefreshed() {
  const std::string ui = readFile("src/ui_gravitymon_gateway.cpp");
  const std::string create =
      functionBody(ui, "static void create_cuckoo_dashboard(");
  const std::string refresh =
      functionBody(ui, "static void gravitymon_gateway_loop_cuckoo(");

  requireBefore(
      create, "#if !defined(GMT154_06)\n  g_state.cuckoo_index =",
      "#endif\n  g_state.cuckoo_value[0] =",
      "GMT154 must not create the Cuckoo Pi index object");
  const auto runtime_geometry_guard =
      create.find("#if !defined(GMT154_06)");
  const auto runtime_geometry =
      create.find("create_dashboard_rect(root, layout.runtime",
                   runtime_geometry_guard);
  const auto runtime_geometry_guard_end =
      create.find("#endif", runtime_geometry);
  expect(runtime_geometry_guard != std::string::npos &&
             runtime_geometry != std::string::npos &&
             runtime_geometry_guard_end != std::string::npos &&
             runtime_geometry < runtime_geometry_guard_end,
         "GMT154 must not create the runtime geometry object");
  const auto runtime_pair_guard = create.find(
      "#if !defined(GMT154_06)\n  create_cuckoo_colored_pair(");
  const auto runtime_pair = create.find("layout.runtime", runtime_pair_guard);
  const auto runtime_pair_guard_end = create.find("#endif", runtime_pair);
  expect(runtime_pair_guard != std::string::npos &&
             runtime_pair != std::string::npos &&
             runtime_pair_guard_end != std::string::npos &&
             runtime_pair < runtime_pair_guard_end,
         "GMT154 must not create the runtime label/value objects");

  requireBefore(
      refresh, "#if !defined(GMT154_06)\n  if (g_state.cuckoo_index)",
      "#endif\n  if (g_state.cuckoo_value[0])",
      "GMT154 must not refresh the Cuckoo Pi index object");

  const auto runtime_guard = refresh.find(
      "#if !defined(GMT154_06)\n  if (snapshot.has_status_time)");
  const auto runtime_update =
      refresh.find("if (g_state.cuckoo_value[7])", runtime_guard);
  const auto runtime_guard_end = refresh.find("\n#endif", runtime_update);
  expect(runtime_guard != std::string::npos &&
             runtime_update != std::string::npos &&
             runtime_guard_end != std::string::npos &&
             runtime_update < runtime_guard_end,
         "GMT154 must not refresh the runtime label/value objects");

  const char* retained_mqtt_values[] = {
      "g_state.cuckoo_value[0]", "g_state.cuckoo_value[1]",
      "g_state.cuckoo_value[2]", "g_state.cuckoo_value[3]",
      "g_state.cuckoo_value[4]", "g_state.cuckoo_value[6]"};
  for (const char* value : retained_mqtt_values) {
    expect(refresh.find(value) != std::string::npos,
           "GMT154 must retain the real MQTT value refresh path");
  }
}

void testGmt154ThermostatRenderingContract() {
  const std::string ui = readFile("src/ui_gravitymon_gateway.cpp");
  const std::string create =
      functionBody(ui, "static void create_cuckoo_dashboard(");
  const std::string refresh =
      functionBody(ui, "static void gravitymon_gateway_loop_cuckoo(");
  const std::string temperature =
      functionBody(ui, "static void set_cuckoo_temperature_group(");

  expect(create.find("const lv_color_t screen = lv_color_hex(0x000000);") !=
             std::string::npos,
         "GMT154 thermostat screen must use pure black");
  expect(create.find("const lv_color_t panel = lv_color_hex(0x000000);") !=
             std::string::npos,
         "GMT154 thermostat header must use pure black");
  expect(create.find("lv_color_hex(0x061319)") == std::string::npos,
         "GMT154 thermostat must not retain the blue-green screen fill");
  expect(create.find("lv_color_hex(0x0B2029)") == std::string::npos,
         "GMT154 thermostat must not retain the blue-green header fill");
  expect(create.find("lv_color_hex(0x91ABB2)") != std::string::npos,
         "GMT154 thermostat labels must use muted blue-gray");
  expect(create.find("lv_color_hex(0x2F8F78)") != std::string::npos,
         "GMT154 thermostat dividers must use teal");
  expect(create.find("create_dashboard_rect(root, 0, 219,") ==
             std::string::npos,
         "GMT154 thermostat must remove the bottom separator at y=219");
  expect(create.find("lv_color_hex(0xFFBF47)") != std::string::npos,
         "GMT154 thermostat setpoints must use amber values");
  expect(create.find("gmt154_sg_font()") != std::string::npos,
         "GMT154 main measurement must use the available 54-digit font");
  expect(ui.find("#if defined(GMT154_06)\n      54, 40};") !=
             std::string::npos,
         "GMT154 temperature refresh must use the board font selector");
  expect(temperature.find("54") != std::string::npos,
         "GMT154 temperature candidates must include 54 pixels");
  expect(create.find("root, layout.mode, \"MODE\"") != std::string::npos,
         "GMT154 thermostat must render a dynamic MODE label/value pair");
  expect(create.find("root, layout.state, \"STATE\"") != std::string::npos,
         "GMT154 thermostat must render a dynamic STATE label/value pair");
  expect(create.find(
             "g_state.cuckoo_heading = create_settings_centered_label") !=
             std::string::npos,
         "GMT154 thermostat must retain a refreshable main temperature heading");
  expect(create.find("#if !defined(GMT154_06)\n  lv_obj_t* mode_pill") !=
             std::string::npos,
         "GMT154 thermostat must omit the mode pill");
  expect(refresh.find("myConfig.isTempFormatF()") != std::string::npos &&
             refresh.find("convertCtoF(snapshot.beer_temp_c)") !=
                 std::string::npos &&
             refresh.find("snapshot.has_beer_temp") != std::string::npos,
         "GMT154 refresh must preserve live C/F beer temperature handling");
  expect(refresh.find("selectTemperaturePresentation(") !=
             std::string::npos &&
             refresh.find("temperature_presentation.has_temperature") !=
                 std::string::npos &&
             refresh.find("temperature_presentation.left_label") !=
                 std::string::npos,
         "GMT154 refresh must render from the tested temperature presentation");
  expect(refresh.find(
             "unit, temperature_presentation.unit_visible, 54") !=
             std::string::npos,
         "GMT154 temperature unit visibility must follow the selected reading");
  expect(refresh.find("10.50") == std::string::npos &&
             refresh.find("11.51") == std::string::npos &&
             refresh.find("Beer Profile") == std::string::npos,
         "GMT154 refresh must not hardcode SVG sample values");
}

void testGmt154FridgeConstantTemperaturePolicy() {
#if HAS_GMT154_CUCKOO_TEMPERATURE_POLICY
  using gmt154_cuckoo_temperature_policy::selectTemperaturePresentation;

  const auto beer_mode = selectTemperaturePresentation(
      true, true, 2, true, 4.5f, true, 20.0f, false);
  expect(!beer_mode.fridge_constant_mode && beer_mode.has_temperature &&
             beer_mode.temperature_c == 20.0f &&
             std::string(beer_mode.heading) == "BEER TEMPERATURE" &&
             std::string(beer_mode.left_label) == "BEER SET" &&
             beer_mode.left_value == nullptr,
         "GMT154 Beer Constant mode must keep beerTemp and Beer Set");

  const auto fridge_mode = selectTemperaturePresentation(
      true, true, 1, true, 4.5f, true, 20.0f, false);
  expect(fridge_mode.fridge_constant_mode && fridge_mode.has_temperature &&
             fridge_mode.temperature_c == 4.5f &&
             std::string(fridge_mode.heading) == "FRIDGE TEMPERATURE" &&
             std::string(fridge_mode.left_label) == "CURRENT MODE" &&
             std::string(fridge_mode.left_value) == "Fridge Constant",
         "GMT154 Fridge Constant mode must select fridgeTemp and current mode");

  const auto resumed_beer_mode = selectTemperaturePresentation(
      true, true, 3, true, 4.5f, true, 20.0f, false);
  expect(!resumed_beer_mode.fridge_constant_mode &&
             resumed_beer_mode.temperature_c == 20.0f &&
             std::string(resumed_beer_mode.heading) == "BEER TEMPERATURE",
         "leaving GMT154 Fridge Constant mode must restore beerTemp");

  const auto missing_mode = selectTemperaturePresentation(
      true, false, 1, true, 4.5f, true, 20.0f, false);
  expect(!missing_mode.fridge_constant_mode &&
             missing_mode.temperature_c == 20.0f &&
             std::string(missing_mode.left_label) == "BEER SET",
         "missing mode must preserve the existing beer-temperature view");

  const auto missing_fridge_temperature = selectTemperaturePresentation(
      true, true, 1, false, 4.5f, true, 20.0f, false);
  expect(missing_fridge_temperature.fridge_constant_mode &&
             !missing_fridge_temperature.has_temperature &&
             !missing_fridge_temperature.unit_visible &&
             missing_fridge_temperature.temperature_c == 0.0f,
         "missing fridgeTemp must show no value or unit without beerTemp fallback");

  const auto zero_fridge_temperature = selectTemperaturePresentation(
      true, true, 1, true, 0.0f, true, 20.0f, false);
  expect(zero_fridge_temperature.has_temperature &&
             zero_fridge_temperature.unit_visible &&
             zero_fridge_temperature.temperature_c == 0.0f &&
             zero_fridge_temperature.display_temperature == 0.0f,
         "zero fridgeTemp must remain a valid Celsius reading");

  const auto fridge_temperature_f = selectTemperaturePresentation(
      true, true, 1, true, 5.0f, true, 20.0f, true);
  expect(fridge_temperature_f.has_temperature &&
             fridge_temperature_f.unit_visible &&
             fridge_temperature_f.temperature_c == 5.0f &&
             fridge_temperature_f.display_temperature == 41.0f,
         "Fridge Constant mode must convert the selected fridgeTemp to Fahrenheit");

  const auto non_gmt_mode_one = selectTemperaturePresentation(
      false, true, 1, true, 4.5f, true, 20.0f, true);
  expect(!non_gmt_mode_one.fridge_constant_mode &&
             non_gmt_mode_one.temperature_c == 20.0f &&
             non_gmt_mode_one.display_temperature == 68.0f &&
             std::string(non_gmt_mode_one.heading) == "BEER TEMPERATURE" &&
             std::string(non_gmt_mode_one.left_label) == "BEER SET" &&
             non_gmt_mode_one.left_value == nullptr,
         "non-GMT154 mode 1 must retain the existing beer-temperature policy");
#else
  expect(false,
         "GMT154 Fridge Constant behavior policy must be implemented and testable");
#endif
}

void testGmt154AlarmUiUsesPerDeviceConditionState() {
  const std::string ui = readFile("src/ui_gravitymon_gateway.cpp");
  const std::string refresh =
      functionBody(ui, "static void gravitymon_gateway_loop_cuckoo(");

  expect(refresh.find("GatewayAlarmType::CuckooOffline, device_id)") !=
             std::string::npos,
         "GMT154 device ID color must query this device's offline condition");
  expect(refresh.find(
             "GatewayAlarmType::CuckooBeerDeviation, device_id)") !=
             std::string::npos,
         "GMT154 device ID color must query this device's beer-deviation condition");
  expect(refresh.find("offline_alarm_active || beer_deviation_alarm_active") !=
             std::string::npos,
         "GMT154 device ID remains alarm-colored while either condition is active");
  expect(refresh.find("device_alarm_active ? lv_color_hex(0xFF5D62)") !=
             std::string::npos,
         "GMT154 active alarm must color the displayed device ID red");
  expect(refresh.find(
             "lv_obj_set_style_text_color(g_state.cuckoo_title, device_id_color") !=
             std::string::npos,
         "GMT154 refresh must apply the alarm color to the device ID label");
  expect(refresh.find("gatewayAlertState.peek") == std::string::npos &&
             refresh.find("gatewayAudio.active") == std::string::npos,
         "GMT154 device ID color must not derive from popup or audio activity");
}

void testGmt154SuppressesOnlyPopupAndIgnoresLegacyDismissalForAudioState() {
  const std::string main = readFile("src/main_gateway.cpp");
  const std::string lifecycle =
      functionBody(main, "static void serviceGatewayAlarmLifecycleActions(");
  const auto publish_start =
      lifecycle.find("case GatewayAlarmLifecycleActionType::PublishAlert:");
  const auto queue_start =
      lifecycle.find("case GatewayAlarmLifecycleActionType::QueueAudio:",
                     publish_start);
  const auto clear_alert_start = lifecycle.find(
      "case GatewayAlarmLifecycleActionType::ClearAlertKey:", queue_start);
  const std::string publish_case =
      publish_start == std::string::npos || queue_start == std::string::npos
          ? std::string()
          : lifecycle.substr(publish_start, queue_start - publish_start);
  const std::string queue_case =
      queue_start == std::string::npos || clear_alert_start == std::string::npos
          ? std::string()
          : lifecycle.substr(queue_start, clear_alert_start - queue_start);

  expect(publish_case.find("#if defined(GMT154_06)") != std::string::npos &&
             publish_case.find(
                 "result = GatewayAlarmLifecycleActionResult::Completed;") !=
                 std::string::npos &&
             publish_case.find("#else") != std::string::npos &&
             publish_case.find("gatewayAlertState.publish(action.event)") !=
                 std::string::npos,
         "GMT154 must suppress popup publication while other targets retain it");
  expect(queue_case.find("gatewayAudio.queueAlarm(action.event)") !=
             std::string::npos,
         "suppressing the GMT154 popup must retain alarm audio queueing");
  expect(lifecycle.find("gatewayAudio.cancelByKeyResult(") !=
             std::string::npos,
         "alarm recovery must retain audio cancellation by device and type");

  const auto offline_start = main.find("const bool offlineActive");
  const auto offline_policy_start =
      offline_start == std::string::npos
          ? std::string::npos
          : main.rfind("#if defined(GMT154_06)", offline_start);
  const auto offline_end =
      main.find("const GatewayAlarmDecision offlineCondition", offline_start);
  const std::string offline_policy =
      offline_policy_start == std::string::npos ||
              offline_end == std::string::npos || offline_end < offline_policy_start
          ? std::string()
          : main.substr(offline_policy_start,
                        offline_end - offline_policy_start);
  const auto gmt154_branch = offline_policy.find("#if defined(GMT154_06)");
  const auto other_target_branch = offline_policy.find("#else", gmt154_branch);
  expect(gmt154_branch != std::string::npos &&
             offline_policy.find("evaluation.offline_active", gmt154_branch) !=
                 std::string::npos &&
             other_target_branch != std::string::npos &&
             offline_policy.find("!evaluation.offline_suppressed",
                                 other_target_branch) != std::string::npos,
         "GMT154 legacy dismissal must not suppress its active offline condition");
}

}  // namespace

int main() {
  testGmt154LayoutPolicy();
  testEntrypointsNormalizeBeforeLayoutUse();
  testGmt154CuckooPiObjectsAreNotCreatedOrRefreshed();
  testGmt154ThermostatRenderingContract();
  testGmt154FridgeConstantTemperaturePolicy();
  testGmt154AlarmUiUsesPerDeviceConditionState();
  testGmt154SuppressesOnlyPopupAndIgnoresLegacyDismissalForAudioState();
  return failures == 0 ? 0 : 1;
}
