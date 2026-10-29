/*
MIT License

Copyright (c) 2021-2025 Magnus

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
 */
#if defined(GATEWAY)

#include <config_gateway.hpp>
#include <gateway_config_storage.hpp>
#include <gateway_device_settings_json.hpp>
#include <log.hpp>
#include <main.hpp>
#include <LittleFS.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <new>
#include <string>
#include "cuckoo_pi_model.hpp"

namespace {
class LittleFsConfigFileOperations : public ConfigFileOperations {
 public:
  bool exists(const std::string& path) const override {
    return LittleFS.exists(path.c_str());
  }

  size_t write(const std::string& path, const char* data,
               size_t size) override {
    File file = LittleFS.open(path.c_str(), "w");
    if (!file) return 0;
    const size_t written =
        file.write(reinterpret_cast<const uint8_t*>(data), size);
    file.flush();
    file.close();
    return written;
  }

  bool remove(const std::string& path) override {
    return LittleFS.remove(path.c_str());
  }

  bool rename(const std::string& from, const std::string& to) override {
    return LittleFS.rename(from.c_str(), to.c_str());
  }

  size_t fileSize(const std::string& path) const override {
    File file = LittleFS.open(path.c_str(), "r");
    if (!file) return 0;
    const size_t size = file.size();
    file.close();
    return size;
  }

  size_t read(const std::string& path, size_t offset, char* data,
              size_t size) const override {
    if (!data || size == 0) return 0;
    File file = LittleFS.open(path.c_str(), "r");
    if (!file || !file.seek(offset)) {
      if (file) file.close();
      return 0;
    }
    const size_t read_size = file.readBytes(data, size);
    file.close();
    return read_size;
  }

  bool isValidJson(const std::string& path) const override {
    File file = LittleFS.open(path.c_str(), "r");
    if (!file) return false;
    JsonDocument doc;
    const DeserializationError error = deserializeJson(doc, file);
    file.close();
    return !error && doc.is<JsonObject>();
  }
};
}

GravmonGatewayConfig::GravmonGatewayConfig(String baseMDNS, String fileName)
    : BrewingConfig(baseMDNS, fileName), _configFileName(fileName) {
  setTargetHttpPost("http://api.ckbrew.com/rest/public/deviceData");
  _deviceSettingsMutex = xSemaphoreCreateRecursiveMutex();
}

void GravmonGatewayConfig::lockDeviceSettings() const {
  if (_deviceSettingsMutex) xSemaphoreTakeRecursive(_deviceSettingsMutex, portMAX_DELAY);
}

void GravmonGatewayConfig::unlockDeviceSettings() const {
  if (_deviceSettingsMutex) xSemaphoreGiveRecursive(_deviceSettingsMutex);
}

void GravmonGatewayConfig::createJson(JsonObject& doc) const {
  lockDeviceSettings();
  createJsonLocked(doc);
  unlockDeviceSettings();
}

void GravmonGatewayConfig::createJsonLocked(JsonObject& doc) const {
  BrewingConfig::createJson(doc);

  // Call base class functions
  createJsonBase(doc);
  createJsonWifi(doc);
  createJsonPush(doc);

  doc[CONFIG_GRAVITY_UNIT] = String(getGravityUnit());
  doc[CONFIG_TIMEZONE] = getTimezone();
  doc[CONFIG_BLE_ACTIVE_SCAN] = getBleActiveScan();
  doc[CONFIG_BLE_ENABLE] = isBleEnable();
  doc[CONFIG_BLE_SCAN_TIME] = getBleScanTime();
  doc[CONFIG_PUSH_RESEND_TIME] = getPushResendTime();
  doc[CONFIG_PRESSURE_UNIT] = getPressureUnit();
  doc[CONFIG_SD_LOG_FILES] = getSdLogFiles();
  doc[CONFIG_SD_LOG_MIN_TIME] = getSdLogMinTime();
  doc[CONFIG_DISPLAY_LAYOUT_ID] = getDisplayLayoutId();
  doc[CONFIG_AUTO_SWITCH_SECONDS] = getAutoSwitchSeconds();
  doc[CONFIG_DISPLAY_BRIGHTNESS] = getDisplayBrightness();
  doc[CONFIG_DISPLAY_SLEEP_SECONDS] = getDisplaySleepSeconds();

  JsonArray devices = doc[CONFIG_DEVICE_SETTINGS].to<JsonArray>();
  append_gateway_device_settings_json(devices, _deviceSettings,
                                      _deviceSettingsCount);
  appendCuckooPiConfig(doc, _cuckooPiConfig);

  doc[CONFIG_HTTP_POST_GRAVITY_ENABLE] = isHttpPostGravityEnable();
  doc[CONFIG_HTTP_POST_PRESSURE_ENABLE] = isHttpPostPressureEnable();
  doc[CONFIG_HTTP_POST2_GRAVITY_ENABLE] = isHttpPost2GravityEnable();
  doc[CONFIG_HTTP_POST2_PRESSURE_ENABLE] = isHttpPost2PressureEnable();
  doc[CONFIG_HTTP_GET_GRAVITY_ENABLE] = isHttpGetGravityEnable();
  doc[CONFIG_HTTP_GET_PRESSURE_ENABLE] = isHttpGetPressureEnable();
  doc[CONFIG_INFLUXDB2_GRAVITY_ENABLE] = isInfluxdb2GravityEnable();
  doc[CONFIG_INFLUXDB2_PRESSURE_ENABLE] = isInfluxdb2PressureEnable();
  doc[CONFIG_MQTT_GRAVITY_ENABLE] = isMqttGravityEnable();
  doc[CONFIG_MQTT_PRESSURE_ENABLE] = isMqttPressureEnable();
}

void GravmonGatewayConfig::parseJson(JsonObject& doc) {
  lockDeviceSettings();
  parseJsonLocked(doc);
  unlockDeviceSettings();
}

void GravmonGatewayConfig::parseJsonLocked(JsonObject& doc) {
  BrewingConfig::parseJson(doc);

  // Call base class functions
  parseJsonBase(doc);
  parseJsonWifi(doc);
  parseJsonPush(doc);

  if (!doc[CONFIG_GRAVITY_UNIT].isNull()) {
    String s = doc[CONFIG_GRAVITY_UNIT];
    setGravityUnit(s.charAt(0));
  }
  if (!doc["gravity_format"].isNull()) {  // Legacy support for gravity_format
    String s = doc["gravity_format"];
    setGravityUnit(s.charAt(0));
  }

  if (!doc[CONFIG_TIMEZONE].isNull()) setTimezone(doc[CONFIG_TIMEZONE]);
  if (!doc[CONFIG_BLE_ENABLE].isNull())
    setBleEnable(doc[CONFIG_BLE_ENABLE].as<bool>());
  if (!doc[CONFIG_BLE_ACTIVE_SCAN].isNull())
    setBleActiveScan(doc[CONFIG_BLE_ACTIVE_SCAN].as<bool>());
  if (!doc[CONFIG_BLE_SCAN_TIME].isNull())
    setBleScanTime(doc[CONFIG_BLE_SCAN_TIME].as<int>());
  if (!doc[CONFIG_PUSH_RESEND_TIME].isNull())
    setPushResendTime(doc[CONFIG_PUSH_RESEND_TIME].as<int>());
  if (!doc[CONFIG_PRESSURE_UNIT].isNull())
    setPressureUnit(doc[CONFIG_PRESSURE_UNIT].as<String>());
  if (!doc[CONFIG_SD_LOG_FILES].isNull())
    setSdLogFiles(doc[CONFIG_SD_LOG_FILES].as<int>());
  if (!doc[CONFIG_SD_LOG_MIN_TIME].isNull())
    setSdLogMinTime(doc[CONFIG_SD_LOG_MIN_TIME].as<int>());
  if (!doc[CONFIG_DISPLAY_LAYOUT_ID].isNull())
    setDisplayLayoutId(doc[CONFIG_DISPLAY_LAYOUT_ID].as<int>());
  if (!doc[CONFIG_AUTO_SWITCH_SECONDS].isNull())
    setAutoSwitchSeconds(doc[CONFIG_AUTO_SWITCH_SECONDS].as<uint16_t>());
  if (!doc[CONFIG_DISPLAY_BRIGHTNESS].isNull())
    setDisplayBrightness(doc[CONFIG_DISPLAY_BRIGHTNESS].as<uint8_t>());
  if (!doc[CONFIG_DISPLAY_SLEEP_SECONDS].isNull())
    setDisplaySleepSeconds(doc[CONFIG_DISPLAY_SLEEP_SECONDS].as<uint16_t>());

  if (!doc[CONFIG_DEVICE_SETTINGS].isNull()) {
    JsonArray devices = doc[CONFIG_DEVICE_SETTINGS].as<JsonArray>();
    const size_t jsonSize = measureJson(devices);
    char* json = new char[jsonSize + 1];
    serializeJson(devices, json, jsonSize + 1);
    GravityDeviceSettings parsed[MAX_GRAVITY_DEVICE_SETTINGS]{};
    uint8_t parsedCount = 0;
    if (parse_gateway_device_settings(json, jsonSize, parsed, &parsedCount)) {
      memcpy(_deviceSettings, parsed, sizeof(parsed));
      _deviceSettingsCount = parsedCount;
    } else {
      Log.error(F("CFG : Rejecting invalid device_settings array." CR));
    }
    delete[] json;
  }

  if (!doc[CONFIG_CUCKOO_PI].isNull()) {
    CuckooPiConfig parsed = defaultCuckooPiConfig();
    if (parseCuckooPiConfig(doc[CONFIG_CUCKOO_PI], &parsed)) {
      _cuckooPiConfig = parsed;
      _saveNeeded = true;
    } else {
      Log.error(F("CFG : Rejecting invalid cuckoo_pi configuration." CR));
    }
  }

  if (!doc[CONFIG_HTTP_POST_GRAVITY_ENABLE].isNull())
    setHttpPostGravityEnable(doc[CONFIG_HTTP_POST_GRAVITY_ENABLE].as<bool>());
  if (!doc[CONFIG_HTTP_POST_PRESSURE_ENABLE].isNull())
    setHttpPostPressureEnable(doc[CONFIG_HTTP_POST_PRESSURE_ENABLE].as<bool>());

  if (!doc[CONFIG_HTTP_POST2_GRAVITY_ENABLE].isNull())
    setHttpPost2GravityEnable(doc[CONFIG_HTTP_POST2_GRAVITY_ENABLE].as<bool>());
  if (!doc[CONFIG_HTTP_POST2_PRESSURE_ENABLE].isNull())
    setHttpPost2PressureEnable(
        doc[CONFIG_HTTP_POST2_PRESSURE_ENABLE].as<bool>());

  if (!doc[CONFIG_HTTP_GET_GRAVITY_ENABLE].isNull())
    setHttpGetGravityEnable(doc[CONFIG_HTTP_GET_GRAVITY_ENABLE].as<bool>());
  if (!doc[CONFIG_HTTP_GET_PRESSURE_ENABLE].isNull())
    setHttpGetPressureEnable(doc[CONFIG_HTTP_GET_PRESSURE_ENABLE].as<bool>());
  if (!doc[CONFIG_INFLUXDB2_GRAVITY_ENABLE].isNull())
    setInfluxdb2GravityEnable(doc[CONFIG_INFLUXDB2_GRAVITY_ENABLE].as<bool>());
  if (!doc[CONFIG_INFLUXDB2_PRESSURE_ENABLE].isNull())
    setInfluxdb2PressureEnable(
        doc[CONFIG_INFLUXDB2_PRESSURE_ENABLE].as<bool>());
  if (!doc[CONFIG_MQTT_GRAVITY_ENABLE].isNull())
    setMqttGravityEnable(doc[CONFIG_MQTT_GRAVITY_ENABLE].as<bool>());
  if (!doc[CONFIG_MQTT_PRESSURE_ENABLE].isNull())
    setMqttPressureEnable(doc[CONFIG_MQTT_PRESSURE_ENABLE].as<bool>());
}

bool GravmonGatewayConfig::copyDeviceSettings(
    const char* deviceId, GravityDeviceSettings* settings) const {
  if (!settings || !gateway_device_id_is_valid(deviceId)) return false;
  lockDeviceSettings();

  for (uint8_t i = 0; i < _deviceSettingsCount; ++i) {
    if (strcmp(_deviceSettings[i].device_id, deviceId) == 0) {
      *settings = _deviceSettings[i];
      unlockDeviceSettings();
      return true;
    }
  }
  unlockDeviceSettings();
  return false;
}

bool GravmonGatewayConfig::copyCuckooPiConfig(CuckooPiConfig* config) const {
  if (!config) return false;
  lockDeviceSettings();
  *config = _cuckooPiConfig;
  unlockDeviceSettings();
  return true;
}

void GravmonGatewayConfig::setCuckooPiConfig(const CuckooPiConfig& config) {
  lockDeviceSettings();
  _cuckooPiConfig = config;
  _saveNeeded = true;
  unlockDeviceSettings();
}

bool GravmonGatewayConfig::saveCuckooPiDeviceSettings(
    const CuckooPiDeviceConfig& input) {
  CuckooPiDeviceConfig normalized = normalizeCuckooPiDeviceConfig(input);
  char normalized_id[13] = {};
  if (!normalizeCuckooPiId(normalized.id, normalized_id)) {
    Log.error(F("CFG : Invalid Cuckoo Pi ID; save rejected." CR));
    return false;
  }
  if (!_storageState.storageReady()) {
    Log.error(F("CFG : Cuckoo settings save blocked; storage unavailable." CR));
    return false;
  }

  lockDeviceSettings();
  CuckooPiConfig previous = _cuckooPiConfig;
  const bool previousSaveNeeded = _saveNeeded;
  bool found = false;
  for (uint8_t index = 0; index < _cuckooPiConfig.device_count &&
                            index < kMaxCuckooPiDevices;
       ++index) {
    if (strcmp(_cuckooPiConfig.devices[index].id, normalized_id) != 0)
      continue;
    CuckooPiDeviceConfig& target = _cuckooPiConfig.devices[index];
    target.offline_alarm_enabled = normalized.offline_alarm_enabled;
    target.offline_timeout_seconds = normalized.offline_timeout_seconds;
    target.beer_deviation_alarm_enabled =
        normalized.beer_deviation_alarm_enabled;
    target.beer_deviation_threshold_c = normalized.beer_deviation_threshold_c;
    target.beer_deviation_hold_seconds = normalized.beer_deviation_hold_seconds;
    found = true;
    break;
  }
  if (!found) {
    unlockDeviceSettings();
    Log.error(F("CFG : Cuckoo Pi device removed before save; rejected." CR));
    return false;
  }
  _saveNeeded = true;
  const bool persisted = saveFileLocked();
  if (!persisted) {
    _cuckooPiConfig = previous;
    _saveNeeded = previousSaveNeeded;
  }
  unlockDeviceSettings();
  return persisted;
}

bool GravmonGatewayConfig::saveCuckooPiOfflineDismissal(
    const char* device_id, bool dismissed, uint32_t status_time_seconds) {
  if (!_storageState.storageReady()) {
    Log.error(F("CFG : Cuckoo dismissal save blocked; storage unavailable."
                CR));
    return false;
  }
  lockDeviceSettings();
  CuckooPiConfig candidate = _cuckooPiConfig;
  if (!setCuckooPiOfflineDismissed(&candidate, device_id, dismissed,
                                   status_time_seconds)) {
    unlockDeviceSettings();
    return false;
  }
  const CuckooPiConfig previous = _cuckooPiConfig;
  const bool previousSaveNeeded = _saveNeeded;
  _cuckooPiConfig = candidate;
  _saveNeeded = true;
  const bool persisted = saveFileLocked();
  if (!persisted) {
    _cuckooPiConfig = previous;
    _saveNeeded = previousSaveNeeded;
  }
  unlockDeviceSettings();
  return persisted;
}

bool GravmonGatewayConfig::copyDeviceSettingsByIndex(
    uint8_t index, GravityDeviceSettings* settings) const {
  if (!settings) return false;
  lockDeviceSettings();
  if (index >= _deviceSettingsCount) {
    unlockDeviceSettings();
    return false;
  }
  *settings = _deviceSettings[index];
  unlockDeviceSettings();
  return true;
}

bool GravmonGatewayConfig::copyDeviceSettingsOrDefault(
    const char* deviceId, GravityDeviceSettings* settings) const {
  if (!settings || !gateway_device_id_is_valid(deviceId)) return false;
  if (copyDeviceSettings(deviceId, settings)) return true;
  *settings = GravityDeviceSettings();
  snprintf(settings->device_id, sizeof(settings->device_id), "%s", deviceId);
  return true;
}

void GravmonGatewayConfig::createJsonWithDeviceSettings(
    JsonObject& doc, const GravityDeviceSettings* settings, uint8_t count) const {
  lockDeviceSettings();
  createJsonLocked(doc);
  doc.remove(CONFIG_DEVICE_SETTINGS);
  JsonArray devices = doc[CONFIG_DEVICE_SETTINGS].to<JsonArray>();
  const uint8_t safeCount =
      count > MAX_GRAVITY_DEVICE_SETTINGS ? MAX_GRAVITY_DEVICE_SETTINGS : count;
  for (uint8_t i = 0; settings && i < safeCount; ++i) {
    if (!gateway_device_id_is_valid(settings[i].device_id)) continue;
    const GravityDeviceSettings normalized =
        normalize_gateway_device_settings(settings[i]);
    const char* normalizedDeviceId = normalized.device_id;
    JsonObject device = devices.add<JsonObject>();
    device["id"] = normalizedDeviceId;
    if (isnan(normalized.og_sg)) {
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
  unlockDeviceSettings();
}

void GravmonGatewayConfig::setDeviceSettings(
    const GravityDeviceSettings& input) {
  if (!gateway_device_id_is_valid(input.device_id)) return;
  const GravityDeviceSettings settings = normalize_gateway_device_settings(input);
  lockDeviceSettings();
  for (uint8_t i = 0; i < _deviceSettingsCount; ++i) {
    if (strcmp(_deviceSettings[i].device_id, settings.device_id) == 0) {
      _deviceSettings[i] = settings;
      _saveNeeded = true;
      unlockDeviceSettings();
      return;
    }
  }
  if (_deviceSettingsCount < MAX_GRAVITY_DEVICE_SETTINGS) {
    _deviceSettings[_deviceSettingsCount++] = settings;
    _saveNeeded = true;
  }
  unlockDeviceSettings();
}

uint8_t GravmonGatewayConfig::getDeviceSettingsCount() const {
  lockDeviceSettings();
  const uint8_t count = _deviceSettingsCount;
  unlockDeviceSettings();
  return count;
}

bool GravmonGatewayConfig::saveDeviceSettings(
    const GravityDeviceSettings& input) {
  if (!gateway_device_id_is_valid(input.device_id)) {
    Log.error(F("CFG : Invalid device ID; save rejected." CR));
    return false;
  }
  if (!_storageState.storageReady()) {
    Log.error(F("CFG : Save blocked; config storage is unavailable." CR));
    return false;
  }
  lockDeviceSettings();
  std::unique_ptr<GravityDeviceSettings[]> candidate(
      new (std::nothrow) GravityDeviceSettings[MAX_GRAVITY_DEVICE_SETTINGS]);
  if (!candidate) {
    Log.error(F("CFG : Device settings snapshot allocation failed; save rejected." CR));
    unlockDeviceSettings();
    return false;
  }
  memcpy(candidate.get(), _deviceSettings, sizeof(_deviceSettings));
  uint8_t count = _deviceSettingsCount;
  const GravityDeviceSettings normalized = normalize_gateway_device_settings(input);
  bool found = false;
  for (uint8_t i = 0; i < count; ++i) {
    if (strcmp(candidate[i].device_id, normalized.device_id) == 0) {
      candidate[i] = normalized;
      found = true;
      break;
    }
  }
  if (!found) {
    if (count >= MAX_GRAVITY_DEVICE_SETTINGS) {
      Log.error(F("CFG : Device settings capacity full; save rejected." CR));
      unlockDeviceSettings();
      return false;
    }
    candidate[count++] = normalized;
  }

  size_t size = 0;
  char* data = nullptr;
  {
    JsonDocument doc;
    JsonObject object = doc.to<JsonObject>();
    createJsonWithDeviceSettings(object, candidate.get(), count);
    if (doc.overflowed()) {
      Log.error(F("CFG : JSON allocation overflow; save rejected." CR));
      unlockDeviceSettings();
      return false;
    }
    size = measureJson(doc);
    data = new (std::nothrow) char[size + 1];
    if (!data) {
      Log.error(F("CFG : JSON buffer allocation failed; save rejected." CR));
      unlockDeviceSettings();
      return false;
    }
    const size_t serialized = serializeJson(doc, data, size + 1);
    if (serialized != size) {
      Log.error(F("CFG : JSON serialization incomplete (%u/%u); save rejected." CR),
                static_cast<uint64_t>(serialized), static_cast<uint64_t>(size));
      delete[] data;
      unlockDeviceSettings();
      return false;
    }
  }
  const bool persisted = saveFileVerified(data, size);
  delete[] data;
  if (persisted) {
    memcpy(_deviceSettings, candidate.get(), sizeof(_deviceSettings));
    _deviceSettingsCount = count;
    _saveNeeded = false;
  }
  gatewayBackupAfterVerifiedCommit(persisted, [this]() {
    backupWifiPreferencesAfterSuccessfulSave();
  });
  unlockDeviceSettings();
  return persisted;
}

bool GravmonGatewayConfig::saveFile() {
  const GatewayConfigSavePreflightResult preflight = gatewayConfigSavePreflight(
      _saveNeeded, [this]() { return _storageState.storageReady(); });
  if (preflight == GatewayConfigSavePreflightResult::Skip) return true;
  if (preflight == GatewayConfigSavePreflightResult::Reject) {
    Log.error(F("CFG : Save blocked; config storage is unavailable." CR));
    return false;
  }
  lockDeviceSettings();
  const bool result = saveFileLocked();
  unlockDeviceSettings();
  return result;
}

bool GravmonGatewayConfig::saveFileLocked() {
  size_t size = 0;
  char* data = nullptr;
  {
    JsonDocument doc;
    JsonObject object = doc.to<JsonObject>();
    createJsonLocked(object);
    if (doc.overflowed()) {
      Log.error(F("CFG : JSON allocation overflow; save rejected." CR));
      return false;
    }
    size = measureJson(doc);
    data = new (std::nothrow) char[size + 1];
    if (!data) {
      Log.error(F("CFG : JSON buffer allocation failed; save rejected." CR));
      return false;
    }
    const size_t serialized = serializeJson(doc, data, size + 1);
    if (serialized != size) {
      Log.error(F("CFG : JSON serialization incomplete (%u/%u); save rejected." CR),
                static_cast<uint64_t>(serialized), static_cast<uint64_t>(size));
      delete[] data;
      return false;
    }
  }
  const bool result = saveFileVerified(data, size);
  delete[] data;
  if (result) {
    _saveNeeded = false;
  }
  gatewayBackupAfterVerifiedCommit(result, [this]() {
    backupWifiPreferencesAfterSuccessfulSave();
  });
  return result;
}

void GravmonGatewayConfig::backupWifiPreferencesAfterSuccessfulSave() {
#if !defined(ESP8266)
  if (strlen(getWifiSSID(0)) > 0 && strlen(getWifiPass(0)) > 0) {
    setPreference("ssid", getWifiSSID(0), "espfwk");
    setPreference("pass", getWifiPass(0), "espfwk");
    Log.notice(F("CFG : Stored wifi settings in EEPROM as backup." CR));
  }
#endif
}

bool GravmonGatewayConfig::saveFileVerified(const char* data, size_t size) {
  LittleFsConfigFileOperations files;
  VerifiedConfigStorage storage(files, _configFileName.c_str());
  const ConfigStorageResult result =
      storage.replaceWithVerifiedJson(data, size);
  if (!result.ok() && !result.committed) {
    Log.error(F("CFG : Storage error: %s." CR),
              config_storage_error_name(result.error));
    if (result.error == ConfigStorageError::RollbackRenameFailed ||
        result.error == ConfigStorageError::RecoveryRestoreFailed) {
      _storageState.disableStorage();
    }
    return false;
  }
  if (!result.ok()) {
    Log.warning(F("CFG : Storage committed; cleanup warning: %s." CR),
                config_storage_error_name(result.error));
  }
  return result.committed;
}

bool GravmonGatewayConfig::recoverConfigFile() {
  LittleFsConfigFileOperations files;
  VerifiedConfigStorage storage(files, _configFileName.c_str());
  const ConfigStorageResult result = storage.recover();
  if (!result.ok()) {
    if (result.committed) {
      Log.warning(F("CFG : Recovery committed; cleanup warning: %s." CR),
                  config_storage_error_name(result.error));
    } else {
      Log.error(F("CFG : Recovery storage error: %s." CR),
                config_storage_error_name(result.error));
    }
  }
  return _storageState.markRecoveryResult(result.ok() || result.committed);
}

#endif  // GATEWAY

// EOF
