#include "cuckoo_pi_settings_state.hpp"

#include "cuckoo_pi_model.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

bool CuckooPiSettingsUiState::openForDevice(
    const char* device_id, const CuckooPiDeviceConfig& settings) {
  if (_open || !normalizeCuckooPiId(device_id, _device_id)) return false;
  _open = true;
  _save_error = false;
  _draft = normalizeCuckooPiDeviceConfig(settings);
  std::snprintf(_draft.id, sizeof(_draft.id), "%s", _device_id);
  _entry_snapshot = _draft;
  return true;
}

void CuckooPiSettingsUiState::back() { clear(); }

bool CuckooPiSettingsUiState::save(CuckooPiSettingsSaveCallback callback,
                                   void* context) {
  if (!_open || !callback) return false;
  _draft = normalizeCuckooPiDeviceConfig(_draft);
  std::snprintf(_draft.id, sizeof(_draft.id), "%s", _device_id);
  if (!callback(_draft, context)) {
    _save_error = true;
    return false;
  }
  _entry_snapshot = _draft;
  clear();
  return true;
}

void CuckooPiSettingsUiState::toggleOfflineAlarm() {
  if (_open) _draft.offline_alarm_enabled = !_draft.offline_alarm_enabled;
}

void CuckooPiSettingsUiState::adjustOfflineTimeout(int direction) {
  if (!_open || direction == 0) return;
  int64_t value = _draft.offline_timeout_seconds;
  value += direction > 0 ? 60 : -60;
  value = std::max<int64_t>(60, std::min<int64_t>(86400, value));
  _draft.offline_timeout_seconds = static_cast<uint32_t>(value);
}

void CuckooPiSettingsUiState::toggleBeerDeviationAlarm() {
  if (_open)
    _draft.beer_deviation_alarm_enabled =
        !_draft.beer_deviation_alarm_enabled;
}

void CuckooPiSettingsUiState::adjustBeerDeviationThreshold(int direction) {
  if (!_open || direction == 0) return;
  int value = static_cast<int>(
      std::lround(_draft.beer_deviation_threshold_c * 10.0f));
  value += direction > 0 ? 1 : -1;
  value = std::max(1, std::min(200, value));
  _draft.beer_deviation_threshold_c = static_cast<float>(value) / 10.0f;
}

void CuckooPiSettingsUiState::adjustBeerDeviationHold(int direction) {
  if (!_open || direction == 0) return;
  int64_t value = _draft.beer_deviation_hold_seconds;
  value += direction > 0 ? 60 : -60;
  value = std::max<int64_t>(60, std::min<int64_t>(86400, value));
  _draft.beer_deviation_hold_seconds = static_cast<uint32_t>(value);
}
