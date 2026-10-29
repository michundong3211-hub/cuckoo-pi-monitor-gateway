#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/gateway_alarm_lifecycle.hpp"

namespace {

GatewayAlarmEvent makeEvent(
    GatewayAlarmType type, const char* deviceId,
    GatewayAlarmEventToken conditionToken,
    GatewayAlarmEventToken eventToken, uint32_t windowIndex = 0,
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

void requireSameEvent(const GatewayAlarmEvent& actual,
                      const GatewayAlarmEvent& expected) {
  assert(actual.type == expected.type);
  assert(strcmp(actual.device_id, expected.device_id) == 0);
  assert(actual.condition_token == expected.condition_token);
  assert(actual.event_token == expected.event_token);
  assert(actual.window_index == expected.window_index);
  assert(actual.repeat_index == expected.repeat_index);
}

bool nextActionForKey(const GatewayAlarmLifecycleCoordinator& lifecycle,
                      GatewayAlarmType type, const char* deviceId,
                      GatewayAlarmLifecycleAction* action) {
  for (size_t slot = 0; slot < GATEWAY_ALARM_LIFECYCLE_CAPACITY; ++slot) {
    GatewayAlarmLifecycleAction candidate{};
    if (lifecycle.nextMainAction(slot, &candidate) &&
        candidate.alarm_type == type &&
        strcmp(candidate.device_id, deviceId) == 0) {
      if (action != nullptr) *action = candidate;
      return true;
    }
  }
  return false;
}

GatewayAlarmLifecycleAction requireMainAction(
    const GatewayAlarmLifecycleCoordinator& lifecycle,
    GatewayAlarmType type, const char* deviceId,
    GatewayAlarmLifecycleActionType expectedType) {
  GatewayAlarmLifecycleAction action{};
  assert(nextActionForKey(lifecycle, type, deviceId, &action));
  assert(action.type == expectedType);
  assert(action.alarm_type == type);
  assert(strcmp(action.device_id, deviceId) == 0);
  return action;
}

GatewayAlarmLifecycleAction requireCloseAction(
    const GatewayAlarmLifecycleCoordinator& lifecycle,
    GatewayAlarmLifecycleActionType expectedType) {
  GatewayAlarmLifecycleAction action{};
  assert(lifecycle.nextCloseAction(&action));
  if (action.type != expectedType) {
    fprintf(stderr, "unexpected close action: got=%u expected=%u\n",
            static_cast<unsigned>(action.type),
            static_cast<unsigned>(expectedType));
    exit(90);
  }
  return action;
}

void finishSubmit(GatewayAlarmLifecycleCoordinator* lifecycle,
                  const GatewayAlarmEvent& event) {
  GatewayAlarmLifecycleAction action = requireMainAction(
      *lifecycle, event.type, event.device_id,
      GatewayAlarmLifecycleActionType::ResetSession);
  requireSameEvent(action.event, event);
  assert(lifecycle->applyResult(
      action, GatewayAlarmLifecycleActionResult::Completed));

  action = requireMainAction(*lifecycle, event.type, event.device_id,
                             GatewayAlarmLifecycleActionType::PublishAlert);
  requireSameEvent(action.event, event);
  assert(lifecycle->applyResult(
      action, GatewayAlarmLifecycleActionResult::Completed));

  action = requireMainAction(*lifecycle, event.type, event.device_id,
                             GatewayAlarmLifecycleActionType::QueueAudio);
  requireSameEvent(action.event, event);
  assert(lifecycle->applyResult(
      action, GatewayAlarmLifecycleActionResult::Completed));
  assert(!nextActionForKey(*lifecycle, event.type, event.device_id, nullptr));
}

void finishClose(GatewayAlarmLifecycleCoordinator* lifecycle,
                 const GatewayAlarmEvent& event) {
  GatewayAlarmLifecycleAction action{};
  if (event.type == GatewayAlarmType::Offline) {
    action = requireCloseAction(
        *lifecycle,
        GatewayAlarmLifecycleActionType::PersistOfflineDismissal);
    requireSameEvent(action.event, event);
    assert(lifecycle->applyResult(
        action, GatewayAlarmLifecycleActionResult::Completed));
  }
  action = requireCloseAction(
      *lifecycle, GatewayAlarmLifecycleActionType::CloseTrackerExact);
  requireSameEvent(action.event, event);
  assert(lifecycle->applyResult(
      action, GatewayAlarmLifecycleActionResult::Completed));
  action = requireCloseAction(
      *lifecycle, GatewayAlarmLifecycleActionType::CancelAudioExact);
  requireSameEvent(action.event, event);
  assert(lifecycle->applyResult(
      action, GatewayAlarmLifecycleActionResult::Completed));
}

void test_submit_retries_only_the_unfinished_phase() {
  GatewayAlarmLifecycleCoordinator lifecycle;
  const GatewayAlarmEvent event = makeEvent(
      GatewayAlarmType::TemperatureChange, "submit", 1001, 2001, 4, 2);

  assert(lifecycle.reserveScheduleKey(event.type, event.device_id) ==
         GatewayAlarmLifecycleReserveResult::ReservedNew);
  assert(lifecycle.retainScheduledEvent(event));

  GatewayAlarmLifecycleAction action = requireMainAction(
      lifecycle, event.type, event.device_id,
      GatewayAlarmLifecycleActionType::ResetSession);
  assert(lifecycle.applyResult(action,
                               GatewayAlarmLifecycleActionResult::Retry));
  action = requireMainAction(lifecycle, event.type, event.device_id,
                             GatewayAlarmLifecycleActionType::ResetSession);
  requireSameEvent(action.event, event);
  assert(lifecycle.applyResult(
      action, GatewayAlarmLifecycleActionResult::Completed));

  action = requireMainAction(lifecycle, event.type, event.device_id,
                             GatewayAlarmLifecycleActionType::PublishAlert);
  assert(lifecycle.applyResult(
      action, GatewayAlarmLifecycleActionResult::Rejected));
  action = requireMainAction(lifecycle, event.type, event.device_id,
                             GatewayAlarmLifecycleActionType::PublishAlert);
  requireSameEvent(action.event, event);
  assert(lifecycle.applyResult(
      action, GatewayAlarmLifecycleActionResult::Completed));

  action = requireMainAction(lifecycle, event.type, event.device_id,
                             GatewayAlarmLifecycleActionType::QueueAudio);
  assert(lifecycle.applyResult(
      action, GatewayAlarmLifecycleActionResult::Rejected));
  action = requireMainAction(lifecycle, event.type, event.device_id,
                             GatewayAlarmLifecycleActionType::QueueAudio);
  requireSameEvent(action.event, event);
  assert(lifecycle.applyResult(
      action, GatewayAlarmLifecycleActionResult::Completed));
  assert(!nextActionForKey(lifecycle, event.type, event.device_id, nullptr));
}

void test_reservation_failure_precedes_schedule_consumption_and_reuses_slot() {
  GatewayAlarmLifecycleCoordinator lifecycle;
  GatewayAlarmEvent events[GATEWAY_ALARM_LIFECYCLE_CAPACITY] = {};
  for (size_t i = 0; i < GATEWAY_ALARM_LIFECYCLE_CAPACITY; ++i) {
    char deviceId[16] = {};
    snprintf(deviceId, sizeof(deviceId), "full-%u",
             static_cast<unsigned>(i));
    events[i] = makeEvent(GatewayAlarmType::Offline, deviceId, 3000 + i,
                          4000 + i);
    assert(lifecycle.reserveScheduleKey(events[i].type,
                                        events[i].device_id) ==
           GatewayAlarmLifecycleReserveResult::ReservedNew);
    assert(lifecycle.retainScheduledEvent(events[i]));
  }

  int scheduleCalls = 0;
  const GatewayAlarmLifecycleReserveResult full =
      lifecycle.reserveScheduleKey(GatewayAlarmType::Offline, "overflow");
  if (full == GatewayAlarmLifecycleReserveResult::ReservedNew ||
      full == GatewayAlarmLifecycleReserveResult::ReservedExisting) {
    ++scheduleCalls;
  }
  assert(full == GatewayAlarmLifecycleReserveResult::Full);
  assert(scheduleCalls == 0);

  assert(lifecycle.requestCleanup(events[0].type, events[0].device_id));
  GatewayAlarmLifecycleAction action = requireMainAction(
      lifecycle, events[0].type, events[0].device_id,
      GatewayAlarmLifecycleActionType::ClearAlertKey);
  assert(lifecycle.applyResult(
      action, GatewayAlarmLifecycleActionResult::NotFound));
  action = requireMainAction(lifecycle, events[0].type, events[0].device_id,
                             GatewayAlarmLifecycleActionType::CancelAudioKey);
  assert(lifecycle.applyResult(
      action, GatewayAlarmLifecycleActionResult::Completed));
  action = requireMainAction(lifecycle, events[0].type, events[0].device_id,
                             GatewayAlarmLifecycleActionType::RetireExact);
  requireSameEvent(action.event, events[0]);
  assert(lifecycle.applyResult(
      action, GatewayAlarmLifecycleActionResult::Completed));

  const GatewayAlarmLifecycleReserveResult reused =
      lifecycle.reserveScheduleKey(GatewayAlarmType::Offline, "overflow");
  if (reused == GatewayAlarmLifecycleReserveResult::ReservedNew ||
      reused == GatewayAlarmLifecycleReserveResult::ReservedExisting) {
    ++scheduleCalls;
  }
  assert(reused == GatewayAlarmLifecycleReserveResult::ReservedNew);
  assert(scheduleCalls == 1);
}

void test_close_retries_preserve_phase_and_ui() {
  GatewayAlarmLifecycleCoordinator lifecycle;
  const GatewayAlarmEvent event = makeEvent(
      GatewayAlarmType::Offline, "close", 5001, 6001, 3, 1);
  assert(lifecycle.reserveScheduleKey(event.type, event.device_id) ==
         GatewayAlarmLifecycleReserveResult::ReservedNew);
  assert(lifecycle.retainScheduledEvent(event));
  finishSubmit(&lifecycle, event);

  assert(lifecycle.guardClose(event));
  assert(!lifecycle.canTearDownAlertUi());
  assert(!lifecycle.nextCloseAction(nullptr));
  assert(lifecycle.confirmClose(event));

  GatewayAlarmLifecycleAction action = requireCloseAction(
      lifecycle, GatewayAlarmLifecycleActionType::PersistOfflineDismissal);
  assert(lifecycle.applyResult(action,
                               GatewayAlarmLifecycleActionResult::Retry));
  action = requireCloseAction(
      lifecycle, GatewayAlarmLifecycleActionType::PersistOfflineDismissal);
  requireSameEvent(action.event, event);
  assert(lifecycle.applyResult(
      action, GatewayAlarmLifecycleActionResult::Completed));

  action = requireCloseAction(
      lifecycle, GatewayAlarmLifecycleActionType::CloseTrackerExact);
  assert(lifecycle.applyResult(action,
                               GatewayAlarmLifecycleActionResult::Retry));
  action = requireCloseAction(
      lifecycle, GatewayAlarmLifecycleActionType::CloseTrackerExact);
  requireSameEvent(action.event, event);
  assert(lifecycle.applyResult(
      action, GatewayAlarmLifecycleActionResult::Completed));

  action = requireCloseAction(
      lifecycle, GatewayAlarmLifecycleActionType::CancelAudioExact);
  assert(lifecycle.applyResult(action,
                               GatewayAlarmLifecycleActionResult::Retry));
  assert(!lifecycle.canTearDownAlertUi());
  action = requireCloseAction(
      lifecycle, GatewayAlarmLifecycleActionType::CancelAudioExact);
  requireSameEvent(action.event, event);
  assert(lifecycle.applyResult(
      action, GatewayAlarmLifecycleActionResult::NotFound));
  assert(lifecycle.canTearDownAlertUi());
  GatewayAlarmLifecycleAction noAction{};
  assert(!lifecycle.nextCloseAction(&noAction));
}

void test_exact_close_releases_only_matching_pending_audio_queue() {
  GatewayAlarmLifecycleCoordinator lifecycle;
  const GatewayAlarmEvent event = makeEvent(
      GatewayAlarmType::Offline, "close-before-queue", 6101, 6201, 5, 1);
  const GatewayAlarmEvent stale = makeEvent(
      GatewayAlarmType::Offline, "close-before-queue", 6101, 6299, 4, 0);
  const GatewayAlarmEvent nextWindow = makeEvent(
      GatewayAlarmType::Offline, "close-before-queue", 6101, 6202, 6, 0);

  assert(lifecycle.reserveScheduleKey(event.type, event.device_id) ==
         GatewayAlarmLifecycleReserveResult::ReservedNew);
  assert(lifecycle.retainScheduledEvent(event));

  GatewayAlarmLifecycleAction action = requireMainAction(
      lifecycle, event.type, event.device_id,
      GatewayAlarmLifecycleActionType::ResetSession);
  assert(lifecycle.applyResult(
      action, GatewayAlarmLifecycleActionResult::Completed));
  action = requireMainAction(lifecycle, event.type, event.device_id,
                             GatewayAlarmLifecycleActionType::PublishAlert);
  assert(lifecycle.applyResult(
      action, GatewayAlarmLifecycleActionResult::Completed));
  action = requireMainAction(lifecycle, event.type, event.device_id,
                             GatewayAlarmLifecycleActionType::QueueAudio);
  requireSameEvent(action.event, event);

  assert(lifecycle.guardClose(stale));
  assert(lifecycle.confirmClose(stale));
  finishClose(&lifecycle, stale);
  action = requireMainAction(lifecycle, event.type, event.device_id,
                             GatewayAlarmLifecycleActionType::QueueAudio);
  requireSameEvent(action.event, event);

  assert(lifecycle.guardClose(event));
  assert(lifecycle.confirmClose(event));
  finishClose(&lifecycle, event);
  if (nextActionForKey(lifecycle, event.type, event.device_id, nullptr)) {
    fprintf(stderr,
            "matching exact CLOSE left the pending QueueAudio phase busy\n");
    exit(2);
  }

  assert(lifecycle.reserveScheduleKey(event.type, event.device_id) ==
         GatewayAlarmLifecycleReserveResult::ReservedExisting);
  assert(lifecycle.retainScheduledEvent(nextWindow));
  finishSubmit(&lifecycle, nextWindow);
}

void test_not_found_close_releases_only_matching_pending_audio_queue() {
  GatewayAlarmLifecycleCoordinator lifecycle;
  const GatewayAlarmEvent event = makeEvent(
      GatewayAlarmType::Offline, "missing-before-queue", 6301, 6401, 7, 2);
  const GatewayAlarmEvent stale = makeEvent(
      GatewayAlarmType::Offline, "missing-before-queue", 6301, 6499, 6, 1);
  const GatewayAlarmEvent nextWindow = makeEvent(
      GatewayAlarmType::Offline, "missing-before-queue", 6301, 6402, 8, 0);

  assert(lifecycle.reserveScheduleKey(event.type, event.device_id) ==
         GatewayAlarmLifecycleReserveResult::ReservedNew);
  assert(lifecycle.retainScheduledEvent(event));

  GatewayAlarmLifecycleAction action = requireMainAction(
      lifecycle, event.type, event.device_id,
      GatewayAlarmLifecycleActionType::ResetSession);
  assert(lifecycle.applyResult(
      action, GatewayAlarmLifecycleActionResult::Completed));
  action = requireMainAction(lifecycle, event.type, event.device_id,
                             GatewayAlarmLifecycleActionType::PublishAlert);
  assert(lifecycle.applyResult(
      action, GatewayAlarmLifecycleActionResult::Completed));
  action = requireMainAction(lifecycle, event.type, event.device_id,
                             GatewayAlarmLifecycleActionType::QueueAudio);
  requireSameEvent(action.event, event);

  assert(lifecycle.guardClose(stale));
  assert(lifecycle.confirmClose(stale));
  action = requireCloseAction(
      lifecycle, GatewayAlarmLifecycleActionType::PersistOfflineDismissal);
  assert(lifecycle.applyResult(
      action, GatewayAlarmLifecycleActionResult::Completed));
  action = requireCloseAction(
      lifecycle, GatewayAlarmLifecycleActionType::CloseTrackerExact);
  requireSameEvent(action.event, stale);
  assert(lifecycle.applyResult(
      action, GatewayAlarmLifecycleActionResult::Completed));
  action = requireCloseAction(
      lifecycle, GatewayAlarmLifecycleActionType::CancelAudioExact);
  requireSameEvent(action.event, stale);
  assert(lifecycle.applyResult(
      action, GatewayAlarmLifecycleActionResult::NotFound));
  action = requireMainAction(lifecycle, event.type, event.device_id,
                             GatewayAlarmLifecycleActionType::QueueAudio);
  requireSameEvent(action.event, event);

  assert(lifecycle.guardClose(event));
  assert(lifecycle.confirmClose(event));
  action = requireCloseAction(
      lifecycle, GatewayAlarmLifecycleActionType::PersistOfflineDismissal);
  assert(lifecycle.applyResult(
      action, GatewayAlarmLifecycleActionResult::Completed));
  action = requireCloseAction(
      lifecycle, GatewayAlarmLifecycleActionType::CloseTrackerExact);
  requireSameEvent(action.event, event);
  assert(lifecycle.applyResult(
      action, GatewayAlarmLifecycleActionResult::Completed));

  action = requireCloseAction(
      lifecycle, GatewayAlarmLifecycleActionType::CancelAudioExact);
  assert(lifecycle.applyResult(action,
                               GatewayAlarmLifecycleActionResult::Retry));
  action = requireCloseAction(
      lifecycle, GatewayAlarmLifecycleActionType::CancelAudioExact);
  requireSameEvent(action.event, event);
  requireSameEvent(requireMainAction(
                       lifecycle, event.type, event.device_id,
                       GatewayAlarmLifecycleActionType::QueueAudio)
                       .event,
                   event);

  assert(lifecycle.applyResult(
      action, GatewayAlarmLifecycleActionResult::Rejected));
  action = requireCloseAction(
      lifecycle, GatewayAlarmLifecycleActionType::CancelAudioExact);
  requireSameEvent(action.event, event);
  requireSameEvent(requireMainAction(
                       lifecycle, event.type, event.device_id,
                       GatewayAlarmLifecycleActionType::QueueAudio)
                       .event,
                   event);

  assert(lifecycle.applyResult(
      action, GatewayAlarmLifecycleActionResult::NotFound));
  if (nextActionForKey(lifecycle, event.type, event.device_id, nullptr)) {
    fprintf(stderr,
            "matching missing CLOSE left the pending QueueAudio phase busy\n");
    exit(3);
  }
  GatewayAlarmLifecycleAction noCloseAction{};
  assert(!lifecycle.nextCloseAction(&noCloseAction));

  assert(lifecycle.reserveScheduleKey(event.type, event.device_id) ==
         GatewayAlarmLifecycleReserveResult::ReservedExisting);
  assert(lifecycle.requestCleanup(event.type, event.device_id));
  action = requireMainAction(lifecycle, event.type, event.device_id,
                             GatewayAlarmLifecycleActionType::ClearAlertKey);
  requireSameEvent(action.event, event);
  assert(lifecycle.applyResult(
      action, GatewayAlarmLifecycleActionResult::NotFound));
  action = requireMainAction(lifecycle, event.type, event.device_id,
                             GatewayAlarmLifecycleActionType::CancelAudioKey);
  requireSameEvent(action.event, event);
  assert(lifecycle.applyResult(
      action, GatewayAlarmLifecycleActionResult::NotFound));
  action = requireMainAction(lifecycle, event.type, event.device_id,
                             GatewayAlarmLifecycleActionType::RetireExact);
  requireSameEvent(action.event, event);
  assert(lifecycle.applyResult(
      action, GatewayAlarmLifecycleActionResult::Completed));

  assert(lifecycle.reserveScheduleKey(nextWindow.type, nextWindow.device_id) ==
         GatewayAlarmLifecycleReserveResult::ReservedNew);
  assert(lifecycle.retainScheduledEvent(nextWindow));
  finishSubmit(&lifecycle, nextWindow);
}

void test_failed_dismiss_guard_never_becomes_actionable_close() {
  GatewayAlarmLifecycleCoordinator lifecycle;
  const GatewayAlarmEvent event = makeEvent(
      GatewayAlarmType::TemperatureChange, "stale", 7001, 8001);
  assert(lifecycle.guardClose(event));
  assert(!lifecycle.canTearDownAlertUi());
  assert(lifecycle.abortClose(event));
  assert(lifecycle.canTearDownAlertUi());
  GatewayAlarmLifecycleAction action{};
  assert(!lifecycle.nextCloseAction(&action));
}

void test_cuckoo_offline_close_persists_before_tracker_close() {
  GatewayAlarmLifecycleCoordinator lifecycle;
  const GatewayAlarmEvent event = makeEvent(
      GatewayAlarmType::CuckooOffline, "001122334455", 7001, 7002, 0, 0);
  assert(lifecycle.reserveScheduleKey(event.type, event.device_id) ==
         GatewayAlarmLifecycleReserveResult::ReservedNew);
  assert(lifecycle.retainScheduledEvent(event));
  finishSubmit(&lifecycle, event);
  assert(lifecycle.guardClose(event));
  assert(lifecycle.confirmClose(event));

  GatewayAlarmLifecycleAction action = requireCloseAction(
      lifecycle, GatewayAlarmLifecycleActionType::PersistOfflineDismissal);
  requireSameEvent(action.event, event);
}

void test_offline_close_waits_for_persistence_before_destructive_actions() {
  GatewayAlarmLifecycleCoordinator lifecycle;
  const GatewayAlarmEvent event = makeEvent(
      GatewayAlarmType::Offline, "persist-first", 7051, 8051);
  assert(lifecycle.guardClose(event));
  assert(lifecycle.confirmClose(event));

  GatewayAlarmLifecycleAction action{};
  assert(lifecycle.nextCloseAction(&action));
  if (action.type == GatewayAlarmLifecycleActionType::CloseTrackerExact ||
      action.type == GatewayAlarmLifecycleActionType::CancelAudioExact) {
    fprintf(stderr,
            "offline CLOSE exposed destructive action before persistence\n");
    exit(4);
  }
  requireSameEvent(action.event, event);
  assert(lifecycle.applyResult(action,
                               GatewayAlarmLifecycleActionResult::Retry));

  GatewayAlarmLifecycleAction retry{};
  assert(lifecycle.nextCloseAction(&retry));
  assert(retry.type == action.type);
  requireSameEvent(retry.event, event);
  assert(lifecycle.applyResult(retry,
                               GatewayAlarmLifecycleActionResult::Completed));

  const GatewayAlarmLifecycleAction tracker = requireCloseAction(
      lifecycle, GatewayAlarmLifecycleActionType::CloseTrackerExact);
  requireSameEvent(tracker.event, event);
}

void test_alert_ui_teardown_reservation_blocks_close_and_releases() {
  GatewayAlarmLifecycleCoordinator lifecycle;
  const GatewayAlarmEvent event = makeEvent(
      GatewayAlarmType::TemperatureChange, "reservation", 7101, 8101);

  assert(lifecycle.reserveAlertUiTeardown());
  assert(!lifecycle.reserveAlertUiTeardown());
  assert(!lifecycle.canTearDownAlertUi());
  assert(!lifecycle.guardClose(event));
  assert(lifecycle.releaseAlertUiTeardown());
  assert(lifecycle.canTearDownAlertUi());
  assert(lifecycle.guardClose(event));
  assert(lifecycle.abortClose(event));
}

void test_alert_ui_teardown_failure_releases_reservation() {
  GatewayAlarmLifecycleCoordinator lifecycle;
  assert(lifecycle.reserveAlertUiTeardown());
  const bool cleanup_succeeded = false;
  if (!cleanup_succeeded) {
    assert(lifecycle.releaseAlertUiTeardown());
  }
  assert(lifecycle.canTearDownAlertUi());
}

void test_cleanup_waits_for_close_then_retires_and_releases() {
  GatewayAlarmLifecycleCoordinator lifecycle;
  const GatewayAlarmEvent event = makeEvent(
      GatewayAlarmType::TemperatureChange, "wait", 9001, 10001);
  assert(lifecycle.reserveScheduleKey(event.type, event.device_id) ==
         GatewayAlarmLifecycleReserveResult::ReservedNew);
  assert(lifecycle.retainScheduledEvent(event));
  finishSubmit(&lifecycle, event);
  assert(lifecycle.guardClose(event));
  assert(lifecycle.confirmClose(event));
  assert(lifecycle.requestCleanup(event.type, event.device_id));

  GatewayAlarmLifecycleAction action = requireMainAction(
      lifecycle, event.type, event.device_id,
      GatewayAlarmLifecycleActionType::ClearAlertKey);
  assert(lifecycle.applyResult(
      action, GatewayAlarmLifecycleActionResult::Completed));
  action = requireMainAction(lifecycle, event.type, event.device_id,
                             GatewayAlarmLifecycleActionType::CancelAudioKey);
  assert(lifecycle.applyResult(
      action, GatewayAlarmLifecycleActionResult::NotFound));
  assert(!nextActionForKey(lifecycle, event.type, event.device_id, nullptr));

  finishClose(&lifecycle, event);
  action = requireMainAction(lifecycle, event.type, event.device_id,
                             GatewayAlarmLifecycleActionType::RetireExact);
  assert(lifecycle.applyResult(
      action, GatewayAlarmLifecycleActionResult::Completed));
  assert(lifecycle.reserveScheduleKey(event.type, event.device_id) ==
         GatewayAlarmLifecycleReserveResult::ReservedNew);
}

void test_cleanup_retries_only_the_unfinished_phase() {
  GatewayAlarmLifecycleCoordinator lifecycle;
  const GatewayAlarmEvent event = makeEvent(
      GatewayAlarmType::Offline, "cleanup-retry", 10501, 10601, 8, 3);
  assert(lifecycle.reserveScheduleKey(event.type, event.device_id) ==
         GatewayAlarmLifecycleReserveResult::ReservedNew);
  assert(lifecycle.retainScheduledEvent(event));
  finishSubmit(&lifecycle, event);
  assert(lifecycle.requestCleanup(event.type, event.device_id));

  GatewayAlarmLifecycleAction action = requireMainAction(
      lifecycle, event.type, event.device_id,
      GatewayAlarmLifecycleActionType::ClearAlertKey);
  assert(lifecycle.applyResult(action,
                               GatewayAlarmLifecycleActionResult::Retry));
  action = requireMainAction(lifecycle, event.type, event.device_id,
                             GatewayAlarmLifecycleActionType::ClearAlertKey);
  requireSameEvent(action.event, event);
  assert(lifecycle.applyResult(
      action, GatewayAlarmLifecycleActionResult::NotFound));

  action = requireMainAction(lifecycle, event.type, event.device_id,
                             GatewayAlarmLifecycleActionType::CancelAudioKey);
  assert(lifecycle.applyResult(action,
                               GatewayAlarmLifecycleActionResult::Retry));
  action = requireMainAction(lifecycle, event.type, event.device_id,
                             GatewayAlarmLifecycleActionType::CancelAudioKey);
  requireSameEvent(action.event, event);
  assert(lifecycle.applyResult(
      action, GatewayAlarmLifecycleActionResult::Completed));

  action = requireMainAction(lifecycle, event.type, event.device_id,
                             GatewayAlarmLifecycleActionType::RetireExact);
  assert(lifecycle.applyResult(action,
                               GatewayAlarmLifecycleActionResult::Retry));
  action = requireMainAction(lifecycle, event.type, event.device_id,
                             GatewayAlarmLifecycleActionType::RetireExact);
  requireSameEvent(action.event, event);
  assert(lifecycle.applyResult(
      action, GatewayAlarmLifecycleActionResult::Completed));
  assert(lifecycle.reserveScheduleKey(event.type, event.device_id) ==
         GatewayAlarmLifecycleReserveResult::ReservedNew);
}

void test_cleanup_before_close_requires_second_retire_after_exact_cancel() {
  GatewayAlarmLifecycleCoordinator lifecycle;
  const GatewayAlarmEvent event = makeEvent(
      GatewayAlarmType::Offline, "interleave", 11001, 12001);
  assert(lifecycle.reserveScheduleKey(event.type, event.device_id) ==
         GatewayAlarmLifecycleReserveResult::ReservedNew);
  assert(lifecycle.retainScheduledEvent(event));
  finishSubmit(&lifecycle, event);
  assert(lifecycle.requestCleanup(event.type, event.device_id));

  GatewayAlarmLifecycleAction action = requireMainAction(
      lifecycle, event.type, event.device_id,
      GatewayAlarmLifecycleActionType::ClearAlertKey);
  assert(lifecycle.applyResult(
      action, GatewayAlarmLifecycleActionResult::Completed));
  action = requireMainAction(lifecycle, event.type, event.device_id,
                             GatewayAlarmLifecycleActionType::CancelAudioKey);
  assert(lifecycle.applyResult(
      action, GatewayAlarmLifecycleActionResult::Completed));
  const GatewayAlarmLifecycleAction firstRetire = requireMainAction(
      lifecycle, event.type, event.device_id,
      GatewayAlarmLifecycleActionType::RetireExact);

  assert(lifecycle.guardClose(event));
  assert(lifecycle.confirmClose(event));
  assert(lifecycle.applyResult(
      firstRetire, GatewayAlarmLifecycleActionResult::Completed));
  assert(!nextActionForKey(lifecycle, event.type, event.device_id, nullptr));

  finishClose(&lifecycle, event);
  action = requireMainAction(lifecycle, event.type, event.device_id,
                             GatewayAlarmLifecycleActionType::RetireExact);
  assert(lifecycle.applyResult(
      action, GatewayAlarmLifecycleActionResult::Completed));
  assert(lifecycle.reserveScheduleKey(event.type, event.device_id) ==
         GatewayAlarmLifecycleReserveResult::ReservedNew);
}

void test_more_than_capacity_rotations_reuse_fixed_storage() {
  GatewayAlarmLifecycleCoordinator lifecycle;
  for (uint32_t i = 0; i < 80; ++i) {
    char deviceId[20] = {};
    snprintf(deviceId, sizeof(deviceId), "rotate-%lu",
             static_cast<unsigned long>(i));
    const GatewayAlarmEvent event = makeEvent(
        GatewayAlarmType::Offline, deviceId, 20000 + i, 30000 + i);
    assert(lifecycle.reserveScheduleKey(event.type, event.device_id) ==
           GatewayAlarmLifecycleReserveResult::ReservedNew);
    assert(lifecycle.retainScheduledEvent(event));
    finishSubmit(&lifecycle, event);
    assert(lifecycle.requestCleanup(event.type, event.device_id));

    GatewayAlarmLifecycleAction action = requireMainAction(
        lifecycle, event.type, event.device_id,
        GatewayAlarmLifecycleActionType::ClearAlertKey);
    assert(lifecycle.applyResult(
        action, GatewayAlarmLifecycleActionResult::NotFound));
    action = requireMainAction(
        lifecycle, event.type, event.device_id,
        GatewayAlarmLifecycleActionType::CancelAudioKey);
    assert(lifecycle.applyResult(
        action, GatewayAlarmLifecycleActionResult::NotFound));
    action = requireMainAction(lifecycle, event.type, event.device_id,
                               GatewayAlarmLifecycleActionType::RetireExact);
    assert(lifecycle.applyResult(
        action, GatewayAlarmLifecycleActionResult::Completed));
  }
}

}  // namespace

int main() {
  test_submit_retries_only_the_unfinished_phase();
  test_reservation_failure_precedes_schedule_consumption_and_reuses_slot();
  test_close_retries_preserve_phase_and_ui();
  test_cuckoo_offline_close_persists_before_tracker_close();
  test_exact_close_releases_only_matching_pending_audio_queue();
  test_not_found_close_releases_only_matching_pending_audio_queue();
  test_failed_dismiss_guard_never_becomes_actionable_close();
  test_offline_close_waits_for_persistence_before_destructive_actions();
  test_alert_ui_teardown_reservation_blocks_close_and_releases();
  test_alert_ui_teardown_failure_releases_reservation();
  test_cleanup_waits_for_close_then_retires_and_releases();
  test_cleanup_retries_only_the_unfinished_phase();
  test_cleanup_before_close_requires_second_retire_after_exact_cancel();
  test_more_than_capacity_rotations_reuse_fixed_storage();
  return 0;
}
