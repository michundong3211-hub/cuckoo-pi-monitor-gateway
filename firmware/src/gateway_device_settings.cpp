#include "gateway_device_settings.hpp"
#include "gateway_device_settings_json.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

#include <ArduinoJson.h>

namespace {
float clamp_float(float value, float minimum, float maximum, float fallback) {
  if (!std::isfinite(value)) return fallback;
  return std::max(minimum, std::min(maximum, value));
}
}

bool gateway_device_id_is_valid(const char* device_id) {
  if (!device_id || !device_id[0]) return false;
  for (size_t i = 1; i < GRAVITY_DEVICE_ID_CAPACITY; ++i) {
    if (device_id[i] == '\0') return true;
  }
  return false;
}

GravityDeviceSettings normalize_gateway_device_settings(
    const GravityDeviceSettings& settings) {
  GravityDeviceSettings normalized = settings;
  normalized.ring_color_index =
      normalize_ring_color_index(normalized.ring_color_index);
  normalized.og_sg = std::isfinite(normalized.og_sg)
                        ? clamp_float(normalized.og_sg, GRAVITY_DEVICE_OG_MIN,
                                      GRAVITY_DEVICE_OG_MAX, NAN)
                        : NAN;
  normalized.temperature_compare_minutes = static_cast<uint8_t>(std::max(
      static_cast<int>(GRAVITY_DEVICE_COMPARE_MINUTES_MIN),
      std::min(static_cast<int>(GRAVITY_DEVICE_COMPARE_MINUTES_MAX),
               static_cast<int>(normalized.temperature_compare_minutes))));
  normalized.temperature_change_threshold_c = clamp_float(
      normalized.temperature_change_threshold_c, GRAVITY_DEVICE_THRESHOLD_C_MIN,
      GRAVITY_DEVICE_THRESHOLD_C_MAX, 1.0f);
  const uint32_t clamped_offline_timeout = std::max(
      GRAVITY_DEVICE_OFFLINE_TIMEOUT_MIN,
      std::min(GRAVITY_DEVICE_OFFLINE_TIMEOUT_MAX,
               normalized.offline_timeout_seconds));
  normalized.offline_timeout_seconds =
      ((clamped_offline_timeout + GRAVITY_DEVICE_OFFLINE_TIMEOUT_STEP / 2) /
       GRAVITY_DEVICE_OFFLINE_TIMEOUT_STEP) *
      GRAVITY_DEVICE_OFFLINE_TIMEOUT_STEP;
  normalized.offline_timeout_seconds = std::min(
      GRAVITY_DEVICE_OFFLINE_TIMEOUT_MAX, normalized.offline_timeout_seconds);
  return normalized;
}

size_t serialize_gateway_device_settings(const GravityDeviceSettings* settings,
                                         uint8_t count, char* output,
                                         size_t output_capacity) {
  if (!output || output_capacity == 0 || (!settings && count != 0) ||
      count > MAX_GRAVITY_DEVICE_SETTINGS) {
    return 0;
  }
  JsonDocument doc;
  JsonArray devices = doc.to<JsonArray>();
  append_gateway_device_settings_json(devices, settings, count);
  const size_t length = measureJson(doc);
  if (length + 1 > output_capacity) return 0;
  serializeJson(doc, output, output_capacity);
  return length;
}

void append_gateway_device_settings_json(
    JsonArray devices, const GravityDeviceSettings* settings, uint8_t count) {
  if (!settings || devices.isNull()) return;
  const uint8_t safe_count =
      count > MAX_GRAVITY_DEVICE_SETTINGS ? MAX_GRAVITY_DEVICE_SETTINGS : count;
  for (uint8_t i = 0; i < safe_count; ++i) {
    if (!gateway_device_id_is_valid(settings[i].device_id)) continue;
    const GravityDeviceSettings normalized =
        normalize_gateway_device_settings(settings[i]);
    JsonObject device = devices.add<JsonObject>();
    const char* normalized_device_id = normalized.device_id;
    device["id"] = normalized_device_id;
    if (std::isnan(normalized.og_sg)) {
      device["og_sg"] = nullptr;
    } else {
      device["og_sg"] = normalized.og_sg;
    }
    device["temp_min_c"] = normalized.temp_min_c;
    device["temp_max_c"] = normalized.temp_max_c;
    device["low_battery_percent"] = normalized.low_battery_percent;
    device["buzzer_enabled"] = normalized.buzzer_enabled;
    device["ring_color_index"] = normalized.ring_color_index;
    device["temperature_change_alarm_enabled"] =
        normalized.temperature_change_alarm_enabled;
    device["temperature_compare_minutes"] =
        normalized.temperature_compare_minutes;
    device["temperature_change_threshold_c"] =
        normalized.temperature_change_threshold_c;
    device["offline_alarm_enabled"] = normalized.offline_alarm_enabled;
    device["offline_timeout_seconds"] = normalized.offline_timeout_seconds;
    device["offline_alarm_dismissed"] = normalized.offline_alarm_dismissed;
  }
}

bool parse_gateway_device_settings(const char* json, size_t json_size,
                                   GravityDeviceSettings* settings,
                                   uint8_t* count) {
  if (!json || !settings || !count) return false;
  JsonDocument doc;
  if (deserializeJson(doc, json, json_size)) return false;
  JsonArray devices = doc.as<JsonArray>();
  if (devices.isNull()) return false;
  if (devices.size() > MAX_GRAVITY_DEVICE_SETTINGS) return false;
  GravityDeviceSettings parsed_settings[MAX_GRAVITY_DEVICE_SETTINGS]{};
  uint8_t parsed = 0;
  for (JsonObject device : devices) {
    const char* id = device["id"] | "";
    if (!gateway_device_id_is_valid(id)) continue;
    for (uint8_t i = 0; i < parsed; ++i) {
      if (std::strcmp(parsed_settings[i].device_id, id) == 0) return false;
    }
    GravityDeviceSettings next{};
    std::snprintf(next.device_id, sizeof(next.device_id), "%s", id);
    next.og_sg = device["og_sg"] | NAN;
    next.temp_min_c = device["temp_min_c"] | 18.0f;
    next.temp_max_c = device["temp_max_c"] | 28.0f;
    next.low_battery_percent = device["low_battery_percent"] | 20.0f;
    next.buzzer_enabled = device["buzzer_enabled"] | false;
    next.ring_color_index = normalize_ring_color_index(
        device["ring_color_index"] | static_cast<int>(RING_COLOR_DEFAULT_INDEX));
    next.temperature_change_alarm_enabled =
        device["temperature_change_alarm_enabled"] | false;
    next.temperature_compare_minutes = device["temperature_compare_minutes"] | 5;
    next.temperature_change_threshold_c =
        device["temperature_change_threshold_c"] | 1.0f;
    next.offline_alarm_enabled = device["offline_alarm_enabled"] | false;
    next.offline_timeout_seconds =
        device["offline_timeout_seconds"] |
        GRAVITY_DEVICE_OFFLINE_TIMEOUT_LEGACY_FALLBACK;
    next.offline_alarm_dismissed = device["offline_alarm_dismissed"] | false;
    parsed_settings[parsed++] = normalize_gateway_device_settings(next);
  }
  std::memcpy(settings, parsed_settings,
              sizeof(GravityDeviceSettings) * parsed);
  *count = parsed;
  return true;
}
