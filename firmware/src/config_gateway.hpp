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
#ifndef SRC_CONFIG_GATEWAY_HPP_
#define SRC_CONFIG_GATEWAY_HPP_

enum class GatewayConfigSavePreflightResult { Skip, Reject, Proceed };

inline GatewayConfigSavePreflightResult gatewayConfigSavePreflight(
    bool saveNeeded, bool storageReady) {
  if (!saveNeeded) return GatewayConfigSavePreflightResult::Skip;
  return storageReady ? GatewayConfigSavePreflightResult::Proceed
                      : GatewayConfigSavePreflightResult::Reject;
}

template <typename StorageReadyFn>
inline GatewayConfigSavePreflightResult gatewayConfigSavePreflight(
    bool saveNeeded, StorageReadyFn storageReady) {
  if (!saveNeeded) return GatewayConfigSavePreflightResult::Skip;
  return storageReady() ? GatewayConfigSavePreflightResult::Proceed
                        : GatewayConfigSavePreflightResult::Reject;
}

template <typename BackupFn>
inline void gatewayBackupAfterVerifiedCommit(bool committed,
                                             BackupFn backup) {
  if (committed) backup();
}

#if defined(GATEWAY)

#include <config_brewing.hpp>
#include "cuckoo_pi_config.hpp"
#include "gateway_config_boot_state.hpp"
#include "gateway_device_settings.hpp"

#include <math.h>
constexpr uint8_t DEFAULT_RING_COLOR_INDEX = RING_COLOR_DEFAULT_INDEX;

struct GatewayDisplaySettings {
  uint16_t auto_switch_seconds = 4;
  uint8_t brightness_percent = 100;
  uint16_t sleep_seconds = 0;
};

constexpr auto CONFIG_GRAVITY_UNIT = "gravity_unit";
constexpr auto CONFIG_BLE_ENABLE = "ble_enable";
constexpr auto CONFIG_BLE_ACTIVE_SCAN = "ble_active_scan";
constexpr auto CONFIG_BLE_SCAN_TIME = "ble_scan_time";
constexpr auto CONFIG_PUSH_RESEND_TIME = "push_resend_time";
constexpr auto CONFIG_TIMEZONE = "timezone";
constexpr auto CONFIG_PRESSURE_UNIT = "pressure_unit";

constexpr auto CONFIG_SD_LOG_FILES = "sd_log_files";
constexpr auto CONFIG_SD_LOG_MIN_TIME = "sd_log_min_time";

constexpr auto CONFIG_DISPLAY_LAYOUT_ID = "display_layout_id";
constexpr auto CONFIG_AUTO_SWITCH_SECONDS = "auto_switch_seconds";
constexpr auto CONFIG_DISPLAY_BRIGHTNESS = "display_brightness";
constexpr auto CONFIG_DISPLAY_SLEEP_SECONDS = "display_sleep_seconds";
constexpr auto CONFIG_DEVICE_SETTINGS = "device_settings";
constexpr auto CONFIG_CUCKOO_PI = "cuckoo_pi";

constexpr auto CONFIG_HTTP_POST_GRAVITY_ENABLE = "http_post_gravity";
constexpr auto CONFIG_HTTP_POST_PRESSURE_ENABLE = "http_post_pressure";
constexpr auto CONFIG_HTTP_POST2_GRAVITY_ENABLE = "http_post2_gravity";
constexpr auto CONFIG_HTTP_POST2_PRESSURE_ENABLE = "http_post2_pressure";
constexpr auto CONFIG_HTTP_GET_GRAVITY_ENABLE = "http_get_gravity";
constexpr auto CONFIG_HTTP_GET_PRESSURE_ENABLE = "http_get_pressure";
constexpr auto CONFIG_INFLUXDB2_GRAVITY_ENABLE = "influxdb2_gravity";
constexpr auto CONFIG_INFLUXDB2_PRESSURE_ENABLE = "influxdb2_pressure";
constexpr auto CONFIG_MQTT_GRAVITY_ENABLE = "mqtt_gravity";
constexpr auto CONFIG_MQTT_PRESSURE_ENABLE = "mqtt_pressure";

constexpr auto PRESSURE_KPA = "kPa";
constexpr auto PRESSURE_BAR = "Bar";
constexpr auto PRESSURE_PSI = "PSI";

class GravmonGatewayConfig : public BrewingConfig {
 private:
  char _gravityUnit = 'G';

  String _pressureUnit = PRESSURE_PSI;
  String _timezone = "CET-1CEST,M3.5.0,M10.5.0/3";

  int _bleScanTime = 5;
  int _pushResendTime = 300;

  int _sdLogFiles = 4;
  int _sdLogMinTime = 5;  // Minutes

  int _displayLayoutId = 2;

  GatewayDisplaySettings _displaySettings;
  GravityDeviceSettings _deviceSettings[MAX_GRAVITY_DEVICE_SETTINGS];
  uint8_t _deviceSettingsCount = 0;
  CuckooPiConfig _cuckooPiConfig = defaultCuckooPiConfig();
  String _configFileName;
  SemaphoreHandle_t _deviceSettingsMutex = nullptr;
  GatewayConfigBootState _storageState;

  bool _bleEnable = true;
  bool _bleActiveScan = false;
  bool _httpPostGravityEnable = false;
  bool _httpPostPressureEnable = false;
  bool _httpPost2GravityEnable = true;
  bool _httpPost2PressureEnable = true;
  bool _httpGetGravityEnable = true;
  bool _httpGetPressureEnable = true;
  bool _influxdb2GravityEnable = true;
  bool _influxdb2PressureEnable = true;
  bool _mqttGravityEnable = true;
  bool _mqttPressureEnable = true;

  void lockDeviceSettings() const;
  void unlockDeviceSettings() const;
  void createJsonLocked(JsonObject& doc) const;
  void parseJsonLocked(JsonObject& doc);
  bool saveFileLocked();
  bool saveFileVerified(const char* data, size_t size);
  void backupWifiPreferencesAfterSuccessfulSave();

 public:
  GravmonGatewayConfig(String baseMDNS, String fileName);

  int getVoltagePin() const { return 0; }

  const char* getTimezone() const { return _timezone.c_str(); }
  void setTimezone(String s) {
    _timezone = s;
    _saveNeeded = true;
  }

  bool isBleActive() const {
    return true;
  }  // Used in common code for reporting if push targets are defined.

  bool isBleEnable() const { return _bleEnable; }
  void setBleEnable(bool f) {
    _bleEnable = f;
    _saveNeeded = true;
  }

  int getSdLogFiles() const { return _sdLogFiles; }
  void setSdLogFiles(int i) {
    _sdLogFiles = i;
    _saveNeeded = true;
  }

  int getSdLogMinTime() const { return _sdLogMinTime; }
  void setSdLogMinTime(int i) {
    _sdLogMinTime = i;
    _saveNeeded = true;
  }

  int getDisplayLayoutId() const { return _displayLayoutId; }
  void setDisplayLayoutId(int i) {
    _displayLayoutId = i;
    _saveNeeded = true;
  }

  uint16_t getAutoSwitchSeconds() const {
    return _displaySettings.auto_switch_seconds;
  }
  void setAutoSwitchSeconds(uint16_t seconds) {
    _displaySettings.auto_switch_seconds =
        seconds < 1 ? 1 : (seconds > 60 ? 60 : seconds);
    _saveNeeded = true;
  }

  uint8_t getDisplayBrightness() const {
    return _displaySettings.brightness_percent;
  }
  void setDisplayBrightness(uint8_t percent) {
    _displaySettings.brightness_percent = percent > 100 ? 100 : percent;
    _saveNeeded = true;
  }

  uint16_t getDisplaySleepSeconds() const {
    return _displaySettings.sleep_seconds;
  }
  void setDisplaySleepSeconds(uint16_t seconds) {
    _displaySettings.sleep_seconds = seconds;
    _saveNeeded = true;
  }

  static GravityDeviceSettings normalizeDeviceSettings(
      const GravityDeviceSettings& settings) {
    return normalize_gateway_device_settings(settings);
  }
  bool copyDeviceSettings(const char* deviceId,
                          GravityDeviceSettings* settings) const;
  bool copyDeviceSettingsOrDefault(const char* deviceId,
                                   GravityDeviceSettings* settings) const;
  // Indexed access for iterating all configured devices (used at boot to
  // restore persisted alarm state).
  bool copyDeviceSettingsByIndex(uint8_t index,
                                 GravityDeviceSettings* settings) const;
  void setDeviceSettings(const GravityDeviceSettings& settings);
  uint8_t getDeviceSettingsCount() const;
  bool saveDeviceSettings(const GravityDeviceSettings& settings);
  bool copyCuckooPiConfig(CuckooPiConfig* config) const;
  void setCuckooPiConfig(const CuckooPiConfig& config);
  bool saveCuckooPiDeviceSettings(
      const CuckooPiDeviceConfig& settings);
  bool saveCuckooPiOfflineDismissal(const char* device_id, bool dismissed,
                                    uint32_t status_time_seconds = 0U);
  bool recoverConfigFile();
  bool isConfigStorageReady() const { return _storageState.storageReady(); }

  bool isPressureBar() const { return _pressureUnit == PRESSURE_BAR; }
  bool isPressureKpa() const { return _pressureUnit == PRESSURE_KPA; }
  bool isPressurePsi() const { return _pressureUnit == PRESSURE_PSI; }

  const char* getPressureUnit() const { return _pressureUnit.c_str(); }
  void setPressureUnit(String s) {
    _pressureUnit = s;
    _saveNeeded = true;
  }

  bool isHttpPostGravityEnable() const { return _httpPostGravityEnable; }
  void setHttpPostGravityEnable(bool f) {
    _httpPostGravityEnable = f;
    _saveNeeded = true;
  }

  bool isHttpPostPressureEnable() const { return _httpPostPressureEnable; }
  void setHttpPostPressureEnable(bool f) {
    _httpPostPressureEnable = f;
    _saveNeeded = true;
  }

  bool isHttpPost2GravityEnable() const { return _httpPost2GravityEnable; }
  void setHttpPost2GravityEnable(bool f) {
    _httpPost2GravityEnable = f;
    _saveNeeded = true;
  }

  bool isHttpPost2PressureEnable() const { return _httpPost2PressureEnable; }
  void setHttpPost2PressureEnable(bool f) {
    _httpPost2PressureEnable = f;
    _saveNeeded = true;
  }

  bool isHttpGetGravityEnable() const { return _httpGetGravityEnable; }
  void setHttpGetGravityEnable(bool f) {
    _httpGetGravityEnable = f;
    _saveNeeded = true;
  }

  bool isHttpGetPressureEnable() const { return _httpGetPressureEnable; }
  void setHttpGetPressureEnable(bool f) {
    _httpGetPressureEnable = f;
    _saveNeeded = true;
  }

  bool isInfluxdb2GravityEnable() const { return _influxdb2GravityEnable; }
  void setInfluxdb2GravityEnable(bool f) {
    _influxdb2GravityEnable = f;
    _saveNeeded = true;
  }

  bool isInfluxdb2PressureEnable() const { return _influxdb2PressureEnable; }
  void setInfluxdb2PressureEnable(bool f) {
    _influxdb2PressureEnable = f;
    _saveNeeded = true;
  }

  bool isMqttGravityEnable() const { return _mqttGravityEnable; }
  void setMqttGravityEnable(bool f) {
    _mqttGravityEnable = f;
    _saveNeeded = true;
  }

  bool isMqttPressureEnable() const { return _mqttPressureEnable; }
  void setMqttPressureEnable(bool f) {
    _mqttPressureEnable = f;
    _saveNeeded = true;
  }

  int getBleScanTime() const { return _bleScanTime; }
  void setBleScanTime(int v) {
    _bleScanTime = v;
    _saveNeeded = true;
  }

  int getPushResendTime() const { return _pushResendTime; }
  void setPushResendTime(int t) {
    _pushResendTime = t;
    _saveNeeded = true;
  }

  bool getBleActiveScan() const { return _bleActiveScan; }
  void setBleActiveScan(bool b) {
    _bleActiveScan = b;
    _saveNeeded = true;
  }

  char getGravityUnit() const { return _gravityUnit; }
  void setGravityUnit(char c) {
    if (c == 'G' || c == 'P') {
      _gravityUnit = c;
      _saveNeeded = true;
    }
  }
  bool isGravitySG() const { return _gravityUnit == 'G'; }
  bool isGravityPlato() const { return _gravityUnit == 'P'; }

  bool isWifiPushActive() const {
    return (hasTargetHttpPost() || hasTargetHttpPost2() || hasTargetHttpGet() ||
            hasTargetInfluxDb2() || hasTargetMqtt())
               ? true
               : false;
  }

  // IO functions
  void createJson(JsonObject& doc) const;
  void parseJson(JsonObject& doc);
  void createJsonWithDeviceSettings(JsonObject& doc,
                                    const GravityDeviceSettings* settings,
                                    uint8_t count) const;
  bool saveFile() override;
};

extern GravmonGatewayConfig myConfig;

#endif  // GATEWAY

#endif  // SRC_CONFIG_GATEWAY_HPP_

// EOF
