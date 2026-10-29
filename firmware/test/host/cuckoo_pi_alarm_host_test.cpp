#include "cuckoo_pi_alarm.hpp"

#include <cassert>
#include <cmath>

namespace {
CuckooPiSnapshot completeSnapshot(int mode, float beer_temp,
                                  float beer_set) {
  CuckooPiSnapshot snapshot{};
  snapshot.has_mode = true;
  snapshot.mode_raw = mode;
  snapshot.has_beer_temp = true;
  snapshot.beer_temp_c = beer_temp;
  snapshot.has_beer_set = true;
  snapshot.beer_set_c = beer_set;
  return snapshot;
}
}

int main() {
  CuckooPiDeviceConfig config =
      defaultCuckooPiDeviceConfig("001122334455");
  CuckooPiAlarmState state{};

  CuckooPiAlarmEvaluation evaluation = evaluateCuckooPiAlarm(
      config, nullptr, false, 0U, 299999U, 0U, &state);
  assert(!evaluation.offline_active && !evaluation.offline_started);
  evaluation = evaluateCuckooPiAlarm(config, nullptr, false, 0U, 300000U, 0U,
                                     &state);
  assert(evaluation.offline_active && evaluation.offline_started);
  assert(!evaluation.offline_suppressed);

  CuckooPiSnapshot snapshot = completeSnapshot(3, 17.0f, 15.0f);
  evaluation = evaluateCuckooPiAlarm(config, &snapshot, true, 300001U,
                                     300001U, 0U, &state);
  assert(evaluation.offline_cleared && !evaluation.offline_active);
  assert(!evaluation.beer_deviation_active);
  evaluation = evaluateCuckooPiAlarm(config, &snapshot, true, 300001U,
                                     900001U, 0U, &state);
  assert(evaluation.offline_active && evaluation.offline_started);

  CuckooPiAlarmState dismissed_state{};
  config.offline_alarm_dismissed = true;
  evaluation = evaluateCuckooPiAlarm(config, nullptr, false, 0U, 300000U, 0U,
                                     &dismissed_state);
  assert(evaluation.offline_active);
  assert(evaluation.offline_suppressed);
  assert(!evaluation.offline_started);
  config.offline_alarm_dismissed = false;

  CuckooPiDeviceConfig dismissal_policy =
      defaultCuckooPiDeviceConfig("001122334455");
  dismissal_policy.offline_alarm_dismissed = true;
  dismissal_policy.offline_dismissed_status_time_seconds = 120U;
  CuckooPiSnapshot retained_snapshot = completeSnapshot(3, 17.0f, 15.0f);
  retained_snapshot.has_status_time = true;
  retained_snapshot.status_time_seconds = 120U;
  CuckooPiAlarmEvaluation retained_evaluation{};
  assert(!shouldClearCuckooPiOfflineDismissal(
      dismissal_policy, retained_snapshot, true, retained_evaluation));
  retained_evaluation.offline_cleared = true;
  assert(!shouldClearCuckooPiOfflineDismissal(
      dismissal_policy, retained_snapshot, true, retained_evaluation));
  retained_evaluation.offline_cleared = false;
  retained_snapshot.status_time_seconds = 121U;
  assert(shouldClearCuckooPiOfflineDismissal(
      dismissal_policy, retained_snapshot, true, retained_evaluation));
  retained_snapshot.has_status_time = false;
  retained_evaluation.offline_cleared = true;
  assert(shouldClearCuckooPiOfflineDismissal(
      dismissal_policy, retained_snapshot, true, retained_evaluation));

  CuckooPiAlarmState deviation_state{};
  snapshot = completeSnapshot(2, 17.0f, 15.0f);
  evaluation = evaluateCuckooPiAlarm(config, &snapshot, true, 1000U, 1000U,
                                     0U, &deviation_state);
  assert(!evaluation.beer_deviation_active);
  assert(std::fabs(evaluation.deviation_c - 2.0f) < 0.0001f);
  evaluation = evaluateCuckooPiAlarm(config, &snapshot, true, 600999U,
                                     600999U, 0U, &deviation_state);
  assert(!evaluation.beer_deviation_active);
  evaluation = evaluateCuckooPiAlarm(config, &snapshot, true, 601000U,
                                     601000U, 0U, &deviation_state);
  assert(evaluation.beer_deviation_active);
  assert(evaluation.beer_deviation_started);

  snapshot.beer_temp_c = 16.99f;
  evaluation = evaluateCuckooPiAlarm(config, &snapshot, true, 601001U,
                                     601001U, 0U, &deviation_state);
  assert(!evaluation.beer_deviation_active);
  assert(evaluation.beer_deviation_cleared);

  snapshot = completeSnapshot(3, 20.0f, 15.0f);
  evaluation = evaluateCuckooPiAlarm(config, &snapshot, true, 700000U,
                                     1400000U, 0U, &deviation_state);
  assert(!evaluation.beer_deviation_active);

  snapshot = completeSnapshot(2, 17.1f, 15.0f);
  evaluation = evaluateCuckooPiAlarm(config, &snapshot, true, 1500000U,
                                     1500000U, 0U, &deviation_state);
  assert(!evaluation.beer_deviation_active);
  evaluation = evaluateCuckooPiAlarm(config, &snapshot, true, 2100000U,
                                     2100000U, 0U, &deviation_state);
  assert(evaluation.beer_deviation_active);
  assert(evaluation.beer_deviation_started);

  snapshot = completeSnapshot(1, 20.0f, 15.0f);
  evaluation = evaluateCuckooPiAlarm(config, &snapshot, true, 2200000U,
                                     2800000U, 0U, &deviation_state);
  assert(!evaluation.beer_deviation_active);
  assert(evaluation.beer_deviation_cleared);
  snapshot = completeSnapshot(0, 20.0f, 15.0f);
  evaluation = evaluateCuckooPiAlarm(config, &snapshot, true, 2800001U,
                                     3400001U, 0U, &deviation_state);
  assert(!evaluation.beer_deviation_active);
  snapshot = completeSnapshot(3, 20.0f, 15.0f);
  evaluation = evaluateCuckooPiAlarm(config, &snapshot, true, 3400002U,
                                     4000002U, 0U, &deviation_state);
  assert(!evaluation.beer_deviation_active);

  // Leaving Beer Constant clears the hold timer; re-entry starts a fresh hold.
  snapshot = completeSnapshot(2, 17.0f, 15.0f);
  evaluation = evaluateCuckooPiAlarm(config, &snapshot, true, 5000000U,
                                     5000000U, 0U, &deviation_state);
  assert(!evaluation.beer_deviation_active);
  snapshot = completeSnapshot(1, 17.0f, 15.0f);
  evaluation = evaluateCuckooPiAlarm(config, &snapshot, true, 5300000U,
                                     5300000U, 0U, &deviation_state);
  assert(!evaluation.beer_deviation_active);
  assert(!deviation_state.beer_deviation_pending);
  assert(deviation_state.beer_deviation_started_ms == 0U);
  snapshot = completeSnapshot(2, 17.0f, 15.0f);
  evaluation = evaluateCuckooPiAlarm(config, &snapshot, true, 5300001U,
                                     5300001U, 0U, &deviation_state);
  assert(!evaluation.beer_deviation_active);
  evaluation = evaluateCuckooPiAlarm(config, &snapshot, true, 5900000U,
                                     5900000U, 0U, &deviation_state);
  assert(!evaluation.beer_deviation_active);
  evaluation = evaluateCuckooPiAlarm(config, &snapshot, true, 5900001U,
                                     5900001U, 0U, &deviation_state);
  assert(evaluation.beer_deviation_active);

  snapshot.has_beer_set = false;
  evaluation = evaluateCuckooPiAlarm(config, &snapshot, true, 5900002U,
                                     5900002U, 0U, &deviation_state);
  assert(evaluation.beer_deviation_cleared);
  assert(!evaluation.beer_deviation_active);

  CuckooPiAlarmState wrapped{};
  snapshot = completeSnapshot(2, 17.0f, 15.0f);
  const uint32_t wrap_start = 0xfffff000U;
  evaluateCuckooPiAlarm(config, &snapshot, true, wrap_start, wrap_start, 0U,
                        &wrapped);
  evaluation = evaluateCuckooPiAlarm(
      config, &snapshot, true,
      static_cast<uint32_t>(wrap_start + 600000U),
      static_cast<uint32_t>(wrap_start + 600000U), 0U, &wrapped);
  assert(evaluation.beer_deviation_active);
  return 0;
}
