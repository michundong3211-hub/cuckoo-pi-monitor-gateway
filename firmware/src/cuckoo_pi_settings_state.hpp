#pragma once

#include "cuckoo_pi_config.hpp"

using CuckooPiSettingsSaveCallback = bool (*)(
    const CuckooPiDeviceConfig& settings, void* context);

class CuckooPiSettingsUiState {
 public:
  bool openForDevice(const char* device_id,
                     const CuckooPiDeviceConfig& settings);
  void back();
  bool save(CuckooPiSettingsSaveCallback callback, void* context);

  void toggleOfflineAlarm();
  void adjustOfflineTimeout(int direction);
  void toggleBeerDeviationAlarm();
  void adjustBeerDeviationThreshold(int direction);
  void adjustBeerDeviationHold(int direction);

  bool isOpen() const { return _open; }
  bool saveError() const { return _save_error; }
  const char* deviceId() const { return _device_id; }
  CuckooPiDeviceConfig& draft() { return _draft; }
  const CuckooPiDeviceConfig& draft() const { return _draft; }
  void clear() { *this = CuckooPiSettingsUiState{}; }

 private:
  bool _open = false;
  bool _save_error = false;
  char _device_id[13] = {};
  CuckooPiDeviceConfig _draft{};
  CuckooPiDeviceConfig _entry_snapshot{};
};
