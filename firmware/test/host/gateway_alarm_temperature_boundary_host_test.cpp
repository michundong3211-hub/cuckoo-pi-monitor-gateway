#include <assert.h>

#include "../../src/gateway_alarm.hpp"

int main() {
  GatewayAlarmTracker tracker;
  assert(!tracker.recordTemperature("clear-boundary", 20.0f, true, 5,
                                    1.25f, 0)
              .should_play);
  assert(tracker.recordTemperature("clear-boundary", 22.0f, true, 5,
                                   1.25f, 300000U)
             .should_play);

  const GatewayAlarmDecision cleared = tracker.recordTemperature(
      "clear-boundary", 21.0f, true, 5, 1.25f, 600000U);
  assert(!cleared.should_play);
  assert(cleared.should_clear);
  return 0;
}
