#include <assert.h>
#include <stdint.h>

#include "../../src/gateway_alarm.hpp"

int main() {
  const uint32_t t0 = 700000U;
  GatewayAlarmTracker tracker;
  assert(!tracker.recordTemperature("five-minute-event", 20.0f, true, 5,
                                    1.0f, t0 - 300000U)
              .should_play);
  assert(tracker.recordTemperature("five-minute-event", 21.0f, true, 5,
                                   1.0f, t0)
             .should_play);

  const GatewayAlarmDecision first = tracker.schedule(
      GatewayAlarmType::TemperatureChange, "five-minute-event", t0);
  assert(first.should_play);
  assert(first.event.repeat_index == 0);
  for (uint8_t slot = 1; slot < 30; ++slot) {
    const GatewayAlarmDecision next = tracker.schedule(
        GatewayAlarmType::TemperatureChange, "five-minute-event",
        t0 + static_cast<uint32_t>(slot) * 10000U);
    assert(next.should_play);
    assert(next.event.repeat_index == slot);
    assert(next.event.event_token == first.event.event_token);
    assert(next.event.condition_token == first.event.condition_token);
  }
  assert(!tracker.schedule(GatewayAlarmType::TemperatureChange,
                           "five-minute-event", t0 + 300000U)
              .should_play);
  assert(!tracker.schedule(GatewayAlarmType::TemperatureChange,
                           "five-minute-event", t0 + 600000U)
              .should_play);

  GatewayAlarmTracker offline;
  assert(!offline.markOnline("offline-event"));
  assert(offline.evaluateOffline("offline-event", 60, true, 60).should_play);
  const GatewayAlarmDecision offline_first = offline.schedule(
      GatewayAlarmType::Offline, "offline-event", t0);
  assert(offline_first.should_play);
  assert(!offline.schedule(GatewayAlarmType::Offline, "offline-event",
                           t0 + 300000U)
              .should_play);
  assert(!offline.evaluateOffline("offline-event", 600, true, 60)
              .should_play);
  assert(!offline.schedule(GatewayAlarmType::Offline, "offline-event",
                           t0 + 600000U)
              .should_play);

  assert(offline.markOnline("offline-event"));
  assert(offline.evaluateOffline("offline-event", 60, true, 60).should_play);
  const GatewayAlarmDecision offline_rearmed = offline.schedule(
      GatewayAlarmType::Offline, "offline-event", t0 + 700000U);
  assert(offline_rearmed.should_play);
  assert(offline_rearmed.event.condition_token !=
         offline_first.event.condition_token);
  assert(offline_rearmed.event.event_token != offline_first.event.event_token);
  return 0;
}
