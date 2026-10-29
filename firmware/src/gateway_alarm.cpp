#include "gateway_alarm.hpp"

#include <math.h>
#include <string.h>

#if defined(ESP_PLATFORM) || defined(ARDUINO_ARCH_ESP32)
#include <esp_random.h>
#endif

namespace {
constexpr uint8_t kMinCompareMinutes = 1;
constexpr uint8_t kMaxCompareMinutes = 60;
constexpr float kDefaultThresholdC = 1.0f;
constexpr float kMinThresholdC = 0.1f;
constexpr float kMaxThresholdC = 10.0f;
constexpr uint32_t kHistoricalToleranceMs = 90000UL;
constexpr uint32_t kMinOfflineSeconds = 60;
constexpr uint32_t kMaxOfflineSeconds = 86400;
constexpr uint32_t kHostTokenPrefix = 0x47574159UL;
}  // namespace

bool GatewayAlarmTracker::tryCopyAudibleSummary(GatewayAudibleSummary* out) {
  if (out == nullptr) return false;
  StateLockGuard guard(_state_lock);
  if (!guard.owns() || _audible_update_in_progress) return false;
  GatewayAudibleSummary summary{};
  for (const DeviceState& device : _devices) {
    if (!device.in_use) continue;
    const auto audible = [](const AlarmScheduleState& s) {
      return s.active && !s.closed && !s.audible_suppressed;
    };
    const bool temperature = audible(device.temperature_schedule) ||
                             audible(device.cuckoo_beer_deviation_schedule);
    summary.temperature |= temperature;
    summary.any |= temperature || audible(device.offline_schedule) ||
                   audible(device.cuckoo_offline_schedule);
  }
  *out = summary;
  return true;
}

bool GatewayAlarmTracker::beginAudibleUpdate() {
  StateLockGuard guard(_state_lock);
  if (!guard.owns()) return false;
  _audible_update_in_progress = true;
  return true;
}

void GatewayAlarmTracker::endAudibleUpdate() {
  // This is called outside all tracker critical sections.  The host lock is
  // nonblocking and may be contended; do not leave the batch permanently open.
  for (;;) {
    StateLockGuard guard(_state_lock);
    if (guard.owns()) {
      _audible_update_in_progress = false;
      return;
    }
  }
}

bool GatewayAlarmTracker::isValidDeviceId(const char* deviceId) {
  if (deviceId == nullptr || deviceId[0] == '\0') return false;
  size_t length = 0;
  while (length < kDeviceIdCapacity && deviceId[length] != '\0') ++length;
  return length > 0 && length < kDeviceIdCapacity;
}

GatewayAlarmTracker::DeviceState* GatewayAlarmTracker::find(
    const char* deviceId) {
  if (!isValidDeviceId(deviceId)) return nullptr;
  for (DeviceState& state : _devices) {
    if (state.in_use && strcmp(state.device_id, deviceId) == 0) return &state;
  }
  return nullptr;
}

GatewayAlarmTracker::DeviceState* GatewayAlarmTracker::findOrCreate(
    const char* deviceId) {
  if (!isValidDeviceId(deviceId)) return nullptr;
  DeviceState* existing = find(deviceId);
  if (existing != nullptr) return existing;

  for (DeviceState& state : _devices) {
    if (!state.in_use) {
      state = DeviceState{};
      state.in_use = true;
      memcpy(state.device_id, deviceId, strlen(deviceId) + 1);
      return &state;
    }
  }
  return nullptr;
}

void GatewayAlarmTracker::clearTemperature(DeviceState* state) {
  if (state == nullptr) return;
  state->sample_count = 0;
  state->next_sample = 0;
  state->temperature_active = false;
  clearSchedule(&state->temperature_schedule);
}

void GatewayAlarmTracker::storeTemperature(DeviceState* state, uint32_t nowMs,
                                           float temperatureC) {
  const uint32_t minuteBucket = nowMs / 60000UL;
  for (uint8_t i = 0; i < state->sample_count; ++i) {
    if (state->samples[i].timestamp_ms / 60000UL == minuteBucket) {
      state->samples[i].timestamp_ms = nowMs;
      state->samples[i].temperature_c = temperatureC;
      return;
    }
  }

  state->samples[state->next_sample].timestamp_ms = nowMs;
  state->samples[state->next_sample].temperature_c = temperatureC;
  state->next_sample =
      static_cast<uint8_t>((state->next_sample + 1U) % kSamplesPerDevice);
  if (state->sample_count < kSamplesPerDevice) ++state->sample_count;
}

void GatewayAlarmTracker::clearSchedule(AlarmScheduleState* schedule) {
  if (schedule == nullptr) return;
  *schedule = AlarmScheduleState{};
}

bool GatewayAlarmTracker::beginCondition(AlarmScheduleState* schedule,
                                         uint32_t nowMs,
                                         bool hasWindowStart) {
  if (schedule == nullptr) return false;
  clearSchedule(schedule);
  schedule->condition_token = allocateToken();
  if (schedule->condition_token == GATEWAY_INVALID_EVENT_TOKEN) {
    clearSchedule(schedule);
    return false;
  }
  schedule->active = true;
  schedule->window_start_ms = nowMs;
  schedule->next_repeat_ms = nowMs;
  schedule->has_window_start = hasWindowStart;
  return true;
}

GatewayAlarmTracker::AlarmScheduleState* GatewayAlarmTracker::scheduleFor(
    DeviceState* state, GatewayAlarmType type) {
  if (state == nullptr) return nullptr;
  switch (type) {
    case GatewayAlarmType::TemperatureChange:
      return &state->temperature_schedule;
    case GatewayAlarmType::Offline:
      return &state->offline_schedule;
    case GatewayAlarmType::CuckooOffline:
      return &state->cuckoo_offline_schedule;
    case GatewayAlarmType::CuckooBeerDeviation:
      return &state->cuckoo_beer_deviation_schedule;
  }
  return nullptr;
}

bool GatewayAlarmTracker::StateLock::tryEnter() {
#if defined(ESP_PLATFORM) || defined(ARDUINO_ARCH_ESP32)
  portENTER_CRITICAL(&mux_);
  return true;
#else
  return !flag_.test_and_set(std::memory_order_acquire);
#endif
}

void GatewayAlarmTracker::StateLock::exit() {
#if defined(ESP_PLATFORM) || defined(ARDUINO_ARCH_ESP32)
  portEXIT_CRITICAL(&mux_);
#else
  flag_.clear(std::memory_order_release);
#endif
}

uint32_t GatewayAlarmTracker::initialTokenPrefix() {
#if defined(ESP_PLATFORM) || defined(ARDUINO_ARCH_ESP32)
  const uint32_t entropy = esp_random();
  return entropy == 0 ? kHostTokenPrefix : entropy;
#else
  return kHostTokenPrefix;
#endif
}

GatewayAlarmEventToken GatewayAlarmTracker::allocateToken() {
  if (_token_exhausted) return GATEWAY_INVALID_EVENT_TOKEN;
  if (_token_prefix == 0) _token_prefix = initialTokenPrefix();
  if (_token_sequence == 0) {
    if (_token_prefix == UINT32_MAX) {
      _token_exhausted = true;
      return GATEWAY_INVALID_EVENT_TOKEN;
    }
    ++_token_prefix;
    _token_sequence = 1;
  }

  const GatewayAlarmEventToken token =
      (static_cast<GatewayAlarmEventToken>(_token_prefix) << 32) |
      static_cast<GatewayAlarmEventToken>(_token_sequence);
  if (_token_sequence == UINT32_MAX) {
    _token_sequence = 0;
  } else {
    ++_token_sequence;
  }
  if (token == GATEWAY_INVALID_EVENT_TOKEN) {
    _token_exhausted = true;
    return GATEWAY_INVALID_EVENT_TOKEN;
  }
  return token;
}

GatewayAlarmDecision GatewayAlarmTracker::recordTemperature(
    const char* deviceId, float temperatureC, bool enabled,
    uint8_t compareMinutes, float thresholdC, uint32_t nowMs) {
  GatewayAlarmDecision decision;
  if (!isValidDeviceId(deviceId)) return decision;
  StateLockGuard lock(_state_lock);
  if (!lock.owns()) return decision;

  if (!enabled) {
    DeviceState* state = find(deviceId);
    if (state != nullptr) {
      decision.should_clear = state->temperature_active;
      clearTemperature(state);
    }
    return decision;
  }
  if (!isfinite(temperatureC)) return decision;

  if (compareMinutes < kMinCompareMinutes) compareMinutes = kMinCompareMinutes;
  if (compareMinutes > kMaxCompareMinutes) compareMinutes = kMaxCompareMinutes;
  if (!isfinite(thresholdC)) thresholdC = kDefaultThresholdC;
  if (thresholdC < kMinThresholdC) thresholdC = kMinThresholdC;
  if (thresholdC > kMaxThresholdC) thresholdC = kMaxThresholdC;

  DeviceState* state = findOrCreate(deviceId);
  if (state == nullptr) return decision;

  const uint32_t targetAgeMs =
      static_cast<uint32_t>(compareMinutes) * 60000UL;
  const TemperatureSample* historical = nullptr;
  uint32_t bestAgeMs = UINT32_MAX;
  for (uint8_t i = 0; i < state->sample_count; ++i) {
    const uint32_t ageMs = nowMs - state->samples[i].timestamp_ms;
    if (ageMs >= targetAgeMs &&
        ageMs <= targetAgeMs + kHistoricalToleranceMs && ageMs < bestAgeMs) {
      historical = &state->samples[i];
      bestAgeMs = ageMs;
    }
  }

  if (historical != nullptr) {
    const float delta = fabsf(temperatureC - historical->temperature_c);
    if (state->temperature_active) {
      if (delta <= thresholdC * 0.8f) {
        state->temperature_active = false;
        clearSchedule(&state->temperature_schedule);
        decision.should_clear = true;
      }
    } else if (delta >= thresholdC) {
      if (beginCondition(&state->temperature_schedule, nowMs, true)) {
        state->temperature_active = true;
        decision.should_play = true;
      }
    }
  }

  storeTemperature(state, nowMs, temperatureC);
  return decision;
}

GatewayAlarmDecision GatewayAlarmTracker::evaluateOffline(
    const char* deviceId, uint32_t updateAgeSeconds, bool enabled,
    uint32_t timeoutSeconds) {
  GatewayAlarmDecision decision;
  decision.type = GatewayAlarmType::Offline;
  if (!isValidDeviceId(deviceId)) return decision;
  StateLockGuard lock(_state_lock);
  if (!lock.owns()) return decision;

  DeviceState* state = find(deviceId);
  if (!enabled) {
    if (state != nullptr) {
      decision.should_clear = state->offline_active;
      state->offline_active = false;
      clearSchedule(&state->offline_schedule);
    }
    return decision;
  }
  if (state == nullptr || !state->observed) return decision;

  if (timeoutSeconds < kMinOfflineSeconds) timeoutSeconds = kMinOfflineSeconds;
  if (timeoutSeconds > kMaxOfflineSeconds) timeoutSeconds = kMaxOfflineSeconds;
  if (updateAgeSeconds < timeoutSeconds) {
    decision.should_clear = state->offline_active;
    state->offline_active = false;
    clearSchedule(&state->offline_schedule);
    return decision;
  }
  if (!state->offline_active) {
    if (beginCondition(&state->offline_schedule, 0, false)) {
      state->offline_active = true;
      decision.should_play = true;
    }
  }
  return decision;
}

GatewayAlarmDecision GatewayAlarmTracker::setExternalCondition(
    GatewayAlarmType type, const char* deviceId, bool active,
    uint32_t nowMs, bool audibleSuppressed) {
  GatewayAlarmDecision decision;
  decision.type = type;
  if (type != GatewayAlarmType::CuckooOffline &&
      type != GatewayAlarmType::CuckooBeerDeviation) {
    return decision;
  }
  if (!isValidDeviceId(deviceId)) return decision;
  StateLockGuard lock(_state_lock);
  if (!lock.owns()) return decision;

  DeviceState* state = active ? findOrCreate(deviceId) : find(deviceId);
  AlarmScheduleState* scheduleState = scheduleFor(state, type);
  if (!scheduleState) return decision;
  if (!active) {
    decision.should_clear = scheduleState->active;
    clearSchedule(scheduleState);
    return decision;
  }
  if (!scheduleState->active && beginCondition(scheduleState, nowMs, true)) {
    decision.should_play = true;
  }
  scheduleState->audible_suppressed = audibleSuppressed;
  return decision;
}

bool GatewayAlarmTracker::isConditionActive(GatewayAlarmType type,
                                            const char* deviceId) {
  if (!isValidDeviceId(deviceId)) return false;
  StateLockGuard lock(_state_lock);
  if (!lock.owns()) return false;

  DeviceState* state = find(deviceId);
  if (state == nullptr) return false;
  if (type == GatewayAlarmType::TemperatureChange) {
    return state->temperature_active;
  }
  if (type == GatewayAlarmType::Offline) return state->offline_active;

  AlarmScheduleState* scheduleState = scheduleFor(state, type);
  return scheduleState != nullptr && scheduleState->active;
}

GatewayAlarmDecision GatewayAlarmTracker::schedule(GatewayAlarmType type,
                                                   const char* deviceId,
                                                   uint32_t nowMs) {
  GatewayAlarmDecision decision;
  decision.type = type;
  decision.event.type = type;
  if (!isValidDeviceId(deviceId)) return decision;
  StateLockGuard lock(_state_lock);
  if (!lock.owns()) return decision;

  DeviceState* state = find(deviceId);
  AlarmScheduleState* scheduleState = scheduleFor(state, type);
  if (scheduleState == nullptr || !scheduleState->active) return decision;

  if (!scheduleState->has_window_start) {
    scheduleState->window_start_ms = nowMs;
    scheduleState->next_repeat_ms = nowMs;
    scheduleState->has_window_start = true;
  }

  const uint32_t elapsed = nowMs - scheduleState->window_start_ms;
  if (elapsed >= kAlarmCyclePeriodMs) return decision;

  if (scheduleState->event_token == GATEWAY_INVALID_EVENT_TOKEN) {
    scheduleState->event_token = allocateToken();
    if (scheduleState->event_token == GATEWAY_INVALID_EVENT_TOKEN) {
      return decision;
    }
  }
  if (scheduleState->closed) return decision;

  const uint32_t windowElapsed = nowMs - scheduleState->window_start_ms;
  if (windowElapsed >= kAlarmAudibleWindowMs) return decision;

  const uint8_t repeatIndex = static_cast<uint8_t>(
      windowElapsed / kAlarmRepeatIntervalMs);
  if (repeatIndex >= kAlarmCyclePeriodMs / kAlarmRepeatIntervalMs ||
      (scheduleState->last_repeat_index != kNoRepeatIndex &&
       repeatIndex <= scheduleState->last_repeat_index)) {
    return decision;
  }

  decision.should_play = true;
  decision.event.type = type;
  memcpy(decision.event.device_id, deviceId, strlen(deviceId) + 1);
  decision.event.condition_token = scheduleState->condition_token;
  decision.event.event_token = scheduleState->event_token;
  decision.event.window_index = scheduleState->window_index;
  decision.event.repeat_index = repeatIndex;
  scheduleState->last_repeat_index = repeatIndex;
  scheduleState->next_repeat_ms = static_cast<uint32_t>(
      scheduleState->window_start_ms +
      (static_cast<uint32_t>(repeatIndex) + 1U) * kAlarmRepeatIntervalMs);
  return decision;
}

GatewayAlarmCloseResult GatewayAlarmTracker::closeExact(
    GatewayAlarmEventToken eventToken) {
  if (eventToken == GATEWAY_INVALID_EVENT_TOKEN) {
    return GatewayAlarmCloseResult::NotFound;
  }
  // A host contention result is explicit and retryable; no close is dropped.
  StateLockGuard lock(_state_lock, kCloseLockAttempts);
  if (!lock.owns()) return GatewayAlarmCloseResult::Retry;
  for (DeviceState& state : _devices) {
    if (!state.in_use) continue;
    AlarmScheduleState* schedules[] = {
        &state.temperature_schedule, &state.offline_schedule,
        &state.cuckoo_offline_schedule,
        &state.cuckoo_beer_deviation_schedule};
    for (AlarmScheduleState* scheduleState : schedules) {
      if (scheduleState->event_token == eventToken) {
        scheduleState->closed = true;
        return GatewayAlarmCloseResult::Closed;
      }
    }
  }
  return GatewayAlarmCloseResult::NotFound;
}

GatewayAlarmCloseResult GatewayAlarmTracker::validateOfflineDismissalExact(
    const GatewayAlarmEvent& event) {
  if ((event.type != GatewayAlarmType::Offline &&
       event.type != GatewayAlarmType::CuckooOffline) ||
      !isValidDeviceId(event.device_id) ||
      event.condition_token == GATEWAY_INVALID_EVENT_TOKEN ||
      event.event_token == GATEWAY_INVALID_EVENT_TOKEN) {
    return GatewayAlarmCloseResult::NotFound;
  }
  StateLockGuard lock(_state_lock, kCloseLockAttempts);
  if (!lock.owns()) return GatewayAlarmCloseResult::Retry;
  DeviceState* state = find(event.device_id);
  if (state == nullptr ||
      (event.type == GatewayAlarmType::Offline && !state->offline_active)) {
    return GatewayAlarmCloseResult::NotFound;
  }
  const AlarmScheduleState& schedule =
      event.type == GatewayAlarmType::Offline
          ? state->offline_schedule
          : state->cuckoo_offline_schedule;
  return schedule.active &&
                 schedule.condition_token == event.condition_token &&
                 schedule.event_token == event.event_token
             ? GatewayAlarmCloseResult::Closed
             : GatewayAlarmCloseResult::NotFound;
}

GatewayAlarmCloseResult GatewayAlarmTracker::commitOfflineDismissalExact(
    const GatewayAlarmEvent& event) {
  if ((event.type != GatewayAlarmType::Offline &&
       event.type != GatewayAlarmType::CuckooOffline) ||
      !isValidDeviceId(event.device_id) ||
      event.condition_token == GATEWAY_INVALID_EVENT_TOKEN ||
      event.event_token == GATEWAY_INVALID_EVENT_TOKEN) {
    return GatewayAlarmCloseResult::NotFound;
  }
  StateLockGuard lock(_state_lock, kCloseLockAttempts);
  if (!lock.owns()) return GatewayAlarmCloseResult::Retry;
  DeviceState* state = find(event.device_id);
  if (state == nullptr ||
      (event.type == GatewayAlarmType::Offline && !state->offline_active)) {
    return GatewayAlarmCloseResult::NotFound;
  }
  AlarmScheduleState& schedule =
      event.type == GatewayAlarmType::Offline
          ? state->offline_schedule
          : state->cuckoo_offline_schedule;
  if (!schedule.active || schedule.condition_token != event.condition_token ||
      schedule.event_token != event.event_token) {
    return GatewayAlarmCloseResult::NotFound;
  }
  schedule.closed = true;
  return GatewayAlarmCloseResult::Closed;
}

bool GatewayAlarmTracker::markOnline(const char* deviceId) {
  StateLockGuard lock(_state_lock);
  if (!lock.owns()) return false;
  DeviceState* state = findOrCreate(deviceId);
  if (state == nullptr) return false;
  const bool wasOffline = state->offline_active;
  state->observed = true;
  state->offline_active = false;
  clearSchedule(&state->offline_schedule);
  return wasOffline;
}

void GatewayAlarmTracker::setOfflineDismissed(const char* deviceId,
                                              bool dismissed) {
  StateLockGuard lock(_state_lock);
  if (!lock.owns() || !isValidDeviceId(deviceId)) return;
  DeviceState* state = findOrCreate(deviceId);
  if (state == nullptr) return;
  state->observed = true;
  if (dismissed) {
    // Mark the offline schedule closed so it cannot re-arm while the device
    // stays offline. condition/event tokens are reset; the next offline
    // episode after real recovery will start a fresh condition.
    state->offline_active = true;
    AlarmScheduleState* schedule = &state->offline_schedule;
    if (!schedule->active) {
      schedule->active = true;
      schedule->condition_token = allocateToken();
      if (schedule->condition_token == GATEWAY_INVALID_EVENT_TOKEN) {
        clearSchedule(schedule);
        state->offline_active = false;
        return;
      }
    }
    schedule->closed = true;
    schedule->event_token = GATEWAY_INVALID_EVENT_TOKEN;
    schedule->window_start_ms = 0;
    schedule->has_window_start = false;
  } else {
    state->offline_active = false;
    clearSchedule(&state->offline_schedule);
  }
}

void GatewayAlarmTracker::restoreOfflineDismissed(const char* deviceId) {
  StateLockGuard lock(_state_lock);
  if (!lock.owns() || !isValidDeviceId(deviceId)) return;
  DeviceState* state = findOrCreate(deviceId);
  if (state == nullptr) return;
  state->observed = true;
  state->offline_active = true;
  AlarmScheduleState* schedule = &state->offline_schedule;
  if (!schedule->active) {
    schedule->active = true;
    schedule->condition_token = allocateToken();
    if (schedule->condition_token == GATEWAY_INVALID_EVENT_TOKEN) {
      clearSchedule(schedule);
      state->offline_active = false;
      return;
    }
  }
  schedule->closed = true;
  schedule->event_token = GATEWAY_INVALID_EVENT_TOKEN;
  schedule->window_start_ms = 0;
  schedule->has_window_start = false;
}

bool GatewayAlarmTracker::isOfflineDismissed(const char* deviceId) {
  StateLockGuard lock(_state_lock);
  if (!lock.owns() || !isValidDeviceId(deviceId)) return false;
  DeviceState* state = find(deviceId);
  if (state == nullptr) return false;
  return state->offline_active && state->offline_schedule.closed;
}

void GatewayAlarmTracker::retainDevices(const char* const* deviceIds,
                                        size_t count) {
  StateLockGuard lock(_state_lock);
  if (!lock.owns()) return;
  for (DeviceState& state : _devices) {
    if (!state.in_use) continue;
    bool retained = false;
    for (size_t i = 0; i < count; ++i) {
      if (isValidDeviceId(deviceIds == nullptr ? nullptr : deviceIds[i]) &&
          strcmp(state.device_id, deviceIds[i]) == 0) {
        retained = true;
        break;
      }
    }
    if (!retained) state = DeviceState{};
  }
}
