#include "gateway_settings_state.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

bool gateway_settings_copy_display_device_id(
    const char* input, char output[GRAVITY_DEVICE_ID_CAPACITY]) {
  if (output == nullptr) return false;
  output[0] = '\0';
  if (!gateway_device_id_is_valid(input)) return false;
  const size_t length = std::strlen(input);
  std::memcpy(output, input, length + 1);
  return true;
}

bool GatewaySettingsUiState::openForDevice(
    const char* deviceId, const GravityDeviceSettings& settings) {
  if (_open || !gateway_device_id_is_valid(deviceId)) return false;
  _open = true;
  _page = SettingsPage::Home;
  _save_error = false;
  std::snprintf(_device_id, sizeof(_device_id), "%s", deviceId);
  _draft = normalize_gateway_device_settings(settings);
  restoreTargetId();
  _entry_snapshot = _draft;
  return true;
}

bool GatewaySettingsUiState::enterPage(SettingsPage page) {
  return enterPage(page, _draft);
}

bool GatewaySettingsUiState::enterPage(
    SettingsPage page, const GravityDeviceSettings& latestSettings) {
  if (!_open || _page != SettingsPage::Home || page == SettingsPage::Home)
    return false;
  _draft = normalize_gateway_device_settings(latestSettings);
  restoreTargetId();
  _page = page;
  _entry_snapshot = _draft;
  _save_error = false;
  return true;
}

void GatewaySettingsUiState::close() {
  clear();
}

void GatewaySettingsUiState::back() {
  if (!_open) return;
  if (_page == SettingsPage::Home) {
    close();
    return;
  }
  _draft = _entry_snapshot;
  restoreTargetId();
  _page = SettingsPage::Home;
  _save_error = false;
}

bool GatewaySettingsUiState::save(GatewaySettingsSaveCallback callback,
                                  void* context) {
  if (!_open || _page == SettingsPage::Home || callback == nullptr)
    return false;
  _draft = normalize_gateway_device_settings(_draft);
  restoreTargetId();
  if (!callback(_draft, context)) {
    _save_error = true;
    return false;
  }
  _entry_snapshot = _draft;
  _page = SettingsPage::Home;
  _save_error = false;
  return true;
}

void GatewaySettingsUiState::restoreTargetId() {
  std::snprintf(_draft.device_id, sizeof(_draft.device_id), "%s", _device_id);
}

void GatewaySettingsUiState::adjustOg(int direction) {
  if (!_open || _page != SettingsPage::Og || direction == 0) return;
  if (!std::isfinite(_draft.og_sg)) {
    _draft.og_sg = 1.050f;
    return;
  }
  int value = static_cast<int>(std::lround(_draft.og_sg * 1000.0f));
  value += direction > 0 ? 1 : -1;
  value = std::max(1000, std::min(1200, value));
  _draft.og_sg = static_cast<float>(value) / 1000.0f;
}

void GatewaySettingsUiState::toggleTemperatureAlarm() {
  if (_open && _page == SettingsPage::TemperatureChange) {
    _draft.temperature_change_alarm_enabled =
        !_draft.temperature_change_alarm_enabled;
  }
}

void GatewaySettingsUiState::adjustTemperatureCompare(int direction) {
  if (!_open || _page != SettingsPage::TemperatureChange || direction == 0)
    return;
  int value = _draft.temperature_compare_minutes;
  value += direction > 0 ? 1 : -1;
  value = std::max(1, std::min(60, value));
  _draft.temperature_compare_minutes = static_cast<uint8_t>(value);
}

void GatewaySettingsUiState::adjustTemperatureThreshold(int direction) {
  if (!_open || _page != SettingsPage::TemperatureChange || direction == 0)
    return;
  int tenths = static_cast<int>(
      std::lround(_draft.temperature_change_threshold_c * 10.0f));
  tenths += direction > 0 ? 1 : -1;
  tenths = std::max(1, std::min(100, tenths));
  _draft.temperature_change_threshold_c =
      static_cast<float>(tenths) / 10.0f;
}

void GatewaySettingsUiState::selectRingColor(uint8_t index) {
  if (_open && _page == SettingsPage::RingColor) {
    _draft.ring_color_index = normalize_ring_color_index(index);
  }
}

void GatewaySettingsUiState::toggleOfflineAlarm() {
  if (_open && _page == SettingsPage::OfflineAlarm) {
    _draft.offline_alarm_enabled = !_draft.offline_alarm_enabled;
  }
}

void GatewaySettingsUiState::adjustOfflineTimeout(int direction) {
  if (!_open || _page != SettingsPage::OfflineAlarm || direction == 0) return;
  int64_t value = _draft.offline_timeout_seconds;
  value += direction > 0 ? GRAVITY_DEVICE_OFFLINE_TIMEOUT_STEP
                         : -static_cast<int64_t>(
                               GRAVITY_DEVICE_OFFLINE_TIMEOUT_STEP);
  value = std::max<int64_t>(
      GRAVITY_DEVICE_OFFLINE_TIMEOUT_MIN,
      std::min<int64_t>(GRAVITY_DEVICE_OFFLINE_TIMEOUT_MAX, value));
  _draft.offline_timeout_seconds = static_cast<uint32_t>(value);
}
