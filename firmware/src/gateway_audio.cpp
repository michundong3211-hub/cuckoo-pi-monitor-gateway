#include "gateway_audio.hpp"
#if defined(GMT154_06)
#include "gmt154_merged_audio.hpp"
#endif

#include <math.h>
#include <stdio.h>
#include <string.h>

#if defined(ARDUINO_ARCH_ESP32)
#include <Arduino.h>
#endif

#if defined(ENABLE_GATEWAY_AUDIO) && defined(ARDUINO_ARCH_ESP32)
#include <log.hpp>
#if defined(GATEWAY_35)
#include "waveshare_35_profile.hpp"
#include <Wire.h>
#elif defined(GMT154_06)
#include "gmt154_audio_profile.hpp"
#include <driver/gpio.h>
#if defined(GMT154_AUDIO_STD_INIT_DIAG) && GMT154_AUDIO_STD_INIT_DIAG
#include <esp_err.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <cstdio>
#endif
#if defined(GMT154_AUDIO_STD_ENABLE_DIAG) && GMT154_AUDIO_STD_ENABLE_DIAG
#include <esp_err.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <cstdio>
#endif
#endif
#endif

bool gatewayTemperatureOutOfRangeC(float tempC, float minTempC, float maxTempC) {
  return isTemperatureOutOfRangeC(tempC, minTempC, maxTempC);
}

bool gatewayBatteryLowPercent(float batteryPercent, float lowThresholdPercent) {
  return isBatteryPercentageLow(batteryPercent, lowThresholdPercent);
}

void gatewayFormatDashboardAbv(float abvPercent, char* out, size_t outSize) {
  formatDashboardAbvText(abvPercent, out, outSize);
}

int16_t gatewayAudioPcmSample(float normalizedSample, float amplitudeScale) {
  float value = normalizedSample * kGatewayAudioPcmAmplitude * amplitudeScale;
  if (value != value) value = 0.0f;
  if (value > 32767.0f) value = 32767.0f;
  if (value < -32768.0f) value = -32768.0f;
  return static_cast<int16_t>(value);
}

GatewayAudioPlaybackStepResult gatewayAudioPlayMergedWindow(
    const GatewayAudioQueueItem& item, GatewayAudioPlaybackSink* sink,
    uint32_t (*nowMs)()) {
  if (!sink || !nowMs || !item.decision.should_play)
    return GatewayAudioPlaybackStepResult::WriteFailed;
  const uint32_t start = nowMs();
  while (static_cast<uint32_t>(nowMs() - start) < 15000UL) {
    const uint16_t durations[] = {item.decision.first_duration_ms,
                                  item.decision.gap_ms,
                                  item.decision.second_duration_ms};
    for (unsigned phase = 0; phase < 3; ++phase) {
      const uint32_t elapsed = nowMs() - start;
      if (elapsed >= 15000UL) break;
      const uint16_t remaining = static_cast<uint16_t>(15000UL - elapsed);
      const uint16_t duration = durations[phase] < remaining
                                    ? durations[phase] : remaining;
      const auto result = phase == 1
          ? sink->waitGap(item, duration)
          : sink->writeTone(item, phase == 0 ? item.decision.first_frequency_hz
                                             : item.decision.second_frequency_hz,
                            duration);
      if (result != GatewayAudioPlaybackStepResult::Completed) {
        sink->writeSilence(item);
        return result;
      }
    }
  }
  sink->writeSilence(item);
  return GatewayAudioPlaybackStepResult::Completed;
}

GatewayAudioPlaybackStepResult gatewayAudioPlayTone(
    const GatewayAudioQueueItem& item, GatewayAudioPlaybackSink* sink) {
  if (sink == nullptr) return GatewayAudioPlaybackStepResult::WriteFailed;
  if (!item.decision.should_play) {
    sink->writeSilence(item);
    return GatewayAudioPlaybackStepResult::Completed;
  }

  GatewayAudioPlaybackPhase phase = gatewayAudioAdvancePlaybackPhase(
      GatewayAudioPlaybackPhase::FirstTone,
      sink->writeTone(item, item.decision.first_frequency_hz,
                      item.decision.first_duration_ms));
  if (phase != GatewayAudioPlaybackPhase::Gap) {
    sink->writeSilence(item);
    return phase == GatewayAudioPlaybackPhase::Cancelled
               ? GatewayAudioPlaybackStepResult::Cancelled
               : GatewayAudioPlaybackStepResult::WriteFailed;
  }

  phase = gatewayAudioAdvancePlaybackPhase(
      phase, sink->waitGap(item, item.decision.gap_ms));
  if (phase != GatewayAudioPlaybackPhase::SecondTone) {
    sink->writeSilence(item);
    return phase == GatewayAudioPlaybackPhase::Cancelled
               ? GatewayAudioPlaybackStepResult::Cancelled
               : GatewayAudioPlaybackStepResult::WriteFailed;
  }

  const GatewayAudioPlaybackStepResult secondResult = sink->writeTone(
      item, item.decision.second_frequency_hz,
      item.decision.second_duration_ms);
  sink->writeSilence(item);
  return secondResult;
}

bool GatewayAudioScheduler::StateLock::tryEnter() {
#if defined(ENABLE_GATEWAY_AUDIO) && defined(ARDUINO_ARCH_ESP32)
  portENTER_CRITICAL(&mux_);
  return true;
#else
  return !flag_.test_and_set(std::memory_order_acquire);
#endif
}

void GatewayAudioScheduler::StateLock::exit() {
#if defined(ENABLE_GATEWAY_AUDIO) && defined(ARDUINO_ARCH_ESP32)
  portEXIT_CRITICAL(&mux_);
#else
  flag_.clear(std::memory_order_release);
#endif
}

bool GatewayAudioScheduler::isValidDeviceId(const char* deviceId) {
  if (deviceId == nullptr || deviceId[0] == '\0') return false;
  size_t length = 0;
  while (length < GATEWAY_AUDIO_DEVICE_ID_CAPACITY &&
         deviceId[length] != '\0') {
    ++length;
  }
  return length > 0 && length < GATEWAY_AUDIO_DEVICE_ID_CAPACITY;
}

bool GatewayAudioScheduler::sameKey(GatewayAlarmType leftType,
                                    const char* leftDevice,
                                    GatewayAlarmType rightType,
                                    const char* rightDevice) {
  return leftType == rightType && leftDevice != nullptr &&
         rightDevice != nullptr && strcmp(leftDevice, rightDevice) == 0;
}

int GatewayAudioScheduler::compareKeys(GatewayAlarmType leftType,
                                       const char* leftDevice,
                                       GatewayAlarmType rightType,
                                       const char* rightDevice) {
  const int typeComparison =
      static_cast<int>(leftType) - static_cast<int>(rightType);
  if (typeComparison != 0) return typeComparison;
  return strcmp(leftDevice, rightDevice);
}

void GatewayAudioScheduler::clearSlot(Slot* slot) {
  if (slot == nullptr) return;
  *slot = Slot{};
}

size_t GatewayAudioScheduler::findCancellationKey(
    GatewayAlarmType type, const char* deviceId) const {
  for (size_t i = 0; i < GATEWAY_AUDIO_CANCEL_KEY_CAPACITY; ++i) {
    const CancellationHighWatermark& entry = _cancelled_keys[i];
    if (entry.occupied && sameKey(entry.type, entry.device_id, type,
                                  deviceId)) {
      return i;
    }
  }
  return GATEWAY_AUDIO_CANCEL_KEY_CAPACITY;
}

bool GatewayAudioScheduler::isClosed(const GatewayAlarmEvent& event) const {
  const size_t index = findCancellationKey(event.type, event.device_id);
  if (index == GATEWAY_AUDIO_CANCEL_KEY_CAPACITY) return false;

  const CancellationHighWatermark& entry = _cancelled_keys[index];
  if (event.condition_token != entry.condition_token_floor) return true;
  return event.event_token <= entry.event_token;
}

bool GatewayAudioScheduler::canRecordCancellation(
    GatewayAlarmType type, const char* deviceId) const {
  if (findCancellationKey(type, deviceId) !=
      GATEWAY_AUDIO_CANCEL_KEY_CAPACITY) {
    return true;
  }
  for (const CancellationHighWatermark& entry : _cancelled_keys) {
    if (!entry.occupied) return true;
  }
  return false;
}

void GatewayAudioScheduler::recordCancellation(const GatewayAlarmEvent& event) {
  size_t index = findCancellationKey(event.type, event.device_id);
  if (index == GATEWAY_AUDIO_CANCEL_KEY_CAPACITY) {
    for (size_t i = 0; i < GATEWAY_AUDIO_CANCEL_KEY_CAPACITY; ++i) {
      if (!_cancelled_keys[i].occupied) {
        index = i;
        _cancelled_keys[index].occupied = true;
        _cancelled_keys[index].type = event.type;
        strncpy(_cancelled_keys[index].device_id, event.device_id,
                sizeof(_cancelled_keys[index].device_id) - 1);
        _cancelled_keys[index]
            .device_id[sizeof(_cancelled_keys[index].device_id) - 1] = '\0';
        _cancelled_keys[index].condition_token_floor = event.condition_token;
        _cancelled_keys[index].event_token = event.event_token;
        return;
      }
    }
    return;
  }

  if (event.condition_token >
      _cancelled_keys[index].condition_token_floor) {
    _cancelled_keys[index].condition_token_floor = event.condition_token;
    _cancelled_keys[index].event_token = event.event_token;
  } else if (event.event_token > _cancelled_keys[index].event_token) {
    _cancelled_keys[index].event_token = event.event_token;
  }
}

bool GatewayAudioScheduler::enqueue(const GatewayAudioQueueItem& item) {
  if (item.event.condition_token == GATEWAY_INVALID_EVENT_TOKEN ||
      item.event.event_token == GATEWAY_INVALID_EVENT_TOKEN ||
      item.decision.type != item.event.type ||
      !isValidDeviceId(item.event.device_id)) {
    return false;
  }

  StateLockGuard lock(_state_lock);
  if (!lock.owns()) return false;
  if (isClosed(item.event)) {
    return false;
  }

  for (Slot& slot : _slots) {
    if (slot.occupied && slot.item.cancelled &&
        (!_active || _active_slot != static_cast<size_t>(&slot - _slots))) {
      clearSlot(&slot);
    }
  }

  for (const Slot& slot : _slots) {
    if (slot.occupied && !slot.item.cancelled &&
        slot.item.event.event_token == item.event.event_token &&
        slot.item.event.repeat_index == item.event.repeat_index) {
      return false;
    }
  }

  for (Slot& slot : _slots) {
    if (!slot.occupied) {
      slot.occupied = true;
      slot.item = item;
      slot.item.cancelled = false;
      return true;
    }
  }
  return false;
}

GatewayAudioCancelResult GatewayAudioScheduler::cancelExactResult(
    const GatewayAlarmEvent& event) {
  if (event.condition_token == GATEWAY_INVALID_EVENT_TOKEN ||
      event.event_token == GATEWAY_INVALID_EVENT_TOKEN ||
      !isValidDeviceId(event.device_id)) {
    return GatewayAudioCancelResult::NotFound;
  }

  StateLockGuard lock(_state_lock);
  if (!lock.owns()) return GatewayAudioCancelResult::Retry;

  if (isClosed(event)) {
    return GatewayAudioCancelResult::NotFound;
  }
  if (!canRecordCancellation(event.type, event.device_id)) {
    return GatewayAudioCancelResult::Retry;
  }

  recordCancellation(event);
  for (Slot& slot : _slots) {
    if (slot.occupied && slot.item.event.type == event.type &&
        strcmp(slot.item.event.device_id, event.device_id) == 0 &&
        slot.item.event.condition_token == event.condition_token &&
        slot.item.event.event_token == event.event_token) {
      slot.item.cancelled = true;
    }
  }
  return GatewayAudioCancelResult::Cancelled;
}

bool GatewayAudioScheduler::cancelExact(const GatewayAlarmEvent& event) {
  return cancelExactResult(event) == GatewayAudioCancelResult::Cancelled;
}

GatewayAudioCancelResult GatewayAudioScheduler::cancelByKeyResult(
    GatewayAlarmType type, const char* deviceId) {
  if (!isValidDeviceId(deviceId)) return GatewayAudioCancelResult::NotFound;

  StateLockGuard lock(_state_lock);
  if (!lock.owns()) return GatewayAudioCancelResult::Retry;
  GatewayAlarmEvent highestEvent{};
  bool found = false;
  for (const Slot& slot : _slots) {
    if (slot.occupied && !slot.item.cancelled &&
        sameKey(slot.item.event.type, slot.item.event.device_id, type,
                deviceId)) {
      if (!found || slot.item.event.condition_token >
                        highestEvent.condition_token ||
          (slot.item.event.condition_token == highestEvent.condition_token &&
           slot.item.event.event_token > highestEvent.event_token)) {
        highestEvent = slot.item.event;
      }
      found = true;
    }
  }
  if (!found) return GatewayAudioCancelResult::NotFound;
  if (!canRecordCancellation(type, deviceId)) {
    return GatewayAudioCancelResult::Retry;
  }

  recordCancellation(highestEvent);
  for (Slot& slot : _slots) {
    if (slot.occupied && !slot.item.cancelled &&
        sameKey(slot.item.event.type, slot.item.event.device_id, type,
                deviceId)) {
      slot.item.cancelled = true;
    }
  }
  return GatewayAudioCancelResult::Cancelled;
}

bool GatewayAudioScheduler::cancelByKey(GatewayAlarmType type,
                                        const char* deviceId) {
  return cancelByKeyResult(type, deviceId) ==
         GatewayAudioCancelResult::Cancelled;
}

GatewayAudioCancelResult GatewayAudioScheduler::resetKeySession(
    GatewayAlarmType type, const char* deviceId,
    GatewayAlarmEventToken conditionToken) {
  if (!isValidDeviceId(deviceId) ||
      conditionToken == GATEWAY_INVALID_EVENT_TOKEN) {
    return GatewayAudioCancelResult::NotFound;
  }

  StateLockGuard lock(_state_lock);
  if (!lock.owns()) return GatewayAudioCancelResult::Retry;

  size_t index = findCancellationKey(type, deviceId);
  if (index == GATEWAY_AUDIO_CANCEL_KEY_CAPACITY) {
    for (size_t i = 0; i < GATEWAY_AUDIO_CANCEL_KEY_CAPACITY; ++i) {
      if (!_cancelled_keys[i].occupied) {
        index = i;
        break;
      }
    }
    if (index == GATEWAY_AUDIO_CANCEL_KEY_CAPACITY) {
      return GatewayAudioCancelResult::Retry;
    }
    _cancelled_keys[index].occupied = true;
    _cancelled_keys[index].type = type;
    strncpy(_cancelled_keys[index].device_id, deviceId,
            sizeof(_cancelled_keys[index].device_id) - 1);
    _cancelled_keys[index]
        .device_id[sizeof(_cancelled_keys[index].device_id) - 1] = '\0';
    _cancelled_keys[index].condition_token_floor = conditionToken;
    _cancelled_keys[index].event_token = GATEWAY_INVALID_EVENT_TOKEN;
  } else if (conditionToken >
             _cancelled_keys[index].condition_token_floor) {
    _cancelled_keys[index].condition_token_floor = conditionToken;
    _cancelled_keys[index].event_token = GATEWAY_INVALID_EVENT_TOKEN;
  } else if (conditionToken <
             _cancelled_keys[index].condition_token_floor) {
    return GatewayAudioCancelResult::NotFound;
  } else {
    return GatewayAudioCancelResult::NotFound;
  }

  for (Slot& slot : _slots) {
    if (slot.occupied &&
        sameKey(slot.item.event.type, slot.item.event.device_id, type,
                deviceId)) {
      slot.item.cancelled = true;
    }
  }
  return GatewayAudioCancelResult::Cancelled;
}

GatewayAudioCancelResult GatewayAudioScheduler::retireExact(
    const GatewayAlarmEvent& event) {
  if (event.condition_token == GATEWAY_INVALID_EVENT_TOKEN ||
      event.event_token == GATEWAY_INVALID_EVENT_TOKEN ||
      !isValidDeviceId(event.device_id)) {
    return GatewayAudioCancelResult::NotFound;
  }

  StateLockGuard lock(_state_lock);
  if (!lock.owns()) return GatewayAudioCancelResult::Retry;

  const size_t index = findCancellationKey(event.type, event.device_id);
  if (index != GATEWAY_AUDIO_CANCEL_KEY_CAPACITY &&
      _cancelled_keys[index].condition_token_floor != event.condition_token) {
    return GatewayAudioCancelResult::NotFound;
  }

  bool cancelled = false;
  for (Slot& slot : _slots) {
    if (slot.occupied &&
        sameKey(slot.item.event.type, slot.item.event.device_id, event.type,
                event.device_id) &&
        slot.item.event.condition_token == event.condition_token) {
      slot.item.cancelled = true;
      cancelled = true;
    }
  }

  if (index == GATEWAY_AUDIO_CANCEL_KEY_CAPACITY && !cancelled) {
    return GatewayAudioCancelResult::NotFound;
  }

  if (index != GATEWAY_AUDIO_CANCEL_KEY_CAPACITY) {
    _cancelled_keys[index] = CancellationHighWatermark{};
  }
  return GatewayAudioCancelResult::Cancelled;
}

bool GatewayAudioScheduler::takeNext(GatewayAudioQueueItem* item) {
  if (item == nullptr) return false;

  StateLockGuard lock(_state_lock);
  if (!lock.owns()) return false;
  if (_active) return false;

  for (Slot& slot : _slots) {
    if (slot.occupied && slot.item.cancelled) clearSlot(&slot);
  }

  size_t firstSlot = GATEWAY_AUDIO_QUEUE_CAPACITY;
  size_t firstAfterLastSlot = GATEWAY_AUDIO_QUEUE_CAPACITY;
  for (size_t i = 0; i < GATEWAY_AUDIO_QUEUE_CAPACITY; ++i) {
    const Slot& slot = _slots[i];
    if (!slot.occupied) continue;

    if (firstSlot == GATEWAY_AUDIO_QUEUE_CAPACITY ||
        compareKeys(slot.item.event.type, slot.item.event.device_id,
                    _slots[firstSlot].item.event.type,
                    _slots[firstSlot].item.event.device_id) < 0) {
      firstSlot = i;
    }

    if (_has_last_key &&
        compareKeys(_last_type, _last_device_id, slot.item.event.type,
                    slot.item.event.device_id) < 0 &&
        (firstAfterLastSlot == GATEWAY_AUDIO_QUEUE_CAPACITY ||
         compareKeys(slot.item.event.type, slot.item.event.device_id,
                     _slots[firstAfterLastSlot].item.event.type,
                     _slots[firstAfterLastSlot].item.event.device_id) < 0)) {
      firstAfterLastSlot = i;
    }
  }

  const size_t selected = firstAfterLastSlot != GATEWAY_AUDIO_QUEUE_CAPACITY
                              ? firstAfterLastSlot
                              : firstSlot;
  if (selected == GATEWAY_AUDIO_QUEUE_CAPACITY) return false;

  _active = true;
  _active_slot = selected;
  *item = _slots[selected].item;
  _last_type = item->event.type;
  strncpy(_last_device_id, item->event.device_id,
          sizeof(_last_device_id) - 1);
  _last_device_id[sizeof(_last_device_id) - 1] = '\0';
  _has_last_key = true;
  return true;
}

void GatewayAudioScheduler::finishActive(
    GatewayAlarmEventToken eventToken) {
  StateLockGuard lock(_state_lock);
  if (!lock.owns()) return;
  if (!_active ||
      _slots[_active_slot].item.event.event_token != eventToken) {
    return;
  }
  clearSlot(&_slots[_active_slot]);
  _active = false;
}

size_t GatewayAudioScheduler::pendingCount() const {
  StateLockGuard lock(_state_lock);
  if (!lock.owns()) return 0;

  size_t count = 0;
  for (size_t i = 0; i < GATEWAY_AUDIO_QUEUE_CAPACITY; ++i) {
    if (_slots[i].occupied && !_slots[i].item.cancelled &&
        (!_active || _active_slot != i)) {
      ++count;
    }
  }
  return count;
}

bool GatewayAudioScheduler::active() const {
  StateLockGuard lock(_state_lock);
  return lock.owns() && _active;
}

bool GatewayAudioScheduler::isCancelled(
    GatewayAlarmEventToken eventToken) const {
  StateLockGuard lock(_state_lock);
  if (!lock.owns()) return true;
  return _active && _slots[_active_slot].occupied &&
         _slots[_active_slot].item.event.event_token == eventToken &&
         _slots[_active_slot].item.cancelled;
}

GatewayAudio gatewayAudio;

#if defined(ENABLE_GATEWAY_AUDIO) && defined(ARDUINO_ARCH_ESP32) && \
    defined(GATEWAY_35)
namespace {

bool setGateway35PowerAmplifierEnabled(TwoWire& wire, bool enabled) {
  uint8_t output = 0;
  uint8_t configuration = 0;

  wire.beginTransmission(gateway35::kTca9554Address);
  wire.write(gateway35::kTca9554OutputRegister);
  if (wire.endTransmission(false) != 0 ||
      wire.requestFrom(static_cast<int>(gateway35::kTca9554Address), 1) != 1 ||
      wire.available() < 1) {
    return false;
  }
  output = static_cast<uint8_t>(wire.read());

  wire.beginTransmission(gateway35::kTca9554Address);
  wire.write(gateway35::kTca9554ConfigRegister);
  if (wire.endTransmission(false) != 0 ||
      wire.requestFrom(static_cast<int>(gateway35::kTca9554Address), 1) != 1 ||
      wire.available() < 1) {
    return false;
  }
  configuration = static_cast<uint8_t>(wire.read());
  configuration &= static_cast<uint8_t>(~gateway35::kPowerAmplifierEnableBit);
  if (enabled) {
    output |= gateway35::kPowerAmplifierEnableBit;
  } else {
    output &= static_cast<uint8_t>(~gateway35::kPowerAmplifierEnableBit);
  }

  wire.beginTransmission(gateway35::kTca9554Address);
  wire.write(gateway35::kTca9554ConfigRegister);
  wire.write(configuration);
  if (wire.endTransmission(true) != 0) return false;

  wire.beginTransmission(gateway35::kTca9554Address);
  wire.write(gateway35::kTca9554OutputRegister);
  wire.write(output);
  return wire.endTransmission(true) == 0;
}

}  // namespace
#endif

#if defined(ENABLE_GATEWAY_AUDIO) && defined(ARDUINO_ARCH_ESP32) && \
    defined(GMT154_06)
namespace {

esp_err_t initializeGmt154AmplifierDisabled(const char** failure_stage) {
  const gpio_num_t pin =
      static_cast<gpio_num_t>(gmt154::kAmplifierControlPin);
  if (failure_stage != nullptr) {
    *failure_stage = "gpio_set_level(preload-disabled)";
  }
  esp_err_t err = gpio_set_level(pin, gmt154::kAmplifierDisabledLevel);
  if (err != ESP_OK) return err;

  if (failure_stage != nullptr) {
    *failure_stage = "gpio_set_direction(output)";
  }
  err = gpio_set_direction(pin, GPIO_MODE_OUTPUT);
  if (err != ESP_OK) {
    (void)gpio_set_level(pin, gmt154::kAmplifierDisabledLevel);
    return err;
  }

  if (failure_stage != nullptr) {
    *failure_stage = "gpio_set_level(confirm-disabled)";
  }
  return gpio_set_level(pin, gmt154::kAmplifierDisabledLevel);
}

bool initializeGmt154AmplifierDisabled() {
  return initializeGmt154AmplifierDisabled(nullptr) == ESP_OK;
}

bool setGmt154PowerAmplifierEnabled(bool enabled) {
  const int level = enabled ? gmt154::kAmplifierEnabledLevel
                            : gmt154::kAmplifierDisabledLevel;
  return gpio_set_level(
             static_cast<gpio_num_t>(gmt154::kAmplifierControlPin), level) ==
         ESP_OK;
}

}  // namespace
#endif

#if defined(GMT154_06) && defined(ARDUINO_ARCH_ESP32) && \
    defined(ENABLE_GATEWAY_AUDIO) && \
    ((defined(GMT154_AUDIO_HW_INIT_BYPASS) && GMT154_AUDIO_HW_INIT_BYPASS && defined(GMT154_AUDIO_GPIO_ONLY_DIAG) && GMT154_AUDIO_GPIO_ONLY_DIAG) || \
     (defined(GMT154_AUDIO_HW_INIT_BYPASS) && GMT154_AUDIO_HW_INIT_BYPASS && defined(GMT154_AUDIO_ALLOC_ONLY_DIAG) && GMT154_AUDIO_ALLOC_ONLY_DIAG) || \
     (defined(GMT154_AUDIO_GPIO_ONLY_DIAG) && GMT154_AUDIO_GPIO_ONLY_DIAG && defined(GMT154_AUDIO_ALLOC_ONLY_DIAG) && GMT154_AUDIO_ALLOC_ONLY_DIAG) || \
     (defined(GMT154_AUDIO_STD_INIT_DIAG) && GMT154_AUDIO_STD_INIT_DIAG && defined(GMT154_AUDIO_HW_INIT_BYPASS) && GMT154_AUDIO_HW_INIT_BYPASS) || \
     (defined(GMT154_AUDIO_STD_INIT_DIAG) && GMT154_AUDIO_STD_INIT_DIAG && defined(GMT154_AUDIO_GPIO_ONLY_DIAG) && GMT154_AUDIO_GPIO_ONLY_DIAG) || \
     (defined(GMT154_AUDIO_STD_INIT_DIAG) && GMT154_AUDIO_STD_INIT_DIAG && defined(GMT154_AUDIO_ALLOC_ONLY_DIAG) && GMT154_AUDIO_ALLOC_ONLY_DIAG) || \
     (defined(GMT154_AUDIO_STD_ENABLE_DIAG) && GMT154_AUDIO_STD_ENABLE_DIAG && defined(GMT154_AUDIO_HW_INIT_BYPASS) && GMT154_AUDIO_HW_INIT_BYPASS) || \
     (defined(GMT154_AUDIO_STD_ENABLE_DIAG) && GMT154_AUDIO_STD_ENABLE_DIAG && defined(GMT154_AUDIO_GPIO_ONLY_DIAG) && GMT154_AUDIO_GPIO_ONLY_DIAG) || \
     (defined(GMT154_AUDIO_STD_ENABLE_DIAG) && GMT154_AUDIO_STD_ENABLE_DIAG && defined(GMT154_AUDIO_ALLOC_ONLY_DIAG) && GMT154_AUDIO_ALLOC_ONLY_DIAG) || \
     (defined(GMT154_AUDIO_STD_ENABLE_DIAG) && GMT154_AUDIO_STD_ENABLE_DIAG && defined(GMT154_AUDIO_STD_INIT_DIAG) && GMT154_AUDIO_STD_INIT_DIAG))
#error "GMT154 audio diagnostic modes are mutually exclusive."
#endif

void GatewayAudio::begin() {
  if (_started) return;
#if defined(GMT154_06) && defined(ARDUINO_ARCH_ESP32) && \
    defined(GMT154_AUDIO_HW_INIT_BYPASS) && GMT154_AUDIO_HW_INIT_BYPASS
  _started = true;
  Log.notice(F("AUD : GMT154 audio hardware initialization bypassed." CR));
  return;
#endif
#if defined(GMT154_06) && defined(ARDUINO_ARCH_ESP32) && \
    defined(ENABLE_GATEWAY_AUDIO) && \
    defined(GMT154_AUDIO_GPIO_ONLY_DIAG) && GMT154_AUDIO_GPIO_ONLY_DIAG
  const bool amplifier_disabled = initializeGmt154AmplifierDisabled();
  Log.notice(F("AUD : GMT154 GPIO-only diagnostic; disable API=%s; pin level not measured." CR),
             amplifier_disabled ? "ok" : "failed");
  _started = true;
  return;
#endif
#if defined(GMT154_06) && defined(ARDUINO_ARCH_ESP32) && \
    defined(ENABLE_GATEWAY_AUDIO) && \
    defined(GMT154_AUDIO_STD_INIT_DIAG) && GMT154_AUDIO_STD_INIT_DIAG
  _started = true;
  _available = false;

  const char* failure_stage = nullptr;
  const esp_err_t disable_err =
      initializeGmt154AmplifierDisabled(&failure_stage);
  if (disable_err != ESP_OK) {
    Log.warning(F("AUD : GMT154 C2b failed; stage=%s esp_err_t=%d; unavailable." CR),
                failure_stage, static_cast<int>(disable_err));
    return;
  }

  const i2s_chan_config_t diagnostic_channel_config =
      I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
  const esp_err_t alloc_err =
      i2s_new_channel(&diagnostic_channel_config, &_tx_handle, nullptr);
  if (alloc_err != ESP_OK) {
    Log.warning(F("AUD : GMT154 C2b failed; stage=i2s_new_channel esp_err_t=%d; unavailable." CR),
                static_cast<int>(alloc_err));
    return;
  }

  i2s_std_config_t diagnostic_std_config = {};
  diagnostic_std_config.clk_cfg =
      I2S_STD_CLK_DEFAULT_CONFIG(gmt154::kAudioSampleRateHz);
  diagnostic_std_config.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;
  diagnostic_std_config.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
      I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);
  diagnostic_std_config.gpio_cfg.mclk =
      static_cast<gpio_num_t>(gmt154::kAudioMclkPin);
  diagnostic_std_config.gpio_cfg.bclk =
      static_cast<gpio_num_t>(gmt154::kAudioBclkPin);
  diagnostic_std_config.gpio_cfg.ws =
      static_cast<gpio_num_t>(gmt154::kAudioWsPin);
  diagnostic_std_config.gpio_cfg.dout =
      static_cast<gpio_num_t>(gmt154::kAudioDoutPin);
  diagnostic_std_config.gpio_cfg.din = I2S_GPIO_UNUSED;
  diagnostic_std_config.gpio_cfg.invert_flags.mclk_inv = false;
  diagnostic_std_config.gpio_cfg.invert_flags.bclk_inv = false;
  diagnostic_std_config.gpio_cfg.invert_flags.ws_inv = false;

  const size_t diagnostic_before_internal_free =
      heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  const size_t diagnostic_before_internal_largest =
      heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  const size_t diagnostic_before_dma_free =
      heap_caps_get_free_size(MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
  const size_t diagnostic_before_dma_largest =
      heap_caps_get_largest_free_block(MALLOC_CAP_DMA | MALLOC_CAP_8BIT);

  char diagnostic_log[320];
  snprintf(diagnostic_log, sizeof(diagnostic_log),
           "AUD : GMT154 C2b heap before init; MCLK may run after std-init on GPIO38 (4.096MHz); internal free=%zu largest=%zu; DMA free=%zu largest=%zu.",
           diagnostic_before_internal_free,
           diagnostic_before_internal_largest,
           diagnostic_before_dma_free,
           diagnostic_before_dma_largest);
  Log.notice(F("%s" CR), diagnostic_log);

  const int64_t init_start_us = esp_timer_get_time();
  const esp_err_t init_err =
      i2s_channel_init_std_mode(_tx_handle, &diagnostic_std_config);
  const int64_t init_duration_us = esp_timer_get_time() - init_start_us;

  const size_t internal_free_after =
      heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  const size_t internal_largest_after =
      heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  const size_t dma_free_after =
      heap_caps_get_free_size(MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
  const size_t dma_largest_after =
      heap_caps_get_largest_free_block(MALLOC_CAP_DMA | MALLOC_CAP_8BIT);

  snprintf(diagnostic_log, sizeof(diagnostic_log),
           "AUD : GMT154 C2b std-init esp_err_t=%d (%s); duration_us=%lld.",
           static_cast<int>(init_err), esp_err_to_name(init_err),
           static_cast<long long>(init_duration_us));
  Log.notice(F("%s" CR), diagnostic_log);

  snprintf(diagnostic_log, sizeof(diagnostic_log),
           "AUD : GMT154 C2b heap internal free=%zu largest=%zu -> free=%zu largest=%zu; DMA free=%zu largest=%zu -> free=%zu largest=%zu.",
           diagnostic_before_internal_free,
           diagnostic_before_internal_largest, internal_free_after,
           internal_largest_after, diagnostic_before_dma_free,
           diagnostic_before_dma_largest, dma_free_after,
           dma_largest_after);
  Log.notice(F("%s" CR), diagnostic_log);
  Log.notice(F("AUD : GMT154 C2b; I2S channel remains disabled; MCLK may be running on GPIO38; codec skipped; amp enable skipped; task skipped; playback skipped; unavailable." CR));
  return;
#endif
#if defined(GMT154_06) && defined(ARDUINO_ARCH_ESP32) && \
    defined(ENABLE_GATEWAY_AUDIO) && \
    defined(GMT154_AUDIO_STD_ENABLE_DIAG) && GMT154_AUDIO_STD_ENABLE_DIAG
  _started = true;
  _available = false;

  const char* failure_stage = nullptr;
  const esp_err_t disable_err =
      initializeGmt154AmplifierDisabled(&failure_stage);
  if (disable_err != ESP_OK) {
    Log.warning(F("AUD : GMT154 C2c failed; stage=%s esp_err_t=%d; unavailable." CR),
                failure_stage, static_cast<int>(disable_err));
    return;
  }

  const i2s_chan_config_t diagnostic_channel_config =
      I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
  const esp_err_t alloc_err =
      i2s_new_channel(&diagnostic_channel_config, &_tx_handle, nullptr);
  if (alloc_err != ESP_OK) {
    Log.warning(F("AUD : GMT154 C2c failed; stage=i2s_new_channel esp_err_t=%d; unavailable." CR),
                static_cast<int>(alloc_err));
    return;
  }

  i2s_std_config_t diagnostic_std_config = {};
  diagnostic_std_config.clk_cfg =
      I2S_STD_CLK_DEFAULT_CONFIG(gmt154::kAudioSampleRateHz);
  diagnostic_std_config.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;
  diagnostic_std_config.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
      I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);
  diagnostic_std_config.gpio_cfg.mclk =
      static_cast<gpio_num_t>(gmt154::kAudioMclkPin);
  diagnostic_std_config.gpio_cfg.bclk =
      static_cast<gpio_num_t>(gmt154::kAudioBclkPin);
  diagnostic_std_config.gpio_cfg.ws =
      static_cast<gpio_num_t>(gmt154::kAudioWsPin);
  diagnostic_std_config.gpio_cfg.dout =
      static_cast<gpio_num_t>(gmt154::kAudioDoutPin);
  diagnostic_std_config.gpio_cfg.din = I2S_GPIO_UNUSED;
  diagnostic_std_config.gpio_cfg.invert_flags.mclk_inv = false;
  diagnostic_std_config.gpio_cfg.invert_flags.bclk_inv = false;
  diagnostic_std_config.gpio_cfg.invert_flags.ws_inv = false;

  const size_t diagnostic_before_internal_free =
      heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  const size_t diagnostic_before_internal_largest =
      heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  const size_t diagnostic_before_dma_free =
      heap_caps_get_free_size(MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
  const size_t diagnostic_before_dma_largest =
      heap_caps_get_largest_free_block(MALLOC_CAP_DMA | MALLOC_CAP_8BIT);

  char diagnostic_log[384];
  snprintf(diagnostic_log, sizeof(diagnostic_log),
           "AUD : GMT154 C2c heap before init; MCLK may run after std-init on GPIO38 (4.096MHz); internal free=%zu largest=%zu; DMA free=%zu largest=%zu.",
           diagnostic_before_internal_free,
           diagnostic_before_internal_largest,
           diagnostic_before_dma_free,
           diagnostic_before_dma_largest);
  Log.notice(F("%s" CR), diagnostic_log);

  const int64_t init_start_us = esp_timer_get_time();
  const esp_err_t init_err =
      i2s_channel_init_std_mode(_tx_handle, &diagnostic_std_config);
  const int64_t init_duration_us = esp_timer_get_time() - init_start_us;
  if (init_err != ESP_OK) {
    snprintf(diagnostic_log, sizeof(diagnostic_log),
             "AUD : GMT154 C2c std-init esp_err_t=%d (%s); duration_us=%lld; unavailable.",
             static_cast<int>(init_err), esp_err_to_name(init_err),
             static_cast<long long>(init_duration_us));
    Log.warning(F("%s" CR), diagnostic_log);
    return;
  }

  const size_t diagnostic_internal_free_before_enable =
      heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  const size_t diagnostic_internal_largest_before_enable =
      heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  const size_t diagnostic_dma_free_before_enable =
      heap_caps_get_free_size(MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
  const size_t diagnostic_dma_largest_before_enable =
      heap_caps_get_largest_free_block(MALLOC_CAP_DMA | MALLOC_CAP_8BIT);

  snprintf(diagnostic_log, sizeof(diagnostic_log),
           "AUD : GMT154 C2c pre-enable; std-init esp_err_t=%d (%s); duration_us=%lld; internal free=%zu largest=%zu; DMA free=%zu largest=%zu.",
           static_cast<int>(init_err), esp_err_to_name(init_err),
           static_cast<long long>(init_duration_us),
           diagnostic_internal_free_before_enable,
           diagnostic_internal_largest_before_enable,
           diagnostic_dma_free_before_enable,
           diagnostic_dma_largest_before_enable);
  Log.notice(F("%s" CR), diagnostic_log);

  const int64_t enable_start_us = esp_timer_get_time();
  const esp_err_t enable_err = i2s_channel_enable(_tx_handle);
  const int64_t enable_duration_us = esp_timer_get_time() - enable_start_us;

  const size_t diagnostic_internal_free_after_enable =
      heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  const size_t diagnostic_internal_largest_after_enable =
      heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  const size_t diagnostic_dma_free_after_enable =
      heap_caps_get_free_size(MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
  const size_t diagnostic_dma_largest_after_enable =
      heap_caps_get_largest_free_block(MALLOC_CAP_DMA | MALLOC_CAP_8BIT);

  snprintf(diagnostic_log, sizeof(diagnostic_log),
           "AUD : GMT154 C2c post-enable esp_err_t=%d (%s); enable_duration_us=%lld; internal free=%zu largest=%zu -> free=%zu largest=%zu; DMA free=%zu largest=%zu -> free=%zu largest=%zu.",
           static_cast<int>(enable_err), esp_err_to_name(enable_err),
           static_cast<long long>(enable_duration_us),
           diagnostic_internal_free_before_enable,
           diagnostic_internal_largest_before_enable,
           diagnostic_internal_free_after_enable,
           diagnostic_internal_largest_after_enable,
           diagnostic_dma_free_before_enable,
           diagnostic_dma_largest_before_enable,
           diagnostic_dma_free_after_enable,
           diagnostic_dma_largest_after_enable);
  Log.notice(F("%s" CR), diagnostic_log);
  if (enable_err != ESP_OK) {
    Log.warning(F("AUD : GMT154 C2c channel enable failed; unavailable." CR));
    return;
  }

  Log.notice(F("AUD : GMT154 C2c channel enabled; codec skipped; amp enable skipped; task skipped; playback skipped; unavailable." CR));
  return;
#endif
#if defined(GMT154_06) && defined(ARDUINO_ARCH_ESP32) && \
    defined(ENABLE_GATEWAY_AUDIO) && \
    defined(GMT154_AUDIO_ALLOC_ONLY_DIAG) && GMT154_AUDIO_ALLOC_ONLY_DIAG
  _started = true;
  _available = false;

  const char* failure_stage = nullptr;
  const esp_err_t disable_err =
      initializeGmt154AmplifierDisabled(&failure_stage);
  if (disable_err != ESP_OK) {
    Log.warning(F("AUD : GMT154 allocated-only diagnostic failed; stage=%s esp_err_t=%d; unavailable." CR),
                failure_stage, static_cast<int>(disable_err));
    return;
  }

  const i2s_chan_config_t diagnostic_channel_config =
      I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
  const esp_err_t alloc_err =
      i2s_new_channel(&diagnostic_channel_config, &_tx_handle, nullptr);
  if (alloc_err != ESP_OK) {
    Log.warning(F("AUD : GMT154 allocated-only diagnostic failed; stage=i2s_new_channel esp_err_t=%d; unavailable." CR),
                static_cast<int>(alloc_err));
    return;
  }

  Log.notice(F("AUD : GMT154 allocated-only; clocks-not-started; codec skipped; amp enable skipped; task skipped; unavailable." CR));
  return;
#endif
#if !defined(ENABLE_GATEWAY_AUDIO) || !defined(ARDUINO_ARCH_ESP32) || \
    !defined(GMT154_06)
  _started = true;
#endif

#if defined(ENABLE_GATEWAY_AUDIO) && defined(ARDUINO_ARCH_ESP32)
#if defined(GMT154_06)
  if (!initializeGmt154AmplifierDisabled()) {
    Log.warning(F("AUD : Unable to configure GMT154 NS4150B disabled." CR));
    releaseRuntime();
    return;
  }
#endif

  const i2s_chan_config_t channel_config =
      I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
  if (i2s_new_channel(&channel_config, &_tx_handle, nullptr) != ESP_OK) {
    Log.warning(F("AUD : Unable to allocate I2S TX channel." CR));
    releaseRuntime();
    return;
  }

  i2s_std_config_t std_config = {};
  std_config.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(16000);
  std_config.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
      I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);
#if defined(GATEWAY_35)
  std_config.clk_cfg =
      I2S_STD_CLK_DEFAULT_CONFIG(gateway35::kAudioSampleRateHz);
  std_config.gpio_cfg.mclk =
      static_cast<gpio_num_t>(gateway35::kAudioMclkPin);
  std_config.gpio_cfg.bclk =
      static_cast<gpio_num_t>(gateway35::kAudioBclkPin);
  std_config.gpio_cfg.ws =
      static_cast<gpio_num_t>(gateway35::kAudioLrckPin);
  std_config.gpio_cfg.dout =
      static_cast<gpio_num_t>(gateway35::kAudioDoutPin);
#elif defined(GMT154_06)
  std_config.clk_cfg =
      I2S_STD_CLK_DEFAULT_CONFIG(gmt154::kAudioSampleRateHz);
  std_config.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;
  std_config.gpio_cfg.mclk =
      static_cast<gpio_num_t>(gmt154::kAudioMclkPin);
  std_config.gpio_cfg.bclk =
      static_cast<gpio_num_t>(gmt154::kAudioBclkPin);
  std_config.gpio_cfg.ws = static_cast<gpio_num_t>(gmt154::kAudioWsPin);
  std_config.gpio_cfg.dout =
      static_cast<gpio_num_t>(gmt154::kAudioDoutPin);
#else
  std_config.gpio_cfg.mclk = I2S_GPIO_UNUSED;
  std_config.gpio_cfg.bclk = GPIO_NUM_48;
  std_config.gpio_cfg.ws = GPIO_NUM_38;
  std_config.gpio_cfg.dout = GPIO_NUM_47;
#endif
  std_config.gpio_cfg.din = I2S_GPIO_UNUSED;
  std_config.gpio_cfg.invert_flags.mclk_inv = false;
  std_config.gpio_cfg.invert_flags.bclk_inv = false;
  std_config.gpio_cfg.invert_flags.ws_inv = false;

  const bool i2s_ready =
      i2s_channel_init_std_mode(_tx_handle, &std_config) == ESP_OK &&
      i2s_channel_enable(_tx_handle) == ESP_OK;
  if (!i2s_ready) {
#if defined(GATEWAY_35)
    Log.warning(F("AUD : Unable to initialize ES8311 I2S output." CR));
#elif defined(GMT154_06)
    Log.warning(F("AUD : Unable to initialize GMT154 ES8311 I2S output." CR));
#else
    Log.warning(F("AUD : Unable to initialize PCM5101 I2S output." CR));
#endif
    releaseRuntime();
    return;
  }

#if defined(GATEWAY_35)
  const bool codec_ready = _codec.begin();
#elif defined(GMT154_06)
  const bool codec_ready = _gmt154_codec.begin();
#else
  const bool codec_ready = true;
#endif
  if (!codec_ready) {
    Log.warning(F("AUD : Unable to initialize ES8311 codec." CR));
    releaseRuntime();
    return;
  }

#if defined(GATEWAY_35)
  const bool amplifier_ready = setGateway35PowerAmplifierEnabled(Wire, true);
  if (!amplifier_ready) {
    Log.warning(F("AUD : Unable to enable NS4150B through TCA9554 EXIO7." CR));
    releaseRuntime();
    return;
  }
  _power_amplifier_enabled = true;
#elif defined(GMT154_06)
  const bool amplifier_ready = setGmt154PowerAmplifierEnabled(true);
  if (!amplifier_ready) {
    Log.warning(F("AUD : Unable to enable GMT154 NS4150B." CR));
    releaseRuntime();
    return;
  }
  _power_amplifier_enabled = true;
#else
  const bool amplifier_ready = true;
#endif

  const bool task_ready =
      xTaskCreatePinnedToCore(audioTaskEntry, "GWAudio", 4096, this, 1, &_task,
                              0) == pdPASS &&
      _task != nullptr;
#if defined(GATEWAY_35)
  if (!gateway35AudioStartupReady(i2s_ready, codec_ready, amplifier_ready,
                                  task_ready)) {
#else
  if (!i2s_ready || !codec_ready || !amplifier_ready || !task_ready) {
#endif
    Log.warning(F("AUD : Unable to start audio task." CR));
    releaseRuntime();
    return;
  }

  _available = true;
#if defined(ENABLE_GATEWAY_AUDIO) && defined(ARDUINO_ARCH_ESP32) && \
    defined(GMT154_06)
  _started = true;
#endif
#if defined(GATEWAY_35)
  Log.notice(F("AUD : ES8311 I2S audio enabled on GPIO12/13/15/16." CR));
#elif defined(GMT154_06)
  Log.notice(F("AUD : GMT154 ES8311 playback enabled; I2S GPIO4/7/16/38." CR));
#else
  Log.notice(F("AUD : I2S audio enabled on GPIO38/47/48." CR));
#endif
#endif
}

bool GatewayAudio::queueAlarm(const GatewayAlarmEvent& event) {
#if defined(ENABLE_GATEWAY_AUDIO) && defined(ARDUINO_ARCH_ESP32)
  if (!_available) return false;
#if defined(GMT154_06)
  // GMT uses one gateway-wide audible session, not per-event queue entries.
  (void)event;
  return true;
#else
  GatewayAudioQueueItem item{};
  item.decision = gatewayAudioDecisionForAlarm(event.type);
  item.event = event;
  if (!_scheduler.enqueue(item)) {
    Log.warning(F("AUD : Audio enqueue rejected; token=%x:%x type=%d "
                  "device=%s." CR),
                static_cast<int>(static_cast<uint32_t>(event.event_token >> 32)),
                static_cast<int>(static_cast<uint32_t>(event.event_token)),
                static_cast<int>(event.type), event.device_id);
    traceAlarmEvent("queue-rejected dev=%s tok=%lx:%lx", event.device_id,
                    (unsigned long)(uint32_t)(event.event_token >> 32),
                    (unsigned long)(uint32_t)(event.event_token & 0xffffffffUL));
    return false;
  }
  Log.notice(F("AUD : queued tok=%x:%x rep=%d pending=%d." CR),
             static_cast<int>(static_cast<uint32_t>(event.event_token >> 32)),
             static_cast<int>(static_cast<uint32_t>(event.event_token)),
             (int)event.repeat_index, (int)_scheduler.pendingCount());
  traceAlarmEvent("queued dev=%s tok=%lx:%lx rep=%d", event.device_id,
                  (unsigned long)(uint32_t)(event.event_token >> 32),
                  (unsigned long)(uint32_t)(event.event_token & 0xffffffffUL),
                  (int)event.repeat_index);
  return true;
#endif
#else
  (void)event;
  return false;
#endif
}

bool GatewayAudio::cancelExact(const GatewayAlarmEvent& event) {
  return cancelExactResult(event) == GatewayAudioCancelResult::Cancelled;
}

GatewayAudioCancelResult GatewayAudio::cancelExactResult(
    const GatewayAlarmEvent& event) {
#if defined(ENABLE_GATEWAY_AUDIO) && defined(ARDUINO_ARCH_ESP32)
  if (!_available) return GatewayAudioCancelResult::NotFound;
  const GatewayAudioCancelResult result =
      _scheduler.cancelExactResult(event);
  // Silence the I2S DMA ring immediately: its content keeps cycling when no
  // new data arrives, so the tail of the last tone would otherwise keep
  // sounding after the cancel.
#if !defined(GMT154_06)
  writeSilence();
#endif
  return result;
#else
  (void)event;
  return GatewayAudioCancelResult::NotFound;
#endif
}

GatewayAudioCancelResult GatewayAudio::cancelByKeyResult(
    GatewayAlarmType type, const char* deviceId) {
#if defined(ENABLE_GATEWAY_AUDIO) && defined(ARDUINO_ARCH_ESP32)
  if (!_available) return GatewayAudioCancelResult::NotFound;
  const GatewayAudioCancelResult result =
      _scheduler.cancelByKeyResult(type, deviceId);
#if !defined(GMT154_06)
  writeSilence();
#endif
  return result;
#else
  (void)type;
  (void)deviceId;
  return GatewayAudioCancelResult::NotFound;
#endif
}

bool GatewayAudio::cancelByKey(GatewayAlarmType type, const char* deviceId) {
  return cancelByKeyResult(type, deviceId) ==
         GatewayAudioCancelResult::Cancelled;
}

GatewayAudioCancelResult GatewayAudio::resetKeySession(
    GatewayAlarmType type, const char* deviceId,
    GatewayAlarmEventToken conditionToken) {
  return _scheduler.resetKeySession(type, deviceId, conditionToken);
}

GatewayAudioCancelResult GatewayAudio::retireExact(
    const GatewayAlarmEvent& event) {
  return _scheduler.retireExact(event);
}

size_t GatewayAudio::pendingCount() const {
  return _scheduler.pendingCount();
}

bool GatewayAudio::isActive() const {
#if defined(GMT154_06) && defined(ENABLE_GATEWAY_AUDIO) && defined(ARDUINO_ARCH_ESP32)
  return _mergedPlaying.load(std::memory_order_acquire);
#else
  return _scheduler.active();
#endif
}

#if defined(ENABLE_GATEWAY_AUDIO) && defined(ARDUINO_ARCH_ESP32)
void GatewayAudio::audioTaskEntry(void* parameter) {
  static_cast<GatewayAudio*>(parameter)->audioTaskLoop();
}

void GatewayAudio::audioTaskLoop() {
#if defined(GMT154_06)
  Gmt154MergedAudioCycle cycle;
  GatewayAudibleSummary summary{};
  for (;;) {
    if (!_available) { vTaskDelay(pdMS_TO_TICKS(10)); continue; }
    GatewayAudibleSummary current{};
    if (gatewayAlarmTracker.tryCopyAudibleSummary(&current)) summary = current;
    if (cycle.update(summary.any, millis())) {
      GatewayAudioQueueItem item{};
      item.decision = gatewayAudioDecisionForAlarm(
          summary.temperature ? GatewayAlarmType::TemperatureChange
                              : GatewayAlarmType::Offline);
      Log.notice(F("AUD : merged tone start." CR));
      _mergedPlaying.store(true, std::memory_order_release);
      const auto result = playMergedTone(item);
      _mergedPlaying.store(false, std::memory_order_release);
      if (result == GatewayAudioPlaybackStepResult::Cancelled)
        cycle.playbackCancelled();
      if (result == GatewayAudioPlaybackStepResult::WriteFailed) {
        Log.warning(F("AUD : merged tone stopped: I2S write failed." CR));
      } else {
        Log.notice(F("AUD : merged tone done, result=%d." CR),
                   static_cast<int>(result));
      }
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
#else
  GatewayAudioQueueItem item{};
  uint32_t idleIterations = 0;
  for (;;) {
    if (_scheduler.takeNext(&item)) {
      idleIterations = 0;
      char cond[24], tok[24];
      snprintf(cond, sizeof(cond), "%08lx:%08lx",
               (unsigned long)(uint32_t)(item.event.condition_token >> 32),
               (unsigned long)(uint32_t)(item.event.condition_token &
                                         0xffffffffUL));
      snprintf(tok, sizeof(tok), "%08lx:%08lx",
               (unsigned long)(uint32_t)(item.event.event_token >> 32),
               (unsigned long)(uint32_t)(item.event.event_token & 0xffffffffUL));
      Log.notice(F("AUD : tone start type=%d dev=%s rep=%d cond=%s tok=%s "
                   "d1=%d g=%d d2=%d." CR),
                 (int)item.event.type, item.event.device_id,
                 (int)item.event.repeat_index, cond, tok,
                 (int)item.decision.first_duration_ms,
                 (int)item.decision.gap_ms,
                 (int)item.decision.second_duration_ms);
      traceAlarmEvent("tone-start dev=%s rep=%d tok=%s", item.event.device_id,
                      (int)item.event.repeat_index, tok);
      playTone(item);
      _scheduler.finishActive(item.event.event_token);
      Log.notice(F("AUD : tone done tok=%s." CR), tok);
      traceAlarmEvent("tone-done dev=%s tok=%s", item.event.device_id, tok);
    } else {
      if (++idleIterations >= 5000U) {
        idleIterations = 0;
        Log.notice(F("AUD : idle (queue empty)." CR));
      }
      vTaskDelay(pdMS_TO_TICKS(1));
    }
  }
#endif
}

bool GatewayAudio::isActiveCancelled(
    const GatewayAudioQueueItem& item) const {
#if defined(GMT154_06)
  (void)item;
  GatewayAudibleSummary summary{};
  return gatewayAlarmTracker.tryCopyAudibleSummary(&summary) && !summary.any;
#else
  return _scheduler.isCancelled(item.event.event_token);
#endif
}

class GatewayAudio::I2sPlaybackSink : public GatewayAudioPlaybackSink {
 public:
  I2sPlaybackSink(GatewayAudio& audio, const GatewayAudioQueueItem& item)
      : audio_(audio), item_(item) {}

  GatewayAudioPlaybackStepResult writeTone(
      const GatewayAudioQueueItem&, uint16_t frequencyHz,
      uint16_t durationMs) override {
    return audio_.playSingleTone(item_, frequencyHz, durationMs);
  }

  GatewayAudioPlaybackStepResult waitGap(const GatewayAudioQueueItem&,
                                         uint16_t durationMs) override {
    if (audio_.isActiveCancelled(item_)) {
      return GatewayAudioPlaybackStepResult::Cancelled;
    }
    // Stream silence through the gap: a plain delay would leave the previous
    // tone's tail looping in the DMA ring between the two bursts.
    constexpr uint32_t kSampleRate = 16000;
    constexpr size_t kFramesPerChunk = 128;
    size_t remainingFrames =
        (kSampleRate * static_cast<size_t>(durationMs)) / 1000;
    while (remainingFrames > 0) {
      if (audio_.isActiveCancelled(item_)) {
        return GatewayAudioPlaybackStepResult::Cancelled;
      }
      if (!audio_.writeSilenceChunk()) {
        return GatewayAudioPlaybackStepResult::WriteFailed;
      }
      remainingFrames -= remainingFrames > kFramesPerChunk
                             ? kFramesPerChunk
                             : remainingFrames;
    }
    return audio_.isActiveCancelled(item_)
               ? GatewayAudioPlaybackStepResult::Cancelled
               : GatewayAudioPlaybackStepResult::Completed;
  }

  void writeSilence(const GatewayAudioQueueItem&) override {
    audio_.writeSilence();
  }

 private:
  GatewayAudio& audio_;
  const GatewayAudioQueueItem& item_;
};

void GatewayAudio::playTone(const GatewayAudioQueueItem& item) {
  if (!gatewayAudioShouldEnterPlaybackControl(_available,
                                              _tx_handle != nullptr,
                                              item.decision.should_play)) {
    return;
  }

  I2sPlaybackSink sink(*this, item);
  (void)gatewayAudioPlayTone(item, &sink);
}

#if defined(GMT154_06)
GatewayAudioPlaybackStepResult GatewayAudio::playMergedTone(const GatewayAudioQueueItem& item) {
  if (!_available || _tx_handle == nullptr)
    return GatewayAudioPlaybackStepResult::WriteFailed;
  I2sPlaybackSink sink(*this, item);
  return gatewayAudioPlayMergedWindow(item, &sink, []() -> uint32_t {
    return millis();
  });
}
#endif

GatewayAudioPlaybackStepResult GatewayAudio::playSingleTone(
    const GatewayAudioQueueItem& item, uint16_t frequencyHz,
    uint16_t durationMs) {
  if (frequencyHz == 0 || durationMs == 0) {
    return GatewayAudioPlaybackStepResult::Completed;
  }
  if (isActiveCancelled(item)) {
    return GatewayAudioPlaybackStepResult::Cancelled;
  }

  constexpr uint32_t kSampleRate = 16000;
  constexpr size_t kFramesPerChunk = 128;
  int16_t samples[kFramesPerChunk * 2] = {0};
  const size_t totalFrames =
      (kSampleRate * static_cast<size_t>(durationMs)) / 1000;
  if (totalFrames == 0) {
    return GatewayAudioPlaybackStepResult::Completed;
  }
  const float phaseStep =
      2.0f * static_cast<float>(M_PI) * frequencyHz / kSampleRate;
  float phase = 0.0f;
  size_t remaining = totalFrames;
  GatewayAudioPlaybackStepResult result =
      GatewayAudioPlaybackStepResult::Completed;

  while (remaining > 0) {
    if (isActiveCancelled(item)) {
      Log.notice(F("AUD : playback cancelled mid-tone tok=%u:%u." CR),
                 static_cast<uint64_t>(item.event.event_token >> 32),
                 static_cast<uint64_t>(item.event.event_token & 0xffffffffUL));
      result = GatewayAudioPlaybackStepResult::Cancelled;
      break;
    }
    const size_t framesThisChunk =
        remaining > kFramesPerChunk ? kFramesPerChunk : remaining;
    for (size_t i = 0; i < framesThisChunk; ++i) {
      const int16_t sample = gatewayAudioPcmSample(
          sinf(phase), item.decision.amplitude_scale);
      samples[i * 2] = sample;
      samples[i * 2 + 1] = sample;
      phase += phaseStep;
      if (phase > 2.0f * static_cast<float>(M_PI)) {
        phase -= 2.0f * static_cast<float>(M_PI);
      }
    }

    size_t written = 0;
    const size_t expectedBytes = framesThisChunk * 2 * sizeof(int16_t);
    const esp_err_t err = i2s_channel_write(
        _tx_handle, samples, expectedBytes, &written, pdMS_TO_TICKS(50));
    if (err != ESP_OK || written != expectedBytes) {
      Log.warning(F("AUD : I2S write failed; alarm sound stopped." CR));
      result = GatewayAudioPlaybackStepResult::WriteFailed;
      break;
    }
    remaining -= framesThisChunk;
  }

  return result;
}

bool GatewayAudio::writeSilenceChunk() {
  int16_t samples[128 * 2] = {0};
  size_t written = 0;
  const esp_err_t result = i2s_channel_write(
      _tx_handle, samples, sizeof(samples), &written, pdMS_TO_TICKS(50));
  return result == ESP_OK && written == sizeof(samples);
}

void GatewayAudio::writeSilence() {
  // The I2S DMA ring (default 6 x 1024 bytes) keeps cycling its content while
  // no new data is supplied, so the tail of the last tone would otherwise be
  // replayed indefinitely. Flush the whole ring with silence after every tone
  // and on every cancel.
  constexpr size_t kSilenceChunks = 16;  // 16 x 512 = 8192 bytes > 6 x 1024
  for (size_t i = 0; i < kSilenceChunks; ++i) {
    writeSilenceChunk();
  }
}

void GatewayAudio::releaseRuntime() {
  _available = false;

#if defined(GATEWAY_35)
  if (_power_amplifier_enabled) {
    (void)setGateway35PowerAmplifierEnabled(Wire, false);
    _power_amplifier_enabled = false;
  }
  _codec.end();
#elif defined(GMT154_06)
  (void)setGmt154PowerAmplifierEnabled(false);
  _power_amplifier_enabled = false;
  _gmt154_codec.end();
#endif

  if (_tx_handle != nullptr) {
    i2s_channel_disable(_tx_handle);
    i2s_del_channel(_tx_handle);
    _tx_handle = nullptr;
  }
}
#endif
