#include "cuckoo_pi_settings_state.hpp"

#include <cassert>
#include <cstring>

namespace {
bool saved = false;
bool saveSettings(const CuckooPiDeviceConfig& settings, void*) {
  saved = true;
  return std::strcmp(settings.id, "001122334455") == 0;
}
}  // namespace

int main() {
  CuckooPiDeviceConfig initial = defaultCuckooPiDeviceConfig("001122334455");
  CuckooPiSettingsUiState state;
  assert(state.openForDevice(initial.id, initial));
  assert(state.isOpen());
  assert(!state.openForDevice(initial.id, initial));

  state.toggleOfflineAlarm();
  state.adjustOfflineTimeout(-1);
  state.adjustBeerDeviationThreshold(-1);
  state.adjustBeerDeviationHold(-1);
  assert(state.draft().offline_alarm_enabled != initial.offline_alarm_enabled);
  assert(state.draft().offline_timeout_seconds == 240);
  assert(state.draft().beer_deviation_threshold_c == 1.9f);
  assert(state.draft().beer_deviation_hold_seconds == 540);

  state.back();
  assert(!state.isOpen());
  assert(state.openForDevice(initial.id, initial));
  assert(state.draft().offline_timeout_seconds == initial.offline_timeout_seconds);
  assert(state.draft().beer_deviation_threshold_c ==
         initial.beer_deviation_threshold_c);
  assert(state.draft().beer_deviation_hold_seconds ==
         initial.beer_deviation_hold_seconds);

  state.toggleBeerDeviationAlarm();
  assert(state.save(saveSettings, nullptr));
  assert(saved);
  assert(!state.isOpen());
  return 0;
}
