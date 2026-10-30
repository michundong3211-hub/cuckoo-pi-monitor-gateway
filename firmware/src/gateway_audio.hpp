#ifndef SRC_GATEWAY_AUDIO_HPP_
#define SRC_GATEWAY_AUDIO_HPP_

#include <stddef.h>
#include <stdint.h>
#include <atomic>

#include <gateway_alarm.hpp>
#include <gravity_display_units.hpp>

#if defined(ENABLE_GATEWAY_AUDIO) && defined(ARDUINO_ARCH_ESP32)
#include <driver/i2s_std.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#if defined(GATEWAY_35)
#include "gateway35_audio_codec.hpp"
#elif defined(GMT154_06)
#include "gmt154_audio_codec.hpp"
#endif
#endif

constexpr size_t GATEWAY_AUDIO_DEVICE_ID_CAPACITY = 64;
constexpr float kGatewayAudioPcmAmplitude = 12000.0f;

constexpr uint16_t kTemperatureFirstFrequencyHz = 880;
constexpr uint16_t kTemperatureFirstDurationMs = 120;
constexpr uint16_t kTemperatureGapMs = 60;
constexpr uint16_t kTemperatureSecondFrequencyHz = 1320;
constexpr uint16_t kTemperatureSecondDurationMs = 180;
constexpr float kTemperatureAmplitudeScale = 1.00f;

constexpr uint16_t kOfflineFirstFrequencyHz = 880;
constexpr uint16_t kOfflineFirstDurationMs = 80;
constexpr uint16_t kOfflineGapMs = 50;
constexpr uint16_t kOfflineSecondFrequencyHz = 1320;
constexpr uint16_t kOfflineSecondDurationMs = 100;
constexpr float kOfflineAmplitudeScale = 0.50f;

static_assert(kTemperatureFirstDurationMs + kTemperatureGapMs +
                      kTemperatureSecondDurationMs ==
                  360,
              "temperature double tone duration changed");
static_assert(kOfflineFirstDurationMs + kOfflineGapMs +
                      kOfflineSecondDurationMs ==
                  230,
              "offline double tone duration changed");

struct GatewayAudioAlarmDecision {
  constexpr GatewayAudioAlarmDecision(
      GatewayAlarmType alarmType = GatewayAlarmType::TemperatureChange,
      bool shouldPlay = false, uint16_t firstFrequencyHz = 0,
      uint16_t firstDurationMs = 0, uint16_t secondFrequencyHz = 0,
      uint16_t secondDurationMs = 0, uint16_t gapMs = 0,
      float amplitudeScale = 1.0f)
      : type(alarmType),
        should_play(shouldPlay),
        first_frequency_hz(firstFrequencyHz),
        first_duration_ms(firstDurationMs),
        second_frequency_hz(secondFrequencyHz),
        second_duration_ms(secondDurationMs),
        gap_ms(gapMs),
        amplitude_scale(amplitudeScale) {}

  GatewayAlarmType type;
  bool should_play;
  uint16_t first_frequency_hz;
  uint16_t first_duration_ms;
  uint16_t second_frequency_hz;
  uint16_t second_duration_ms;
  uint16_t gap_ms;
  float amplitude_scale;
};

struct GatewayAudioQueueItem {
  GatewayAudioAlarmDecision decision{};
  GatewayAlarmEvent event{};
  bool cancelled = false;
};

constexpr uint8_t GATEWAY_AUDIO_QUEUE_CAPACITY = 4;
// Shared capacity for active Gravity and Cuckoo alarm keys.
constexpr uint8_t GATEWAY_AUDIO_CANCEL_KEY_CAPACITY = 40;

constexpr GatewayAudioAlarmDecision gatewayAudioDecisionForAlarm(
    GatewayAlarmType type) {
  return (type == GatewayAlarmType::TemperatureChange ||
          type == GatewayAlarmType::CuckooBeerDeviation)
             ? GatewayAudioAlarmDecision(
                   type, true, kTemperatureFirstFrequencyHz,
                   kTemperatureFirstDurationMs, kTemperatureSecondFrequencyHz,
                   kTemperatureSecondDurationMs, kTemperatureGapMs,
                   kTemperatureAmplitudeScale)
             : GatewayAudioAlarmDecision(
                   type, true, kOfflineFirstFrequencyHz,
                   kOfflineFirstDurationMs, kOfflineSecondFrequencyHz,
                   kOfflineSecondDurationMs, kOfflineGapMs,
                   kOfflineAmplitudeScale);
}

enum class GatewayAudioPlaybackPhase : uint8_t {
  FirstTone,
  Gap,
  SecondTone,
  Complete,
  Cancelled,
  WriteFailed,
};

enum class GatewayAudioPlaybackStepResult : uint8_t {
  Completed,
  Cancelled,
  WriteFailed,
};

enum class GatewayAudioCancelResult : uint8_t {
  Cancelled,
  NotFound,
  Retry,
};

constexpr GatewayAudioPlaybackPhase gatewayAudioAdvancePlaybackPhase(
    GatewayAudioPlaybackPhase phase,
    GatewayAudioPlaybackStepResult result) {
  return phase == GatewayAudioPlaybackPhase::Complete ||
                 phase == GatewayAudioPlaybackPhase::Cancelled ||
                 phase == GatewayAudioPlaybackPhase::WriteFailed
             ? phase
             : result == GatewayAudioPlaybackStepResult::Cancelled
                   ? GatewayAudioPlaybackPhase::Cancelled
                   : result == GatewayAudioPlaybackStepResult::WriteFailed
                         ? GatewayAudioPlaybackPhase::WriteFailed
                         : phase == GatewayAudioPlaybackPhase::FirstTone
                               ? GatewayAudioPlaybackPhase::Gap
                               : phase == GatewayAudioPlaybackPhase::Gap
                                     ? GatewayAudioPlaybackPhase::SecondTone
                                     : GatewayAudioPlaybackPhase::Complete;
}

class GatewayAudioPlaybackSink {
 public:
  virtual ~GatewayAudioPlaybackSink() {}
  virtual GatewayAudioPlaybackStepResult writeTone(
      const GatewayAudioQueueItem& item, uint16_t frequencyHz,
      uint16_t durationMs) = 0;
  virtual GatewayAudioPlaybackStepResult waitGap(
      const GatewayAudioQueueItem& item, uint16_t durationMs) = 0;
  virtual void writeSilence(const GatewayAudioQueueItem& item) = 0;
};

GatewayAudioPlaybackStepResult gatewayAudioPlayTone(
    const GatewayAudioQueueItem& item, GatewayAudioPlaybackSink* sink);

int16_t gatewayAudioPcmSample(float normalizedSample, float amplitudeScale);

constexpr bool gatewayAudioShouldEnterPlaybackControl(bool available,
                                                       bool txReady,
                                                       bool shouldPlay) {
  return available && txReady && shouldPlay;
}

class GatewayAudioScheduler {
 public:
  bool enqueue(const GatewayAudioQueueItem& item);
  GatewayAudioCancelResult cancelExactResult(
      const GatewayAlarmEvent& event);
  bool cancelExact(const GatewayAlarmEvent& event);
  GatewayAudioCancelResult cancelByKeyResult(GatewayAlarmType type,
                                             const char* deviceId);
  bool cancelByKey(GatewayAlarmType type, const char* deviceId);
  GatewayAudioCancelResult resetKeySession(
      GatewayAlarmType type, const char* deviceId,
      GatewayAlarmEventToken conditionToken);
  // The producer must quiesce this exact condition session before retiring it.
  // Active and pending work is cancelled before its key high-water slot is
  // released; a reclaimed key does not reject arbitrary later events.
  GatewayAudioCancelResult retireExact(const GatewayAlarmEvent& event);
  bool takeNext(GatewayAudioQueueItem* item);
  void finishActive(GatewayAlarmEventToken eventToken);
  size_t pendingCount() const;
  bool active() const;

 private:
  friend class GatewayAudio;

  class StateLock {
   public:
    bool tryEnter();
    void exit();

   private:
#if defined(ENABLE_GATEWAY_AUDIO) && defined(ARDUINO_ARCH_ESP32)
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

  struct Slot {
    bool occupied = false;
    GatewayAudioQueueItem item{};
  };

  struct CancellationHighWatermark {
    bool occupied = false;
    GatewayAlarmType type = GatewayAlarmType::TemperatureChange;
    char device_id[GATEWAY_AUDIO_DEVICE_ID_CAPACITY] = {};
    GatewayAlarmEventToken condition_token_floor =
        GATEWAY_INVALID_EVENT_TOKEN;
    GatewayAlarmEventToken event_token = GATEWAY_INVALID_EVENT_TOKEN;
  };

  Slot _slots[GATEWAY_AUDIO_QUEUE_CAPACITY] = {};
  CancellationHighWatermark
      _cancelled_keys[GATEWAY_AUDIO_CANCEL_KEY_CAPACITY] = {};
  mutable StateLock _state_lock;
  bool _active = false;
  size_t _active_slot = 0;
  bool _has_last_key = false;
  GatewayAlarmType _last_type = GatewayAlarmType::TemperatureChange;
  char _last_device_id[GATEWAY_AUDIO_DEVICE_ID_CAPACITY] = {};

  static bool isValidDeviceId(const char* deviceId);
  static bool sameKey(GatewayAlarmType leftType, const char* leftDevice,
                      GatewayAlarmType rightType, const char* rightDevice);
  static int compareKeys(GatewayAlarmType leftType, const char* leftDevice,
                         GatewayAlarmType rightType,
                         const char* rightDevice);
  static void clearSlot(Slot* slot);
  size_t findCancellationKey(GatewayAlarmType type,
                             const char* deviceId) const;
  bool isClosed(const GatewayAlarmEvent& event) const;
  bool canRecordCancellation(GatewayAlarmType type,
                             const char* deviceId) const;
  void recordCancellation(const GatewayAlarmEvent& event);
  bool isCancelled(GatewayAlarmEventToken eventToken) const;
};

bool gatewayTemperatureOutOfRangeC(float tempC, float minTempC, float maxTempC);
bool gatewayBatteryLowPercent(float batteryPercent, float lowThresholdPercent);
void gatewayFormatDashboardAbv(float abvPercent, char* out, size_t outSize);

class GatewayAudio {
 public:
  void begin();
  bool queueAlarm(const GatewayAlarmEvent& event);
  GatewayAudioCancelResult cancelExactResult(
      const GatewayAlarmEvent& event);
  bool cancelExact(const GatewayAlarmEvent& event);
  GatewayAudioCancelResult cancelByKeyResult(GatewayAlarmType type,
                                             const char* deviceId);
  bool cancelByKey(GatewayAlarmType type, const char* deviceId);
  GatewayAudioCancelResult resetKeySession(
      GatewayAlarmType type, const char* deviceId,
      GatewayAlarmEventToken conditionToken);
  GatewayAudioCancelResult retireExact(const GatewayAlarmEvent& event);
  size_t pendingCount() const;
  bool isActive() const;

 private:
  bool _started = false;
  bool _available = false;
  GatewayAudioScheduler _scheduler;

#if defined(ENABLE_GATEWAY_AUDIO) && defined(ARDUINO_ARCH_ESP32)
  TaskHandle_t _task = nullptr;
  i2s_chan_handle_t _tx_handle = nullptr;
#if defined(GATEWAY_35)
  Gateway35AudioCodec _codec;
#elif defined(GMT154_06)
  Gmt154AudioCodec _gmt154_codec;
#endif
#if defined(GATEWAY_35) || defined(GMT154_06)
  bool _power_amplifier_enabled = false;
#endif

  class I2sPlaybackSink;
  static void audioTaskEntry(void* parameter);
  void audioTaskLoop();
  void playTone(const GatewayAudioQueueItem& item);
  GatewayAudioPlaybackStepResult playSingleTone(
      const GatewayAudioQueueItem& item, uint16_t frequencyHz,
      uint16_t durationMs);
  void writeSilence();
  bool writeSilenceChunk();
  bool isActiveCancelled(const GatewayAudioQueueItem& item) const;
  void releaseRuntime();
#endif
};

extern GatewayAudio gatewayAudio;

#endif  // SRC_GATEWAY_AUDIO_HPP_
