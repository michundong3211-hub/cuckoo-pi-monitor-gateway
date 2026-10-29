#include "gateway_alert_state.hpp"

#include <string.h>

bool GatewayAlertState::StateLock::tryEnter() {
#if defined(ESP_PLATFORM) || defined(ARDUINO_ARCH_ESP32)
  portENTER_CRITICAL(&mux_);
  return true;
#else
  return !flag_.test_and_set(std::memory_order_acquire);
#endif
}

void GatewayAlertState::StateLock::exit() {
#if defined(ESP_PLATFORM) || defined(ARDUINO_ARCH_ESP32)
  portEXIT_CRITICAL(&mux_);
#else
  flag_.clear(std::memory_order_release);
#endif
}

GatewayAlertState::GatewayAlertState() = default;

bool GatewayAlertState::isValidDeviceId(const char* deviceId) {
  if (deviceId == nullptr || deviceId[0] == '\0') return false;
  size_t length = 0;
  while (length < GATEWAY_ALERT_DEVICE_ID_CAPACITY &&
         deviceId[length] != '\0') {
    ++length;
  }
  return length > 0 && length < GATEWAY_ALERT_DEVICE_ID_CAPACITY;
}

bool GatewayAlertState::sameKey(const GatewayAlertEvent& left,
                                const GatewayAlertEvent& right) {
  return left.type == right.type &&
         strcmp(left.device_id, right.device_id) == 0;
}

bool GatewayAlertState::sameIdentity(const GatewayAlertEvent& left,
                                     const GatewayAlertEvent& right) {
  if (left.event_token == right.event_token) return true;
  return sameKey(left, right) &&
         left.condition_token == right.condition_token &&
         left.window_index == right.window_index &&
         left.repeat_index == right.repeat_index;
}

bool GatewayAlertState::matches(const GatewayAlertEvent& event,
                                GatewayAlarmType type, const char* deviceId) {
  return deviceId != nullptr && event.type == type &&
         strcmp(event.device_id, deviceId) == 0;
}

bool GatewayAlertState::copyEvent(GatewayAlertEvent* destination,
                                  const GatewayAlertEvent& source) {
  if (destination == nullptr) return false;
  *destination = source;
  return true;
}

bool GatewayAlertState::publish(const GatewayAlertEvent& event) {
  if (event.condition_token == GATEWAY_INVALID_EVENT_TOKEN ||
      event.event_token == GATEWAY_INVALID_EVENT_TOKEN ||
      !isValidDeviceId(event.device_id)) {
    return false;
  }

  StateLockGuard lock(state_lock_);
  if (!lock.owns()) return false;
  // Publishing an event that is already the current or queued alert is an
  // idempotent success: the alert is already visible, and the lifecycle must
  // be able to advance past PublishAlert for same-window repeats (which share
  // one event_token) so their audio can be queued.
  if (has_current_ && sameIdentity(current_, event)) return true;
  for (size_t i = 0; i < queue_count_; ++i) {
    if (sameIdentity(queue_[i], event)) return true;
  }

  if (!has_current_) {
    current_ = event;
    has_current_ = true;
    return true;
  }
  if (queue_count_ >= GATEWAY_ALERT_QUEUE_CAPACITY - 1) return false;
  queue_[queue_count_++] = event;
  return true;
}

GatewayAlertClearResult GatewayAlertState::clearByKeyResult(
    GatewayAlarmType type, const char* deviceId) {
  if (!isValidDeviceId(deviceId)) return GatewayAlertClearResult::NotFound;

  StateLockGuard lock(state_lock_);
  if (!lock.owns()) return GatewayAlertClearResult::Retry;
  GatewayAlertEvent retained[GATEWAY_ALERT_QUEUE_CAPACITY] = {};
  size_t retainedCount = 0;
  const bool currentMatches =
      has_current_ && matches(current_, type, deviceId);
  bool cleared = currentMatches;
  for (size_t i = 0; i < queue_count_; ++i) {
    if (matches(queue_[i], type, deviceId)) {
      cleared = true;
    } else if (retainedCount < GATEWAY_ALERT_QUEUE_CAPACITY) {
      retained[retainedCount++] = queue_[i];
    }
  }

  if (!currentMatches) {
    for (size_t i = 0; i < retainedCount; ++i) queue_[i] = retained[i];
    queue_count_ = retainedCount;
    return cleared ? GatewayAlertClearResult::Cleared
                   : GatewayAlertClearResult::NotFound;
  }

  if (retainedCount == 0) {
    current_ = GatewayAlertEvent{};
    queue_count_ = 0;
    has_current_ = false;
    return cleared ? GatewayAlertClearResult::Cleared
                   : GatewayAlertClearResult::NotFound;
  }

  current_ = retained[0];
  for (size_t i = 1; i < retainedCount; ++i) queue_[i - 1] = retained[i];
  queue_count_ = retainedCount - 1;
  has_current_ = true;
  return cleared ? GatewayAlertClearResult::Cleared
                 : GatewayAlertClearResult::NotFound;
}

bool GatewayAlertState::clearByKey(GatewayAlarmType type,
                                   const char* deviceId) {
  return clearByKeyResult(type, deviceId) ==
         GatewayAlertClearResult::Cleared;
}

GatewayAlertClearResult GatewayAlertState::clearByTokenResult(
    GatewayAlarmEventToken eventToken) {
  if (eventToken == GATEWAY_INVALID_EVENT_TOKEN) {
    return GatewayAlertClearResult::NotFound;
  }

  StateLockGuard lock(state_lock_);
  if (!lock.owns()) return GatewayAlertClearResult::Retry;
  GatewayAlertEvent retained[GATEWAY_ALERT_QUEUE_CAPACITY] = {};
  size_t retainedCount = 0;
  const bool currentMatches = has_current_ && current_.event_token == eventToken;
  bool cleared = currentMatches;
  for (size_t i = 0; i < queue_count_; ++i) {
    if (queue_[i].event_token == eventToken) {
      cleared = true;
    } else if (retainedCount < GATEWAY_ALERT_QUEUE_CAPACITY) {
      retained[retainedCount++] = queue_[i];
    }
  }

  if (!currentMatches) {
    for (size_t i = 0; i < retainedCount; ++i) queue_[i] = retained[i];
    queue_count_ = retainedCount;
    return cleared ? GatewayAlertClearResult::Cleared
                   : GatewayAlertClearResult::NotFound;
  }

  if (retainedCount == 0) {
    current_ = GatewayAlertEvent{};
    queue_count_ = 0;
    has_current_ = false;
    return GatewayAlertClearResult::Cleared;
  }

  current_ = retained[0];
  for (size_t i = 1; i < retainedCount; ++i) queue_[i - 1] = retained[i];
  queue_count_ = retainedCount - 1;
  has_current_ = true;
  return GatewayAlertClearResult::Cleared;
}

bool GatewayAlertState::peek(GatewayAlertEvent* event) const {
  StateLockGuard lock(state_lock_);
  if (!lock.owns()) return false;
  return has_current_ && copyEvent(event, current_);
}

bool GatewayAlertState::dismissCurrentExact(GatewayAlarmEventToken eventToken,
                                             GatewayAlertEvent* dismissed) {
  if (eventToken == GATEWAY_INVALID_EVENT_TOKEN || dismissed == nullptr) {
    return false;
  }

  StateLockGuard lock(state_lock_);
  if (!lock.owns()) return false;
  if (!has_current_ || current_.event_token != eventToken) return false;
  *dismissed = current_;

  if (queue_count_ > 0) {
    current_ = queue_[0];
    for (size_t i = 1; i < queue_count_; ++i) queue_[i - 1] = queue_[i];
    --queue_count_;
  } else {
    current_ = GatewayAlertEvent{};
    has_current_ = false;
  }
  return true;
}

size_t GatewayAlertState::pendingCount() const {
  StateLockGuard lock(state_lock_);
  if (!lock.owns()) return 0;
  return (has_current_ ? 1U : 0U) + queue_count_;
}
