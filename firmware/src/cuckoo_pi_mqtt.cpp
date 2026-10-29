#if defined(GATEWAY)

#include "cuckoo_pi_mqtt.hpp"

#include "cuckoo_pi_mqtt_policy.hpp"

#include <Arduino.h>
#include <cstring>

CuckooPiMqttService* CuckooPiMqttService::active_instance_ = nullptr;
CuckooPiMqttService cuckooPiMqttService;

CuckooPiMqttService::CuckooPiMqttService()
    : client_(1536, 256),
      config_(defaultCuckooPiConfig()),
      devices_{},
      diagnostics_{},
      mutex_(nullptr),
      client_id_{},
      begun_(false) {}

void CuckooPiMqttService::lock() const {
  if (mutex_) xSemaphoreTake(mutex_, portMAX_DELAY);
}

void CuckooPiMqttService::unlock() const {
  if (mutex_) xSemaphoreGive(mutex_);
}

void CuckooPiMqttService::begin() {
  if (begun_) return;
  mutex_ = xSemaphoreCreateMutex();
  const uint64_t chip_id = ESP.getEfuseMac();
  std::snprintf(client_id_, sizeof(client_id_), "gravitymon-cuckoo-%08lx",
                static_cast<unsigned long>(chip_id & 0xffffffffULL));
  active_instance_ = this;
  client_.onMessageAdvanced(&CuckooPiMqttService::onMessage);
  client_.setOptions(30, true, 1000);
  client_.dropOverflow(true);
  begun_ = true;
}

void CuckooPiMqttService::applyConfig(const CuckooPiConfig& config) {
  if (!begun_) begin();
  lock();
  if (cuckooPiSubscriptionConfigEqual(config_, config)) {
    config_ = config;
    unlock();
    return;
  }

  CuckooPiRuntimeDevice next_devices[kMaxCuckooPiDevices]{};
  for (uint8_t next = 0; next < config.device_count &&
                         next < kMaxCuckooPiDevices;
       ++next) {
    for (uint8_t current = 0; current < config_.device_count &&
                            current < kMaxCuckooPiDevices;
         ++current) {
      if (std::strcmp(config.devices[next].id, config_.devices[current].id) ==
          0) {
        next_devices[next] = devices_[current];
        break;
      }
    }
  }
  std::memcpy(devices_, next_devices, sizeof(devices_));
  config_ = config;
  diagnostics_.connected = false;
  diagnostics_.failure_count = 0;
  diagnostics_.next_attempt_ms = 0;
  unlock();

  if (client_.connected()) client_.disconnect();
  client_.begin(config.broker, config.port, network_);
}

void CuckooPiMqttService::setConnected(bool connected) {
  lock();
  diagnostics_.connected = connected;
  unlock();
}

void CuckooPiMqttService::scheduleFailure(uint32_t now_ms) {
  lock();
  diagnostics_.connected = false;
  diagnostics_.last_error = static_cast<int>(client_.lastError());
  diagnostics_.next_attempt_ms =
      now_ms + cuckooPiReconnectDelayMs(diagnostics_.failure_count);
  if (diagnostics_.failure_count < 255) ++diagnostics_.failure_count;
  unlock();
}

void CuckooPiMqttService::tick(uint32_t now_ms, bool wifi_connected) {
  if (!begun_) begin();
  CuckooPiConfig config{};
  CuckooPiMqttDiagnostics diagnostics{};
  lock();
  config = config_;
  diagnostics = diagnostics_;
  unlock();

  if (!config.enabled || !wifi_connected || config.device_count == 0) {
    if (client_.connected()) client_.disconnect();
    lock();
    diagnostics_.connected = false;
    diagnostics_.failure_count = 0;
    diagnostics_.next_attempt_ms = now_ms;
    unlock();
    return;
  }

  if (client_.connected()) {
    client_.loop();
    if (client_.connected()) {
      lock();
      diagnostics_.connected = true;
      diagnostics_.failure_count = 0;
      diagnostics_.dropped_message_count = client_.droppedMessages();
      unlock();
      return;
    }
    scheduleFailure(now_ms);
    return;
  }

  if (!cuckooPiShouldConnect(config.enabled, wifi_connected,
                             config.device_count, now_ms,
                             diagnostics.next_attempt_ms)) {
    return;
  }

  lock();
  ++diagnostics_.connect_attempt_count;
  unlock();
  if (!client_.connect(config.client_id[0] ? config.client_id : client_id_,
                       config.username[0] ? config.username : nullptr,
                       config.username[0] ? config.password : nullptr)) {
    scheduleFailure(now_ms);
    return;
  }

  bool subscribed = true;
  for (uint8_t index = 0; index < config.device_count &&
                          index < kMaxCuckooPiDevices;
       ++index) {
    CuckooPiTopics topics{};
    if (!buildCuckooPiTopics(config.devices[index].id, &topics) ||
        !client_.subscribe(topics.status, 0)) {
      subscribed = false;
      break;
    }
  }
  if (!subscribed) {
    client_.disconnect();
    scheduleFailure(now_ms);
    return;
  }

  lock();
  diagnostics_.connected = true;
  diagnostics_.failure_count = 0;
  diagnostics_.next_attempt_ms = now_ms;
  ++diagnostics_.successful_connection_count;
  diagnostics_.last_error = 0;
  unlock();
}

void CuckooPiMqttService::onMessage(MQTTClient*, char topic[], char bytes[],
                                    int length) {
  if (!active_instance_) return;
  active_instance_->handleMessage(topic, bytes, length, millis());
}

void CuckooPiMqttService::handleMessage(const char* topic, const char* bytes,
                                        int length, uint32_t now_ms) {
  CuckooPiConfig config{};
  lock();
  config = config_;
  unlock();
  const int index = cuckooPiFindStatusTopicDeviceIndex(config, topic);
  if (index < 0 || index >= static_cast<int>(kMaxCuckooPiDevices)) return;

  CuckooPiSnapshot parsed{};
  const bool valid = bytes && length > 0 && length <= 1536 &&
                     parseCuckooPiSnapshot(config.devices[index].id, bytes,
                                           static_cast<size_t>(length),
                                           &parsed);
  lock();
  if (valid) {
    devices_[index].snapshot = parsed;
    devices_[index].has_snapshot = true;
    devices_[index].last_seen_ms = now_ms;
  } else {
    ++devices_[index].parse_error_count;
  }
  unlock();
}

bool CuckooPiMqttService::copyDeviceRuntime(
    const char* device_id, CuckooPiRuntimeDevice* output) const {
  if (!output) return false;
  char normalized[13] = {};
  if (!normalizeCuckooPiId(device_id, normalized)) return false;
  lock();
  for (uint8_t index = 0; index < config_.device_count &&
                          index < kMaxCuckooPiDevices;
       ++index) {
    if (std::strcmp(config_.devices[index].id, normalized) == 0) {
      *output = devices_[index];
      unlock();
      return true;
    }
  }
  unlock();
  return false;
}

void CuckooPiMqttService::copyDiagnostics(
    CuckooPiMqttDiagnostics* output) const {
  if (!output) return;
  lock();
  *output = diagnostics_;
  unlock();
}

#endif  // GATEWAY
