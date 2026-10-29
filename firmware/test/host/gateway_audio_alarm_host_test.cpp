#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <type_traits>
#include <utility>

#include "../../src/gateway_audio.hpp"

namespace {

GatewayAudioQueueItem makeEvent(GatewayAlarmType type, const char* deviceId,
                                GatewayAlarmEventToken eventToken,
                                uint8_t repeatIndex,
                                GatewayAlarmEventToken conditionToken =
                                    1001) {
  GatewayAudioQueueItem item{};
  item.event.type = type;
  strncpy(item.event.device_id, deviceId,
          sizeof(item.event.device_id) - 1);
  item.event.condition_token = conditionToken;
  item.event.event_token = eventToken;
  item.event.window_index = 7;
  item.event.repeat_index = repeatIndex;
  item.decision = gatewayAudioDecisionForAlarm(type);
  return item;
}

void testSchedulerFairnessAndDeduplication() {
  GatewayAudioScheduler scheduler;
  assert(scheduler.enqueue(makeEvent(GatewayAlarmType::TemperatureChange, "A",
                                     11, 0)));
  assert(scheduler.enqueue(
      makeEvent(GatewayAlarmType::Offline, "A", 12, 0)));
  assert(!scheduler.enqueue(
      makeEvent(GatewayAlarmType::Offline, "A", 12, 0)));

  GatewayAudioQueueItem item{};
  assert(scheduler.takeNext(&item));
  assert(item.event.type == GatewayAlarmType::TemperatureChange);
  assert(strcmp(item.event.device_id, "A") == 0);
  assert(scheduler.active());
  scheduler.finishActive(item.event.event_token);
  assert(!scheduler.active());
  assert(scheduler.takeNext(&item));
  assert(item.event.type == GatewayAlarmType::Offline);
  scheduler.finishActive(item.event.event_token);

  GatewayAudioScheduler sameKey;
  assert(sameKey.enqueue(makeEvent(GatewayAlarmType::TemperatureChange, "A",
                                   21, 0)));
  assert(sameKey.enqueue(makeEvent(GatewayAlarmType::TemperatureChange, "A",
                                   22, 1)));
  assert(sameKey.enqueue(makeEvent(GatewayAlarmType::Offline, "A", 23, 0)));
  assert(sameKey.enqueue(makeEvent(GatewayAlarmType::TemperatureChange, "B",
                                   24, 0)));
  assert(sameKey.takeNext(&item));
  assert(item.event.event_token == 21);
  sameKey.finishActive(item.event.event_token);
  assert(sameKey.takeNext(&item));
  assert(item.event.event_token == 24);
  sameKey.finishActive(item.event.event_token);
  assert(sameKey.takeNext(&item));
  assert(item.event.event_token == 23);
  sameKey.finishActive(item.event.event_token);
  assert(sameKey.takeNext(&item));
  assert(item.event.event_token == 22);
  sameKey.finishActive(item.event.event_token);
  assert(!sameKey.takeNext(&item));
}

void testCapacityAndExactCancellation() {
  GatewayAudioScheduler invalid;
  GatewayAudioQueueItem invalidItem =
      makeEvent(GatewayAlarmType::Offline, "invalid", 30, 0);
  invalidItem.event.event_token = GATEWAY_INVALID_EVENT_TOKEN;
  assert(!invalid.enqueue(invalidItem));
  invalidItem = makeEvent(GatewayAlarmType::Offline, "invalid", 30, 0);
  invalidItem.event.condition_token = GATEWAY_INVALID_EVENT_TOKEN;
  assert(!invalid.enqueue(invalidItem));
  invalidItem = makeEvent(GatewayAlarmType::Offline, "invalid", 30, 0);
  invalidItem.event.device_id[0] = '\0';
  assert(!invalid.enqueue(invalidItem));

  char overlongDevice[GATEWAY_AUDIO_DEVICE_ID_CAPACITY];
  memset(overlongDevice, 'x', sizeof(overlongDevice));
  invalidItem = makeEvent(GatewayAlarmType::Offline, overlongDevice, 30, 0);
  memset(invalidItem.event.device_id, 'x',
         sizeof(invalidItem.event.device_id));
  assert(!invalid.enqueue(invalidItem));

  GatewayAudioScheduler scheduler;
  for (GatewayAlarmEventToken token = 31; token <= 34; ++token) {
    assert(scheduler.enqueue(
        makeEvent(GatewayAlarmType::Offline, "capacity", token, 0)));
  }
  assert(scheduler.pendingCount() == GATEWAY_AUDIO_QUEUE_CAPACITY);
  assert(!scheduler.enqueue(
      makeEvent(GatewayAlarmType::Offline, "capacity", 35, 0)));
  const GatewayAudioQueueItem cancelled32 =
      makeEvent(GatewayAlarmType::Offline, "capacity", 32, 0);
  assert(scheduler.cancelExact(cancelled32.event));
  assert(!scheduler.cancelExact(cancelled32.event));
  assert(scheduler.enqueue(
      makeEvent(GatewayAlarmType::Offline, "capacity", 35, 0)));

  GatewayAudioQueueItem item{};
  assert(scheduler.takeNext(&item));
  assert(item.event.event_token != 32);
  scheduler.finishActive(item.event.event_token);

  GatewayAudioScheduler exact;
  assert(exact.enqueue(makeEvent(GatewayAlarmType::TemperatureChange, "A",
                                 41, 0)));
  assert(exact.enqueue(makeEvent(GatewayAlarmType::TemperatureChange, "A",
                                 42, 0)));
  const GatewayAudioQueueItem cancelled41 =
      makeEvent(GatewayAlarmType::TemperatureChange, "A", 41, 0);
  assert(exact.cancelExact(cancelled41.event));
  assert(exact.takeNext(&item));
  assert(item.event.event_token == 42);
  exact.finishActive(item.event.event_token);
  assert(exact.cancelExactResult(cancelled41.event) ==
         GatewayAudioCancelResult::NotFound);
  assert(!exact.cancelExact(cancelled41.event));

  GatewayAudioScheduler key;
  assert(key.enqueue(
      makeEvent(GatewayAlarmType::TemperatureChange, "A", 51, 0)));
  assert(key.enqueue(makeEvent(GatewayAlarmType::Offline, "A", 52, 0)));
  assert(key.enqueue(makeEvent(GatewayAlarmType::TemperatureChange, "B",
                               53, 0)));
  assert(key.cancelByKeyResult(GatewayAlarmType::TemperatureChange, "A") ==
         GatewayAudioCancelResult::Cancelled);
  assert(key.takeNext(&item));
  assert(item.event.event_token == 53);
  key.finishActive(item.event.event_token);
  assert(key.takeNext(&item));
  assert(item.event.event_token == 52);
  key.finishActive(item.event.event_token);

  GatewayAudioScheduler active;
  assert(active.enqueue(
      makeEvent(GatewayAlarmType::TemperatureChange, "active", 61, 0)));
  assert(active.takeNext(&item));
  assert(active.cancelExact(item.event));
  assert(!active.takeNext(&item));
  active.finishActive(61);
  assert(!active.active());
  assert(!active.takeNext(&item));
}

void testExactCancellationCoversAllRepeats() {
  GatewayAudioScheduler scheduler;
  assert(scheduler.enqueue(makeEvent(GatewayAlarmType::TemperatureChange, "A",
                                     81, 0)));
  assert(scheduler.enqueue(makeEvent(GatewayAlarmType::TemperatureChange, "A",
                                     81, 1)));
  assert(scheduler.enqueue(makeEvent(GatewayAlarmType::TemperatureChange, "A",
                                     81, 2)));
  assert(scheduler.enqueue(makeEvent(GatewayAlarmType::Offline, "A", 82, 0)));

  GatewayAudioQueueItem item{};
  assert(scheduler.takeNext(&item));
  assert(item.event.event_token == 81);
  const GatewayAudioQueueItem repeatedEvent =
      makeEvent(GatewayAlarmType::TemperatureChange, "A", 81, 0);
  assert(scheduler.cancelExact(repeatedEvent.event));
  assert(scheduler.active());
  assert(scheduler.pendingCount() == 1);
  scheduler.finishActive(81);
  assert(scheduler.takeNext(&item));
  assert(item.event.event_token == 82);
  scheduler.finishActive(82);
  assert(!scheduler.takeNext(&item));
}

void testPcmScalingAndPlaybackFailureModel() {
  assert(gatewayAudioPcmSample(1.0f, 1.00f) == 12000);
  assert(gatewayAudioPcmSample(1.0f, 0.50f) == 6000);
  assert(gatewayAudioPcmSample(4.0f, 1.00f) == 32767);
  assert(gatewayAudioPcmSample(-4.0f, 1.00f) == -32768);

  GatewayAudioPlaybackPhase phase = GatewayAudioPlaybackPhase::FirstTone;
  phase = gatewayAudioAdvancePlaybackPhase(
      phase, GatewayAudioPlaybackStepResult::WriteFailed);
  assert(phase == GatewayAudioPlaybackPhase::WriteFailed);
  assert(gatewayAudioAdvancePlaybackPhase(
             phase, GatewayAudioPlaybackStepResult::Completed) ==
         GatewayAudioPlaybackPhase::WriteFailed);

  assert(gatewayAudioAdvancePlaybackPhase(
             GatewayAudioPlaybackPhase::FirstTone,
             GatewayAudioPlaybackStepResult::Completed) ==
         GatewayAudioPlaybackPhase::Gap);
  assert(gatewayAudioAdvancePlaybackPhase(
             GatewayAudioPlaybackPhase::Gap,
             GatewayAudioPlaybackStepResult::Completed) ==
         GatewayAudioPlaybackPhase::SecondTone);
  assert(gatewayAudioAdvancePlaybackPhase(
             GatewayAudioPlaybackPhase::SecondTone,
             GatewayAudioPlaybackStepResult::Completed) ==
         GatewayAudioPlaybackPhase::Complete);
}

struct RecordingPlaybackSink : GatewayAudioPlaybackSink {
  bool fail_first_tone = false;
  bool cancel_first_tone = false;
  bool cancel_gap = false;
  size_t tone_calls = 0;
  size_t gap_calls = 0;
  size_t silence_calls = 0;

  GatewayAudioPlaybackStepResult writeTone(const GatewayAudioQueueItem&,
                                           uint16_t, uint16_t) override {
    ++tone_calls;
    if (tone_calls == 1 && fail_first_tone) {
      return GatewayAudioPlaybackStepResult::WriteFailed;
    }
    if (tone_calls == 1 && cancel_first_tone) {
      return GatewayAudioPlaybackStepResult::Cancelled;
    }
    return GatewayAudioPlaybackStepResult::Completed;
  }

  GatewayAudioPlaybackStepResult waitGap(const GatewayAudioQueueItem&,
                                         uint16_t) override {
    ++gap_calls;
    return cancel_gap ? GatewayAudioPlaybackStepResult::Cancelled
                      : GatewayAudioPlaybackStepResult::Completed;
  }

  void writeSilence(const GatewayAudioQueueItem&) override { ++silence_calls; }
};

void testCancellationHighWatermarksAndCapacity() {
  GatewayAudioScheduler scheduler;
  const GatewayAudioQueueItem lateEvent =
      makeEvent(GatewayAlarmType::TemperatureChange, "late", 91, 0, 1001);
  assert(scheduler.cancelExactResult(lateEvent.event) ==
         GatewayAudioCancelResult::Cancelled);

  GatewayAudioQueueItem item{};
  for (size_t i = 0; i < 128; ++i) {
    assert(!scheduler.takeNext(&item));
  }
  assert(!scheduler.enqueue(makeEvent(GatewayAlarmType::TemperatureChange,
                                      "late", 91, 1, 1001)));
  assert(scheduler.enqueue(
      makeEvent(GatewayAlarmType::TemperatureChange, "late", 92, 0, 1001)));
  assert(scheduler.enqueue(
      makeEvent(GatewayAlarmType::Offline, "late", 1, 0, 1002)));
  const bool otherQueued = scheduler.enqueue(
      makeEvent(GatewayAlarmType::TemperatureChange, "other", 2, 0, 1001));
  assert(otherQueued);
  assert(scheduler.takeNext(&item));
  assert(item.event.event_token == 92);
  scheduler.finishActive(item.event.event_token);
  assert(scheduler.cancelExactResult(lateEvent.event) ==
         GatewayAudioCancelResult::NotFound);

  GatewayAudioScheduler session;
  const GatewayAudioQueueItem oldSession =
      makeEvent(GatewayAlarmType::TemperatureChange, "session", 200, 0,
                5001);
  assert(session.cancelExactResult(oldSession.event) ==
         GatewayAudioCancelResult::Cancelled);
  assert(!session.enqueue(oldSession));
  const GatewayAudioQueueItem newSessionBeforeReset =
      makeEvent(GatewayAlarmType::TemperatureChange, "session", 1, 0, 5002);
  assert(!session.enqueue(newSessionBeforeReset));
  assert(session.resetKeySession(GatewayAlarmType::TemperatureChange,
                                 "session", 5002) ==
         GatewayAudioCancelResult::Cancelled);
  assert(session.enqueue(newSessionBeforeReset));
  assert(!session.enqueue(oldSession));

  GatewayAudioScheduler byKey;
  assert(byKey.enqueue(
      makeEvent(GatewayAlarmType::TemperatureChange, "key", 101, 0)));
  assert(byKey.enqueue(
      makeEvent(GatewayAlarmType::TemperatureChange, "key", 102, 1)));
  assert(byKey.enqueue(makeEvent(GatewayAlarmType::Offline, "key", 103, 0)));
  assert(byKey.takeNext(&item));
  assert(item.event.event_token == 101);
  assert(byKey.cancelByKeyResult(GatewayAlarmType::TemperatureChange, "key") ==
         GatewayAudioCancelResult::Cancelled);
  assert(byKey.active());
  assert(byKey.pendingCount() == 1);
  byKey.finishActive(item.event.event_token);
  assert(byKey.takeNext(&item));
  assert(item.event.event_token == 103);
  byKey.finishActive(item.event.event_token);
  assert(byKey.enqueue(makeEvent(GatewayAlarmType::Offline, "key", 103, 1)));
  assert(!byKey.enqueue(
      makeEvent(GatewayAlarmType::TemperatureChange, "key", 101, 2)));
  assert(byKey.enqueue(
      makeEvent(GatewayAlarmType::TemperatureChange, "key", 104, 0)));
  assert(byKey.enqueue(makeEvent(GatewayAlarmType::Offline, "key", 1, 0)));

  GatewayAudioScheduler full;
  for (uint32_t index = 0; index < GATEWAY_AUDIO_CANCEL_KEY_CAPACITY;
       ++index) {
    char deviceId[16];
    snprintf(deviceId, sizeof(deviceId), "dev%u",
             static_cast<unsigned>(index / 2));
    const GatewayAlarmType type =
        index % 2 == 0 ? GatewayAlarmType::TemperatureChange
                       : GatewayAlarmType::Offline;
    const GatewayAlarmEventToken token = 1000 + index;
    const GatewayAudioQueueItem fullEvent =
        makeEvent(type, deviceId, token, 0, 3000 + index);
    assert(full.enqueue(fullEvent));
    assert(full.cancelExactResult(fullEvent.event) ==
           GatewayAudioCancelResult::Cancelled);
    assert(!full.takeNext(&item));
  }

  assert(full.enqueue(makeEvent(GatewayAlarmType::TemperatureChange, "overflow",
                                2000, 0)));
  assert(full.cancelByKeyResult(GatewayAlarmType::TemperatureChange,
                                "overflow") == GatewayAudioCancelResult::Retry);
  assert(full.pendingCount() == 1);
  assert(full.takeNext(&item));
  assert(item.event.event_token == 2000);
  full.finishActive(item.event.event_token);

  assert(full.resetKeySession(GatewayAlarmType::TemperatureChange, "dev0",
                              9000) == GatewayAudioCancelResult::Cancelled);
  const GatewayAudioQueueItem resetEvent =
      makeEvent(GatewayAlarmType::TemperatureChange, "dev0", 1, 0, 9000);
  assert(full.enqueue(resetEvent));
  assert(!full.enqueue(makeEvent(GatewayAlarmType::TemperatureChange, "dev0",
                                  2001, 0, 3000)));
}

void testSafeAlarmSessionLifecycle() {
  GatewayAudio facade;
  const GatewayAudioQueueItem facadeEvent =
      makeEvent(GatewayAlarmType::TemperatureChange, "facade", 601, 0,
                7001);
  assert(facade.resetKeySession(GatewayAlarmType::TemperatureChange, "facade",
                                7001) == GatewayAudioCancelResult::Cancelled);
  assert(facade.retireExact(facadeEvent.event) ==
         GatewayAudioCancelResult::Cancelled);
  assert(facade.retireExact(facadeEvent.event) ==
         GatewayAudioCancelResult::NotFound);

  GatewayAudioScheduler session;
  const GatewayAudioQueueItem oldSession =
      makeEvent(GatewayAlarmType::TemperatureChange, "session", 701, 0,
                8001);
  assert(session.enqueue(oldSession));
  assert(session.resetKeySession(GatewayAlarmType::TemperatureChange,
                                 "session", 8002) ==
         GatewayAudioCancelResult::Cancelled);
  GatewayAudioQueueItem item{};
  assert(!session.takeNext(&item));
  for (size_t i = 0; i < 128; ++i) {
    assert(!session.takeNext(&item));
  }
  assert(!session.enqueue(oldSession));
  assert(session.enqueue(makeEvent(GatewayAlarmType::TemperatureChange,
                                   "session", 702, 0, 8002)));
  assert(session.takeNext(&item));
  assert(item.event.event_token == 702);
  session.finishActive(item.event.event_token);

  GatewayAudioScheduler retire;
  const GatewayAudioQueueItem activeSession =
      makeEvent(GatewayAlarmType::Offline, "retire", 801, 0, 9001);
  assert(retire.enqueue(activeSession));
  assert(retire.enqueue(makeEvent(GatewayAlarmType::Offline, "retire", 801,
                                  1, 9001)));
  assert(retire.takeNext(&item));
  assert(item.event.repeat_index == 0);
  assert(retire.retireExact(activeSession.event) ==
         GatewayAudioCancelResult::Cancelled);
  assert(retire.active());
  assert(retire.pendingCount() == 0);
  retire.finishActive(item.event.event_token);
  assert(!retire.active());
  assert(!retire.takeNext(&item));
  assert(retire.enqueue(makeEvent(GatewayAlarmType::Offline, "retire", 802,
                                  0, 9002)));

  GatewayAudioScheduler reusable;
  for (uint32_t index = 0;
       index < static_cast<uint32_t>(GATEWAY_AUDIO_CANCEL_KEY_CAPACITY) * 2U;
       ++index) {
    char deviceId[24];
    snprintf(deviceId, sizeof(deviceId), "rotate%u",
             static_cast<unsigned>(index));
    const GatewayAlarmType type =
        index % 2 == 0 ? GatewayAlarmType::TemperatureChange
                       : GatewayAlarmType::Offline;
    const GatewayAudioQueueItem event = makeEvent(
        type, deviceId, 10000 + index, 0, 10000 + index);
    assert(reusable.cancelExactResult(event.event) ==
           GatewayAudioCancelResult::Cancelled);
    assert(reusable.retireExact(event.event) ==
           GatewayAudioCancelResult::Cancelled);
  }

  GatewayAudioScheduler full;
  for (uint32_t index = 0; index < GATEWAY_AUDIO_CANCEL_KEY_CAPACITY;
       ++index) {
    char deviceId[16];
    snprintf(deviceId, sizeof(deviceId), "full%u",
             static_cast<unsigned>(index));
    const GatewayAudioQueueItem event = makeEvent(
        GatewayAlarmType::TemperatureChange, deviceId, 11000 + index, 0,
        11000 + index);
    assert(full.cancelExactResult(event.event) ==
           GatewayAudioCancelResult::Cancelled);
  }

  const GatewayAudioQueueItem overflow =
      makeEvent(GatewayAlarmType::Offline, "overflow", 12000, 0, 12000);
  assert(full.enqueue(overflow));
  assert(full.resetKeySession(GatewayAlarmType::Offline, "new-session",
                              12001) == GatewayAudioCancelResult::Retry);
  assert(full.cancelExactResult(overflow.event) ==
         GatewayAudioCancelResult::Retry);
  assert(full.pendingCount() == 1);
  assert(full.takeNext(&item));
  assert(item.event.event_token == overflow.event.event_token);
  full.finishActive(item.event.event_token);
}

void testPlaybackEntryPreservesCancellationCleanup() {
  assert(gatewayAudioShouldEnterPlaybackControl(true, true, true));
  assert(!gatewayAudioShouldEnterPlaybackControl(false, true, true));
  assert(!gatewayAudioShouldEnterPlaybackControl(true, false, true));
  assert(!gatewayAudioShouldEnterPlaybackControl(true, true, false));

  const GatewayAudioQueueItem item =
      makeEvent(GatewayAlarmType::Offline, "pre-cancel", 510, 0);
  RecordingPlaybackSink cancelledBeforeFirstTone;
  cancelledBeforeFirstTone.cancel_first_tone = true;
  assert(gatewayAudioPlayTone(item, &cancelledBeforeFirstTone) ==
         GatewayAudioPlaybackStepResult::Cancelled);
  assert(cancelledBeforeFirstTone.tone_calls == 1);
  assert(cancelledBeforeFirstTone.gap_calls == 0);
  assert(cancelledBeforeFirstTone.silence_calls == 1);
}

void testTemperatureAndOfflineAlternateWhenBothActive() {
  // Confirmed requirement: while temperature and offline alarms coexist, the
  // audio scheduler must alternate between the two alarm types so that neither
  // type monopolizes the sound queue. The queue holds 4 items; both types must
  // fit and the fifth item (another temperature repeat) must be rejected.
  GatewayAudioScheduler scheduler;
  for (uint8_t i = 0; i < 2; ++i) {
    assert(scheduler.enqueue(makeEvent(GatewayAlarmType::TemperatureChange,
                                       "T", 301 + i, i)));
    assert(scheduler.enqueue(
        makeEvent(GatewayAlarmType::Offline, "O", 401 + i, i)));
  }
  assert(!scheduler.enqueue(
      makeEvent(GatewayAlarmType::TemperatureChange, "T", 303, 2)));

  const GatewayAlarmType expected[] = {
      GatewayAlarmType::TemperatureChange, GatewayAlarmType::Offline,
      GatewayAlarmType::TemperatureChange, GatewayAlarmType::Offline};
  GatewayAudioQueueItem item{};
  for (size_t i = 0; i < sizeof(expected) / sizeof(expected[0]); ++i) {
    assert(scheduler.takeNext(&item));
    assert(item.event.type == expected[i]);
    scheduler.finishActive(item.event.event_token);
  }
  assert(!scheduler.takeNext(&item));
}

void testCloseOneTypeWhileBothActive() {
  // Confirmed requirement: CLOSE must immediately stop the current sound and
  // hide the alert for that exact alarm, while the other alarm type keeps
  // playing without being covered.
  GatewayAudioScheduler scheduler;
  assert(scheduler.enqueue(makeEvent(GatewayAlarmType::TemperatureChange, "T",
                                     501, 0)));
  assert(scheduler.enqueue(makeEvent(GatewayAlarmType::Offline, "O", 601, 0)));
  assert(scheduler.enqueue(makeEvent(GatewayAlarmType::TemperatureChange, "T",
                                     501, 1)));
  assert(scheduler.enqueue(makeEvent(GatewayAlarmType::Offline, "O", 601, 1)));

  const GatewayAudioQueueItem closeTemperature =
      makeEvent(GatewayAlarmType::TemperatureChange, "T", 501, 0);
  assert(scheduler.cancelExact(closeTemperature.event));
  GatewayAudioQueueItem item{};
  assert(scheduler.takeNext(&item));
  assert(item.event.type == GatewayAlarmType::Offline);
  assert(item.event.event_token == 601);
  scheduler.finishActive(item.event.event_token);
  assert(scheduler.takeNext(&item));
  assert(item.event.type == GatewayAlarmType::Offline);
  assert(item.event.event_token == 601);
  scheduler.finishActive(item.event.event_token);
  assert(!scheduler.takeNext(&item));
}

void testRealPlaybackControlFlow() {
  const GatewayAudioQueueItem item =
      makeEvent(GatewayAlarmType::Offline, "playback", 111, 0);

  RecordingPlaybackSink writeFailure;
  writeFailure.fail_first_tone = true;
  assert(gatewayAudioPlayTone(item, &writeFailure) ==
         GatewayAudioPlaybackStepResult::WriteFailed);
  assert(writeFailure.tone_calls == 1);
  assert(writeFailure.gap_calls == 0);
  assert(writeFailure.silence_calls == 1);

  RecordingPlaybackSink cancelled;
  cancelled.cancel_first_tone = true;
  assert(gatewayAudioPlayTone(item, &cancelled) ==
         GatewayAudioPlaybackStepResult::Cancelled);
  assert(cancelled.tone_calls == 1);
  assert(cancelled.gap_calls == 0);
  assert(cancelled.silence_calls == 1);

  RecordingPlaybackSink successful;
  assert(gatewayAudioPlayTone(item, &successful) ==
         GatewayAudioPlaybackStepResult::Completed);
  assert(successful.tone_calls == 2);
  assert(successful.gap_calls == 1);
  assert(successful.silence_calls == 1);
}

}  // namespace

int main() {
  assert(!gatewayTemperatureOutOfRangeC(23.0f, 18.0f, 28.0f));
  assert(gatewayTemperatureOutOfRangeC(29.0f, 18.0f, 28.0f));
  assert(gatewayTemperatureOutOfRangeC(17.0f, 18.0f, 28.0f));
  assert(!gatewayTemperatureOutOfRangeC(NAN, 18.0f, 28.0f));
  assert(!gatewayTemperatureOutOfRangeC(fahrenheitToCelsius(82.0f), 18.0f,
                                        28.0f));
  assert(gatewayTemperatureOutOfRangeC(fahrenheitToCelsius(82.6f), 18.0f,
                                       28.0f));

  assert(!gatewayBatteryLowPercent(25.0f, 20.0f));
  assert(gatewayBatteryLowPercent(19.0f, 20.0f));
  assert(!gatewayBatteryLowPercent(NAN, 20.0f));

  char abv_text[16];
  gatewayFormatDashboardAbv(NAN, abv_text, sizeof(abv_text));
  assert(strcmp(abv_text, "--") == 0);
  gatewayFormatDashboardAbv(3.44f, abv_text, sizeof(abv_text));
  assert(strcmp(abv_text, "ABV 3.4%") == 0);

  static_assert(GATEWAY_AUDIO_QUEUE_CAPACITY == 4);
  static_assert(GATEWAY_AUDIO_CANCEL_KEY_CAPACITY == 40);
  static_assert(
      std::is_same<decltype(std::declval<GatewayAudio&>().queueAlarm(
                       std::declval<const GatewayAlarmEvent&>())),
                   bool>::value,
      "queueAlarm must return bool");
  static_assert(
      std::is_same<decltype(std::declval<GatewayAudio&>().cancelExact(
                       std::declval<const GatewayAlarmEvent&>())),
                   bool>::value,
      "cancelExact must return bool");
  static_assert(
      std::is_same<decltype(std::declval<GatewayAudio&>().cancelExactResult(
                       std::declval<const GatewayAlarmEvent&>())),
                   GatewayAudioCancelResult>::value,
      "cancelExactResult must expose cancellation status");
  static_assert(
      std::is_same<decltype(std::declval<GatewayAudio&>().cancelByKeyResult(
                       std::declval<GatewayAlarmType>(),
                       std::declval<const char*>())),
                   GatewayAudioCancelResult>::value,
      "cancelByKeyResult must expose cancellation status");
  static_assert(
      std::is_same<decltype(std::declval<GatewayAudio&>().resetKeySession(
                       std::declval<GatewayAlarmType>(),
                       std::declval<const char*>(),
                       std::declval<GatewayAlarmEventToken>())),
                   GatewayAudioCancelResult>::value,
      "resetKeySession must expose lifecycle status");
  static_assert(
      std::is_same<decltype(std::declval<GatewayAudio&>().retireExact(
                       std::declval<const GatewayAlarmEvent&>())),
                   GatewayAudioCancelResult>::value,
      "retireExact must expose lifecycle status");

  constexpr GatewayAudioAlarmDecision temperature =
      gatewayAudioDecisionForAlarm(GatewayAlarmType::TemperatureChange);
  constexpr GatewayAudioAlarmDecision offline =
      gatewayAudioDecisionForAlarm(GatewayAlarmType::Offline);
  static_assert(temperature.should_play &&
                temperature.first_frequency_hz == 880 &&
                temperature.second_frequency_hz == 1320 &&
                temperature.first_duration_ms == 120 &&
                temperature.second_duration_ms == 180 &&
                temperature.gap_ms == 60 &&
                temperature.amplitude_scale == 1.00f);
  static_assert(offline.should_play && offline.first_frequency_hz == 880 &&
                offline.second_frequency_hz == 1320 &&
                offline.first_duration_ms == 80 &&
                offline.second_duration_ms == 100 && offline.gap_ms == 50 &&
                offline.amplitude_scale == 0.50f);
  static_assert(temperature.first_frequency_hz == offline.first_frequency_hz);

  GatewayAudio audio;
  const GatewayAudioQueueItem event =
      makeEvent(GatewayAlarmType::TemperatureChange, "temp", 71, 0);
  assert(!audio.queueAlarm(event.event));
  assert(!audio.cancelExact(event.event));
  assert(!audio.cancelByKey(GatewayAlarmType::TemperatureChange, "temp"));

  testSchedulerFairnessAndDeduplication();
  testCapacityAndExactCancellation();
  testExactCancellationCoversAllRepeats();
  testPcmScalingAndPlaybackFailureModel();
  testCancellationHighWatermarksAndCapacity();
  testSafeAlarmSessionLifecycle();
  testRealPlaybackControlFlow();
  testPlaybackEntryPreservesCancellationCleanup();
  testTemperatureAndOfflineAlternateWhenBothActive();
  testCloseOneTypeWhileBothActive();

  puts("gateway audio alarm tests passed");
  return 0;
}
