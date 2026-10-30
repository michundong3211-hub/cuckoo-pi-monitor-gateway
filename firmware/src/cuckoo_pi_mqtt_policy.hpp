#ifndef SRC_CUCKOO_PI_MQTT_POLICY_HPP_
#define SRC_CUCKOO_PI_MQTT_POLICY_HPP_

#include "cuckoo_pi_config.hpp"

#include <cstdint>

uint32_t cuckooPiReconnectDelayMs(uint8_t failure_count);
bool cuckooPiHasMqttEndpoint(const CuckooPiConfig& config);
bool cuckooPiShouldConnect(bool enabled, bool wifi_connected,
                           uint8_t device_count, uint32_t now_ms,
                           uint32_t next_attempt_ms);
bool cuckooPiSubscriptionConfigEqual(const CuckooPiConfig& left,
                                     const CuckooPiConfig& right);
int cuckooPiFindStatusTopicDeviceIndex(const CuckooPiConfig& config,
                                       const char* topic);

#endif  // SRC_CUCKOO_PI_MQTT_POLICY_HPP_
