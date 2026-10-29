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
  uint32_t now_ms = 0;
  uint32_t save_times[8] = {};
  int save_calls = 0;
  int runtime_commits = 0;
};

GatewayAlarmCloseResult validateRuntime(const GatewayAlarmEvent& event,
                                        void* opaque) {
  PersistenceContext* context = static_cast<PersistenceContext*>(opaque);
  if (!context || !context->tracker) return GatewayAlarmCloseResult::NotFound;
  return context->tracker->validateOfflineDismissalExact(event);
}

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
  if (!context || context->save_calls >= 8) return false;
  context->save_times[context->save_calls++] = context->now_ms;
  if (context->now_ms < 5000U) return false;
  context->stored = settings;
  return true;
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

GatewayAlarmLifecycleActionResult attemptAt(
    uint32_t now_ms, const GatewayAlarmLifecycleAction& action,
    const GatewayOfflineDismissalPersistenceOps& operations,
    PersistenceContext* context,
    GatewayAlarmPersistenceRetryBackoff* retry_backoff) {
  context->now_ms = now_ms;
  GatewayAlarmLifecycleActionResult result =
      GatewayAlarmLifecycleActionResult::Retry;
  attemptGatewayOfflineDismissalWithBackoff(
      action.event, operations, retry_backoff, now_ms, &result);
  return result;
}

void requireAlarmStillLive(const GatewayAlarmEvent& event,
                           GatewayAlarmTracker* tracker,
                           GatewayAlarmLifecycleCoordinator* lifecycle,
                           GatewayAlertState* alert,
                           GatewayAudioScheduler* audio) {
  GatewayAlarmLifecycleAction pending{};
  assert(lifecycle->nextCloseAction(&pending));
  assert(pending.type ==
         GatewayAlarmLifecycleActionType::PersistOfflineDismissal);
  assert(pending.event.condition_token == event.condition_token);
  assert(pending.event.event_token == event.event_token);
  GatewayAlertEvent visible{};
  assert(alert->peek(&visible));
  assert(visible.event_token == event.event_token);
  assert(!tracker->isOfflineDismissed(event.device_id));
  assert(audio->pendingCount() == 1);
}

}  // namespace

int main() {
  const char* device_id = "retry-backoff";
  const uint32_t alarm_t0 = 1200000U;
  GatewayAlarmTracker tracker;
  GatewayAlarmLifecycleCoordinator lifecycle;
  GatewayAlertState alert;
  GatewayAudioScheduler audio;

  assert(!tracker.markOnline(device_id));
  assert(tracker.evaluateOffline(device_id, 60, true, 60).should_play);
  const GatewayAlarmDecision alarm =
      tracker.schedule(GatewayAlarmType::Offline, device_id, alarm_t0);
  assert(alarm.should_play);
  assert(alert.publish(alarm.event));
  GatewayAudioQueueItem audio_item{};
  audio_item.decision = gatewayAudioDecisionForAlarm(alarm.event.type);
  audio_item.event = alarm.event;
  assert(audio.enqueue(audio_item));

  assert(lifecycle.guardClose(alarm.event));
  assert(lifecycle.confirmClose(alarm.event));
  GatewayAlarmLifecycleAction action{};
  assert(lifecycle.nextCloseAction(&action));

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
  GatewayAlarmPersistenceRetryBackoff retry_backoff;

  GatewayAlarmLifecycleActionResult result =
      attemptAt(0U, action, operations, &context, &retry_backoff);
  assert(result == GatewayAlarmLifecycleActionResult::Retry);
  assert(lifecycle.applyResult(action, result));
  requireAlarmStillLive(alarm.event, &tracker, &lifecycle, &alert, &audio);

  result = attemptAt(1U, action, operations, &context, &retry_backoff);
  assert(result == GatewayAlarmLifecycleActionResult::Retry);
  assert(lifecycle.applyResult(action, result));
  requireAlarmStillLive(alarm.event, &tracker, &lifecycle, &alert, &audio);

  result = attemptAt(4999U, action, operations, &context, &retry_backoff);
  assert(result == GatewayAlarmLifecycleActionResult::Retry);
  assert(lifecycle.applyResult(action, result));
  requireAlarmStillLive(alarm.event, &tracker, &lifecycle, &alert, &audio);
  const int calls_before_deadline = context.save_calls;

  result = attemptAt(5000U, action, operations, &context, &retry_backoff);
  assert(result == GatewayAlarmLifecycleActionResult::Completed);
  assert(lifecycle.applyResult(action, result));

  if (calls_before_deadline != 1 || context.save_calls != 2 ||
      context.save_times[0] != 0U || context.save_times[1] != 5000U) {
    fprintf(stderr,
            "retry backoff violated: before_5000=%d total=%d times=%u,%u\n",
            calls_before_deadline, context.save_calls,
            static_cast<unsigned>(context.save_times[0]),
            static_cast<unsigned>(context.save_times[1]));
    return 40;
  }
  assert(context.runtime_commits == 1);
  assert(context.stored.offline_alarm_dismissed);
  assert(tracker.isOfflineDismissed(device_id));
  GatewayAlertEvent visible{};
  assert(!alert.peek(&visible));
  assert(audio.pendingCount() == 1);
  assert(lifecycle.nextCloseAction(&action));
  assert(action.type == GatewayAlarmLifecycleActionType::CloseTrackerExact);
  return 0;
}
