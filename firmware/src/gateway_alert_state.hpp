#pragma once

#include <stddef.h>
#include <stdint.h>

#include <gateway_alarm.hpp>

#if defined(ESP_PLATFORM) || defined(ARDUINO_ARCH_ESP32)
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#else
#include <atomic>
#endif

constexpr size_t GATEWAY_ALERT_QUEUE_CAPACITY = 8;
constexpr size_t GATEWAY_ALERT_DEVICE_ID_CAPACITY = 64;

using GatewayAlertEvent = GatewayAlarmEvent;

enum class GatewayAlertClearResult : uint8_t {
  Cleared,
  NotFound,
  Retry,
};

class GatewayAlertState {
 public:
  GatewayAlertState();

  bool publish(const GatewayAlarmEvent& event);
  GatewayAlertClearResult clearByKeyResult(GatewayAlarmType type,
                                           const char* deviceId);
  bool clearByKey(GatewayAlarmType type, const char* deviceId);
  GatewayAlertClearResult clearByTokenResult(
      GatewayAlarmEventToken eventToken);
  bool peek(GatewayAlertEvent* event) const;
  bool dismissCurrentExact(GatewayAlarmEventToken eventToken,
                           GatewayAlertEvent* dismissed);
  size_t pendingCount() const;

 private:
  class StateLock {
   public:
    bool tryEnter();
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
        : lock_(lock), owns_(lock_.tryEnter()) {}
    ~StateLockGuard() {
      if (owns_) lock_.exit();
    }

    bool owns() const { return owns_; }

   private:
    StateLock& lock_;
    bool owns_;
  };

  static bool isValidDeviceId(const char* deviceId);
  static bool sameKey(const GatewayAlertEvent& left,
                      const GatewayAlertEvent& right);
  static bool sameIdentity(const GatewayAlertEvent& left,
                           const GatewayAlertEvent& right);
  static bool matches(const GatewayAlertEvent& event, GatewayAlarmType type,
                      const char* deviceId);
  static bool copyEvent(GatewayAlertEvent* destination,
                        const GatewayAlertEvent& source);

  mutable StateLock state_lock_;
  bool has_current_ = false;
  GatewayAlertEvent current_{};
  GatewayAlertEvent queue_[GATEWAY_ALERT_QUEUE_CAPACITY] = {};
  size_t queue_count_ = 0;
};

extern GatewayAlertState gatewayAlertState;
