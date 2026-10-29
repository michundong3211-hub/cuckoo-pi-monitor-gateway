#pragma once

#include <cstddef>
#include <cstdint>
#include <cmath>

#include "gateway_ring_color.hpp"

constexpr uint8_t MAX_GRAVITY_DEVICE_SETTINGS = 20;
constexpr size_t GRAVITY_DEVICE_ID_CAPACITY = 64;
constexpr size_t GRAVITY_DEVICE_ID_MAX_LENGTH = 63;
constexpr float GRAVITY_DEVICE_OG_MIN = 1.000f;
constexpr float GRAVITY_DEVICE_OG_MAX = 1.200f;
constexpr uint8_t GRAVITY_DEVICE_COMPARE_MINUTES_MIN = 1;
constexpr uint8_t GRAVITY_DEVICE_COMPARE_MINUTES_MAX = 60;
constexpr float GRAVITY_DEVICE_THRESHOLD_C_MIN = 0.1f;
constexpr float GRAVITY_DEVICE_THRESHOLD_C_MAX = 10.0f;
constexpr uint32_t GRAVITY_DEVICE_OFFLINE_TIMEOUT_MIN = 60;
constexpr uint32_t GRAVITY_DEVICE_OFFLINE_TIMEOUT_MAX = 86400;
constexpr uint32_t GRAVITY_DEVICE_OFFLINE_TIMEOUT_STEP = 60;
constexpr uint32_t GRAVITY_DEVICE_OFFLINE_TIMEOUT_DEFAULT = 3660;
constexpr uint32_t GRAVITY_DEVICE_OFFLINE_TIMEOUT_LEGACY_FALLBACK = 900;

struct GravityDeviceSettings {
  char device_id[GRAVITY_DEVICE_ID_CAPACITY] = {};
  float og_sg = NAN;
  float temp_min_c = 18.0f;
  float temp_max_c = 28.0f;
  float low_battery_percent = 20.0f;
  bool buzzer_enabled = false;
  uint8_t ring_color_index = RING_COLOR_DEFAULT_INDEX;
  bool temperature_change_alarm_enabled = false;
  uint8_t temperature_compare_minutes = 5;
  float temperature_change_threshold_c = 1.0f;
  bool offline_alarm_enabled = false;
  uint32_t offline_timeout_seconds = GRAVITY_DEVICE_OFFLINE_TIMEOUT_DEFAULT;
  // Persisted user dismissal of the offline alarm (CLOSE). Survives reboots so
  // a still-offline device does not re-alarm after the gateway restarts. Cleared
  // only when the device reports data again (markOnline).
  bool offline_alarm_dismissed = false;
};

bool gateway_device_id_is_valid(const char* device_id);
GravityDeviceSettings normalize_gateway_device_settings(
    const GravityDeviceSettings& settings);

size_t serialize_gateway_device_settings(const GravityDeviceSettings* settings,
                                         uint8_t count, char* output,
                                         size_t output_capacity);
bool parse_gateway_device_settings(const char* json, size_t json_size,
                                   GravityDeviceSettings* settings,
                                   uint8_t* count);
