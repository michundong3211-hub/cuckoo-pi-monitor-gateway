// Regression test: periodic alarm pipeline + exact CLOSE.
//
// Locks the fixed contracts:
//  1. Same-window repeats (shared event_token) must flow through
//     PublishAlert -> QueueAudio and actually enqueue audio, instead of
//     wedging the lifecycle slot in PublishAlert forever.
//  2. CLOSE must immediately cancel the closed window's audio and leave the
//     alert state empty; no stale PublishAlert retry may re-publish the
//     dismissed alert afterwards (panel must not reappear).
//  3. CLOSE must return the closed window's lifecycle slot to Idle even when
//     it is mid-repeat (ResetSession/PublishAlert/QueueAudio), and the close
//     stays effective across 5-minute window rollovers: the alarm must not
//     re-arm every cycle while the condition persists (it re-arms only after
//     the condition clears).
//
// The pipeline mirrors main_gateway.cpp: tracker.schedule -> retain ->
// service (ResetSession -> PublishAlert -> QueueAudio) with the real
// GatewayAlertState and GatewayAudioScheduler.
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../src/gateway_alarm.hpp"
#include "../../src/gateway_alarm_lifecycle.hpp"
#include "../../src/gateway_alert_state.hpp"
#include "../../src/gateway_audio.hpp"

namespace {

GatewayAlarmEvent makeEvent(GatewayAlarmType type, const char* deviceId,
                            GatewayAlarmEventToken conditionToken,
                            GatewayAlarmEventToken eventToken,
                            uint32_t windowIndex = 0,
                            uint8_t repeatIndex = 0) {
  GatewayAlarmEvent event{};
  event.type = type;
  strncpy(event.device_id, deviceId, sizeof(event.device_id) - 1);
  event.condition_token = conditionToken;
  event.event_token = eventToken;
  event.window_index = windowIndex;
  event.repeat_index = repeatIndex;
  return event;
}

GatewayAlarmLifecycleActionResult toLifecycle(
    GatewayAudioCancelResult result) {
  if (result == GatewayAudioCancelResult::Retry)
    return GatewayAlarmLifecycleActionResult::Retry;
  if (result == GatewayAudioCancelResult::NotFound)
    return GatewayAlarmLifecycleActionResult::NotFound;
  return GatewayAlarmLifecycleActionResult::Completed;
}

GatewayAlarmLifecycleActionResult toLifecycle(
    GatewayAlertClearResult result) {
  if (result == GatewayAlertClearResult::Retry)
    return GatewayAlarmLifecycleActionResult::Retry;
  if (result == GatewayAlertClearResult::NotFound)
    return GatewayAlarmLifecycleActionResult::NotFound;
  return GatewayAlarmLifecycleActionResult::Completed;
}

struct Pipeline {
  GatewayAlarmTracker tracker;
  GatewayAlarmLifecycleCoordinator lifecycle;
  GatewayAlertState alert;
  GatewayAudioScheduler audio;

  // One pass of serviceGatewayAlarmLifecycleActions() (main actions only).
  void service() {
    for (size_t slot = 0; slot < GATEWAY_ALARM_LIFECYCLE_CAPACITY; ++slot) {
      GatewayAlarmLifecycleAction action{};
      if (!lifecycle.nextMainAction(slot, &action)) continue;
      GatewayAlarmLifecycleActionResult result =
          GatewayAlarmLifecycleActionResult::Rejected;
      switch (action.type) {
        case GatewayAlarmLifecycleActionType::ResetSession:
          result = toLifecycle(audio.resetKeySession(
              action.alarm_type, action.device_id,
              action.event.condition_token));
          break;
        case GatewayAlarmLifecycleActionType::PublishAlert:
          result = alert.publish(action.event)
                       ? GatewayAlarmLifecycleActionResult::Completed
                       : GatewayAlarmLifecycleActionResult::Rejected;
          break;
        case GatewayAlarmLifecycleActionType::QueueAudio: {
          GatewayAudioQueueItem item{};
          item.decision = gatewayAudioDecisionForAlarm(action.event.type);
          item.event = action.event;
          result = audio.enqueue(item)
                       ? GatewayAlarmLifecycleActionResult::Completed
                       : GatewayAlarmLifecycleActionResult::Rejected;
          break;
        }
        case GatewayAlarmLifecycleActionType::ClearAlertKey:
          result = toLifecycle(alert.clearByKeyResult(
              action.alarm_type, action.device_id));
          break;
        case GatewayAlarmLifecycleActionType::CancelAudioKey:
          result = toLifecycle(audio.cancelByKeyResult(
              action.alarm_type, action.device_id));
          break;
        case GatewayAlarmLifecycleActionType::RetireExact:
          result = toLifecycle(audio.retireExact(action.event));
          break;
        default:
          continue;
      }
      assert(lifecycle.applyResult(action, result));
    }
  }

  // main_gateway.cpp style schedule+retain for one device.
  bool tick(GatewayAlarmType type, const char* id, uint32_t nowMs) {
    const GatewayAlarmLifecycleReserveResult reservation =
        lifecycle.reserveScheduleKey(type, id);
    if (reservation == GatewayAlarmLifecycleReserveResult::Full) return false;
    const GatewayAlarmDecision decision = tracker.schedule(type, id, nowMs);
    if (decision.should_play) {
      return lifecycle.retainScheduledEvent(decision.event);
    }
    if (reservation == GatewayAlarmLifecycleReserveResult::ReservedNew) {
      lifecycle.releaseUnusedReservation(type, id);
    }
    return false;
  }

  bool hasAction(GatewayAlarmType type, const char* deviceId) {
    for (size_t slot = 0; slot < GATEWAY_ALARM_LIFECYCLE_CAPACITY; ++slot) {
      GatewayAlarmLifecycleAction candidate{};
      if (lifecycle.nextMainAction(slot, &candidate) &&
          candidate.alarm_type == type &&
          strcmp(candidate.device_id, deviceId) == 0) {
        return true;
      }
    }
    return false;
  }

  // CLOSE button flow (ui_gravitymon_gateway.cpp): guard, exact-dismiss,
  // confirm, then one close action per call.
  bool closeOnce() {
    GatewayAlarmEvent local{};
    if (!alert.peek(&local)) return false;
    assert(lifecycle.guardClose(local));
    GatewayAlarmEvent dismissed{};
    assert(alert.dismissCurrentExact(local.event_token, &dismissed));
    assert(lifecycle.confirmClose(local));
    return attemptClose();
  }

  bool attemptClose() {
    GatewayAlarmLifecycleAction action{};
    if (!lifecycle.nextCloseAction(&action)) return false;
    GatewayAlarmLifecycleActionResult result =
        GatewayAlarmLifecycleActionResult::Rejected;
    if (action.type == GatewayAlarmLifecycleActionType::CloseTrackerExact) {
      const GatewayAlarmCloseResult r =
          tracker.closeExact(action.event.event_token);
      result = r == GatewayAlarmCloseResult::Retry
                   ? GatewayAlarmLifecycleActionResult::Retry
                   : r == GatewayAlarmCloseResult::NotFound
                         ? GatewayAlarmLifecycleActionResult::NotFound
                         : GatewayAlarmLifecycleActionResult::Completed;
    } else if (action.type ==
               GatewayAlarmLifecycleActionType::CancelAudioExact) {
      result = toLifecycle(audio.cancelExactResult(action.event));
    } else {
      return false;
    }
    assert(lifecycle.applyResult(action, result));
    return true;
  }
};

void testRepeatsQueueThroughFullPipeline() {
  const uint32_t t0 = 100000U;
  Pipeline p;
  assert(!p.tracker.recordTemperature("T", 20.0f, true, 5, 1.0f,
                                      t0 - 5U * 60U * 1000U)
              .should_play);
  assert(p.tracker.recordTemperature("T", 21.0f, true, 5, 1.0f, t0)
             .should_play);

  assert(p.tick(GatewayAlarmType::TemperatureChange, "T", t0));
  p.service();  // ResetSession
  p.service();  // PublishAlert
  p.service();  // QueueAudio
  assert(p.audio.pendingCount() == 1);

  // Repeat 1 and repeat 2 must queue audio (not wedge at PublishAlert).
  assert(p.tick(GatewayAlarmType::TemperatureChange, "T", t0 + 10000U));
  p.service();
  p.service();
  p.service();
  assert(p.audio.pendingCount() == 2);
  assert(!p.hasAction(GatewayAlarmType::TemperatureChange, "T"));

  assert(p.tick(GatewayAlarmType::TemperatureChange, "T", t0 + 20000U));
  p.service();
  p.service();
  p.service();
  assert(p.audio.pendingCount() == 3);
  assert(!p.hasAction(GatewayAlarmType::TemperatureChange, "T"));

  GatewayAlarmEvent current{};
  assert(p.alert.peek(&current));
  assert(current.event_token != GATEWAY_INVALID_EVENT_TOKEN);
}

void testCloseCancelsAndNeverRepublishesClosedWindow() {
  const uint32_t t0 = 200000U;
  Pipeline p;
  assert(!p.tracker.recordTemperature("C", 20.0f, true, 5, 1.0f,
                                      t0 - 5U * 60U * 1000U)
              .should_play);
  assert(p.tracker.recordTemperature("C", 21.0f, true, 5, 1.0f, t0)
             .should_play);

  assert(p.tick(GatewayAlarmType::TemperatureChange, "C", t0));
  p.service();
  p.service();
  p.service();
  assert(p.tick(GatewayAlarmType::TemperatureChange, "C", t0 + 10000U));
  p.service();
  p.service();
  p.service();
  assert(p.tick(GatewayAlarmType::TemperatureChange, "C", t0 + 20000U));
  p.service();
  p.service();
  p.service();
  assert(p.audio.pendingCount() == 3);

  assert(p.closeOnce());                       // CloseTrackerExact
  assert(p.attemptClose());                    // CancelAudioExact
  assert(p.lifecycle.canTearDownAlertUi());

  GatewayAlarmEvent stale{};
  assert(!p.alert.peek(&stale));               // alert dismissed, stays empty
  assert(p.audio.pendingCount() == 0);
  assert(!p.hasAction(GatewayAlarmType::TemperatureChange, "C"));

  // A post-close repeat tick must not re-publish the closed window.
  p.tick(GatewayAlarmType::TemperatureChange, "C", t0 + 30000U);
  p.service();
  p.service();
  p.service();
  assert(!p.alert.peek(&stale));
  assert(p.audio.pendingCount() == 0);
  assert(!p.hasAction(GatewayAlarmType::TemperatureChange, "C"));
}

void testNextWindowStaysSilentAfterClose() {
  const uint32_t t0 = 300000U;
  Pipeline p;
  assert(!p.tracker.recordTemperature("W", 20.0f, true, 5, 1.0f,
                                      t0 - 5U * 60U * 1000U)
              .should_play);
  assert(p.tracker.recordTemperature("W", 21.0f, true, 5, 1.0f, t0)
             .should_play);
  assert(p.tick(GatewayAlarmType::TemperatureChange, "W", t0));
  p.service();
  p.service();
  p.service();

  assert(p.closeOnce());
  assert(p.attemptClose());
  assert(p.lifecycle.canTearDownAlertUi());
  assert(!p.hasAction(GatewayAlarmType::TemperatureChange, "W"));

  // The CLOSE stays effective across window rollovers: the next 5-minute
  // window must NOT re-arm while the condition persists. The schedule only
  // re-arms after the condition actually clears.
  assert(!p.tick(GatewayAlarmType::TemperatureChange, "W",
                 t0 + kAlarmCyclePeriodMs));
  p.service();
  p.service();
  p.service();
  GatewayAlarmEvent fresh{};
  assert(!p.alert.peek(&fresh));
  assert(p.audio.pendingCount() == 0);
  assert(!p.hasAction(GatewayAlarmType::TemperatureChange, "W"));

  assert(!p.tick(GatewayAlarmType::TemperatureChange, "W",
                 t0 + 2UL * kAlarmCyclePeriodMs));
  p.service();
  p.service();
  p.service();
  assert(!p.alert.peek(&fresh));
  assert(p.audio.pendingCount() == 0);
}

void testCloseClearsMidRepeatSlotStates() {
  const uint32_t t0 = 400000U;

  // Slot stuck at ResetSession (retained but not yet serviced).
  {
    Pipeline p;
    assert(!p.tracker.recordTemperature("R", 20.0f, true, 5, 1.0f,
                                        t0 - 5U * 60U * 1000U)
                .should_play);
    assert(p.tracker.recordTemperature("R", 21.0f, true, 5, 1.0f, t0)
               .should_play);
    assert(p.tick(GatewayAlarmType::TemperatureChange, "R", t0));
    p.service();  // ResetSession -> PublishAlert
    p.service();  // PublishAlert -> QueueAudio
    p.service();  // QueueAudio -> Idle
    assert(p.tick(GatewayAlarmType::TemperatureChange, "R", t0 + 10000U));
    // Slot is now at ResetSession for repeat 1; close before servicing.
    assert(p.closeOnce());
    p.service();  // must be blocked: closed window's ResetSession is paused
    assert(!p.hasAction(GatewayAlarmType::TemperatureChange, "R"));
    assert(p.attemptClose());  // CancelAudioExact clears the slot
    assert(!p.hasAction(GatewayAlarmType::TemperatureChange, "R"));
    assert(p.lifecycle.canTearDownAlertUi());
    assert(p.audio.pendingCount() == 0);
  }

  // Slot stuck at QueueAudio because the audio queue is full.
  {
    Pipeline p;
    assert(!p.tracker.recordTemperature("Q", 20.0f, true, 5, 1.0f,
                                        t0 - 5U * 60U * 1000U)
                .should_play);
    assert(p.tracker.recordTemperature("Q", 21.0f, true, 5, 1.0f, t0)
               .should_play);
    assert(p.tick(GatewayAlarmType::TemperatureChange, "Q", t0));
    p.service();
    p.service();
    p.service();
    // Fill the audio queue with unrelated keys so repeat 1's QueueAudio
    // rejects and the slot wedges at QueueAudio. One slot already holds Q's
    // repeat 0, so GATEWAY_AUDIO_QUEUE_CAPACITY - 1 fillers fill it up.
    for (uint8_t i = 0; i + 1 < GATEWAY_AUDIO_QUEUE_CAPACITY; ++i) {
      char id[16] = {};
      snprintf(id, sizeof(id), "filler-%u", (unsigned)i);
      GatewayAudioQueueItem item{};
      item.decision = gatewayAudioDecisionForAlarm(GatewayAlarmType::Offline);
      item.event = makeEvent(GatewayAlarmType::Offline, id, 50000 + i,
                             60000 + i, 0, 0);
      assert(p.audio.enqueue(item));
    }
    assert(p.tick(GatewayAlarmType::TemperatureChange, "Q", t0 + 10000U));
    p.service();  // ResetSession
    p.service();  // PublishAlert (idempotent success)
    p.service();  // QueueAudio -> rejected, slot stays at QueueAudio
    assert(p.hasAction(GatewayAlarmType::TemperatureChange, "Q"));

    assert(p.closeOnce());
    p.service();  // blocked while close in flight
    assert(p.attemptClose());
    assert(p.lifecycle.canTearDownAlertUi());
    assert(!p.hasAction(GatewayAlarmType::TemperatureChange, "Q"));
    GatewayAlarmEvent stale{};
    assert(!p.alert.peek(&stale));
  }
}

void testOfflineSaveFailureLeavesCurrentAlarmUndismissedAndUncancelled() {
  const uint32_t t0 = 500000U;
  Pipeline p;
  assert(!p.tracker.markOnline("offline-save-failure"));
  assert(p.tracker
             .evaluateOffline("offline-save-failure", 60, true, 60)
             .should_play);
  assert(p.tick(GatewayAlarmType::Offline, "offline-save-failure", t0));
  p.service();
  p.service();
  p.service();
  assert(p.audio.pendingCount() == 1);

  GatewayAlarmEvent current{};
  assert(p.alert.peek(&current));
  assert(current.type == GatewayAlarmType::Offline);
  assert(p.lifecycle.guardClose(current));
  assert(p.lifecycle.confirmClose(current));

  GatewayAlarmLifecycleAction action{};
  assert(p.lifecycle.nextCloseAction(&action));
  if (action.type == GatewayAlarmLifecycleActionType::CloseTrackerExact) {
    const GatewayAlarmCloseResult closed =
        p.tracker.closeExact(action.event.event_token);
    assert(p.lifecycle.applyResult(
        action, closed == GatewayAlarmCloseResult::Retry
                    ? GatewayAlarmLifecycleActionResult::Retry
                    : closed == GatewayAlarmCloseResult::NotFound
                          ? GatewayAlarmLifecycleActionResult::NotFound
                          : GatewayAlarmLifecycleActionResult::Completed));
  } else {
    // The non-destructive first phase represents the failed persistence
    // attempt. A retry result must keep all live alarm state intact.
    assert(p.lifecycle.applyResult(action,
                                   GatewayAlarmLifecycleActionResult::Retry));
  }

  GatewayAlarmEvent retained{};
  assert(p.alert.peek(&retained));
  assert(retained.event_token == current.event_token);
  assert(p.audio.pendingCount() == 1);
  assert(!p.tracker.isOfflineDismissed("offline-save-failure"));
  const GatewayAlarmDecision repeat = p.tracker.schedule(
      GatewayAlarmType::Offline, "offline-save-failure", t0 + 10000U);
  if (!repeat.should_play) {
    fprintf(stderr, "failed offline persistence cancelled the live alarm\n");
    exit(5);
  }
  assert(repeat.event.event_token == current.event_token);
}

}  // namespace

int main() {
  testRepeatsQueueThroughFullPipeline();
  testCloseCancelsAndNeverRepublishesClosedWindow();
  testNextWindowStaysSilentAfterClose();
  testCloseClearsMidRepeatSlotStates();
  testOfflineSaveFailureLeavesCurrentAlarmUndismissedAndUncancelled();
  printf("gateway alarm close pipeline regression tests passed\n");
  return 0;
}
