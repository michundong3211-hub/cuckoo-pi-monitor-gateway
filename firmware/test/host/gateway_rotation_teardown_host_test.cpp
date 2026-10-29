#include <assert.h>

#include <vector>

#include "../../src/gateway_rotation_teardown.hpp"

namespace {

enum class Event {
  ResetAllIndev,
  CleanupDelete,
  Rebuild,
  LvglHandler,
  OrientationPoll,
};

struct EventLog {
  std::vector<Event> events;
};

void resetAll(void* context) {
  static_cast<EventLog*>(context)->events.push_back(Event::ResetAllIndev);
}

bool cleanupDelete(void* context) {
  static_cast<EventLog*>(context)->events.push_back(Event::CleanupDelete);
  return true;
}

bool cleanupFailure(void* context) {
  static_cast<EventLog*>(context)->events.push_back(Event::CleanupDelete);
  return false;
}

void rebuild(void* context) {
  static_cast<EventLog*>(context)->events.push_back(Event::Rebuild);
}

bool runLvgl(void* context) {
  static_cast<EventLog*>(context)->events.push_back(Event::LvglHandler);
  return true;
}

bool failLvgl(void* context) {
  (void)context;
  return false;
}

void pollOrientation(void* context) {
  static_cast<EventLog*>(context)->events.push_back(Event::OrientationPoll);
}

GatewayRotationTeardown makeTeardown(
    EventLog* log, GatewayRotationCleanupStep cleanup = cleanupDelete) {
  return {resetAll, cleanup, rebuild, log};
}

void assertCycle(const std::vector<Event>& events, size_t offset) {
  assert(events[offset] == Event::ResetAllIndev);
  assert(events[offset + 1] == Event::CleanupDelete);
  assert(events[offset + 2] == Event::Rebuild);
}

void testPressedRotationAlwaysResetsBeforeDeleteAndRebuild() {
  EventLog log;
  const GatewayRotationTeardown teardown = makeTeardown(&log);
  assert(runGatewayRotationTeardown(teardown));
  assert(log.events.size() == 3);
  assertCycle(log.events, 0);
}

void testRapidRotationsNeverInterleaveOrSkipOrder() {
  EventLog log;
  const GatewayRotationTeardown teardown = makeTeardown(&log);
  for (int i = 0; i < 100; ++i) {
    assert(runGatewayRotationTeardown(teardown));
  }
  assert(log.events.size() == 300);
  for (size_t offset = 0; offset < log.events.size(); offset += 3) {
    assertCycle(log.events, offset);
  }
}

void testInvalidSequenceDoesNothing() {
  EventLog log;
  GatewayRotationTeardown teardown = makeTeardown(&log);
  teardown.cleanup_delete = nullptr;
  assert(!runGatewayRotationTeardown(teardown));
  assert(log.events.empty());
}

void testCleanupFailureStopsBeforeRebuild() {
  EventLog log;
  const GatewayRotationTeardown teardown = makeTeardown(&log, cleanupFailure);
  assert(!runGatewayRotationTeardown(teardown));
  assert(log.events.size() == 2);
  assert(log.events[0] == Event::ResetAllIndev);
  assert(log.events[1] == Event::CleanupDelete);
}

void testPointerPressAndPressLostGatePendingRotation() {
  GatewayRotationGuard guard;
  guard.requestOrientation(GatewayOrientation::Portrait);
  guard.pointerPressed();
  assert(guard.isBlocked());
  GatewayOrientation accepted = GatewayOrientation::Landscape;
  assert(!guard.takePendingOrientation(&accepted));
  guard.pointerLost();
  assert(!guard.isBlocked());
  assert(guard.takePendingOrientation(&accepted));
  assert(accepted == GatewayOrientation::Portrait);
}

void testSettingsCloseRequiresFreshSampleBeforePendingRotation() {
  GatewayRotationGuard guard;
  guard.settingsOpened();
  guard.requestOrientation(GatewayOrientation::Portrait);
  assert(guard.isBlocked());
  GatewayOrientation accepted = GatewayOrientation::Landscape;
  assert(!guard.takePendingOrientation(&accepted));
  guard.settingsClosed();
  assert(!guard.isBlocked());
  assert(guard.consumeFreshSampleRequest());
  assert(!guard.takePendingOrientation(&accepted));
  guard.requestOrientation(GatewayOrientation::Portrait);
  assert(guard.takePendingOrientation(&accepted));
  assert(accepted == GatewayOrientation::Portrait);
}

void testOneHundredSettingsEntryExitCyclesReleaseTheGuard() {
  GatewayRotationGuard guard;
  for (int i = 0; i < 100; ++i) {
    guard.settingsOpened();
    assert(guard.isBlocked());
    guard.settingsClosed();
    assert(!guard.isBlocked());
    assert(guard.consumeFreshSampleRequest());
  }
}

void testLvglInputDispatchAlwaysPrecedesOrientationPolling() {
  EventLog log;
  const GatewayLvglLoopIteration iteration = {
      runLvgl, pollOrientation, &log};
  assert(runGatewayLvglLoopIteration(iteration));
  assert(log.events.size() == 2);
  assert(log.events[0] == Event::LvglHandler);
  assert(log.events[1] == Event::OrientationPoll);
}

void testFailedLvglLockDoesNotPollAndNextIterationRecovers() {
  EventLog log;
  const GatewayLvglLoopIteration failed = {
      failLvgl, pollOrientation, &log};
  assert(!runGatewayLvglLoopIteration(failed));
  assert(log.events.empty());

  const GatewayLvglLoopIteration recovered = {
      runLvgl, pollOrientation, &log};
  assert(runGatewayLvglLoopIteration(recovered));
  assert(log.events.size() == 2);
  assert(log.events[0] == Event::LvglHandler);
  assert(log.events[1] == Event::OrientationPoll);
}

}  // namespace

int main() {
  testPressedRotationAlwaysResetsBeforeDeleteAndRebuild();
  testRapidRotationsNeverInterleaveOrSkipOrder();
  testInvalidSequenceDoesNothing();
  testCleanupFailureStopsBeforeRebuild();
  testPointerPressAndPressLostGatePendingRotation();
  testSettingsCloseRequiresFreshSampleBeforePendingRotation();
  testOneHundredSettingsEntryExitCyclesReleaseTheGuard();
  testLvglInputDispatchAlwaysPrecedesOrientationPolling();
  testFailedLvglLockDoesNotPollAndNextIterationRecovers();
  return 0;
}
