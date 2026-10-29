#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../../src/gateway_alarm.hpp"
#include "../../src/gateway_alarm_lifecycle.hpp"
#include "../../src/gateway_alert_state.hpp"
#include "../../src/gateway_audio.hpp"
#include "../../src/gateway_device_settings.hpp"

namespace {

struct PersistenceContext {
  GravityDeviceSettings stored{};
  GatewayAlarmTracker* tracker = nullptr;
  GatewayAlertState* alert = nullptr;
  bool save_succeeds = false;
  int save_calls = 0;
  int runtime_commits = 0;
};

bool loadSettings(const char* deviceId, GravityDeviceSettings* settings,
                  void* opaque) {
  PersistenceContext* context = static_cast<PersistenceContext*>(opaque);
  if (!context || !settings ||
      strcmp(context->stored.device_id, deviceId) != 0) {
    return false;
  }
  *settings = context->stored;
  return true;
}

bool saveSettings(const GravityDeviceSettings& settings, void* opaque) {
  PersistenceContext* context = static_cast<PersistenceContext*>(opaque);
  if (!context) return false;
  ++context->save_calls;
  if (!context->save_succeeds) return false;
  context->stored = settings;
  return true;
}

GatewayAlarmCloseResult validateRuntime(const GatewayAlarmEvent& event,
                                        void* opaque) {
  PersistenceContext* context = static_cast<PersistenceContext*>(opaque);
  if (!context || !context->tracker) return GatewayAlarmCloseResult::NotFound;
  return context->tracker->validateOfflineDismissalExact(event);
}

GatewayAlarmCloseResult commitRuntime(const GatewayAlarmEvent& event,
                                      void* opaque) {
  PersistenceContext* context = static_cast<PersistenceContext*>(opaque);
  if (!context || !context->tracker || !context->alert) {
    return GatewayAlarmCloseResult::NotFound;
  }
  const GatewayAlarmCloseResult tracker_result =
      context->tracker->commitOfflineDismissalExact(event);
  if (tracker_result != GatewayAlarmCloseResult::Closed) {
    return tracker_result;
  }
  GatewayAlertEvent dismissed{};
  if (!context->alert->dismissCurrentExact(event.event_token, &dismissed)) {
    return GatewayAlarmCloseResult::Retry;
  }
  ++context->runtime_commits;
  return GatewayAlarmCloseResult::Closed;
}

void enqueue(GatewayAudioScheduler* audio, const GatewayAlarmEvent& event) {
  GatewayAudioQueueItem item{};
  item.decision = gatewayAudioDecisionForAlarm(event.type);
  item.event = event;
  assert(audio->enqueue(item));
}

}  // namespace

int main() {
  const char* device_id = "stale-close";
  const uint32_t old_t0 = 1000000U;
  const uint32_t new_t0 = 1100000U;
  GatewayAlarmTracker tracker;
  GatewayAlarmLifecycleCoordinator lifecycle;
  GatewayAlertState alert;
  GatewayAudioScheduler audio;

  assert(!tracker.markOnline(device_id));
  assert(tracker.evaluateOffline(device_id, 60, true, 60).should_play);
  const GatewayAlarmDecision old_alarm =
      tracker.schedule(GatewayAlarmType::Offline, device_id, old_t0);
  assert(old_alarm.should_play);
  assert(alert.publish(old_alarm.event));
  enqueue(&audio, old_alarm.event);

  assert(lifecycle.guardClose(old_alarm.event));
  assert(lifecycle.confirmClose(old_alarm.event));
  GatewayAlarmLifecycleAction action{};
  assert(lifecycle.nextCloseAction(&action));
  assert(action.type ==
         GatewayAlarmLifecycleActionType::PersistOfflineDismissal);

  PersistenceContext context{};
  strcpy(context.stored.device_id, device_id);
  context.stored.offline_alarm_enabled = true;
  context.tracker = &tracker;
  context.alert = &alert;
  GatewayOfflineDismissalPersistenceOps operations{};
  operations.validateRuntime = validateRuntime;
  operations.load = loadSettings;
  operations.save = saveSettings;
  operations.commitRuntime = commitRuntime;
  operations.context = &context;

  const GatewayAlarmLifecycleActionResult first_attempt =
      persistGatewayOfflineDismissal(action.event, operations);
  assert(first_attempt == GatewayAlarmLifecycleActionResult::Retry);
  assert(lifecycle.applyResult(action, first_attempt));
  assert(context.save_calls == 1);
  assert(!context.stored.offline_alarm_dismissed);

  assert(tracker.markOnline(device_id));
  assert(tracker.evaluateOffline(device_id, 60, true, 60).should_play);
  const GatewayAlarmDecision new_alarm =
      tracker.schedule(GatewayAlarmType::Offline, device_id, new_t0);
  assert(new_alarm.should_play);
  assert(new_alarm.event.condition_token != old_alarm.event.condition_token);
  assert(new_alarm.event.event_token != old_alarm.event.event_token);
  assert(alert.publish(new_alarm.event));
  enqueue(&audio, new_alarm.event);

  assert(lifecycle.reserveScheduleKey(GatewayAlarmType::Offline, device_id) ==
         GatewayAlarmLifecycleReserveResult::ReservedNew);
  assert(lifecycle.retainScheduledEvent(new_alarm.event));

  context.save_succeeds = true;
  assert(lifecycle.nextCloseAction(&action));
  const GatewayAlarmLifecycleActionResult stale_attempt =
      persistGatewayOfflineDismissal(action.event, operations);
  if (stale_attempt != GatewayAlarmLifecycleActionResult::NotFound) {
    fprintf(stderr,
            "stale CLOSE was not rejected before persistence: result=%u "
            "save_calls=%d dismissed=%d\n",
            static_cast<unsigned>(stale_attempt), context.save_calls,
            context.stored.offline_alarm_dismissed ? 1 : 0);
    return 30;
  }
  assert(lifecycle.applyResult(action, stale_attempt));

  assert(context.save_calls == 1);
  assert(context.runtime_commits == 0);
  assert(!context.stored.offline_alarm_dismissed);
  assert(!tracker.isOfflineDismissed(device_id));
  assert(alert.pendingCount() == 2);
  assert(audio.pendingCount() == 2);
  const GatewayAlarmDecision new_repeat = tracker.schedule(
      GatewayAlarmType::Offline, device_id, new_t0 + 10000U);
  assert(new_repeat.should_play);
  assert(new_repeat.event.condition_token == new_alarm.event.condition_token);
  assert(new_repeat.event.event_token == new_alarm.event.event_token);

  GatewayAlarmLifecycleAction new_action{};
  assert(lifecycle.nextMainAction(0, &new_action));
  assert(new_action.event.condition_token == new_alarm.event.condition_token);
  assert(new_action.event.event_token == new_alarm.event.event_token);
  assert(!lifecycle.nextCloseAction(&action));
  return 0;
}
