#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

static bool contains(const std::string& text, const std::string& needle) {
  return text.find(needle) != std::string::npos;
}

static std::string function_body(const std::string& source,
                                 const char* signature) {
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

int main() {
  const char* path = "src/ui_gravitymon_gateway.cpp";
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    std::cerr << "cannot open " << path << "\n";
    return 1;
  }

  std::ostringstream buffer;
  buffer << file.rdbuf();
  const std::string source = buffer.str();

  if (!contains(source,
                "battery_alert ? lv_color_hex(0xFF5D62) : lv_color_hex(0x45D9B2)")) {
    std::cerr << "missing red low-battery header fill logic\n";
    return 2;
  }

  if (!contains(source,
                "battery_alert ? lv_color_hex(0xFF5D62) : lv_color_hex(0xF4F7F7)")) {
    std::cerr << "missing red low-battery header text logic\n";
    return 3;
  }

  if (contains(source, "\"ABV --\"")) {
    std::cerr << "legacy ABV placeholder copy still present\n";
    return 4;
  }

  if (!contains(source, "snprintf(abv_text, sizeof(abv_text), \"--\")")) {
    std::cerr << "missing compact invalid ABV fallback\n";
    return 5;
  }

  if (contains(source, "temperature_alert") ||
      contains(source, "isTemperatureOutOfRangeC(")) {
    std::cerr << "legacy fixed high/low temperature alarm coloring remains\n";
    return 6;
  }

  const char* required[] = {
      "#include <gateway_alert_state.hpp>",
      "#include <gateway_alarm_lifecycle.hpp>",
      "#include <gateway_audio.hpp>",
      "GatewayAlertEvent",
      "GatewayAlarmType::TemperatureChange",
      "GatewayAlarmType::Offline",
      "CLOSE",
      "gatewayAlertState.peek(&local)",
      "gatewayAlarmLifecycle.guardClose(local)",
      "gatewayAlarmLifecycle.abortClose(local)",
      "gatewayAlarmLifecycle.confirmClose(local)",
      "attemptGatewayOfflineDismissalWithBackoff(",
      "validate_gateway_offline_dismissal_runtime",
      "gateway_alarm_persistence_retry_backoff",
      "save_gateway_offline_dismissal",
      "commit_gateway_offline_dismissal_runtime",
      "gatewayAlarmTracker.closeExact(local.event_token)",
      "gatewayAudio.cancelExactResult(local)",
      "GatewayAlarmCloseResult::Retry",
      "GatewayAudioCancelResult::Retry",
      "gatewayAlarmLifecycle.nextCloseAction(&action)",
      "gatewayAlarmLifecycle.applyResult(action, result)",
      "gatewayAlarmLifecycle.canTearDownAlertUi()",
      "void gravitymon_gateway_loop(void)",
      "gravitymon_gateway_update_alert_ui();",
      "gravitymon_gateway_cleanup_layout();",
      "gravitymon_gateway_setup_layout_2();",
      "reset_gateway_widget_pointers",
      "wid_alert_panel",
      "lbl_alert_device",
      "btn_alert_close",
  };
  for (const char* token : required) {
    if (!contains(source, token)) {
      std::cerr << "missing alert UI contract: " << token << "\n";
      return 7;
    }
  }

  const std::string close_handler =
      function_body(source, "static void gravitymon_gateway_close_alert");
  const auto peeked = close_handler.find("gatewayAlertState.peek(&local)");
  const auto guarded =
      close_handler.find("gatewayAlarmLifecycle.guardClose(local)");
  const auto confirmed =
      close_handler.find("gatewayAlarmLifecycle.confirmClose(local)");
  const auto attempted = close_handler.find(
      "gravitymon_gateway_attempt_pending_alert_close();");
  if (peeked == std::string::npos || guarded == std::string::npos ||
      confirmed == std::string::npos || attempted == std::string::npos ||
      peeked > guarded || guarded > confirmed || confirmed > attempted) {
    std::cerr << "CLOSE must guard, confirm, then run lifecycle actions\n";
    return 8;
  }

  const std::string close_attempt = function_body(
      source, "static void gravitymon_gateway_attempt_pending_alert_close() {");
  const auto tracker_close =
      close_attempt.find("gatewayAlarmTracker.closeExact(local.event_token)");
  const auto audio_cancel = close_attempt.find(
      "gatewayAudio.cancelExactResult(local)");
  const auto persistence =
      close_attempt.find("attemptGatewayOfflineDismissalWithBackoff(");
  if (persistence == std::string::npos ||
      tracker_close == std::string::npos || audio_cancel == std::string::npos ||
      persistence > tracker_close || tracker_close > audio_cancel) {
    std::cerr << "pending CLOSE must persist before tracker/audio cancel\n";
    return 11;
  }
  if (contains(close_handler, "gatewayAlarmTracker.closeExact") ||
      contains(close_handler, "gatewayAudio.cancelExactResult")) {
    std::cerr << "dismiss failure path must not cancel tracker/audio\n";
    return 12;
  }
  if (contains(close_attempt, "cancelExactResult(event)") ||
      contains(close_attempt, "cancelAlarm(")) {
    std::cerr << "CLOSE must cancel the retained full event, not a newer event\n";
    return 13;
  }

  const std::string loop = function_body(source, "void gravitymon_gateway_loop(");
  if (!contains(loop, "gravitymon_gateway_update_alert_ui();")) {
    std::cerr << "LVGL loop does not consume GatewayAlertState\n";
    return 9;
  }

  if (!contains(loop, "gravitymon_gateway_attempt_pending_alert_close();")) {
    std::cerr << "LVGL loop does not retry pending CLOSE operations\n";
    return 14;
  }

  if (contains(source, "alert_close_pending") ||
      contains(source, "alert_close_tracker_done") ||
      contains(source, "alert_close_audio_done")) {
    std::cerr << "UI must not own duplicate pending CLOSE state\n";
    return 15;
  }

  const std::string main_source = [&]() {
    std::ifstream main_file("src/main_gateway.cpp", std::ios::binary);
    std::ostringstream main_buffer;
    main_buffer << main_file.rdbuf();
    return main_buffer.str();
  }();
  if (contains(main_source, "lv_obj_")) {
    std::cerr << "main_gateway.cpp must not call lv_obj_* directly\n";
    return 10;
  }

  return 0;
}
