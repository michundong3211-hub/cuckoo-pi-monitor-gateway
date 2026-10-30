#include "cuckoo_pi_config.hpp"

#include "cuckoo_pi_model.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

namespace {
constexpr uint32_t kMinimumAlarmSeconds = 60;
constexpr uint32_t kMaximumAlarmSeconds = 86400;
constexpr float kMinimumDeviationThresholdC = 0.1f;
constexpr float kMaximumDeviationThresholdC = 20.0f;

template <size_t Capacity>
void copyString(char (&destination)[Capacity], const char* source) {
  destination[0] = '\0';
  if (!source) return;
  std::strncpy(destination, source, Capacity - 1);
  destination[Capacity - 1] = '\0';
}

template <size_t Capacity>
bool parseOptionalCredential(JsonObjectConst object, const char* key,
                             char (&destination)[Capacity]) {
  const JsonVariantConst field = object[key];
  if (field.isUnbound()) return true;
  if (!field.is<const char*>()) return false;
  const char* value = field.as<const char*>();
  if (!value || std::strlen(value) >= Capacity) return false;
  copyString(destination, value);
  return true;
}

uint32_t clampSeconds(uint32_t value) {
  return std::max(kMinimumAlarmSeconds,
                  std::min(kMaximumAlarmSeconds, value));
}

float clampThreshold(float value) {
  if (!std::isfinite(value)) return kDefaultCuckooPiBeerDeviationThresholdC;
  return std::max(kMinimumDeviationThresholdC,
                  std::min(kMaximumDeviationThresholdC, value));
}

void parseOptionalBool(JsonObjectConst object, const char* key, bool* value) {
  if (!value || object[key].isNull() || !object[key].is<bool>()) return;
  *value = object[key].as<bool>();
}

void parseOptionalSeconds(JsonObjectConst object, const char* key,
                          uint32_t* value) {
  if (!value || object[key].isNull()) return;
  if (object[key].is<unsigned long>()) {
    *value = clampSeconds(object[key].as<unsigned long>());
    return;
  }
  if (object[key].is<long>()) {
    const long parsed = object[key].as<long>();
    *value = parsed <= 0 ? kMinimumAlarmSeconds
                         : clampSeconds(static_cast<uint32_t>(parsed));
  }
}
}

CuckooPiConfig defaultCuckooPiConfig() {
  CuckooPiConfig config{};
  config.enabled = true;
  copyString(config.broker, kDefaultCuckooPiBroker);
  config.port = kDefaultCuckooPiPort;
  copyString(config.client_id, kDefaultCuckooPiClientId);
  copyString(config.username, kDefaultCuckooPiUsername);
  copyString(config.password, kDefaultCuckooPiPassword);
  config.device_count = 0;
  return config;
}

CuckooPiDeviceConfig defaultCuckooPiDeviceConfig(const char* device_id) {
  CuckooPiDeviceConfig config{};
  normalizeCuckooPiId(device_id, config.id);
  config.offline_alarm_enabled = true;
  config.offline_timeout_seconds = kDefaultCuckooPiOfflineTimeoutSeconds;
  config.beer_deviation_alarm_enabled = true;
  config.beer_deviation_threshold_c =
      kDefaultCuckooPiBeerDeviationThresholdC;
  config.beer_deviation_hold_seconds =
      kDefaultCuckooPiBeerDeviationHoldSeconds;
  config.offline_alarm_dismissed = false;
  config.offline_dismissed_status_time_seconds = 0U;
  return config;
}

CuckooPiDeviceConfig normalizeCuckooPiDeviceConfig(
    const CuckooPiDeviceConfig& input) {
  CuckooPiDeviceConfig normalized = input;
  char id[13] = {};
  if (normalizeCuckooPiId(input.id, id)) {
    std::memcpy(normalized.id, id, sizeof(normalized.id));
  } else {
    normalized.id[0] = '\0';
  }
  normalized.offline_timeout_seconds =
      clampSeconds(input.offline_timeout_seconds);
  normalized.beer_deviation_threshold_c =
      clampThreshold(input.beer_deviation_threshold_c);
  normalized.beer_deviation_hold_seconds =
      clampSeconds(input.beer_deviation_hold_seconds);
  return normalized;
}

bool setCuckooPiOfflineDismissed(CuckooPiConfig* config,
                                 const char* device_id, bool dismissed,
                                 uint32_t status_time_seconds) {
  if (!config || !device_id) return false;
  char normalized_id[13] = {};
  if (!normalizeCuckooPiId(device_id, normalized_id)) return false;
  const uint8_t count = static_cast<uint8_t>(
      std::min<size_t>(config->device_count, kMaxCuckooPiDevices));
  for (uint8_t index = 0; index < count; ++index) {
    if (std::strcmp(config->devices[index].id, normalized_id) != 0) continue;
    config->devices[index].offline_alarm_dismissed = dismissed;
    config->devices[index].offline_dismissed_status_time_seconds =
        dismissed ? status_time_seconds : 0U;
    return true;
  }
  return false;
}

bool parseCuckooPiConfig(JsonVariantConst source, CuckooPiConfig* output) {
  if (!output || !source.is<JsonObjectConst>()) return false;
  const JsonObjectConst object = source.as<JsonObjectConst>();
  CuckooPiConfig parsed = defaultCuckooPiConfig();

  parseOptionalBool(object, "enabled", &parsed.enabled);
  if (object["broker"].is<const char*>()) {
    const char* broker = object["broker"].as<const char*>();
    const size_t length = broker ? std::strlen(broker) : 0;
    if (length > 0 && length < sizeof(parsed.broker)) {
      copyString(parsed.broker, broker);
    }
  }
  if (object["port"].is<unsigned int>()) {
    const unsigned int port = object["port"].as<unsigned int>();
    if (port > 0 && port <= 65535U) parsed.port = static_cast<uint16_t>(port);
  }

  const bool hasNoBroker = parsed.broker[0] == '\0';
  const bool usesPreviousDefaultPort =
      parsed.port == kPreviousDefaultCuckooPiPort ||
      parsed.port == kLegacyDefaultCuckooPiPort;
  if (hasNoBroker && usesPreviousDefaultPort)
    parsed.port = kDefaultCuckooPiPort;

  if (!parseOptionalCredential(object, "client_id", parsed.client_id) ||
      !parseOptionalCredential(object, "username", parsed.username) ||
      !parseOptionalCredential(object, "password", parsed.password)) {
    return false;
  }
  if (!object["devices"].isNull()) {
    if (!object["devices"].is<JsonArrayConst>()) return false;
    const JsonArrayConst devices = object["devices"].as<JsonArrayConst>();
    if (devices.size() > kMaxCuckooPiDevices) return false;
    parsed.device_count = 0;
    for (JsonVariantConst item : devices) {
      if (!item.is<JsonObjectConst>()) return false;
      const JsonObjectConst device_object = item.as<JsonObjectConst>();
      const char* raw_id = device_object["id"].as<const char*>();
      CuckooPiDeviceConfig device = defaultCuckooPiDeviceConfig(raw_id);
      if (device.id[0] == '\0') return false;
      for (uint8_t existing = 0; existing < parsed.device_count; ++existing) {
        if (std::strcmp(parsed.devices[existing].id, device.id) == 0)
          return false;
      }
      parseOptionalBool(device_object, "offline_alarm_enabled",
                        &device.offline_alarm_enabled);
      parseOptionalSeconds(device_object, "offline_timeout_seconds",
                           &device.offline_timeout_seconds);
      parseOptionalBool(device_object, "beer_deviation_alarm_enabled",
                        &device.beer_deviation_alarm_enabled);
      if (device_object["beer_deviation_threshold_c"].is<float>()) {
        device.beer_deviation_threshold_c = clampThreshold(
            device_object["beer_deviation_threshold_c"].as<float>());
      }
      parseOptionalSeconds(device_object, "beer_deviation_hold_seconds",
                           &device.beer_deviation_hold_seconds);
      parseOptionalBool(device_object, "offline_alarm_dismissed",
                        &device.offline_alarm_dismissed);
      if (device_object["offline_dismissed_status_time_seconds"]
              .is<unsigned long>()) {
        device.offline_dismissed_status_time_seconds =
            device_object["offline_dismissed_status_time_seconds"]
                .as<unsigned long>();
      }
      parsed.devices[parsed.device_count++] = device;
    }
  }

  *output = parsed;
  return true;
}

void appendCuckooPiConfig(JsonObject destination,
                          const CuckooPiConfig& input) {
  const CuckooPiConfig defaults = defaultCuckooPiConfig();
  const char* broker = input.broker[0] ? input.broker : defaults.broker;
  const uint16_t port = input.port == 0 ? defaults.port : input.port;
  JsonObject object = destination["cuckoo_pi"].to<JsonObject>();
  object["enabled"] = input.enabled;
  object["broker"] = std::string(broker);
  object["port"] = port;
  object["client_id"] = std::string(input.client_id);
  object["username"] = std::string(input.username);
  object["password"] = std::string(input.password);
  JsonArray devices = object["devices"].to<JsonArray>();
  const uint8_t count = static_cast<uint8_t>(
      std::min<size_t>(input.device_count, kMaxCuckooPiDevices));
  for (uint8_t index = 0; index < count; ++index) {
    const CuckooPiDeviceConfig normalized =
        normalizeCuckooPiDeviceConfig(input.devices[index]);
    if (normalized.id[0] == '\0') continue;
    JsonObject device = devices.add<JsonObject>();
    device["id"] = std::string(normalized.id);
    device["offline_alarm_enabled"] = normalized.offline_alarm_enabled;
    device["offline_timeout_seconds"] =
        normalized.offline_timeout_seconds;
    device["beer_deviation_alarm_enabled"] =
        normalized.beer_deviation_alarm_enabled;
    device["beer_deviation_threshold_c"] =
        normalized.beer_deviation_threshold_c;
    device["beer_deviation_hold_seconds"] =
        normalized.beer_deviation_hold_seconds;
    device["offline_alarm_dismissed"] =
        normalized.offline_alarm_dismissed;
    device["offline_dismissed_status_time_seconds"] =
        normalized.offline_dismissed_status_time_seconds;
  }
}
