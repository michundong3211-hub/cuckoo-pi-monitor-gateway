#include "gateway_alarm_lifecycle.hpp"
#include "gateway_device_settings.hpp"

#include <string.h>

namespace {
constexpr size_t kNoLifecycleSlot = GATEWAY_ALARM_LIFECYCLE_CAPACITY;
constexpr uint8_t kHostLockAttempts = 64;
}  // namespace

GatewayAlarmLifecycleActionResult persistGatewayOfflineDismissal(
    const GatewayAlarmEvent& event,
    const GatewayOfflineDismissalPersistenceOps& operations) {
  if (event.type != GatewayAlarmType::Offline ||
      operations.validateRuntime == nullptr || operations.load == nullptr ||
      operations.save == nullptr || operations.commitRuntime == nullptr) {
    return GatewayAlarmLifecycleActionResult::Rejected;
  }

  const GatewayAlarmCloseResult validation =
      operations.validateRuntime(event, operations.context);
  if (validation == GatewayAlarmCloseResult::Retry) {
    return GatewayAlarmLifecycleActionResult::Retry;
  }
  if (validation != GatewayAlarmCloseResult::Closed) {
    return GatewayAlarmLifecycleActionResult::NotFound;
  }

  GravityDeviceSettings settings{};
  if (!operations.load(event.device_id, &settings, operations.context)) {
    return GatewayAlarmLifecycleActionResult::Retry;
  }
  if (!settings.offline_alarm_dismissed) {
    settings.offline_alarm_dismissed = true;
    if (!operations.save(settings, operations.context)) {
      return GatewayAlarmLifecycleActionResult::Retry;
    }
  }
  const GatewayAlarmCloseResult committed =
      operations.commitRuntime(event, operations.context);
  if (committed == GatewayAlarmCloseResult::Retry) {
    return GatewayAlarmLifecycleActionResult::Retry;
  }
  return committed == GatewayAlarmCloseResult::Closed
             ? GatewayAlarmLifecycleActionResult::Completed
             : GatewayAlarmLifecycleActionResult::NotFound;
}

bool attemptGatewayOfflineDismissalWithBackoff(
    const GatewayAlarmEvent& event,
    const GatewayOfflineDismissalPersistenceOps& operations,
    GatewayAlarmPersistenceRetryBackoff* retryBackoff, uint32_t nowMs,
    GatewayAlarmLifecycleActionResult* result) {
  if (retryBackoff == nullptr || result == nullptr ||
      !retryBackoff->canAttempt(nowMs)) {
    return false;
  }
  *result = persistGatewayOfflineDismissal(event, operations);
  retryBackoff->recordResult(*result, nowMs);
  return true;
}

bool GatewayAlarmLifecycleCoordinator::StateLock::enter() {
#if defined(ESP_PLATFORM) || defined(ARDUINO_ARCH_ESP32)
  portENTER_CRITICAL(&mux_);
  return true;
#else
  for (uint8_t attempt = 0; attempt < kHostLockAttempts; ++attempt) {
    if (!flag_.test_and_set(std::memory_order_acquire)) return true;
  }
  return false;
#endif
}

void GatewayAlarmLifecycleCoordinator::StateLock::exit() {
#if defined(ESP_PLATFORM) || defined(ARDUINO_ARCH_ESP32)
  portEXIT_CRITICAL(&mux_);
#else
  flag_.clear(std::memory_order_release);
#endif
}

bool GatewayAlarmLifecycleCoordinator::isValidDeviceId(
    const char* deviceId) {
  if (deviceId == nullptr || deviceId[0] == '\0') return false;
  size_t length = 0;
  while (length < GATEWAY_ALARM_LIFECYCLE_DEVICE_ID_CAPACITY &&
         deviceId[length] != '\0') {
    ++length;
  }
  return length > 0 &&
         length < GATEWAY_ALARM_LIFECYCLE_DEVICE_ID_CAPACITY;
}

bool GatewayAlarmLifecycleCoordinator::isValidEvent(
    const GatewayAlarmEvent& event) {
  return isValidDeviceId(event.device_id) &&
         event.condition_token != GATEWAY_INVALID_EVENT_TOKEN &&
         event.event_token != GATEWAY_INVALID_EVENT_TOKEN;
}

bool GatewayAlarmLifecycleCoordinator::sameKey(const Slot& slot,
                                               GatewayAlarmType type,
                                               const char* deviceId) {
  return slot.in_use && slot.type == type && deviceId != nullptr &&
         strcmp(slot.device_id, deviceId) == 0;
}

bool GatewayAlarmLifecycleCoordinator::sameEvent(
    const GatewayAlarmEvent& left, const GatewayAlarmEvent& right) {
  return left.type == right.type &&
         strcmp(left.device_id, right.device_id) == 0 &&
         left.condition_token == right.condition_token &&
         left.event_token == right.event_token &&
         left.window_index == right.window_index &&
         left.repeat_index == right.repeat_index;
}

bool GatewayAlarmLifecycleCoordinator::sameSession(
    const GatewayAlarmEvent& left, const GatewayAlarmEvent& right) {
  return left.type == right.type &&
         strcmp(left.device_id, right.device_id) == 0 &&
         left.condition_token == right.condition_token;
}

size_t GatewayAlarmLifecycleCoordinator::findSlot(
    GatewayAlarmType type, const char* deviceId) const {
  for (size_t i = 0; i < GATEWAY_ALARM_LIFECYCLE_CAPACITY; ++i) {
    if (sameKey(slots_[i], type, deviceId)) return i;
  }
  return kNoLifecycleSlot;
}

size_t GatewayAlarmLifecycleCoordinator::findFreeSlot() const {
  for (size_t i = 0; i < GATEWAY_ALARM_LIFECYCLE_CAPACITY; ++i) {
    if (!slots_[i].in_use) return i;
  }
  return kNoLifecycleSlot;
}

void GatewayAlarmLifecycleCoordinator::initializeSlot(
    Slot* slot, GatewayAlarmType type, const char* deviceId) {
  if (slot == nullptr) return;
  *slot = Slot{};
  slot->in_use = true;
  slot->type = type;
  memcpy(slot->device_id, deviceId, strlen(deviceId) + 1);
}

void GatewayAlarmLifecycleCoordinator::fillAction(
    const Slot& slot, GatewayAlarmLifecycleActionType type,
    GatewayAlarmLifecycleAction* action) {
  *action = GatewayAlarmLifecycleAction{};
  action->type = type;
  action->alarm_type = slot.type;
  memcpy(action->device_id, slot.device_id, strlen(slot.device_id) + 1);
  if (slot.has_event) action->event = slot.last_event;
}

void GatewayAlarmLifecycleCoordinator::fillCloseAction(
    const GatewayAlarmEvent& event, GatewayAlarmLifecycleActionType type,
    GatewayAlarmLifecycleAction* action) {
  *action = GatewayAlarmLifecycleAction{};
  action->type = type;
  action->alarm_type = event.type;
  memcpy(action->device_id, event.device_id, strlen(event.device_id) + 1);
  action->event = event;
}

bool GatewayAlarmLifecycleCoordinator::closeBlocksRetire(
    const Slot& slot) const {
  return close_phase_ != ClosePhase::None && slot.has_event &&
         sameSession(slot.last_event, close_event_);
}

GatewayAlarmLifecycleReserveResult
GatewayAlarmLifecycleCoordinator::reserveScheduleKey(
    GatewayAlarmType type, const char* deviceId) {
  if (!isValidDeviceId(deviceId)) {
    return GatewayAlarmLifecycleReserveResult::Invalid;
  }
  StateLockGuard lock(state_lock_);
  if (!lock.owns()) return GatewayAlarmLifecycleReserveResult::Retry;

  const size_t existing = findSlot(type, deviceId);
  if (existing != kNoLifecycleSlot) {
    const Slot& slot = slots_[existing];
    if (slot.cleanup_requested || slot.phase != SlotPhase::Idle) {
      return GatewayAlarmLifecycleReserveResult::Busy;
    }
    return GatewayAlarmLifecycleReserveResult::ReservedExisting;
  }

  const size_t freeSlot = findFreeSlot();
  if (freeSlot == kNoLifecycleSlot) {
    return GatewayAlarmLifecycleReserveResult::Full;
  }
  initializeSlot(&slots_[freeSlot], type, deviceId);
  return GatewayAlarmLifecycleReserveResult::ReservedNew;
}

bool GatewayAlarmLifecycleCoordinator::releaseUnusedReservation(
    GatewayAlarmType type, const char* deviceId) {
  StateLockGuard lock(state_lock_);
  if (!lock.owns()) return false;
  const size_t index = findSlot(type, deviceId);
  if (index == kNoLifecycleSlot) return false;
  Slot& slot = slots_[index];
  if (slot.has_event || slot.cleanup_requested ||
      slot.phase != SlotPhase::Idle) {
    return false;
  }
  slot = Slot{};
  return true;
}

bool GatewayAlarmLifecycleCoordinator::retainScheduledEvent(
    const GatewayAlarmEvent& event) {
  if (!isValidEvent(event)) return false;
  StateLockGuard lock(state_lock_);
  if (!lock.owns()) return false;
  const size_t index = findSlot(event.type, event.device_id);
  if (index == kNoLifecycleSlot) return false;
  Slot& slot = slots_[index];
  if (slot.cleanup_requested || slot.phase != SlotPhase::Idle) return false;
  slot.last_event = event;
  slot.has_event = true;
  slot.phase = SlotPhase::ResetSession;
  return true;
}

bool GatewayAlarmLifecycleCoordinator::requestCleanup(
    GatewayAlarmType type, const char* deviceId) {
  if (!isValidDeviceId(deviceId)) return false;
  StateLockGuard lock(state_lock_);
  if (!lock.owns()) return false;
  size_t index = findSlot(type, deviceId);
  if (index == kNoLifecycleSlot) {
    index = findFreeSlot();
    if (index == kNoLifecycleSlot) return false;
    initializeSlot(&slots_[index], type, deviceId);
  }
  Slot& slot = slots_[index];
  if (slot.cleanup_requested) return true;
  slot.cleanup_requested = true;
  slot.phase = SlotPhase::ClearAlertKey;
  return true;
}

bool GatewayAlarmLifecycleCoordinator::guardClose(
    const GatewayAlarmEvent& event) {
  if (!isValidEvent(event)) return false;
  StateLockGuard lock(state_lock_);
  if (!lock.owns() || close_phase_ != ClosePhase::None ||
      alert_ui_teardown_reserved_) {
    return false;
  }
  close_event_ = event;
  close_phase_ = ClosePhase::Guarded;
  return true;
}

bool GatewayAlarmLifecycleCoordinator::reserveAlertUiTeardown() {
  StateLockGuard lock(state_lock_);
  if (!lock.owns() || close_phase_ != ClosePhase::None ||
      alert_ui_teardown_reserved_) {
    return false;
  }
  alert_ui_teardown_reserved_ = true;
  return true;
}

bool GatewayAlarmLifecycleCoordinator::releaseAlertUiTeardown() {
  StateLockGuard lock(state_lock_);
  if (!lock.owns() || !alert_ui_teardown_reserved_) return false;
  alert_ui_teardown_reserved_ = false;
  return true;
}

bool GatewayAlarmLifecycleCoordinator::confirmClose(
    const GatewayAlarmEvent& event) {
  StateLockGuard lock(state_lock_);
  if (!lock.owns() || close_phase_ != ClosePhase::Guarded ||
      !sameEvent(close_event_, event)) {
    return false;
  }
  close_phase_ = (event.type == GatewayAlarmType::Offline ||
                  event.type == GatewayAlarmType::CuckooOffline)
                     ? ClosePhase::PersistOfflineDismissal
                     : ClosePhase::CloseTrackerExact;
  return true;
}

bool GatewayAlarmLifecycleCoordinator::abortClose(
    const GatewayAlarmEvent& event) {
  StateLockGuard lock(state_lock_);
  if (!lock.owns() || close_phase_ != ClosePhase::Guarded ||
      !sameEvent(close_event_, event)) {
    return false;
  }
  close_event_ = GatewayAlarmEvent{};
  close_phase_ = ClosePhase::None;
  return true;
}

bool GatewayAlarmLifecycleCoordinator::nextMainAction(
    size_t slotIndex, GatewayAlarmLifecycleAction* action) const {
  if (action == nullptr || slotIndex >= GATEWAY_ALARM_LIFECYCLE_CAPACITY) {
    return false;
  }
  StateLockGuard lock(state_lock_);
  if (!lock.owns()) return false;
  const Slot& slot = slots_[slotIndex];
  if (!slot.in_use) return false;

  // While a CLOSE for this window is in flight, pause the closed window's
  // pre-queue phases. This prevents a pending PublishAlert retry from
  // re-publishing a dismissed (closed) alert after the alert state has been
  // emptied by the close's exact dismissal.
  if (close_phase_ != ClosePhase::None && slot.has_event &&
      slot.last_event.event_token == close_event_.event_token &&
      (slot.phase == SlotPhase::ResetSession ||
       slot.phase == SlotPhase::PublishAlert)) {
    return false;
  }

  switch (slot.phase) {
    case SlotPhase::ResetSession:
      fillAction(slot, GatewayAlarmLifecycleActionType::ResetSession, action);
      return true;
    case SlotPhase::PublishAlert:
      fillAction(slot, GatewayAlarmLifecycleActionType::PublishAlert, action);
      return true;
    case SlotPhase::QueueAudio:
      fillAction(slot, GatewayAlarmLifecycleActionType::QueueAudio, action);
      return true;
    case SlotPhase::ClearAlertKey:
      fillAction(slot, GatewayAlarmLifecycleActionType::ClearAlertKey, action);
      return true;
    case SlotPhase::CancelAudioKey:
      fillAction(slot, GatewayAlarmLifecycleActionType::CancelAudioKey,
                 action);
      return true;
    case SlotPhase::WaitToRetire:
      if (closeBlocksRetire(slot)) return false;
      fillAction(slot, GatewayAlarmLifecycleActionType::RetireExact, action);
      return true;
    case SlotPhase::Idle:
      return false;
  }
  return false;
}

bool GatewayAlarmLifecycleCoordinator::nextCloseAction(
    GatewayAlarmLifecycleAction* action) const {
  if (action == nullptr) return false;
  StateLockGuard lock(state_lock_);
  if (!lock.owns()) return false;
  if (close_phase_ == ClosePhase::PersistOfflineDismissal) {
    fillCloseAction(close_event_,
                    GatewayAlarmLifecycleActionType::PersistOfflineDismissal,
                    action);
    return true;
  }
  if (close_phase_ == ClosePhase::CloseTrackerExact) {
    fillCloseAction(close_event_,
                    GatewayAlarmLifecycleActionType::CloseTrackerExact,
                    action);
    return true;
  }
  if (close_phase_ == ClosePhase::CancelAudioExact) {
    fillCloseAction(close_event_,
                    GatewayAlarmLifecycleActionType::CancelAudioExact,
                    action);
    return true;
  }
  return false;
}

bool GatewayAlarmLifecycleCoordinator::applyResult(
    const GatewayAlarmLifecycleAction& action,
    GatewayAlarmLifecycleActionResult result) {
  if (action.type == GatewayAlarmLifecycleActionType::None) return false;
  StateLockGuard lock(state_lock_);
  if (!lock.owns()) return false;

  if (action.type ==
          GatewayAlarmLifecycleActionType::PersistOfflineDismissal ||
      action.type == GatewayAlarmLifecycleActionType::CloseTrackerExact ||
      action.type == GatewayAlarmLifecycleActionType::CancelAudioExact) {
    ClosePhase expected = ClosePhase::CancelAudioExact;
    if (action.type ==
        GatewayAlarmLifecycleActionType::PersistOfflineDismissal) {
      expected = ClosePhase::PersistOfflineDismissal;
    } else if (action.type ==
               GatewayAlarmLifecycleActionType::CloseTrackerExact) {
      expected = ClosePhase::CloseTrackerExact;
    }
    if (close_phase_ != expected || !sameEvent(close_event_, action.event)) {
      return false;
    }
    if (result == GatewayAlarmLifecycleActionResult::Retry ||
        result == GatewayAlarmLifecycleActionResult::Rejected) {
      return true;
    }
    if (expected == ClosePhase::PersistOfflineDismissal) {
      if (result == GatewayAlarmLifecycleActionResult::Completed) {
        close_phase_ = ClosePhase::CloseTrackerExact;
      } else if (result == GatewayAlarmLifecycleActionResult::NotFound) {
        close_phase_ = ClosePhase::None;
        close_event_ = GatewayAlarmEvent{};
      }
    } else if (expected == ClosePhase::CloseTrackerExact) {
      close_phase_ = ClosePhase::CancelAudioExact;
    } else {
      if (result == GatewayAlarmLifecycleActionResult::Completed ||
          result == GatewayAlarmLifecycleActionResult::NotFound) {
        const size_t index =
            findSlot(close_event_.type, close_event_.device_id);
        if (index != kNoLifecycleSlot) {
          Slot& slot = slots_[index];
          // Clear the whole closed window from the alarm flow. Repeats of the
          // same window share one event_token, so match on the token (not the
          // full event): this returns the slot to Idle for future windows and
          // leaves no PublishAlert/QueueAudio retry behind that could
          // re-publish or re-queue the closed alarm.
          if (slot.has_event &&
              slot.last_event.event_token == close_event_.event_token &&
              (slot.phase == SlotPhase::ResetSession ||
               slot.phase == SlotPhase::PublishAlert ||
               slot.phase == SlotPhase::QueueAudio)) {
            slot.phase = SlotPhase::Idle;
          }
        }
      }
      close_phase_ = ClosePhase::None;
      close_event_ = GatewayAlarmEvent{};
    }
    return true;
  }

  const size_t index = findSlot(action.alarm_type, action.device_id);
  if (index == kNoLifecycleSlot) return false;
  Slot& slot = slots_[index];
  GatewayAlarmLifecycleActionType expected =
      GatewayAlarmLifecycleActionType::None;
  switch (slot.phase) {
    case SlotPhase::ResetSession:
      expected = GatewayAlarmLifecycleActionType::ResetSession;
      break;
    case SlotPhase::PublishAlert:
      expected = GatewayAlarmLifecycleActionType::PublishAlert;
      break;
    case SlotPhase::QueueAudio:
      expected = GatewayAlarmLifecycleActionType::QueueAudio;
      break;
    case SlotPhase::ClearAlertKey:
      expected = GatewayAlarmLifecycleActionType::ClearAlertKey;
      break;
    case SlotPhase::CancelAudioKey:
      expected = GatewayAlarmLifecycleActionType::CancelAudioKey;
      break;
    case SlotPhase::WaitToRetire:
      expected = GatewayAlarmLifecycleActionType::RetireExact;
      break;
    case SlotPhase::Idle:
      return false;
  }
  if (action.type != expected) return false;
  if (slot.has_event &&
      (expected == GatewayAlarmLifecycleActionType::ResetSession ||
       expected == GatewayAlarmLifecycleActionType::PublishAlert ||
       expected == GatewayAlarmLifecycleActionType::QueueAudio ||
       expected == GatewayAlarmLifecycleActionType::RetireExact) &&
      !sameEvent(slot.last_event, action.event)) {
    return false;
  }
  if (result == GatewayAlarmLifecycleActionResult::Retry ||
      result == GatewayAlarmLifecycleActionResult::Rejected) {
    return true;
  }

  switch (slot.phase) {
    case SlotPhase::ResetSession:
      slot.phase = SlotPhase::PublishAlert;
      break;
    case SlotPhase::PublishAlert:
      slot.phase = SlotPhase::QueueAudio;
      break;
    case SlotPhase::QueueAudio:
      slot.phase = SlotPhase::Idle;
      break;
    case SlotPhase::ClearAlertKey:
      slot.phase = SlotPhase::CancelAudioKey;
      break;
    case SlotPhase::CancelAudioKey:
      if (!slot.has_event) {
        slot = Slot{};
      } else {
        slot.phase = SlotPhase::WaitToRetire;
      }
      break;
    case SlotPhase::WaitToRetire:
      if (closeBlocksRetire(slot)) {
        slot.phase = SlotPhase::WaitToRetire;
      } else {
        slot = Slot{};
      }
      break;
    case SlotPhase::Idle:
      return false;
  }
  return true;
}

bool GatewayAlarmLifecycleCoordinator::copyKeyAt(
    size_t slotIndex, GatewayAlarmType* type, char* deviceId,
    size_t deviceIdCapacity) const {
  if (type == nullptr || deviceId == nullptr || deviceIdCapacity == 0 ||
      slotIndex >= GATEWAY_ALARM_LIFECYCLE_CAPACITY) {
    return false;
  }
  StateLockGuard lock(state_lock_);
  if (!lock.owns() || !slots_[slotIndex].in_use) return false;
  const size_t length = strlen(slots_[slotIndex].device_id);
  if (length + 1 > deviceIdCapacity) return false;
  *type = slots_[slotIndex].type;
  memcpy(deviceId, slots_[slotIndex].device_id, length + 1);
  return true;
}

bool GatewayAlarmLifecycleCoordinator::canTearDownAlertUi() const {
  StateLockGuard lock(state_lock_);
  return lock.owns() && close_phase_ == ClosePhase::None &&
         !alert_ui_teardown_reserved_;
}

bool GatewayAlarmLifecycleCoordinator::canTearDownAlertUiWithReservation() const {
  StateLockGuard lock(state_lock_);
  return lock.owns() && close_phase_ == ClosePhase::None &&
         alert_ui_teardown_reserved_;
}

GatewayAlarmLifecycleCoordinator gatewayAlarmLifecycle;
