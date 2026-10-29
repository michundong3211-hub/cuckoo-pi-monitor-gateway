#pragma once

#include <stddef.h>
#include <stdint.h>

#if defined(ESP_PLATFORM) || defined(ARDUINO_ARCH_ESP32)
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#else
#include <atomic>
#endif

enum class GatewayAlarmType : uint8_t {
  TemperatureChange,
  Offline,
  CuckooOffline,
  CuckooBeerDeviation,
};

using GatewayAlarmEventToken = uint64_t;
constexpr GatewayAlarmEventToken GATEWAY_INVALID_EVENT_TOKEN = 0;

constexpr uint32_t kAlarmCyclePeriodMs = 300000UL;
constexpr uint32_t kAlarmAudibleWindowMs = 300000UL;
constexpr uint32_t kAlarmQuietWindowMs = 0UL;
constexpr uint32_t kAlarmRepeatIntervalMs = 10000UL;

struct GatewayAlarmEvent {
  GatewayAlarmType type = GatewayAlarmType::TemperatureChange;
  char device_id[64] = {};
  GatewayAlarmEventToken condition_token = GATEWAY_INVALID_EVENT_TOKEN;
  GatewayAlarmEventToken event_token = GATEWAY_INVALID_EVENT_TOKEN;
  uint32_t window_index = 0;
  uint8_t repeat_index = 0;
};

enum class GatewayAlarmCloseResult : uint8_t {
  Closed,
  NotFound,
  Retry,
};

struct GatewayAlarmDecision {
  bool should_play = false;
  bool should_clear = false;
  GatewayAlarmType type = GatewayAlarmType::TemperatureChange;
  GatewayAlarmEvent event{};
};

struct GatewayAudibleSummary {
  bool any = false;
  bool temperature = false;
};

class GatewayAlarmTracker {
 public:
  GatewayAlarmDecision recordTemperature(
      const char* deviceId, float temperatureC, bool enabled,
      uint8_t compareMinutes, float thresholdC, uint32_t nowMs);
  GatewayAlarmDecision evaluateOffline(const char* deviceId,
                                       uint32_t updateAgeSeconds, bool enabled,
                                       uint32_t timeoutSeconds);
  GatewayAlarmDecision setExternalCondition(GatewayAlarmType type,
                                            const char* deviceId, bool active,
                                            uint32_t nowMs,
                                            bool audibleSuppressed = false);
  bool isConditionActive(GatewayAlarmType type, const char* deviceId);
  bool tryCopyAudibleSummary(GatewayAudibleSummary* summary);
  bool beginAudibleUpdate();
  void endAudibleUpdate();
  GatewayAlarmDecision schedule(GatewayAlarmType type, const char* deviceId,
                                uint32_t nowMs);
  GatewayAlarmCloseResult closeExact(GatewayAlarmEventToken eventToken);
  GatewayAlarmCloseResult validateOfflineDismissalExact(
      const GatewayAlarmEvent& event);
  GatewayAlarmCloseResult commitOfflineDismissalExact(
      const GatewayAlarmEvent& event);
  bool markOnline(const char* deviceId);
  void retainDevices(const char* const* deviceIds, size_t count);
  // Persists/clears the dismissed (CLOSE) flag for a device's offline alarm.
  // Used to survive gateway reboots: the flag is written to device settings on
  // CLOSE and re-applied after boot via restoreOfflineDismissed.
  void setOfflineDismissed(const char* deviceId, bool dismissed);
  // Re-applies a persisted dismissed flag after boot (device currently offline).
  void restoreOfflineDismissed(const char* deviceId);
  // True while a device is still offline and its offline alarm was dismissed
  // (CLOSE). The UI shows a persistent offline marker until data recovers.
  bool isOfflineDismissed(const char* deviceId);

 private:
  static constexpr size_t kMaxDevices = 20;
  static constexpr size_t kDeviceIdCapacity = 64;
  static constexpr size_t kSamplesPerDevice = 61;
  static constexpr uint8_t kNoRepeatIndex = UINT8_MAX;
  static constexpr uint8_t kCloseLockAttempts = 8;

  struct AlarmScheduleState {
    bool active = false;
    GatewayAlarmEventToken condition_token = GATEWAY_INVALID_EVENT_TOKEN;
    uint32_t window_start_ms = 0;
    uint32_t window_index = 0;
    GatewayAlarmEventToken event_token = GATEWAY_INVALID_EVENT_TOKEN;
    uint8_t last_repeat_index = kNoRepeatIndex;
    uint32_t next_repeat_ms = 0;
    bool closed = false;
    bool audible_suppressed = false;
    bool has_window_start = false;
  };

  struct TemperatureSample {
    uint32_t timestamp_ms = 0;
    float temperature_c = 0.0f;
  };

  struct DeviceState {
    bool in_use = false;
    char device_id[kDeviceIdCapacity] = {};
    TemperatureSample samples[kSamplesPerDevice] = {};
    uint8_t sample_count = 0;
    uint8_t next_sample = 0;
    bool temperature_active = false;
    bool observed = false;
    bool offline_active = false;
    AlarmScheduleState temperature_schedule = {};
    AlarmScheduleState offline_schedule = {};
    AlarmScheduleState cuckoo_offline_schedule = {};
    AlarmScheduleState cuckoo_beer_deviation_schedule = {};
  };

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

  // The guard protects only tracker state. Host try-lock failure is immediate;
  // ESP32 uses the same short portMUX critical-section model as GatewayAudio.
  class StateLockGuard {
   public:
    explicit StateLockGuard(StateLock& lock, uint8_t attempts = 1)
        : lock_(lock), owns_(false) {
      for (uint8_t attempt = 0; attempt < attempts; ++attempt) {
        if (lock_.tryEnter()) {
          owns_ = true;
          break;
        }
      }
    }
    ~StateLockGuard() {
      if (owns_) lock_.exit();
    }

    bool owns() const { return owns_; }

   private:
    StateLock& lock_;
    bool owns_;
  };

  DeviceState _devices[kMaxDevices] = {};
  StateLock _state_lock;
  uint32_t _token_prefix = 0;
  uint32_t _token_sequence = 1;
  bool _token_exhausted = false;
  bool _audible_update_in_progress = false;

  static bool isValidDeviceId(const char* deviceId);
  DeviceState* find(const char* deviceId);
  DeviceState* findOrCreate(const char* deviceId);
  static void clearTemperature(DeviceState* state);
  static void storeTemperature(DeviceState* state, uint32_t nowMs,
                               float temperatureC);
  static void clearSchedule(AlarmScheduleState* schedule);
  bool beginCondition(AlarmScheduleState* schedule, uint32_t nowMs,
                      bool hasWindowStart);
  static AlarmScheduleState* scheduleFor(DeviceState* state,
                                         GatewayAlarmType type);
  GatewayAlarmEventToken allocateToken();
  static uint32_t initialTokenPrefix();
};

extern GatewayAlarmTracker gatewayAlarmTracker;

// Diagnostic trace: appends one timestamped line to /alarm_trace.log on the
// gateway storage. ESP32-only; used to correlate alarm/audio/close events
// without relying on the lossy serial output.
#if defined(ARDUINO_ARCH_ESP32)
void traceAlarmEvent(const char* format, ...);
#endif
