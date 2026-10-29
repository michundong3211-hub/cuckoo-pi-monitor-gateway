#include <assert.h>
#include <math.h>
#include <string.h>

#include "../../src/gateway_settings_state.hpp"

namespace {

struct FakePersistence {
  bool result = true;
  int calls = 0;
  GravityDeviceSettings saved{};
};

bool saveFake(const GravityDeviceSettings& settings, void* context) {
  FakePersistence* fake = static_cast<FakePersistence*>(context);
  ++fake->calls;
  fake->saved = settings;
  return fake->result;
}

GravityDeviceSettings settingsFor(const char* id) {
  GravityDeviceSettings settings{};
  strcpy(settings.device_id, id);
  settings.og_sg = NAN;
  return settings;
}

void testOpenCapturesImmutableTargetAndNavigates() {
  GatewaySettingsUiState state;
  GravityDeviceSettings settings = settingsFor("device-a");
  assert(state.openForDevice("device-a", settings));
  assert(state.isOpen());
  assert(state.page() == SettingsPage::Home);
  assert(strcmp(state.deviceId(), "device-a") == 0);

  GravityDeviceSettings other = settingsFor("device-b");
  assert(!state.openForDevice("device-b", other));
  assert(strcmp(state.deviceId(), "device-a") == 0);

  const SettingsPage pages[] = {SettingsPage::Og,
                                SettingsPage::TemperatureChange,
                                SettingsPage::RingColor,
                                SettingsPage::OfflineAlarm};
  for (SettingsPage page : pages) {
    assert(state.enterPage(page));
    assert(state.page() == page);
    state.back();
    assert(state.page() == SettingsPage::Home);
  }
  state.back();
  assert(!state.isOpen());
}

void testBackRestoresPageEntrySnapshot() {
  GatewaySettingsUiState state;
  GravityDeviceSettings settings = settingsFor("device-a");
  settings.og_sg = 1.060f;
  assert(state.openForDevice("device-a", settings));
  assert(state.enterPage(SettingsPage::Og));
  state.adjustOg(1);
  assert(fabsf(state.draft().og_sg - 1.061f) < 0.0001f);
  state.back();
  assert(state.page() == SettingsPage::Home);
  assert(fabsf(state.draft().og_sg - 1.060f) < 0.0001f);
  assert(!state.saveError());
}

void testPageEntryUsesLatestCopiedRecordButKeepsCapturedId() {
  GatewaySettingsUiState state;
  GravityDeviceSettings opened = settingsFor("device-a");
  opened.og_sg = 1.050f;
  assert(state.openForDevice("device-a", opened));

  GravityDeviceSettings latest = settingsFor("device-b");
  latest.og_sg = 1.070f;
  assert(state.enterPage(SettingsPage::Og, latest));
  assert(fabsf(state.draft().og_sg - 1.070f) < 0.0001f);
  assert(fabsf(state.entrySnapshot().og_sg - 1.070f) < 0.0001f);
  assert(strcmp(state.draft().device_id, "device-a") == 0);
  assert(strcmp(state.deviceId(), "device-a") == 0);
}

void testSaveSuccessNormalizesAndReturnsHome() {
  GatewaySettingsUiState state;
  GravityDeviceSettings settings = settingsFor("device-a");
  assert(state.openForDevice("device-a", settings));
  assert(state.enterPage(SettingsPage::TemperatureChange));
  state.draft().temperature_compare_minutes = 0;
  state.draft().temperature_change_threshold_c = 99.0f;

  FakePersistence fake;
  assert(state.save(saveFake, &fake));
  assert(fake.calls == 1);
  assert(strcmp(fake.saved.device_id, "device-a") == 0);
  assert(fake.saved.temperature_compare_minutes == 1);
  assert(fabsf(fake.saved.temperature_change_threshold_c - 10.0f) < 0.0001f);
  assert(state.page() == SettingsPage::Home);
  assert(!state.saveError());
}

void testSaveFailureKeepsPageDraftTargetAndError() {
  GatewaySettingsUiState state;
  GravityDeviceSettings settings = settingsFor("device-a");
  assert(state.openForDevice("device-a", settings));
  assert(state.enterPage(SettingsPage::OfflineAlarm));
  state.toggleOfflineAlarm();
  state.adjustOfflineTimeout(1);

  FakePersistence fake;
  fake.result = false;
  assert(!state.save(saveFake, &fake));
  assert(fake.calls == 1);
  assert(state.page() == SettingsPage::OfflineAlarm);
  assert(state.draft().offline_alarm_enabled);
  assert(state.draft().offline_timeout_seconds ==
         GRAVITY_DEVICE_OFFLINE_TIMEOUT_DEFAULT +
             GRAVITY_DEVICE_OFFLINE_TIMEOUT_STEP);
  assert(strcmp(state.deviceId(), "device-a") == 0);
  assert(state.saveError());
}

void testRebuildPreservesPageDraftAndTarget() {
  GatewaySettingsUiState state;
  GravityDeviceSettings settings = settingsFor("device-a");
  assert(state.openForDevice("device-a", settings));
  assert(state.enterPage(SettingsPage::RingColor));
  state.selectRingColor(3);
  state.onLayoutRebuild();
  assert(state.page() == SettingsPage::RingColor);
  assert(state.draft().ring_color_index == 3);
  assert(strcmp(state.deviceId(), "device-a") == 0);
}

void testDocumentedDefaultsStepsAndBounds() {
  GatewaySettingsUiState state;
  GravityDeviceSettings settings = settingsFor("device-a");
  assert(!settings.temperature_change_alarm_enabled);
  assert(!settings.offline_alarm_enabled);
  assert(settings.ring_color_index == RING_COLOR_DEFAULT_INDEX);
  assert(state.openForDevice("device-a", settings));

  assert(state.enterPage(SettingsPage::Og));
  state.adjustOg(1);
  assert(fabsf(state.draft().og_sg - 1.050f) < 0.0001f);
  for (int i = 0; i < 300; ++i) state.adjustOg(1);
  assert(fabsf(state.draft().og_sg - 1.200f) < 0.0001f);
  state.back();

  assert(state.enterPage(SettingsPage::TemperatureChange));
  state.toggleTemperatureAlarm();
  assert(state.draft().temperature_change_alarm_enabled);
  for (int i = 0; i < 100; ++i) state.adjustTemperatureCompare(1);
  assert(state.draft().temperature_compare_minutes == 60);
  for (int i = 0; i < 200; ++i) state.adjustTemperatureThreshold(-1);
  assert(fabsf(state.draft().temperature_change_threshold_c - 0.1f) < 0.0001f);
  state.back();

  assert(state.enterPage(SettingsPage::RingColor));
  state.selectRingColor(RING_COLOR_DEFAULT_INDEX);
  assert(state.draft().ring_color_index == RING_COLOR_DEFAULT_INDEX);
  state.selectRingColor(6);
  assert(state.draft().ring_color_index == 6);
  state.back();

  assert(state.enterPage(SettingsPage::OfflineAlarm));
  assert(state.draft().offline_timeout_seconds ==
         GRAVITY_DEVICE_OFFLINE_TIMEOUT_DEFAULT);
  state.adjustOfflineTimeout(1);
  assert(state.draft().offline_timeout_seconds ==
         GRAVITY_DEVICE_OFFLINE_TIMEOUT_DEFAULT +
             GRAVITY_DEVICE_OFFLINE_TIMEOUT_STEP);
  state.adjustOfflineTimeout(-1);
  assert(state.draft().offline_timeout_seconds ==
         GRAVITY_DEVICE_OFFLINE_TIMEOUT_DEFAULT);
  state.toggleOfflineAlarm();
  assert(state.draft().offline_alarm_enabled);
  for (int i = 0; i < 2000; ++i) state.adjustOfflineTimeout(1);
  assert(state.draft().offline_timeout_seconds == 86400);
  for (int i = 0; i < 2000; ++i) state.adjustOfflineTimeout(-1);
  assert(state.draft().offline_timeout_seconds == 60);
}

void testRejectsOverlongIdWithoutTruncation() {
  char id64[65];
  memset(id64, 'x', 64);
  id64[64] = '\0';
  GatewaySettingsUiState state;
  GravityDeviceSettings settings{};
  assert(!state.openForDevice(id64, settings));
  assert(!state.isOpen());
}

}  // namespace

int main() {
  testOpenCapturesImmutableTargetAndNavigates();
  testBackRestoresPageEntrySnapshot();
  testPageEntryUsesLatestCopiedRecordButKeepsCapturedId();
  testSaveSuccessNormalizesAndReturnsHome();
  testSaveFailureKeepsPageDraftTargetAndError();
  testRebuildPreservesPageDraftAndTarget();
  testDocumentedDefaultsStepsAndBounds();
  testRejectsOverlongIdWithoutTruncation();
  return 0;
}
