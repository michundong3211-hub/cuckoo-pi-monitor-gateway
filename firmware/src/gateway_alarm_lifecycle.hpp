#pragma once

#include <stddef.h>
#include <stdint.h>

#include <gateway_alarm.hpp>

struct GravityDeviceSettings;

#if defined(ESP_PLATFORM) || defined(ARDUINO_ARCH_ESP32)
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#else
#include <atomic>
#endif

constexpr size_t GATEWAY_ALARM_LIFECYCLE_CAPACITY = 40;
constexpr size_t GATEWAY_ALARM_LIFECYCLE_DEVICE_ID_CAPACITY = 64;
constexpr uint32_t GATEWAY_OFFLINE_DISMISSAL_RETRY_BACKOFF_MS = 5000UL;

enum class GatewayAlarmLifecycleReserveResult : uint8_t {
  ReservedNew,
  ReservedExisting,
  Busy,
  Full,
  Invalid,
  Retry,
};

enum class GatewayAlarmLifecycleActionType : uint8_t {
  None,
  ResetSession,
  PublishAlert,
  QueueAudio,
  ClearAlertKey,
  CancelAudioKey,
  RetireExact,
  PersistOfflineDismissal,
  CloseTrackerExact,
  CancelAudioExact,
};

enum class GatewayAlarmLifecycleActionResult : uint8_t {
  Completed,
  NotFound,
  Retry,
  Rejected,
};

class GatewayAlarmPersistenceRetryBackoff {
 public:
  bool canAttempt(uint32_t nowMs) const {
    return !waiting_ ||
           nowMs - last_attempt_ms_ >=
               GATEWAY_OFFLINE_DISMISSAL_RETRY_BACKOFF_MS;
  }

  void recordResult(GatewayAlarmLifecycleActionResult result,
                    uint32_t nowMs) {
    if (result == GatewayAlarmLifecycleActionResult::Retry) {
      waiting_ = true;
      last_attempt_ms_ = nowMs;
    } else {
      waiting_ = false;
      last_attempt_ms_ = 0;
    }
  }

 private:
  bool waiting_ = false;
  uint32_t last_attempt_ms_ = 0;
};

struct GatewayAlarmLifecycleAction {
  GatewayAlarmLifecycleActionType type =
      GatewayAlarmLifecycleActionType::None;
  GatewayAlarmType alarm_type = GatewayAlarmType::TemperatureChange;
  char device_id[GATEWAY_ALARM_LIFECYCLE_DEVICE_ID_CAPACITY] = {};
  GatewayAlarmEvent event{};
};

struct GatewayOfflineDismissalPersistenceOps {
  GatewayAlarmCloseResult (*validateRuntime)(const GatewayAlarmEvent& event,
                                             void* context) = nullptr;
  bool (*load)(const char* deviceId, GravityDeviceSettings* settings,
               void* context) = nullptr;
  bool (*save)(const GravityDeviceSettings& settings, void* context) = nullptr;
  GatewayAlarmCloseResult (*commitRuntime)(const GatewayAlarmEvent& event,
                                           void* context) = nullptr;
  void* context = nullptr;
};

GatewayAlarmLifecycleActionResult persistGatewayOfflineDismissal(
    const GatewayAlarmEvent& event,
    const GatewayOfflineDismissalPersistenceOps& operations);
bool attemptGatewayOfflineDismissalWithBackoff(
    const GatewayAlarmEvent& event,
    const GatewayOfflineDismissalPersistenceOps& operations,
    GatewayAlarmPersistenceRetryBackoff* retryBackoff, uint32_t nowMs,
    GatewayAlarmLifecycleActionResult* result);

class GatewayAlarmLifecycleCoordinator {
 public:
  GatewayAlarmLifecycleReserveResult reserveScheduleKey(
      GatewayAlarmType type, const char* deviceId);
  bool releaseUnusedReservation(GatewayAlarmType type,
                                const char* deviceId);
  bool retainScheduledEvent(const GatewayAlarmEvent& event);
  bool requestCleanup(GatewayAlarmType type, const char* deviceId);

  bool guardClose(const GatewayAlarmEvent& event);
  bool confirmClose(const GatewayAlarmEvent& event);
  bool abortClose(const GatewayAlarmEvent& event);
  bool reserveAlertUiTeardown();
  bool releaseAlertUiTeardown();

  bool nextMainAction(size_t slotIndex,
                      GatewayAlarmLifecycleAction* action) const;
  bool nextCloseAction(GatewayAlarmLifecycleAction* action) const;
  bool applyResult(const GatewayAlarmLifecycleAction& action,
                   GatewayAlarmLifecycleActionResult result);

  bool copyKeyAt(size_t slotIndex, GatewayAlarmType* type, char* deviceId,
                 size_t deviceIdCapacity) const;
  bool canTearDownAlertUi() const;
  bool canTearDownAlertUiWithReservation() const;

 private:
  enum class SlotPhase : uint8_t {
    Idle,
    ResetSession,
    PublishAlert,
    QueueAudio,
    ClearAlertKey,
    CancelAudioKey,
    WaitToRetire,
  };

  enum class ClosePhase : uint8_t {
    None,
    Guarded,
    PersistOfflineDismissal,
    CloseTrackerExact,
    CancelAudioExact,
  };

  struct Slot {
    bool in_use = false;
    GatewayAlarmType type = GatewayAlarmType::TemperatureChange;
    char device_id[GATEWAY_ALARM_LIFECYCLE_DEVICE_ID_CAPACITY] = {};
    GatewayAlarmEvent last_event{};
    bool has_event = false;
    bool cleanup_requested = false;
    SlotPhase phase = SlotPhase::Idle;
  };

  class StateLock {
   public:
    bool enter();
    void exit();

   private:
#if defined(ESP_PLATFORM) || defined(ARDUINO_ARCH_ESP32)
    portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
#else
    std::atomic_flag flag_ = ATOMIC_FLAG_INIT;
#endif
  };

  class StateLockGuard {
   public:
    explicit StateLockGuard(StateLock& lock)
        : lock_(lock), owns_(lock_.enter()) {}
    ~StateLockGuard() {
      if (owns_) lock_.exit();
    }
    bool owns() const { return owns_; }

   private:
    StateLock& lock_;
    bool owns_;
  };

  Slot slots_[GATEWAY_ALARM_LIFECYCLE_CAPACITY] = {};
  ClosePhase close_phase_ = ClosePhase::None;
  bool alert_ui_teardown_reserved_ = false;
  GatewayAlarmEvent close_event_{};
  mutable StateLock state_lock_;

  static bool isValidDeviceId(const char* deviceId);
  static bool isValidEvent(const GatewayAlarmEvent& event);
  static bool sameKey(const Slot& slot, GatewayAlarmType type,
                      const char* deviceId);
  static bool sameEvent(const GatewayAlarmEvent& left,
                        const GatewayAlarmEvent& right);
  static bool sameSession(const GatewayAlarmEvent& left,
                          const GatewayAlarmEvent& right);
  size_t findSlot(GatewayAlarmType type, const char* deviceId) const;
  size_t findFreeSlot() const;
  static void initializeSlot(Slot* slot, GatewayAlarmType type,
                             const char* deviceId);
  static void fillAction(const Slot& slot,
                         GatewayAlarmLifecycleActionType type,
                         GatewayAlarmLifecycleAction* action);
  static void fillCloseAction(const GatewayAlarmEvent& event,
                              GatewayAlarmLifecycleActionType type,
                              GatewayAlarmLifecycleAction* action);
  bool closeBlocksRetire(const Slot& slot) const;
};

extern GatewayAlarmLifecycleCoordinator gatewayAlarmLifecycle;
