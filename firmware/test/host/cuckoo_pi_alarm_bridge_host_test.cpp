#include "gateway_alarm.hpp"

#include <cassert>

int main() {
  GatewayAlarmTracker tracker;
  constexpr uint32_t t0 = 1000U;

  GatewayAlarmDecision offline = tracker.setExternalCondition(
      GatewayAlarmType::CuckooOffline, "001122334455", true, t0);
  assert(offline.should_play);
  GatewayAlarmDecision offline_event = tracker.schedule(
      GatewayAlarmType::CuckooOffline, "001122334455", t0);
  assert(offline_event.should_play);
  assert(offline_event.event.type == GatewayAlarmType::CuckooOffline);
  assert(tracker.validateOfflineDismissalExact(offline_event.event) ==
         GatewayAlarmCloseResult::Closed);
  assert(tracker.commitOfflineDismissalExact(offline_event.event) ==
         GatewayAlarmCloseResult::Closed);
  assert(!tracker
              .schedule(GatewayAlarmType::CuckooOffline, "001122334455",
                        t0 + 1U)
              .should_play);

  GatewayAlarmDecision deviation = tracker.setExternalCondition(
      GatewayAlarmType::CuckooBeerDeviation, "001122334455", true, t0);
  assert(deviation.should_play);
  GatewayAlarmDecision deviation_event = tracker.schedule(
      GatewayAlarmType::CuckooBeerDeviation, "001122334455", t0);
  assert(deviation_event.should_play);
  assert(deviation_event.event.type ==
         GatewayAlarmType::CuckooBeerDeviation);
  assert(deviation_event.event.event_token != offline_event.event.event_token);

  assert(!tracker
              .setExternalCondition(GatewayAlarmType::CuckooOffline,
                                    "001122334455", true, t0 + 1U)
              .should_play);
  const GatewayAlarmDecision clear_offline = tracker.setExternalCondition(
      GatewayAlarmType::CuckooOffline, "001122334455", false, t0 + 2U);
  assert(clear_offline.should_clear);
  assert(!tracker
              .schedule(GatewayAlarmType::CuckooOffline, "001122334455",
                        t0 + 10000U)
              .should_play);
  assert(tracker
             .schedule(GatewayAlarmType::CuckooBeerDeviation,
                       "001122334455", t0 + 10000U)
             .should_play);

  assert(tracker.closeExact(deviation_event.event.event_token) ==
         GatewayAlarmCloseResult::Closed);
  assert(!tracker
              .schedule(GatewayAlarmType::CuckooBeerDeviation,
                        "001122334455", t0 + 20000U)
              .should_play);
  const GatewayAlarmDecision clear_deviation = tracker.setExternalCondition(
      GatewayAlarmType::CuckooBeerDeviation, "001122334455", false,
      t0 + 20001U);
  assert(clear_deviation.should_clear);
  assert(tracker
             .setExternalCondition(GatewayAlarmType::CuckooBeerDeviation,
                                   "001122334455", true, t0 + 30000U)
             .should_play);
  const GatewayAlarmDecision reentered = tracker.schedule(
      GatewayAlarmType::CuckooBeerDeviation, "001122334455", t0 + 30000U);
  assert(reentered.should_play);
  assert(reentered.event.condition_token !=
         deviation_event.event.condition_token);
  return 0;
}
