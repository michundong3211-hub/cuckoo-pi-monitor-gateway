#include <assert.h>
#include <string.h>

#include "../../src/gateway_settings_state.hpp"

namespace {

struct SavedTarget {
  int calls = 0;
  char id[GRAVITY_DEVICE_ID_CAPACITY] = {};
};

bool captureSaveTarget(const GravityDeviceSettings& settings, void* context) {
  SavedTarget* saved = static_cast<SavedTarget*>(context);
  ++saved->calls;
  memcpy(saved->id, settings.device_id, strlen(settings.device_id) + 1);
  return true;
}

void testMaximumValidDisplayIdReachesSaveWithoutTruncation() {
  char id63[GRAVITY_DEVICE_ID_CAPACITY];
  memset(id63, 'v', GRAVITY_DEVICE_ID_MAX_LENGTH);
  id63[GRAVITY_DEVICE_ID_MAX_LENGTH] = '\0';

  char displayTarget[GRAVITY_DEVICE_ID_CAPACITY] = {};
  assert(gateway_settings_copy_display_device_id(id63, displayTarget));
  assert(strlen(displayTarget) == GRAVITY_DEVICE_ID_MAX_LENGTH);
  assert(strcmp(displayTarget, id63) == 0);

  GravityDeviceSettings settings{};
  GatewaySettingsUiState state;
  assert(state.openForDevice(displayTarget, settings));
  assert(state.enterPage(SettingsPage::Og));
  SavedTarget saved;
  assert(state.save(captureSaveTarget, &saved));
  assert(saved.calls == 1);
  assert(strlen(saved.id) == GRAVITY_DEVICE_ID_MAX_LENGTH);
  assert(strcmp(saved.id, id63) == 0);
}

void testOverlongDisplayIdClearsTargetsAndCannotSave() {
  char id64[GRAVITY_DEVICE_ID_CAPACITY + 1];
  memset(id64, 'x', GRAVITY_DEVICE_ID_CAPACITY);
  id64[GRAVITY_DEVICE_ID_CAPACITY] = '\0';

  char displayTarget[GRAVITY_DEVICE_ID_CAPACITY] = "stale-target";
  assert(!gateway_settings_copy_display_device_id(id64, displayTarget));
  assert(displayTarget[0] == '\0');

  GravityDeviceSettings settings{};
  GatewaySettingsUiState state;
  assert(!state.openForDevice(displayTarget, settings));
  assert(!state.isOpen());
  assert(state.deviceId()[0] == '\0');

  SavedTarget saved;
  assert(!state.save(captureSaveTarget, &saved));
  assert(saved.calls == 0);
}

}  // namespace

int main() {
  testMaximumValidDisplayIdReachesSaveWithoutTruncation();
  testOverlongDisplayIdClearsTargetsAndCannotSave();
  return 0;
}
