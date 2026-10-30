#include "cuckoo_pi_mqtt_policy.hpp"

#include "cuckoo_pi_model.hpp"

#include <cstring>

uint32_t cuckooPiReconnectDelayMs(uint8_t failure_count) {
  constexpr uint32_t delays[] = {1000U, 2000U, 5000U, 10000U, 30000U};
  const size_t index = failure_count < 5 ? failure_count : 4;
  return delays[index];
}

bool cuckooPiHasMqttEndpoint(const CuckooPiConfig& config) {
  return config.broker[0] != '\0' && config.port != 0;
}

bool cuckooPiShouldConnect(bool enabled, bool wifi_connected,
                           uint8_t device_count, uint32_t now_ms,
                           uint32_t next_attempt_ms) {
  if (!enabled || !wifi_connected || device_count == 0) return false;
  return static_cast<int32_t>(now_ms - next_attempt_ms) >= 0;
}

bool cuckooPiSubscriptionConfigEqual(const CuckooPiConfig& left,
                                     const CuckooPiConfig& right) {
  if (left.enabled != right.enabled || left.port != right.port ||
      left.device_count != right.device_count ||
      std::strcmp(left.broker, right.broker) != 0 ||
      std::strcmp(left.client_id, right.client_id) != 0 ||
      std::strcmp(left.username, right.username) != 0 ||
      std::strcmp(left.password, right.password) != 0) {
    return false;
  }
  for (uint8_t index = 0; index < left.device_count; ++index) {
    if (std::strcmp(left.devices[index].id, right.devices[index].id) != 0)
      return false;
  }
  return true;
}

int cuckooPiFindStatusTopicDeviceIndex(const CuckooPiConfig& config,
                                       const char* topic) {
  if (!topic) return -1;
  const uint8_t count = config.device_count > kMaxCuckooPiDevices
                            ? kMaxCuckooPiDevices
                            : config.device_count;
  for (uint8_t index = 0; index < count; ++index) {
    CuckooPiTopics topics{};
    if (!buildCuckooPiTopics(config.devices[index].id, &topics)) continue;
    if (std::strcmp(topic, topics.status) == 0) return index;
  }
  return -1;
}
