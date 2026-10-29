#include "gateway_device_settings.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

int main() {
  assert(GRAVITY_DEVICE_OFFLINE_TIMEOUT_STEP == 60UL);
  assert(GRAVITY_DEVICE_OFFLINE_TIMEOUT_DEFAULT == 3660UL);
  assert(GRAVITY_DEVICE_OFFLINE_TIMEOUT_LEGACY_FALLBACK == 900UL);
  GravityDeviceSettings defaults{};
  assert(!defaults.temperature_change_alarm_enabled);
  assert(defaults.temperature_compare_minutes == 5);
  assert(std::fabs(defaults.temperature_change_threshold_c - 1.0f) < 0.0001f);
  assert(!defaults.offline_alarm_enabled);
  assert(defaults.offline_timeout_seconds ==
         GRAVITY_DEVICE_OFFLINE_TIMEOUT_DEFAULT);

  GravityDeviceSettings invalid{};
  std::strcpy(invalid.device_id, "device");
  invalid.og_sg = 9.0f;
  invalid.temperature_compare_minutes = 0;
  invalid.temperature_change_threshold_c = 99.0f;
  invalid.offline_timeout_seconds = 1;
  invalid.ring_color_index = 255;
  const GravityDeviceSettings normalized =
      normalize_gateway_device_settings(invalid);
  assert(normalized.og_sg == 1.200f);
  assert(normalized.temperature_compare_minutes == 1);
  assert(normalized.temperature_change_threshold_c == 10.0f);
  assert(normalized.offline_timeout_seconds == 60UL);
  assert(normalized.ring_color_index == RING_COLOR_DEFAULT_INDEX);

  GravityDeviceSettings rounded = defaults;
  rounded.offline_timeout_seconds = 61UL;
  assert(normalize_gateway_device_settings(rounded).offline_timeout_seconds ==
         60UL);
  rounded.offline_timeout_seconds = 3661UL;
  assert(normalize_gateway_device_settings(rounded).offline_timeout_seconds ==
         3660UL);
  rounded.offline_timeout_seconds = 86400UL;
  assert(normalize_gateway_device_settings(rounded).offline_timeout_seconds ==
         86400UL);

  char accepted[64] = {};
  std::memset(accepted, 'a', 63);
  assert(gateway_device_id_is_valid(accepted));
  char rejected[65] = {};
  std::memset(rejected, 'b', 64);
  assert(!gateway_device_id_is_valid(rejected));
  char fixed_without_terminator[GRAVITY_DEVICE_ID_CAPACITY];
  std::memset(fixed_without_terminator, 'c', sizeof(fixed_without_terminator));
  assert(!gateway_device_id_is_valid(fixed_without_terminator));

  GravityDeviceSettings records[MAX_GRAVITY_DEVICE_SETTINGS]{};
  for (uint8_t i = 0; i < MAX_GRAVITY_DEVICE_SETTINGS; ++i) {
    std::snprintf(records[i].device_id, sizeof(records[i].device_id),
                  "device-%u", i);
    records[i].temperature_compare_minutes = static_cast<uint8_t>(i + 1);
    records[i].offline_timeout_seconds =
        GRAVITY_DEVICE_OFFLINE_TIMEOUT_STEP * (i + 1);
  }
  char json[32768] = {};
  const size_t json_size = serialize_gateway_device_settings(
      records, MAX_GRAVITY_DEVICE_SETTINGS, json, sizeof(json));
  assert(json_size > 0);

  GravityDeviceSettings restored[MAX_GRAVITY_DEVICE_SETTINGS]{};
  uint8_t restored_count = 0;
  assert(parse_gateway_device_settings(json, json_size, restored,
                                       &restored_count));
  assert(restored_count == MAX_GRAVITY_DEVICE_SETTINGS);
  for (uint8_t i = 0; i < restored_count; ++i) {
    bool found = false;
    for (uint8_t j = 0; j < restored_count; ++j) {
      if (std::strcmp(restored[j].device_id, records[i].device_id) != 0)
        continue;
      assert(restored[j].temperature_compare_minutes ==
             records[i].temperature_compare_minutes);
      assert(restored[j].offline_timeout_seconds ==
             records[i].offline_timeout_seconds);
      found = true;
      break;
    }
    assert(found);
  }

  GravityDeviceSettings legacy{};
  uint8_t legacy_count = 0;
  assert(parse_gateway_device_settings(
      "[{\"id\":\"legacy\",\"og_sg\":1.080,\"ring_color_index\":0}]",
      67, &legacy, &legacy_count));
  assert(legacy_count == 1);
  assert(!legacy.temperature_change_alarm_enabled);
  assert(!legacy.offline_alarm_enabled);
  assert(legacy.offline_timeout_seconds ==
         GRAVITY_DEVICE_OFFLINE_TIMEOUT_LEGACY_FALLBACK);

  GravityDeviceSettings explicit_legacy{};
  uint8_t explicit_legacy_count = 0;
  const char* explicit_legacy_json =
      "[{\"id\":\"legacy-explicit\",\"offline_timeout_seconds\":900}]";
  assert(parse_gateway_device_settings(
      explicit_legacy_json, std::strlen(explicit_legacy_json), &explicit_legacy,
      &explicit_legacy_count));
  assert(explicit_legacy_count == 1);
  assert(explicit_legacy.offline_timeout_seconds ==
         GRAVITY_DEVICE_OFFLINE_TIMEOUT_LEGACY_FALLBACK);

  const char* duplicate_json =
      "[{\"id\":\"duplicate\"},{\"id\":\"duplicate\"}]";
  GravityDeviceSettings rejected_records[MAX_GRAVITY_DEVICE_SETTINGS]{};
  uint8_t rejected_count = 7;
  assert(!parse_gateway_device_settings(duplicate_json,
                                        std::strlen(duplicate_json),
                                        rejected_records, &rejected_count));
  assert(rejected_count == 7);

  std::string too_many = "[";
  for (int i = 0; i <= MAX_GRAVITY_DEVICE_SETTINGS; ++i) {
    if (i) too_many += ',';
    too_many += "{\"id\":\"device-overflow-" + std::to_string(i) + "\"}";
  }
  too_many += ']';
  rejected_count = 9;
  assert(!parse_gateway_device_settings(too_many.c_str(), too_many.size(),
                                        rejected_records, &rejected_count));
  assert(rejected_count == 9);
  return 0;
}
