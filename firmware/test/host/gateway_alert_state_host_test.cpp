#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "../../src/gateway_alert_state.hpp"

namespace {

GatewayAlarmEvent makeAlertEvent(const char* deviceId,
                                 GatewayAlarmEventToken eventToken,
                                 uint8_t repeatIndex,
                                 GatewayAlarmType type =
                                     GatewayAlarmType::TemperatureChange,
                                 GatewayAlarmEventToken conditionToken = 7001,
                                 uint32_t windowIndex = 9) {
  GatewayAlarmEvent event{};
  event.type = type;
  if (deviceId != nullptr) {
    strncpy(event.device_id, deviceId, sizeof(event.device_id) - 1);
  }
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

void test_full_event_fifo_and_exact_dismissal() {
  GatewayAlertState state;
  const GatewayAlarmEvent old_event = makeAlertEvent("A", 101, 0);
  const GatewayAlarmEvent new_event = makeAlertEvent("A", 202, 0);

  assert(state.publish(old_event));

  GatewayAlarmEvent current{};
  assert(state.peek(&current));
  requireSameEvent(current, old_event);

  GatewayAlarmEvent dismissed{};
  assert(!state.dismissCurrentExact(new_event.event_token, &dismissed));
  assert(state.pendingCount() == 1);
  assert(state.dismissCurrentExact(old_event.event_token, &dismissed));
  requireSameEvent(dismissed, old_event);
  assert(state.pendingCount() == 0);

  assert(state.publish(new_event));
  assert(!state.dismissCurrentExact(old_event.event_token, &dismissed));
  assert(state.peek(&current));
  requireSameEvent(current, new_event);
  assert(state.dismissCurrentExact(new_event.event_token, &dismissed));
  requireSameEvent(dismissed, new_event);
  assert(!state.dismissCurrentExact(new_event.event_token, &dismissed));
}

void test_null_dismiss_output_leaves_state_untouched() {
  GatewayAlertState state;
  const GatewayAlarmEvent current_event =
      makeAlertEvent("null-current", 211, 0);
  const GatewayAlarmEvent queued_event = makeAlertEvent(
      "null-queued", 212, 0, GatewayAlarmType::Offline, 7012, 10);

  assert(state.publish(current_event));
  assert(state.publish(queued_event));
  assert(!state.dismissCurrentExact(current_event.event_token, nullptr));
  assert(state.pendingCount() == 2);

  GatewayAlarmEvent current{};
  assert(state.peek(&current));
  requireSameEvent(current, current_event);

  GatewayAlarmEvent dismissed{};
  assert(state.dismissCurrentExact(current_event.event_token, &dismissed));
  requireSameEvent(dismissed, current_event);
  assert(state.peek(&current));
  requireSameEvent(current, queued_event);
}

void test_publish_rejects_invalid_and_deduplicates_with_idempotent_success() {
  GatewayAlertState state;
  const GatewayAlarmEvent event = makeAlertEvent("A", 301, 0);
  const GatewayAlarmEvent same_token =
      makeAlertEvent("B", 301, 1, GatewayAlarmType::Offline, 8002, 10);
  const GatewayAlarmEvent same_key =
      makeAlertEvent("A", 302, 0, GatewayAlarmType::TemperatureChange, 7001,
                     9);

  assert(!state.publish(makeAlertEvent("invalid", 0, 0)));
  assert(state.publish(event));
  // Same-identity republish is an idempotent success: the alert is already
  // visible, and the lifecycle advances past PublishAlert for same-window
  // repeats (which share one event_token) so their audio can be queued. No
  // duplicate is stored.
  assert(state.publish(event));
  assert(state.publish(same_token));
  assert(state.publish(same_key));
  assert(state.pendingCount() == 1);

  const GatewayAlarmEvent other_type =
      makeAlertEvent("A", 303, 0, GatewayAlarmType::Offline, 8003, 11);
  const GatewayAlarmEvent other_device =
      makeAlertEvent("B", 304, 0, GatewayAlarmType::TemperatureChange, 8004,
                     12);
  assert(state.publish(other_type));
  assert(state.publish(other_device));
  assert(state.pendingCount() == 3);
}

void test_clear_by_key_preserves_other_keys_and_fifo() {
  GatewayAlertState state;
  const GatewayAlarmEvent offline_a =
      makeAlertEvent("A", 401, 0, GatewayAlarmType::Offline, 9001, 1);
  const GatewayAlarmEvent temperature_a =
      makeAlertEvent("A", 402, 0, GatewayAlarmType::TemperatureChange, 9002,
                     2);
  const GatewayAlarmEvent offline_b =
      makeAlertEvent("B", 403, 0, GatewayAlarmType::Offline, 9003, 3);
  const GatewayAlarmEvent temperature_b =
      makeAlertEvent("B", 404, 0, GatewayAlarmType::TemperatureChange, 9004,
                     4);
  const GatewayAlarmEvent offline_a_new =
      makeAlertEvent("A", 405, 0, GatewayAlarmType::Offline, 9005, 5);

  assert(state.publish(offline_a));
  assert(state.publish(temperature_a));
  assert(state.publish(offline_b));
  assert(state.publish(temperature_b));

  assert(state.clearByKey(GatewayAlarmType::Offline, "A"));
  assert(state.pendingCount() == 3);
  assert(state.publish(offline_a_new));
  assert(state.pendingCount() == 4);

  GatewayAlarmEvent current{};
  assert(state.peek(&current));
  requireSameEvent(current, temperature_a);

  GatewayAlarmEvent dismissed{};
  assert(!state.dismissCurrentExact(offline_a.event_token, &dismissed));
  assert(state.pendingCount() == 4);
  assert(state.dismissCurrentExact(temperature_a.event_token, &dismissed));
  requireSameEvent(dismissed, temperature_a);
  assert(state.peek(&current));
  requireSameEvent(current, offline_b);
  assert(state.dismissCurrentExact(offline_b.event_token, &dismissed));
  assert(state.peek(&current));
  requireSameEvent(current, temperature_b);
  assert(state.dismissCurrentExact(temperature_b.event_token, &dismissed));
  assert(state.peek(&current));
  requireSameEvent(current, offline_a_new);
  assert(state.dismissCurrentExact(offline_a_new.event_token, &dismissed));
  assert(!state.clearByKey(GatewayAlarmType::Offline, "A"));
}

void test_clear_result_distinguishes_cleared_not_found_and_invalid_key() {
  GatewayAlertState state;
  const GatewayAlarmEvent event = makeAlertEvent(
      "clear-result", 430, 0, GatewayAlarmType::Offline, 9430, 6);

  assert(state.clearByKeyResult(GatewayAlarmType::Offline, "") ==
         GatewayAlertClearResult::NotFound);
  assert(state.clearByKeyResult(GatewayAlarmType::Offline, event.device_id) ==
         GatewayAlertClearResult::NotFound);
  assert(state.publish(event));
  assert(state.clearByKeyResult(GatewayAlarmType::Offline, event.device_id) ==
         GatewayAlertClearResult::Cleared);
  assert(state.clearByKeyResult(GatewayAlarmType::Offline, event.device_id) ==
         GatewayAlertClearResult::NotFound);

  assert(state.publish(event));
  assert(state.clearByKey(GatewayAlarmType::Offline, event.device_id));
  assert(!state.clearByKey(GatewayAlarmType::Offline, event.device_id));
}

void test_clear_by_key_removes_all_queued_matches_and_retains_fifo() {
  GatewayAlertState state;
  const GatewayAlarmEvent current = makeAlertEvent(
      "current", 451, 0, GatewayAlarmType::TemperatureChange, 9451, 1);
  const GatewayAlarmEvent offline_a_first =
      makeAlertEvent("A", 452, 0, GatewayAlarmType::Offline, 9452, 2);
  const GatewayAlarmEvent offline_b =
      makeAlertEvent("B", 453, 0, GatewayAlarmType::Offline, 9453, 3);
  const GatewayAlarmEvent offline_a_second =
      makeAlertEvent("A", 454, 0, GatewayAlarmType::Offline, 9454, 4);
  const GatewayAlarmEvent temperature_a =
      makeAlertEvent("A", 455, 0, GatewayAlarmType::TemperatureChange, 9455,
                     5);
  const GatewayAlarmEvent offline_a_third =
      makeAlertEvent("A", 456, 0, GatewayAlarmType::Offline, 9456, 6);
  const GatewayAlarmEvent temperature_c =
      makeAlertEvent("C", 457, 0, GatewayAlarmType::TemperatureChange, 9457,
                     7);

  assert(state.publish(current));
  assert(state.publish(offline_a_first));
  assert(state.publish(offline_b));
  assert(state.publish(offline_a_second));
  assert(state.publish(temperature_a));
  assert(state.publish(offline_a_third));
  assert(state.publish(temperature_c));

  assert(state.clearByKey(GatewayAlarmType::Offline, "A"));
  assert(state.pendingCount() == 4);

  const GatewayAlarmEvent expected_fifo[] = {current, offline_b, temperature_a,
                                              temperature_c};
  for (const GatewayAlarmEvent& expected : expected_fifo) {
    GatewayAlarmEvent actual{};
    assert(state.peek(&actual));
    requireSameEvent(actual, expected);
    assert(state.dismissCurrentExact(expected.event_token, &actual));
    requireSameEvent(actual, expected);
  }
  assert(state.pendingCount() == 0);
}

void test_clear_by_token_removes_only_closed_window_entries() {
  GatewayAlertState state;
  const GatewayAlarmEvent closed_current =
      makeAlertEvent("A", 471, 0, GatewayAlarmType::Offline, 9471, 1);
  const GatewayAlarmEvent next_window_queued =
      makeAlertEvent("A", 472, 0, GatewayAlarmType::Offline, 9471, 2);
  const GatewayAlarmEvent other_key =
      makeAlertEvent("B", 473, 0, GatewayAlarmType::TemperatureChange, 9473,
                     3);

  assert(state.publish(closed_current));
  assert(state.publish(next_window_queued));
  assert(state.publish(other_key));
  assert(state.pendingCount() == 3);

  // CLOSE semantics: removing the closed window's token must drop only that
  // window (current + any same-token stale entry) and promote the next queued
  // alert, leaving other windows/keys intact.
  assert(state.clearByTokenResult(closed_current.event_token) ==
         GatewayAlertClearResult::Cleared);
  assert(state.pendingCount() == 2);

  GatewayAlarmEvent current{};
  assert(state.peek(&current));
  requireSameEvent(current, next_window_queued);
  assert(state.clearByTokenResult(472) == GatewayAlertClearResult::Cleared);
  assert(state.pendingCount() == 1);
  assert(state.peek(&current));
  requireSameEvent(current, other_key);

  // Unknown token is NotFound; invalid token is NotFound.
  assert(state.clearByTokenResult(999) == GatewayAlertClearResult::NotFound);
  assert(state.clearByTokenResult(GATEWAY_INVALID_EVENT_TOKEN) ==
         GatewayAlertClearResult::NotFound);
  assert(state.pendingCount() == 1);

  // A queued (non-current) token is still cleared.
  const GatewayAlarmEvent queued_only =
      makeAlertEvent("C", 474, 0, GatewayAlarmType::Offline, 9474, 4);
  assert(state.publish(queued_only));
  assert(state.pendingCount() == 2);
  assert(state.clearByTokenResult(474) == GatewayAlertClearResult::Cleared);
  assert(state.pendingCount() == 1);
  assert(state.peek(&current));
  requireSameEvent(current, other_key);
}

void test_fixed_capacity_and_invalid_ids() {
  GatewayAlertState state;
  assert(!state.publish(makeAlertEvent("", 501, 0)));
  assert(!state.publish(makeAlertEvent(nullptr, 502, 0)));

  GatewayAlarmEvent oversized = makeAlertEvent("valid", 503, 0);
  memset(oversized.device_id, 'x', sizeof(oversized.device_id));
  assert(!state.publish(oversized));

  for (uint32_t i = 0; i < GATEWAY_ALERT_QUEUE_CAPACITY; ++i) {
    char deviceId[8] = {};
    deviceId[0] = 'D';
    deviceId[1] = static_cast<char>('0' + i);
    assert(state.publish(makeAlertEvent(deviceId, 600 + i, 0,
                                        GatewayAlarmType::Offline, 9100 + i,
                                        i)));
  }
  assert(state.pendingCount() == GATEWAY_ALERT_QUEUE_CAPACITY);
  assert(!state.publish(makeAlertEvent("overflow", 699, 0,
                                       GatewayAlarmType::Offline, 9199, 99)));

  GatewayAlertEvent dismissed{};
  assert(state.dismissCurrentExact(600, &dismissed));
  assert(state.pendingCount() == GATEWAY_ALERT_QUEUE_CAPACITY - 1);
  assert(state.publish(makeAlertEvent("overflow", 700, 0,
                                      GatewayAlarmType::Offline, 9200, 100)));
}

}  // namespace

int main() {
  test_full_event_fifo_and_exact_dismissal();
  test_null_dismiss_output_leaves_state_untouched();
  test_publish_rejects_invalid_and_deduplicates_with_idempotent_success();
  test_clear_by_key_preserves_other_keys_and_fifo();
  test_clear_result_distinguishes_cleared_not_found_and_invalid_key();
  test_clear_by_key_removes_all_queued_matches_and_retains_fifo();
  test_clear_by_token_removes_only_closed_window_entries();
  test_fixed_capacity_and_invalid_ids();
  return 0;
}
