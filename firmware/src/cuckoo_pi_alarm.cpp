#include "cuckoo_pi_alarm.hpp"

#include <cmath>

namespace {
void clearDeviation(CuckooPiAlarmState* state,
                    CuckooPiAlarmEvaluation* evaluation) {
  if (!state || !evaluation) return;
  evaluation->beer_deviation_cleared = state->beer_deviation_active;
  state->beer_deviation_pending = false;
  state->beer_deviation_active = false;
  state->beer_deviation_started_ms = 0;
}
}

bool shouldClearCuckooPiOfflineDismissal(
    const CuckooPiDeviceConfig& config, const CuckooPiSnapshot& snapshot,
    bool has_snapshot, const CuckooPiAlarmEvaluation& evaluation) {
  // A retained/replayed status with the same device statusTime is not proof
  // that the device recovered. Only a changed statusTime or an actual
  // offline-to-online transition may release the user's dismissal.
  if (!config.offline_alarm_dismissed || !has_snapshot) return false;
  if (snapshot.has_status_time) {
    if (config.offline_dismissed_status_time_seconds == 0U) return true;
    return snapshot.status_time_seconds !=
           config.offline_dismissed_status_time_seconds;
  }
  return evaluation.offline_cleared;
}

CuckooPiAlarmEvaluation evaluateCuckooPiAlarm(
    const CuckooPiDeviceConfig& config, const CuckooPiSnapshot* snapshot,
    bool has_snapshot, uint32_t last_seen_ms, uint32_t now_ms,
    uint32_t configured_since_ms, CuckooPiAlarmState* state) {
  CuckooPiAlarmEvaluation evaluation{};
  if (!state) return evaluation;

  const uint32_t offline_elapsed_ms =
      has_snapshot ? now_ms - last_seen_ms : now_ms - configured_since_ms;
  const uint32_t offline_timeout_ms =
      config.offline_timeout_seconds * 1000UL;
  const bool offline = config.offline_alarm_enabled &&
                       offline_elapsed_ms >= offline_timeout_ms;
  if (offline) {
    evaluation.offline_active = true;
    evaluation.offline_suppressed = config.offline_alarm_dismissed;
    if (!state->offline_active && !evaluation.offline_suppressed) {
      evaluation.offline_started = true;
    }
    state->offline_active = true;
    clearDeviation(state, &evaluation);
    return evaluation;
  }

  if (state->offline_active) evaluation.offline_cleared = true;
  state->offline_active = false;

  const bool beer_mode = snapshot && snapshot->has_mode &&
                         snapshot->mode_raw == 2;
  const bool values_valid = snapshot && snapshot->has_beer_temp &&
                            snapshot->has_beer_set &&
                            std::isfinite(snapshot->beer_temp_c) &&
                            std::isfinite(snapshot->beer_set_c);
  if (!config.beer_deviation_alarm_enabled || !has_snapshot || !beer_mode ||
      !values_valid) {
    clearDeviation(state, &evaluation);
    return evaluation;
  }

  evaluation.deviation_c =
      std::fabs(snapshot->beer_temp_c - snapshot->beer_set_c);
  if (evaluation.deviation_c < config.beer_deviation_threshold_c) {
    clearDeviation(state, &evaluation);
    return evaluation;
  }

  if (!state->beer_deviation_pending) {
    state->beer_deviation_pending = true;
    state->beer_deviation_started_ms = now_ms;
  }
  const uint32_t hold_ms = config.beer_deviation_hold_seconds * 1000UL;
  if (!state->beer_deviation_active &&
      now_ms - state->beer_deviation_started_ms >= hold_ms) {
    state->beer_deviation_active = true;
    evaluation.beer_deviation_started = true;
  }
  evaluation.beer_deviation_active = state->beer_deviation_active;
  return evaluation;
}
