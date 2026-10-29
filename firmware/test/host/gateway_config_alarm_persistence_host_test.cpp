#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <ArduinoJson.h>

#include "../../src/gateway_device_settings.hpp"
#include "../../src/gateway_device_settings_json.hpp"

int main() {
  GravityDeviceSettings source[2]{};
  strcpy(source[0].device_id, "dismissed-device");
  source[0].offline_alarm_enabled = true;
  source[0].offline_alarm_dismissed = true;
  strcpy(source[1].device_id, "active-device");
  source[1].offline_alarm_enabled = true;
  source[1].offline_alarm_dismissed = false;

  JsonDocument full_config;
  full_config["schema"] = 7;
  JsonArray devices = full_config["device_settings"].to<JsonArray>();
  append_gateway_device_settings_json(devices, source, 2);

  char full_json[4096] = {};
  const size_t full_size =
      serializeJson(full_config, full_json, sizeof(full_json));
  assert(full_size > 0);

  JsonDocument restored_config;
  assert(!deserializeJson(restored_config, full_json, full_size));
  assert(restored_config["schema"].as<int>() == 7);
  JsonArray restored_devices =
      restored_config["device_settings"].as<JsonArray>();
  assert(restored_devices.size() == 2);
  if (!restored_devices[0]["offline_alarm_dismissed"].as<bool>() ||
      restored_devices[1]["offline_alarm_dismissed"].as<bool>()) {
    fprintf(stderr,
            "full config did not preserve per-device offline dismissal\n");
    return 6;
  }

  char device_json[4096] = {};
  const size_t device_size =
      serializeJson(restored_devices, device_json, sizeof(device_json));
  GravityDeviceSettings restored[2]{};
  uint8_t restored_count = 0;
  assert(parse_gateway_device_settings(device_json, device_size, restored,
                                       &restored_count));
  assert(restored_count == 2);
  assert(restored[0].offline_alarm_dismissed);
  assert(!restored[1].offline_alarm_dismissed);

  char dedicated_json[4096] = {};
  const size_t dedicated_size = serialize_gateway_device_settings(
      source, 2, dedicated_json, sizeof(dedicated_json));
  assert(dedicated_size > 0);
  GravityDeviceSettings dedicated_restored[2]{};
  uint8_t dedicated_count = 0;
  assert(parse_gateway_device_settings(dedicated_json, dedicated_size,
                                       dedicated_restored,
                                       &dedicated_count));
  assert(dedicated_count == 2);
  assert(dedicated_restored[0].offline_alarm_dismissed);
  assert(!dedicated_restored[1].offline_alarm_dismissed);
  return 0;
}
