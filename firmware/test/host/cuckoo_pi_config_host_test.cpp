#include "cuckoo_pi_config.hpp"

#include <ArduinoJson.h>

#include <cassert>
#include <cmath>
#include <cstring>
#include <string>

namespace {
JsonVariantConst parseVariant(const char* json, JsonDocument* document) {
  assert(document != nullptr);
  assert(!deserializeJson(*document, json));
  return document->as<JsonVariantConst>();
}
}

int main() {
  const CuckooPiConfig defaults = defaultCuckooPiConfig();
  assert(defaults.enabled);
  assert(std::strcmp(defaults.broker, "") == 0);
  assert(defaults.port == 1884);
  assert(std::strcmp(defaults.client_id, "") == 0);
  assert(std::strcmp(defaults.username, "") == 0);
  assert(defaults.password[0] == '\0');
  assert(defaults.device_count == 0);

  const CuckooPiDeviceConfig device_defaults =
      defaultCuckooPiDeviceConfig("00:11:22:33:44:55");
  assert(std::strcmp(device_defaults.id, "001122334455") == 0);
  assert(device_defaults.offline_alarm_enabled);
  assert(device_defaults.offline_timeout_seconds == 300U);
  assert(device_defaults.beer_deviation_alarm_enabled);
  assert(std::fabs(device_defaults.beer_deviation_threshold_c - 2.0f) <
         0.0001f);
  assert(device_defaults.beer_deviation_hold_seconds == 600U);
  assert(!device_defaults.offline_alarm_dismissed);

  CuckooPiConfig dismissal = defaults;
  dismissal.devices[0] = device_defaults;
  dismissal.device_count = 1;
  assert(setCuckooPiOfflineDismissed(&dismissal, "00:11:22:33:44:55", true,
                                     120U));
  assert(dismissal.devices[0].offline_alarm_dismissed);
  assert(dismissal.devices[0].offline_dismissed_status_time_seconds == 120U);
  assert(setCuckooPiOfflineDismissed(&dismissal, "001122334455", false));
  assert(!dismissal.devices[0].offline_alarm_dismissed);
  assert(dismissal.devices[0].offline_dismissed_status_time_seconds == 0U);
  assert(!setCuckooPiOfflineDismissed(&dismissal, "not-an-id", true));
  assert(!setCuckooPiOfflineDismissed(&dismissal, "112233445566", true));

  JsonDocument missing_doc;
  const char* missing_json =
      R"json({"enabled":true,"devices":[{"id":"00-11-22-33-44-55"}]})json";
  CuckooPiConfig missing{};
  assert(parseCuckooPiConfig(parseVariant(missing_json, &missing_doc),
                             &missing));
  assert(missing.enabled);
  assert(std::strcmp(missing.broker, "") == 0);
  assert(missing.port == 1884);
  assert(std::strcmp(missing.client_id, "") == 0);
  assert(std::strcmp(missing.username, "") == 0);
  assert(std::strcmp(missing.password, "") == 0);
  assert(missing.device_count == 1);
  assert(std::strcmp(missing.devices[0].id, "001122334455") == 0);
  assert(missing.devices[0].offline_alarm_enabled);
  assert(missing.devices[0].offline_timeout_seconds == 300U);

  JsonDocument empty_broker_credentials_doc;
  CuckooPiConfig empty_broker_credentials{};
  assert(parseCuckooPiConfig(parseVariant(R"json({"broker":"","port":1884,"client_id":"sample-client","username":"sample-user","password":"sample-password"})json", &empty_broker_credentials_doc), &empty_broker_credentials));
  assert(std::strcmp(empty_broker_credentials.client_id, "sample-client") == 0);
  assert(std::strcmp(empty_broker_credentials.username, "sample-user") == 0);
  assert(std::strcmp(empty_broker_credentials.password, "sample-password") == 0);
  JsonDocument custom_credentials_doc;
  CuckooPiConfig custom_credentials{};
  assert(parseCuckooPiConfig(parseVariant(R"json({"broker":"broker.example","port":1883,"client_id":"sample-client","username":"sample-user","password":"sample-password"})json", &custom_credentials_doc), &custom_credentials));
  assert(std::strcmp(custom_credentials.broker, "broker.example") == 0);
  assert(std::strcmp(custom_credentials.client_id, "sample-client") == 0);
  assert(std::strcmp(custom_credentials.username, "sample-user") == 0);
  assert(std::strcmp(custom_credentials.password, "sample-password") == 0);
  JsonDocument custom_roundtrip_doc;
  appendCuckooPiConfig(custom_roundtrip_doc.to<JsonObject>(), custom_credentials);
  CuckooPiConfig custom_roundtrip{};
  assert(parseCuckooPiConfig(custom_roundtrip_doc["cuckoo_pi"], &custom_roundtrip));
  assert(std::strcmp(custom_roundtrip.broker, "broker.example") == 0);
  assert(custom_roundtrip.port == 1883);
  assert(std::strcmp(custom_roundtrip.username, "sample-user") == 0);
  assert(std::strcmp(custom_roundtrip.password, "sample-password") == 0);
  JsonDocument blank_credentials_doc;
  CuckooPiConfig blank_credentials{};
  assert(parseCuckooPiConfig(parseVariant(R"json({"broker":"broker.example","port":1883,"username":"","password":""})json", &blank_credentials_doc), &blank_credentials));
  assert(blank_credentials.username[0] == '\0' && blank_credentials.password[0] == '\0');
  JsonDocument missing_fields_doc;
  CuckooPiConfig missing_fields{};
  assert(parseCuckooPiConfig(parseVariant("{}", &missing_fields_doc),
                             &missing_fields));
  assert(missing_fields.enabled);
  assert(missing_fields.device_count == 0);

  JsonDocument explicitly_disabled_doc;
  CuckooPiConfig explicitly_disabled{};
  assert(parseCuckooPiConfig(
      parseVariant(R"json({"enabled":false,"devices":[]})json",
                   &explicitly_disabled_doc),
      &explicitly_disabled));
  assert(!explicitly_disabled.enabled);
  assert(explicitly_disabled.device_count == 0);

  JsonDocument legacy_devices_doc;
  CuckooPiConfig legacy_devices{};
  assert(parseCuckooPiConfig(
      parseVariant(R"json({"devices":[{"id":"001122334455"}]})json",
                   &legacy_devices_doc),
      &legacy_devices));
  assert(legacy_devices.enabled);
  assert(legacy_devices.device_count == 1);
  assert(std::strcmp(legacy_devices.devices[0].id, "001122334455") == 0);

  JsonDocument explicit_empty_default_auth_doc;
  CuckooPiConfig explicit_empty_default_auth{};
  assert(parseCuckooPiConfig(
      parseVariant(
          R"json({"broker":"","port":2884,"client_id":"","username":"","password":""})json",
          &explicit_empty_default_auth_doc),
      &explicit_empty_default_auth));
  assert(std::strcmp(explicit_empty_default_auth.client_id,
                     "") == 0);
  assert(std::strcmp(explicit_empty_default_auth.username,
                     "") == 0);
  assert(std::strcmp(explicit_empty_default_auth.password,
                     "") == 0);

  JsonDocument legacy_default_port_doc;
  CuckooPiConfig legacy_default_port{};
  assert(parseCuckooPiConfig(
      parseVariant(
          R"json({"broker":"","port":2884,"client_id":"","username":"","password":""})json",
          &legacy_default_port_doc),
      &legacy_default_port));
  assert(legacy_default_port.port == 1884);
  assert(std::strcmp(legacy_default_port.client_id,
                     "") == 0);
  assert(std::strcmp(legacy_default_port.username,
                     "") == 0);
  assert(std::strcmp(legacy_default_port.password,
                     "") == 0);

  JsonDocument custom_missing_auth_doc;
  CuckooPiConfig custom_missing_auth{};
  assert(parseCuckooPiConfig(
      parseVariant(R"json({"broker":"synthetic-broker.test","port":1883})json",
                   &custom_missing_auth_doc),
      &custom_missing_auth));
  assert(std::strcmp(custom_missing_auth.broker, "synthetic-broker.test") ==
         0);
  assert(custom_missing_auth.port == 1883);
  assert(custom_missing_auth.client_id[0] == '\0');
  assert(custom_missing_auth.username[0] == '\0');
  assert(custom_missing_auth.password[0] == '\0');

  JsonDocument explicit_empty_auth_doc;
  CuckooPiConfig explicit_empty_auth{};
  assert(parseCuckooPiConfig(
      parseVariant(
          R"json({"broker":"synthetic-broker.test","port":1883,"client_id":"","username":"","password":""})json",
          &explicit_empty_auth_doc),
      &explicit_empty_auth));
  assert(explicit_empty_auth.client_id[0] == '\0');
  assert(explicit_empty_auth.username[0] == '\0');
  assert(explicit_empty_auth.password[0] == '\0');

  const std::string valid_length_json =
      std::string(R"json({"broker":"synthetic-broker.test","port":1883,"client_id":")json") +
      std::string(39, 'c') +
      R"json(","username":")json" + std::string(63, 'u') +
      R"json(","password":")json" + std::string(63, 'p') + R"json("})json";
  JsonDocument valid_length_doc;
  CuckooPiConfig valid_length{};
  assert(parseCuckooPiConfig(parseVariant(valid_length_json.c_str(),
                                          &valid_length_doc),
                             &valid_length));
  assert(std::strlen(valid_length.client_id) == 39U);
  assert(std::strlen(valid_length.username) == 63U);
  assert(std::strlen(valid_length.password) == 63U);

  const std::string long_client_id_json =
      std::string(R"json({"client_id":")json") + std::string(40, 'c') +
      R"json("})json";
  const std::string long_username_json =
      std::string(R"json({"username":")json") + std::string(64, 'u') +
      R"json("})json";
  const std::string long_password_json =
      std::string(R"json({"password":")json") + std::string(64, 'p') +
      R"json("})json";
  JsonDocument long_client_id_doc;
  JsonDocument long_username_doc;
  JsonDocument long_password_doc;
  CuckooPiConfig invalid_credentials{};
  assert(!parseCuckooPiConfig(parseVariant(long_client_id_json.c_str(),
                                           &long_client_id_doc),
                              &invalid_credentials));
  assert(!parseCuckooPiConfig(parseVariant(long_username_json.c_str(),
                                           &long_username_doc),
                              &invalid_credentials));
  assert(!parseCuckooPiConfig(parseVariant(long_password_json.c_str(),
                                           &long_password_doc),
                              &invalid_credentials));
  JsonDocument invalid_type_doc;
  assert(!parseCuckooPiConfig(
      parseVariant(R"json({"username":123})json", &invalid_type_doc),
      &invalid_credentials));

  JsonDocument bounds_doc;
  const char* bounds_json =
      R"json({"enabled":true,"broker":"mqtt.local","port":65535,"client_id":"client-a","username":"user-a","password":"pass-a","devices":[{"id":"001122334455","offline_alarm_enabled":false,"offline_timeout_seconds":1,"beer_deviation_alarm_enabled":false,"beer_deviation_threshold_c":99,"beer_deviation_hold_seconds":999999,"offline_alarm_dismissed":true,"offline_dismissed_status_time_seconds":123},{"id":"aabbccddeeff","offline_timeout_seconds":86401,"beer_deviation_threshold_c":0,"beer_deviation_hold_seconds":1}]})json";
  CuckooPiConfig bounded{};
  assert(parseCuckooPiConfig(parseVariant(bounds_json, &bounds_doc),
                             &bounded));
  assert(std::strcmp(bounded.broker, "mqtt.local") == 0);
  assert(bounded.port == 65535);
  assert(std::strcmp(bounded.client_id, "client-a") == 0);
  assert(std::strcmp(bounded.username, "user-a") == 0);
  assert(std::strcmp(bounded.password, "pass-a") == 0);
  assert(bounded.device_count == 2);
  assert(!bounded.devices[0].offline_alarm_enabled);
  assert(bounded.devices[0].offline_timeout_seconds == 60U);
  assert(!bounded.devices[0].beer_deviation_alarm_enabled);
  assert(bounded.devices[0].beer_deviation_threshold_c == 20.0f);
  assert(bounded.devices[0].beer_deviation_hold_seconds == 86400U);
  assert(bounded.devices[0].offline_alarm_dismissed);
  assert(bounded.devices[0].offline_dismissed_status_time_seconds == 123U);
  assert(bounded.devices[1].offline_timeout_seconds == 86400U);
  assert(bounded.devices[1].beer_deviation_threshold_c == 0.1f);
  assert(bounded.devices[1].beer_deviation_hold_seconds == 60U);

  JsonDocument duplicate_doc;
  const char* duplicate_json =
      R"json({"devices":[{"id":"001122334455"},{"id":"00:11:22:33:44:55"}]})json";
  CuckooPiConfig unchanged = defaults;
  unchanged.port = 1883;
  assert(!parseCuckooPiConfig(parseVariant(duplicate_json, &duplicate_doc),
                              &unchanged));
  assert(unchanged.port == 1883);

  JsonDocument invalid_id_doc;
  assert(!parseCuckooPiConfig(
      parseVariant(R"json({"devices":[{"id":"not-an-id"}]})json",
                   &invalid_id_doc),
      &unchanged));

  JsonDocument too_many_doc;
  const char* too_many_json =
      R"json({"devices":[{"id":"000000000000"},{"id":"000000000001"},{"id":"000000000002"},{"id":"000000000003"},{"id":"000000000004"},{"id":"000000000005"},{"id":"000000000006"},{"id":"000000000007"},{"id":"000000000008"}]})json";
  assert(!parseCuckooPiConfig(parseVariant(too_many_json, &too_many_doc),
                              &unchanged));

  JsonDocument invalid_transport_doc;
  const char* invalid_transport_json =
      R"json({"broker":"","port":0,"devices":[]})json";
  CuckooPiConfig transport{};
  assert(parseCuckooPiConfig(
      parseVariant(invalid_transport_json, &invalid_transport_doc),
      &transport));
  assert(std::strcmp(transport.broker, "") == 0);
  assert(transport.port == 1884);
  assert(std::strcmp(transport.client_id, "") == 0);
  assert(std::strcmp(transport.username, "") == 0);
  assert(transport.password[0] == '\0');
  assert(transport.device_count == 0);

  JsonDocument serialized_doc;
  JsonObject parent = serialized_doc.to<JsonObject>();
  appendCuckooPiConfig(parent, bounded);
  assert(!serialized_doc.overflowed());
  assert(parent["cuckoo_pi"]["enabled"].as<bool>());
  assert(parent["cuckoo_pi"]["devices"].size() == 2);

  CuckooPiConfig restored{};
  assert(parseCuckooPiConfig(parent["cuckoo_pi"], &restored));
  assert(restored.enabled == bounded.enabled);
  assert(std::strcmp(restored.broker, bounded.broker) == 0);
  assert(restored.port == bounded.port);
  assert(std::strcmp(restored.client_id, bounded.client_id) == 0);
  assert(std::strcmp(restored.username, bounded.username) == 0);
  assert(std::strcmp(restored.password, bounded.password) == 0);
  assert(restored.device_count == bounded.device_count);
  for (uint8_t index = 0; index < restored.device_count; ++index) {
    assert(std::strcmp(restored.devices[index].id,
                       bounded.devices[index].id) == 0);
    assert(restored.devices[index].offline_alarm_enabled ==
           bounded.devices[index].offline_alarm_enabled);
    assert(restored.devices[index].offline_timeout_seconds ==
           bounded.devices[index].offline_timeout_seconds);
    assert(restored.devices[index].beer_deviation_alarm_enabled ==
           bounded.devices[index].beer_deviation_alarm_enabled);
    assert(restored.devices[index].beer_deviation_threshold_c ==
           bounded.devices[index].beer_deviation_threshold_c);
    assert(restored.devices[index].beer_deviation_hold_seconds ==
           bounded.devices[index].beer_deviation_hold_seconds);
    assert(restored.devices[index].offline_alarm_dismissed ==
           bounded.devices[index].offline_alarm_dismissed);
    assert(restored.devices[index].offline_dismissed_status_time_seconds ==
           bounded.devices[index].offline_dismissed_status_time_seconds);
  }

  assert(!parseCuckooPiConfig(JsonVariantConst(), &restored));
  assert(!parseCuckooPiConfig(parent["cuckoo_pi"], nullptr));
  return 0;
}
