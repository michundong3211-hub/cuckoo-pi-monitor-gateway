#include <assert.h>
#include <stdint.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <stdlib.h>

#include "../../src/gateway_measurement_policy.hpp"

namespace {
std::string readFile(const char* path) {
  std::ifstream input(path);
  assert(input.good());
  std::ostringstream contents;
  contents << input.rdbuf();
  return contents.str();
}

void require(const std::string& source, const char* token) {
  if (source.find(token) == std::string::npos) {
    std::cerr << "missing integration contract: " << token << "\n";
    exit(1);
  }
}

void reject(const std::string& source, const char* token) {
  if (source.find(token) != std::string::npos) {
    std::cerr << "forbidden integration contract: " << token << "\n";
    exit(1);
  }
}

void requireInOrder(const std::string& source, const char* first,
                    const char* second) {
  const std::string::size_type first_pos = source.find(first);
  const std::string::size_type second_pos = source.find(second, first_pos);
  if (first_pos == std::string::npos || second_pos == std::string::npos ||
      first_pos >= second_pos) {
    std::cerr << "integration order violation: " << first << " -> " << second
              << "\n";
    exit(1);
  }
}

std::string functionBody(const std::string& source, const char* signature) {
  const std::string::size_type signature_pos = source.find(signature);
  assert(signature_pos != std::string::npos);
  const std::string::size_type body_start = source.find('{', signature_pos);
  assert(body_start != std::string::npos);
  int depth = 0;
  for (std::string::size_type i = body_start; i < source.size(); ++i) {
    if (source[i] == '{') ++depth;
    if (source[i] == '}' && --depth == 0) {
      return source.substr(body_start, i - body_start + 1);
    }
  }
  assert(false);
  return {};
}
}  // namespace

int main() {
  static_assert(GATEWAY_MEASUREMENT_MAX_ENTRIES == 20);
  assert(gatewayMeasurementCanStore(19, false));
  assert(!gatewayMeasurementCanStore(20, false));
  assert(gatewayMeasurementCanStore(20, true));
  assert(gatewayNextMeasurementGeneration(7) == 8);
  assert(gatewayNextMeasurementGeneration(8) == 9);

  const std::string measurement = readFile("src/measurement.hpp");
  require(measurement, "getUpdateGeneration() const");
  require(measurement, "gatewayNextMeasurementGeneration(_updateGeneration)");
  require(measurement, "struct GatewayMeasurementSnapshot");
  require(measurement, "copySnapshot(");
  require(measurement, "xSemaphoreCreateRecursiveMutex");
  require(measurement, "xSemaphoreTakeRecursive");
  require(measurement, "gatewayMeasurementCanStore");

  const std::string main_source = readFile("src/main_gateway.cpp");
  const std::string alarm_header = readFile("src/gateway_alarm.hpp");
  require(alarm_header, "extern GatewayAlarmTracker gatewayAlarmTracker;");
  require(main_source, "#include <gateway_alarm_lifecycle.hpp>");
  require(main_source, "void processGatewayAlarms()");
  require(main_source, "copySnapshot(");
  require(main_source, "gatewayPushResourcesAvailable(");
  reject(main_source, "ESP.getFreeHeap() > 50000");
  require(main_source, "copyDeviceSettings(");
  require(main_source, "gatewayAlarmTracker.markOnline(");
  require(main_source, "gatewayAlarmTracker.recordTemperature(");
  require(main_source, "gatewayAlarmTracker.evaluateOffline(");
  require(main_source, "gatewayAlarmTracker.retainDevices(");
  require(main_source, "GatewayAlertState gatewayAlertState;");
  const std::string process =
      functionBody(main_source, "void processGatewayAlarms() {");
  require(process,
          "gatewayAlarmTracker.schedule(GatewayAlarmType::TemperatureChange,");
  require(process, "gatewayAlarmTracker.schedule(GatewayAlarmType::Offline,");
  require(main_source, "gatewayAlarmLifecycle.reserveScheduleKey(");
  require(main_source, "gatewayAlarmLifecycle.retainScheduledEvent(decision.event)");
  require(main_source, "gatewayAlarmLifecycle.nextMainAction(");
  require(main_source, "GatewayAlarmLifecycleActionType::ResetSession");
  require(main_source, "GatewayAlarmLifecycleActionType::PublishAlert");
  require(main_source, "GatewayAlarmLifecycleActionType::QueueAudio");
  require(main_source, "GatewayAlarmLifecycleActionType::ClearAlertKey");
  require(main_source, "GatewayAlarmLifecycleActionType::CancelAudioKey");
  require(main_source, "GatewayAlarmLifecycleActionType::RetireExact");
  require(main_source, "gatewayAudio.resetKeySession(");
  require(main_source, "gatewayAlertState.publish(action.event)");
  require(main_source, "gatewayAudio.queueAlarm(action.event)");
  require(main_source, "gatewayAlertState.clearByKeyResult(");
  require(main_source, "gatewayAudio.cancelByKeyResult(");
  require(main_source, "gatewayAudio.retireExact(action.event)");
  require(main_source, "gatewayAlarmLifecycle.applyResult(action, result)");
  require(main_source, "GatewayAudioCancelResult::Retry");
  requireInOrder(process, "gatewayAlarmTracker.markOnline(snapshot.id)",
                 "gatewayAlarmTracker.recordTemperature(");
  requireInOrder(process, "gatewayAlarmLifecycle.reserveScheduleKey(",
                 "gatewayAlarmTracker.schedule(GatewayAlarmType::TemperatureChange");
  requireInOrder(process, "gatewayAlarmTracker.schedule(GatewayAlarmType::TemperatureChange",
                 "gatewayAlarmLifecycle.retainScheduledEvent(decision.event)");
  requireInOrder(main_source, "gatewayAlarmTracker.markOnline(snapshot.id)",
                 "gatewayAlarmTracker.schedule(GatewayAlarmType::TemperatureChange");
  reject(process, "gatewayAlertState.clear(");
  reject(process, "gatewayAudio.cancelAlarm(");
  reject(process, "gatewayAudio.queueAlarm(decision.type, snapshot.id)");
  reject(process, "gatewayAudio.queueAlarm(offlineDecision.type, snapshot.id)");
  require(main_source, "retainedIds[retainedCount++] = snapshots[i].id");
  require(main_source,
          "retainedIds[retainedCount++] = cuckooConfig.devices[index].id");
  require(main_source,
          "gatewayAlarmTracker.retainDevices(retainedIds, retainedCount)");
  reject(main_source, "struct GatewayAlarmSessionState");
  reject(main_source, "pendingGatewayAlarmCleanup");
  reject(main_source, "submitGatewayAlarmDecision");
  reject(main_source, "AudioReadingStamp");
  reject(main_source, "audioReadingStamps");
  reject(main_source, "processTemperatureAudioAlerts");
  reject(main_source, "updateTemperatureAlarm");

  const std::string ble = readFile("src/ble_gateway.cpp");
  const std::string web = readFile("src/web_gateway.cpp");
  reject(ble, "gatewayAlarmTracker");
  reject(web, "gatewayAlarmTracker");
  reject(measurement, "gatewayAlarmTracker");
  reject(main_source, "i2s_channel");
  return 0;
}
