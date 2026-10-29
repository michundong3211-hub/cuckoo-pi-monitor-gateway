#ifndef SRC_CUCKOO_PI_CONFIG_HPP_
#define SRC_CUCKOO_PI_CONFIG_HPP_

#include <ArduinoJson.h>

#include <cstddef>
#include <cstdint>

constexpr size_t kMaxCuckooPiDevices = 8;
constexpr const char* kDefaultCuckooPiBroker = "";
constexpr uint16_t kDefaultCuckooPiPort = 1884;
constexpr uint16_t kLegacyDefaultCuckooPiPort = 2884;
constexpr const char* kDefaultCuckooPiClientId = "";
constexpr const char* kDefaultCuckooPiUsername = "";
constexpr const char* kDefaultCuckooPiPassword = "";
constexpr const char* kDefaultCuckooPiDeviceId = "";
constexpr uint32_t kDefaultCuckooPiOfflineTimeoutSeconds = 300;
constexpr float kDefaultCuckooPiBeerDeviationThresholdC = 2.0f;
constexpr uint32_t kDefaultCuckooPiBeerDeviationHoldSeconds = 600;

struct CuckooPiDeviceConfig {
  char id[13];
  bool offline_alarm_enabled;
  uint32_t offline_timeout_seconds;
  bool beer_deviation_alarm_enabled;
  float beer_deviation_threshold_c;
  uint32_t beer_deviation_hold_seconds;
  bool offline_alarm_dismissed;
  // Status-time marker captured when the user closes an offline alert. It
  // prevents the same retained MQTT payload from reopening the alert.
  uint32_t offline_dismissed_status_time_seconds;
};

struct CuckooPiConfig {
  bool enabled;
  char broker[64];
  uint16_t port;
  char client_id[40];
  char username[64];
  char password[64];
  CuckooPiDeviceConfig devices[kMaxCuckooPiDevices];
  uint8_t device_count;
};

CuckooPiConfig defaultCuckooPiConfig();
CuckooPiDeviceConfig defaultCuckooPiDeviceConfig(const char* device_id);
CuckooPiDeviceConfig normalizeCuckooPiDeviceConfig(
    const CuckooPiDeviceConfig& input);
bool setCuckooPiOfflineDismissed(CuckooPiConfig* config,
                                 const char* device_id, bool dismissed,
                                 uint32_t status_time_seconds = 0U);
bool parseCuckooPiConfig(JsonVariantConst source, CuckooPiConfig* output);
void appendCuckooPiConfig(JsonObject destination,
                          const CuckooPiConfig& config);

#endif  // SRC_CUCKOO_PI_CONFIG_HPP_
