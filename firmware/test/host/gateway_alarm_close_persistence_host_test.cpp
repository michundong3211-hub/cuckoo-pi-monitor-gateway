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
  PersistenceContext* context =
      static_cast<PersistenceContext*>(opaque);
  if (!context || !settings ||
      strcmp(context->stored.device_id, deviceId) != 0) {
    return false;
  }
  *settings = context->stored;
  return true;
}

bool saveSettings(const GravityDeviceSettings& settings, void* opaque) {
  PersistenceContext* context =
      static_cast<PersistenceContext*>(opaque);
  if (!context) return false;
  ++context->save_calls;
  if (!context->save_succeeds) return false;
  context->stored = settings;
  return true;
}

GatewayAlarmCloseResult validateRuntime(const GatewayAlarmEvent& event,
                                        void* opaque) {
  PersistenceContext* context =
      static_cast<PersistenceContext*>(opaque);
  if (!context || !context->tracker) return GatewayAlarmCloseResult::NotFound;
  return context->tracker->validateOfflineDismissalExact(event);
}

GatewayAlarmCloseResult commitRuntime(const GatewayAlarmEvent& event,
                                      void* opaque) {
  PersistenceContext* context =
      static_cast<PersistenceContext*>(opaque);
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

}  // namespace

int main() {
  const uint32_t t0 = 900000U;
  GatewayAlarmTracker tracker;
  GatewayAlarmLifecycleCoordinator lifecycle;
  GatewayAlertState alert;
  GatewayAudioScheduler audio;

  assert(!tracker.markOnline("save-gate"));
  assert(tracker.evaluateOffline("save-gate", 60, true, 60).should_play);
  const GatewayAlarmDecision scheduled =
      tracker.schedule(GatewayAlarmType::Offline, "save-gate", t0);
  assert(scheduled.should_play);
  assert(alert.publish(scheduled.event));
  GatewayAudioQueueItem audio_item{};
  audio_item.decision = gatewayAudioDecisionForAlarm(scheduled.event.type);
  audio_item.event = scheduled.event;
  assert(audio.enqueue(audio_item));

  assert(lifecycle.guardClose(scheduled.event));
  assert(lifecycle.confirmClose(scheduled.event));
  GatewayAlarmLifecycleAction action{};
  assert(lifecycle.nextCloseAction(&action));
  assert(action.type ==
         GatewayAlarmLifecycleActionType::PersistOfflineDismissal);

  PersistenceContext context{};
  strcpy(context.stored.device_id, "save-gate");
  context.stored.offline_alarm_enabled = true;
  context.tracker = &tracker;
  context.alert = &alert;
  GatewayOfflineDismissalPersistenceOps operations{};
  operations.validateRuntime = validateRuntime;
  operations.load = loadSettings;
  operations.save = saveSettings;
  operations.commitRuntime = commitRuntime;
  operations.context = &context;

  const GatewayAlarmLifecycleActionResult failed =
      persistGatewayOfflineDismissal(action.event, operations);
  if (failed != GatewayAlarmLifecycleActionResult::Retry) {
    fprintf(stderr,
            "failed persistence incorrectly advanced offline CLOSE\n");
    return 20;
  }
  assert(lifecycle.applyResult(action, failed));
  assert(context.save_calls == 1);
  assert(context.runtime_commits == 0);
  assert(!context.stored.offline_alarm_dismissed);
  assert(!tracker.isOfflineDismissed("save-gate"));
  GatewayAlertEvent retained{};
  assert(alert.peek(&retained));
  assert(retained.event_token == scheduled.event.event_token);
  assert(audio.pendingCount() == 1);
  const GatewayAlarmDecision repeat = tracker.schedule(
      GatewayAlarmType::Offline, "save-gate", t0 + 10000U);
  assert(repeat.should_play);

  assert(lifecycle.nextCloseAction(&action));
  context.save_succeeds = true;
  const GatewayAlarmLifecycleActionResult saved =
      persistGatewayOfflineDismissal(action.event, operations);
  assert(saved == GatewayAlarmLifecycleActionResult::Completed);
  assert(lifecycle.applyResult(action, saved));
  assert(context.save_calls == 2);
  assert(context.runtime_commits == 1);
  assert(context.stored.offline_alarm_dismissed);
  assert(tracker.isOfflineDismissed("save-gate"));
  assert(!alert.peek(&retained));
  assert(audio.pendingCount() == 1);

  assert(lifecycle.nextCloseAction(&action));
  assert(action.type == GatewayAlarmLifecycleActionType::CloseTrackerExact);
  return 0;
}
