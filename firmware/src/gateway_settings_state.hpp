#pragma once

#include <cstdint>

#include "gateway_device_settings.hpp"

enum class SettingsPage : uint8_t {
  Home,
  Og,
  TemperatureChange,
  RingColor,
  OfflineAlarm,
};

using GatewaySettingsSaveCallback = bool (*)(
    const GravityDeviceSettings& settings, void* context);

bool gateway_settings_copy_display_device_id(
    const char* input,
    char output[GRAVITY_DEVICE_ID_CAPACITY]);

class GatewaySettingsUiState {
 public:
  bool openForDevice(const char* deviceId,
                     const GravityDeviceSettings& settings);
  bool enterPage(SettingsPage page);
  bool enterPage(SettingsPage page,
                 const GravityDeviceSettings& latestSettings);
  void back();
  bool save(GatewaySettingsSaveCallback callback, void* context);
  void onLayoutRebuild() {}
  void clear() { *this = GatewaySettingsUiState{}; }

  void adjustOg(int direction);
  void toggleTemperatureAlarm();
  void adjustTemperatureCompare(int direction);
  void adjustTemperatureThreshold(int direction);
  void selectRingColor(uint8_t index);
  void toggleOfflineAlarm();
  void adjustOfflineTimeout(int direction);

  bool isOpen() const { return _open; }
  SettingsPage page() const { return _page; }
  bool saveError() const { return _save_error; }
  const char* deviceId() const { return _device_id; }
  GravityDeviceSettings& draft() { return _draft; }
  const GravityDeviceSettings& draft() const { return _draft; }
  const GravityDeviceSettings& entrySnapshot() const {
    return _entry_snapshot;
  }

 private:
  bool _open = false;
  SettingsPage _page = SettingsPage::Home;
  GravityDeviceSettings _draft{};
  GravityDeviceSettings _entry_snapshot{};
  bool _save_error = false;
  char _device_id[GRAVITY_DEVICE_ID_CAPACITY] = {};

  void close();
  void restoreTargetId();
};
