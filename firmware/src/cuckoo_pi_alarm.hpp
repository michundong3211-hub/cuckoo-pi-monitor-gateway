#ifndef SRC_CUCKOO_PI_ALARM_HPP_
#define SRC_CUCKOO_PI_ALARM_HPP_

#include "cuckoo_pi_config.hpp"
#include "cuckoo_pi_model.hpp"

#include <cstdint>

struct CuckooPiAlarmState {
  bool offline_active;
  bool beer_deviation_pending;
  bool beer_deviation_active;
  uint32_t beer_deviation_started_ms;
};

struct CuckooPiAlarmEvaluation {
  bool offline_active;
  bool offline_started;
  bool offline_cleared;
  bool offline_suppressed;
  bool beer_deviation_active;
  bool beer_deviation_started;
  bool beer_deviation_cleared;
  float deviation_c;
};

bool shouldClearCuckooPiOfflineDismissal(
    const CuckooPiDeviceConfig& config, const CuckooPiSnapshot& snapshot,
    bool has_snapshot, const CuckooPiAlarmEvaluation& evaluation);

CuckooPiAlarmEvaluation evaluateCuckooPiAlarm(
    const CuckooPiDeviceConfig& config, const CuckooPiSnapshot* snapshot,
    bool has_snapshot, uint32_t last_seen_ms, uint32_t now_ms,
    uint32_t configured_since_ms, CuckooPiAlarmState* state);

#endif  // SRC_CUCKOO_PI_ALARM_HPP_
