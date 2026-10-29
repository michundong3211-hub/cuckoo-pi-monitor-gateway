#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <atomic>

#define private public
#include "../../src/gateway_alarm.hpp"
#undef private

namespace {

void recordWindow(GatewayAlarmTracker& tracker, const char* id, float first,
                  float second, uint32_t start_ms = 0) {
  assert(!tracker.recordTemperature(id, first, true, 5, 1.0f, start_ms)
              .should_play);
  assert(tracker
             .recordTemperature(id, second, true, 5, 1.0f,
                                start_ms + 5U * 60U * 1000U)
             .should_play);
}

void testTemperatureHistory() {
  GatewayAlarmTracker tracker;

  assert(!tracker.recordTemperature("disabled", 20.0f, false, 5, 1.0f, 0)
              .should_play);
  assert(!tracker.recordTemperature("nan", NAN, true, 5, 1.0f, 0)
              .should_play);
  assert(!tracker.recordTemperature("short", 20.0f, true, 5, 1.0f, 0)
              .should_play);
  assert(!tracker.recordTemperature("short", 22.0f, true, 5, 1.0f, 299999)
              .should_play);

  GatewayAlarmTracker same_minute;
  assert(!same_minute.recordTemperature("same", 10.0f, true, 5, 2.0f, 0)
              .should_play);
  assert(!same_minute
              .recordTemperature("same", 12.0f, true, 5, 2.0f, 30000)
              .should_play);
  assert(!same_minute
              .recordTemperature("same", 13.0f, true, 5, 2.0f, 330000)
              .should_play);

  GatewayAlarmTracker rise;
  recordWindow(rise, "rise", 20.0f, 21.0f);
  assert(!rise.recordTemperature("rise", 22.0f, true, 5, 1.0f, 360000)
              .should_play);

  GatewayAlarmTracker fall;
  recordWindow(fall, "fall", 20.0f, 19.0f);

  GatewayAlarmTracker tolerance;
  assert(!tolerance.recordTemperature("early", 20.0f, true, 5, 1.0f, 0)
              .should_play);
  assert(!tolerance
              .recordTemperature("early", 22.0f, true, 5, 1.0f, 299999)
              .should_play);
  GatewayAlarmTracker too_old;
  assert(!too_old.recordTemperature("old", 20.0f, true, 5, 1.0f, 0)
              .should_play);
  assert(!too_old.recordTemperature("old", 22.0f, true, 5, 1.0f, 390001)
              .should_play);
  GatewayAlarmTracker edge;
  assert(!edge.recordTemperature("edge", 20.0f, true, 5, 1.0f, 0)
              .should_play);
  assert(edge.recordTemperature("edge", 21.0f, true, 5, 1.0f, 390000)
             .should_play);

  GatewayAlarmTracker rearm;
  recordWindow(rearm, "rearm", 20.0f, 21.0f);
  GatewayAlarmDecision temperature_reset =
      rearm.recordTemperature("rearm", 20.8f, true, 5, 1.0f, 600000);
  assert(!temperature_reset.should_play);
  assert(temperature_reset.should_clear);
  GatewayAlarmDecision repeated_temperature_reset =
      rearm.recordTemperature("rearm", 20.7f, true, 5, 1.0f, 900000);
  assert(!repeated_temperature_reset.should_play);
  assert(!repeated_temperature_reset.should_clear);
  assert(!rearm.recordTemperature("rearm", 20.7f, true, 5, 1.0f, 1200000)
              .should_play);
  assert(rearm.recordTemperature("rearm", 21.8f, true, 5, 1.0f, 1500000)
             .should_play);

  GatewayAlarmTracker disabled_reset;
  assert(!disabled_reset
              .recordTemperature("reset", 20.0f, true, 5, 1.0f, 0)
              .should_play);
  assert(!disabled_reset
              .recordTemperature("reset", 21.0f, false, 5, 1.0f, 300000)
              .should_play);
  assert(!disabled_reset
              .recordTemperature("reset", 22.0f, true, 5, 1.0f, 600000)
              .should_play);
  assert(disabled_reset
             .recordTemperature("reset", 23.0f, true, 5, 1.0f, 900000)
             .should_play);

  GatewayAlarmTracker isolated;
  assert(!isolated.recordTemperature("a", 20.0f, true, 5, 1.0f, 0)
              .should_play);
  assert(!isolated.recordTemperature("b", 30.0f, true, 5, 1.0f, 0)
              .should_play);
  assert(isolated.recordTemperature("a", 21.0f, true, 5, 1.0f, 300000)
             .should_play);
  assert(!isolated.recordTemperature("b", 30.5f, true, 5, 1.0f, 300000)
              .should_play);
}

void testTemperatureClearBoundaryIsInclusive() {
  GatewayAlarmTracker tracker;
  assert(!tracker.recordTemperature("clear-boundary", 20.0f, true, 5,
                                    1.25f, 0)
              .should_play);
  assert(tracker.recordTemperature("clear-boundary", 22.0f, true, 5,
                                   1.25f, 300000U)
             .should_play);

  const GatewayAlarmDecision cleared = tracker.recordTemperature(
      "clear-boundary", 21.0f, true, 5, 1.25f, 600000U);
  assert(!cleared.should_play);
  assert(cleared.should_clear);
}

void testSingleEventSoundsEveryTenSecondsForFiveMinutes() {
  const uint32_t t0 = 700000U;
  GatewayAlarmTracker tracker;
  assert(!tracker.recordTemperature("five-minute-event", 20.0f, true, 5,
                                    1.0f, t0 - 300000U)
              .should_play);
  assert(tracker.recordTemperature("five-minute-event", 21.0f, true, 5,
                                   1.0f, t0)
             .should_play);

  GatewayAlarmDecision first = tracker.schedule(
      GatewayAlarmType::TemperatureChange, "five-minute-event", t0);
  assert(first.should_play);
  assert(first.event.repeat_index == 0);
  for (uint8_t slot = 1; slot < 30; ++slot) {
    const GatewayAlarmDecision next = tracker.schedule(
        GatewayAlarmType::TemperatureChange, "five-minute-event",
        t0 + static_cast<uint32_t>(slot) * 10000U);
    if (!next.should_play) {
      fprintf(stderr, "missing 10-second sound slot %u\n",
              static_cast<unsigned>(slot));
      exit(10);
    }
    assert(next.event.repeat_index == slot);
    assert(next.event.event_token == first.event.event_token);
    assert(next.event.condition_token == first.event.condition_token);
  }

  assert(!tracker.schedule(GatewayAlarmType::TemperatureChange,
                           "five-minute-event", t0 + 300000U)
              .should_play);
  assert(!tracker.schedule(GatewayAlarmType::TemperatureChange,
                           "five-minute-event", t0 + 600000U)
              .should_play);
}

void testIdsCapacityRetainAndWraparound() {
  char id63[64];
  memset(id63, 'x', 63);
  id63[63] = '\0';
  char id64[65];
  memset(id64, 'x', 64);
  id64[64] = '\0';

  GatewayAlarmTracker ids;
  assert(!ids.recordTemperature(id63, 20.0f, true, 5, 1.0f, 0)
              .should_play);
  assert(ids.recordTemperature(id63, 21.0f, true, 5, 1.0f, 300000)
             .should_play);
  assert(!ids.recordTemperature(id64, 20.0f, true, 5, 1.0f, 0)
              .should_play);
  assert(!ids.recordTemperature(id64, 30.0f, true, 5, 1.0f, 300000)
              .should_play);

  GatewayAlarmTracker capacity;
  char names[21][8] = {};
  const char* retained[1] = {names[0]};
  for (int i = 0; i < 20; ++i) {
    snprintf(names[i], sizeof(names[i]), "d%02d", i);
    capacity.markOnline(names[i]);
  }
  snprintf(names[20], sizeof(names[20]), "d20");
  assert(!capacity.evaluateOffline(names[20], 60, true, 60).should_play);
  retained[0] = names[0];
  capacity.retainDevices(retained, 1);
  capacity.markOnline(names[20]);
  assert(capacity.evaluateOffline(names[20], 60, true, 60).should_play);

  GatewayAlarmTracker wrapped;
  const uint32_t start = UINT32_MAX - 179999U;
  assert(!wrapped.recordTemperature("wrap", 20.0f, true, 3, 1.0f, start)
              .should_play);
  assert(wrapped.recordTemperature("wrap", 21.0f, true, 3, 1.0f, 0)
             .should_play);
}

void testOffline() {
  GatewayAlarmTracker tracker;
  assert(!tracker.evaluateOffline("disabled", 60, false, 60).should_play);
  assert(!tracker.evaluateOffline("never", 60, true, 60).should_play);

  assert(!tracker.markOnline("one"));
  assert(!tracker.evaluateOffline("one", 59, true, 60).should_play);
  GatewayAlarmDecision decision =
      tracker.evaluateOffline("one", 60, true, 60);
  assert(decision.should_play);
  assert(decision.type == GatewayAlarmType::Offline);
  assert(!tracker.evaluateOffline("one", 61, true, 60).should_play);

  assert(tracker.markOnline("one"));
  assert(tracker.evaluateOffline("one", 60, true, 60).should_play);

  tracker.markOnline("two");
  assert(tracker.evaluateOffline("two", 60, true, 60).should_play);
  assert(!tracker.evaluateOffline("one", 60, true, 60).should_play);

  assert(!tracker.evaluateOffline("two", 60, false, 60).should_play);
  assert(tracker.evaluateOffline("two", 60, true, 60).should_play);
}

void testGatewayLoopSequenceAndPerTypeDeduplication() {
  GatewayAlarmTracker tracker;
  uint32_t processed_generation = 0;
  uint32_t measurement_generation = 1;

  if (measurement_generation != processed_generation) {
    processed_generation = measurement_generation;
    tracker.markOnline("loop");
    assert(!tracker.recordTemperature("loop", 20.0f, true, 5, 1.0f, 0)
                .should_play);
  }
  assert(!tracker.evaluateOffline("loop", 59, true, 60).should_play);
  assert(tracker.evaluateOffline("loop", 60, true, 60).should_play);
  assert(!tracker.evaluateOffline("loop", 61, true, 60).should_play);

  ++measurement_generation;  // A second update in the same wall-clock second.
  if (measurement_generation != processed_generation) {
    processed_generation = measurement_generation;
    tracker.markOnline("loop");
    assert(tracker.recordTemperature("loop", 21.0f, true, 5, 1.0f, 300000)
               .should_play);
  }
  assert(tracker.evaluateOffline("loop", 60, true, 60).should_play);
  assert(!tracker.evaluateOffline("loop", 61, true, 60).should_play);
}

void testIndependentScheduleBoundaryWindows() {
  const uint32_t t0 = 100000U;

  GatewayAlarmTracker delayed_to_boundary;
  assert(!delayed_to_boundary.recordTemperature(
      "boundary-late", 20.0f, true, 5, 1.0f,
      t0 - 5U * 60U * 1000U)
              .should_play);
  assert(delayed_to_boundary
             .recordTemperature("boundary-late", 21.0f, true, 5, 1.0f, t0)
             .should_play);
  const GatewayAlarmDecision last_audible_slot =
      delayed_to_boundary.schedule(GatewayAlarmType::TemperatureChange,
                                   "boundary-late", t0 + 59999U);
  assert(last_audible_slot.should_play);
  assert(last_audible_slot.event.repeat_index == 5);

  GatewayAlarmTracker full_event_window;
  assert(!full_event_window.recordTemperature(
      "boundary-full", 20.0f, true, 5, 1.0f,
      t0 - 5U * 60U * 1000U)
              .should_play);
  assert(full_event_window
             .recordTemperature("boundary-full", 21.0f, true, 5, 1.0f,
                                t0)
             .should_play);
  const GatewayAlarmDecision minute_slot = full_event_window.schedule(
      GatewayAlarmType::TemperatureChange, "boundary-full", t0 + 60000U);
  assert(minute_slot.should_play);
  assert(minute_slot.event.repeat_index == 6);
  const GatewayAlarmDecision final_slot = full_event_window.schedule(
      GatewayAlarmType::TemperatureChange, "boundary-full", t0 + 299999U);
  assert(final_slot.should_play);
  assert(final_slot.event.repeat_index == 29);
  assert(!full_event_window
              .schedule(GatewayAlarmType::TemperatureChange, "boundary-full",
                        t0 + 300000U)
              .should_play);
}

void testPeriodicScheduleAndExactTokens() {
  const uint32_t t0 = 100000U;
  GatewayAlarmTracker tracker;

  assert(!tracker.recordTemperature("T", 20.0f, true, 5, 1.0f,
                                    t0 - 5U * 60U * 1000U)
              .should_play);
  assert(tracker.recordTemperature("T", 21.0f, true, 5, 1.0f, t0)
             .should_play);

  tracker.markOnline("O");
  assert(tracker.evaluateOffline("O", 60, true, 60).should_play);

  const GatewayAlarmDecision first = tracker.schedule(
      GatewayAlarmType::TemperatureChange, "T", t0);
  assert(first.should_play);
  assert(first.event.event_token != GATEWAY_INVALID_EVENT_TOKEN);
  assert(first.event.condition_token != GATEWAY_INVALID_EVENT_TOKEN);
  assert(first.event.condition_token != first.event.event_token);
  assert(first.event.repeat_index == 0);
  assert(strcmp(first.event.device_id, "T") == 0);
  assert(!tracker.schedule(GatewayAlarmType::TemperatureChange, "T", t0)
              .should_play);
  assert(tracker.schedule(GatewayAlarmType::TemperatureChange, "T",
                          t0 + 10000U)
             .event.repeat_index == 1);
  assert(tracker.schedule(GatewayAlarmType::TemperatureChange, "T",
                          t0 + 20000U)
             .event.repeat_index == 2);
  assert(tracker.schedule(GatewayAlarmType::TemperatureChange, "T",
                          t0 + 30000U)
             .event.repeat_index == 3);
  assert(tracker.schedule(GatewayAlarmType::TemperatureChange, "T",
                          t0 + 40000U)
             .event.repeat_index == 4);
  assert(tracker.schedule(GatewayAlarmType::TemperatureChange, "T",
                          t0 + 50000U)
             .event.repeat_index == 5);
  assert(!tracker.schedule(GatewayAlarmType::TemperatureChange, "T",
                           t0 + 59999U)
              .should_play);
  const GatewayAlarmDecision minute_slot = tracker.schedule(
      GatewayAlarmType::TemperatureChange, "T", t0 + 60000U);
  assert(minute_slot.should_play);
  assert(minute_slot.event.repeat_index == 6);
  const GatewayAlarmDecision final_slot = tracker.schedule(
      GatewayAlarmType::TemperatureChange, "T", t0 + 299999U);
  assert(final_slot.should_play);
  assert(final_slot.event.repeat_index == 29);
  assert(!tracker.schedule(GatewayAlarmType::TemperatureChange, "T",
                           t0 + 300000U)
              .should_play);
  assert(tracker.closeExact(first.event.event_token) ==
         GatewayAlarmCloseResult::Closed);
  const GatewayAlarmDecision after_old_close = tracker.schedule(
      GatewayAlarmType::TemperatureChange, "T", t0 + 310000U);
  assert(!after_old_close.should_play);
  assert(!tracker.schedule(GatewayAlarmType::TemperatureChange, "T",
                           t0 + 310000U)
              .should_play);
  // A CLOSE stays effective across window rollovers: the alarm must not re-arm
  // on the next 5-minute cycle while the condition persists.
  assert(!tracker.schedule(GatewayAlarmType::TemperatureChange, "T",
                           t0 + 600000U)
              .should_play);
  assert(!tracker.schedule(GatewayAlarmType::TemperatureChange, "T",
                           t0 + 900000U)
              .should_play);

  const GatewayAlarmDecision offline =
      tracker.schedule(GatewayAlarmType::Offline, "O", t0);
  assert(offline.should_play);
  assert(offline.event.type == GatewayAlarmType::Offline);
  assert(offline.event.condition_token != GATEWAY_INVALID_EVENT_TOKEN);
  assert(offline.event.event_token != GATEWAY_INVALID_EVENT_TOKEN);
  assert(offline.event.condition_token != offline.event.event_token);
  assert(offline.event.repeat_index == 0);
  assert(!tracker.schedule(GatewayAlarmType::Offline, "O", t0)
              .should_play);

  GatewayAlarmTracker isolated;
  assert(!isolated.recordTemperature("A", 20.0f, true, 5, 1.0f,
                                     t0 - 5U * 60U * 1000U)
              .should_play);
  assert(isolated.recordTemperature("A", 21.0f, true, 5, 1.0f, t0)
             .should_play);
  assert(!isolated.recordTemperature("B", 30.0f, true, 5, 1.0f,
                                     t0 - 5U * 60U * 1000U)
              .should_play);
  assert(isolated.recordTemperature("B", 31.0f, true, 5, 1.0f, t0)
             .should_play);
  const GatewayAlarmDecision event_a =
      isolated.schedule(GatewayAlarmType::TemperatureChange, "A", t0);
  const GatewayAlarmDecision event_b =
      isolated.schedule(GatewayAlarmType::TemperatureChange, "B", t0);
  assert(event_a.should_play && event_b.should_play);
  assert(event_a.event.event_token != event_b.event.event_token);
  assert(event_a.event.condition_token != event_b.event.condition_token);

  GatewayAlarmTracker delayed;
  assert(!delayed.recordTemperature("late", 20.0f, true, 5, 1.0f,
                                    t0 - 5U * 60U * 1000U)
              .should_play);
  assert(delayed.recordTemperature("late", 21.0f, true, 5, 1.0f, t0)
             .should_play);
  const GatewayAlarmDecision delayed_event = delayed.schedule(
      GatewayAlarmType::TemperatureChange, "late", t0 + 35000U);
  assert(delayed_event.should_play);
  assert(delayed_event.event.repeat_index == 3);
  assert(!delayed.schedule(GatewayAlarmType::TemperatureChange, "late",
                           t0 + 35000U)
              .should_play);

  GatewayAlarmTracker recovered;
  assert(!recovered.recordTemperature("recover", 20.0f, true, 5, 1.0f,
                                      t0 - 5U * 60U * 1000U)
              .should_play);
  assert(recovered.recordTemperature("recover", 21.0f, true, 5, 1.0f, t0)
             .should_play);
  const GatewayAlarmDecision recovered_first = recovered.schedule(
      GatewayAlarmType::TemperatureChange, "recover", t0);
  assert(recovered_first.should_play);
  const GatewayAlarmDecision cleared = recovered.recordTemperature(
      "recover", 21.5f, true, 5, 1.0f, t0 + 5U * 60U * 1000U);
  assert(cleared.should_clear);
  assert(!recovered.schedule(GatewayAlarmType::TemperatureChange, "recover",
                             t0 + 5U * 60U * 1000U + 10000U)
              .should_play);
  assert(recovered.recordTemperature("recover", 23.0f, true, 5, 1.0f,
                                     t0 + 10U * 60U * 1000U)
             .should_play);
  const GatewayAlarmDecision rearmed = recovered.schedule(
      GatewayAlarmType::TemperatureChange, "recover",
      t0 + 10U * 60U * 1000U);
  assert(rearmed.should_play);
  assert(rearmed.event.condition_token !=
         recovered_first.event.condition_token);

  GatewayAlarmTracker wrapped;
  const uint32_t wrap_t0 = UINT32_MAX - 1000U;
  assert(!wrapped.recordTemperature("wrap-schedule", 20.0f, true, 5, 1.0f,
                                    wrap_t0 - 5U * 60U * 1000U)
              .should_play);
  assert(wrapped.recordTemperature("wrap-schedule", 21.0f, true, 5, 1.0f,
                                  wrap_t0)
             .should_play);
  const GatewayAlarmDecision wrapped_first = wrapped.schedule(
      GatewayAlarmType::TemperatureChange, "wrap-schedule", wrap_t0);
  assert(wrapped_first.should_play);
  const GatewayAlarmDecision wrapped_repeat = wrapped.schedule(
      GatewayAlarmType::TemperatureChange, "wrap-schedule", 8999U);
  assert(wrapped_repeat.should_play);
  assert(wrapped_repeat.event.repeat_index == 1);
  const GatewayAlarmDecision wrapped_quiet = wrapped.schedule(
      GatewayAlarmType::TemperatureChange, "wrap-schedule",
      static_cast<uint32_t>(wrap_t0 + kAlarmCyclePeriodMs - 1U));
  assert(wrapped_quiet.should_play);
  assert(wrapped_quiet.event.repeat_index == 29);
  const GatewayAlarmDecision wrapped_next_window = wrapped.schedule(
      GatewayAlarmType::TemperatureChange, "wrap-schedule",
      static_cast<uint32_t>(wrap_t0 + kAlarmCyclePeriodMs));
  assert(!wrapped_next_window.should_play);

  GatewayAlarmTracker rollover;
  rollover._token_prefix = 0x10203040UL;
  rollover._token_sequence = UINT32_MAX - 1U;
  assert(!rollover.recordTemperature("rollover", 20.0f, true, 5, 1.0f,
                                     t0 - 5U * 60U * 1000U)
              .should_play);
  assert(rollover.recordTemperature("rollover", 21.0f, true, 5, 1.0f, t0)
             .should_play);
  const GatewayAlarmDecision rollover_first = rollover.schedule(
      GatewayAlarmType::TemperatureChange, "rollover", t0);
  assert(rollover_first.should_play);
  assert(static_cast<uint32_t>(rollover_first.event.condition_token >> 32) ==
         0x10203040UL);
  assert(static_cast<uint32_t>(rollover_first.event.event_token >> 32) ==
         0x10203040UL);
  assert(!rollover.recordTemperature("rollover-next", 30.0f, true, 5, 1.0f,
                                     t0 - 5U * 60U * 1000U)
              .should_play);
  assert(rollover.recordTemperature("rollover-next", 31.0f, true, 5, 1.0f,
                                    t0)
             .should_play);
  const GatewayAlarmDecision rollover_second = rollover.schedule(
      GatewayAlarmType::TemperatureChange, "rollover-next", t0);
  assert(rollover_second.should_play);
  assert(static_cast<uint32_t>(rollover_second.event.event_token >> 32) ==
         0x10203041UL);
  assert(rollover_second.event.event_token !=
         rollover_first.event.event_token);
  rollover._token_sequence = UINT32_MAX;
  assert(!rollover.recordTemperature("rollover-third", 40.0f, true, 5,
                                     1.0f, t0 - 5U * 60U * 1000U)
              .should_play);
  assert(rollover.recordTemperature("rollover-third", 41.0f, true, 5,
                                    1.0f, t0)
             .should_play);
  const GatewayAlarmDecision rollover_third = rollover.schedule(
      GatewayAlarmType::TemperatureChange, "rollover-third", t0);
  assert(rollover_third.should_play);
  assert(static_cast<uint32_t>(rollover_third.event.event_token >> 32) ==
         0x10203042UL);
  assert(rollover_third.event.event_token !=
         rollover_second.event.event_token);

  char id64[65];
  memset(id64, 'x', 64);
  id64[64] = '\0';
  assert(!tracker.schedule(GatewayAlarmType::TemperatureChange, "", t0)
              .should_play);
  assert(!tracker.schedule(GatewayAlarmType::TemperatureChange, nullptr, t0)
              .should_play);
  assert(!tracker.schedule(GatewayAlarmType::TemperatureChange, id64, t0)
              .should_play);

  GatewayAlarmTracker capacity;
  char names[20][8] = {};
  for (int i = 0; i < 20; ++i) {
    snprintf(names[i], sizeof(names[i]), "s%02d", i);
    capacity.markOnline(names[i]);
  }
  assert(!capacity.recordTemperature("over-capacity", 20.0f, true, 5, 1.0f,
                                     t0)
              .should_play);
  assert(!capacity.schedule(GatewayAlarmType::TemperatureChange,
                            "over-capacity", t0)
              .should_play);

  GatewayAlarmTracker lock_test;
  assert(!lock_test.recordTemperature("locked", 20.0f, true, 5, 1.0f,
                                      t0 - 5U * 60U * 1000U)
              .should_play);
  assert(lock_test.recordTemperature("locked", 21.0f, true, 5, 1.0f, t0)
             .should_play);
  const GatewayAlarmDecision unlocked = lock_test.schedule(
      GatewayAlarmType::TemperatureChange, "locked", t0);
  assert(unlocked.should_play);
  assert(lock_test._state_lock.tryEnter());
  assert(!lock_test.schedule(GatewayAlarmType::TemperatureChange, "locked",
                             t0 + 10000U)
              .should_play);
  assert(lock_test.closeExact(unlocked.event.event_token) ==
         GatewayAlarmCloseResult::Retry);
  lock_test._state_lock.exit();
  assert(lock_test.closeExact(unlocked.event.event_token) ==
         GatewayAlarmCloseResult::Closed);
}

void testOfflineDismissedMarker() {
  const uint32_t t0 = 500000U;
  GatewayAlarmTracker tracker;
  assert(!tracker.isOfflineDismissed(""));
  tracker.markOnline("dismiss");
  assert(!tracker.isOfflineDismissed("dismiss"));
  assert(tracker.evaluateOffline("dismiss", 60, true, 60).should_play);
  const GatewayAlarmDecision ev =
      tracker.schedule(GatewayAlarmType::Offline, "dismiss", t0);
  assert(ev.should_play);
  // Not dismissed before CLOSE.
  assert(!tracker.isOfflineDismissed("dismiss"));
  assert(tracker.closeExact(ev.event.event_token) ==
         GatewayAlarmCloseResult::Closed);
  // Dismissed while still offline.
  assert(tracker.isOfflineDismissed("dismiss"));
  // Data recovered -> marker cleared.
  assert(tracker.markOnline("dismiss"));
  assert(!tracker.isOfflineDismissed("dismiss"));
}

void testExternalAlarmConditionsRemainQueryablePerDeviceUntilCleared() {
  GatewayAlarmTracker tracker;

  assert(!tracker.isConditionActive(GatewayAlarmType::CuckooOffline,
                                    "fermenter-a"));
  tracker.setExternalCondition(GatewayAlarmType::CuckooOffline, "fermenter-a",
                               true, 100U);
  tracker.setExternalCondition(GatewayAlarmType::CuckooBeerDeviation,
                               "fermenter-a", true, 100U);
  assert(tracker.isConditionActive(GatewayAlarmType::CuckooOffline,
                                  "fermenter-a"));
  assert(tracker.isConditionActive(GatewayAlarmType::CuckooBeerDeviation,
                                   "fermenter-a"));
  assert(!tracker.isConditionActive(GatewayAlarmType::CuckooOffline,
                                    "fermenter-b"));
  assert(!tracker.isConditionActive(GatewayAlarmType::CuckooBeerDeviation,
                                    "fermenter-b"));

  // Scheduling windows may end, but the condition remains active until clear.
  assert(!tracker.schedule(GatewayAlarmType::CuckooOffline, "fermenter-a",
                           300100U)
              .should_play);
  assert(tracker.isConditionActive(GatewayAlarmType::CuckooOffline,
                                   "fermenter-a"));

  tracker.setExternalCondition(GatewayAlarmType::CuckooOffline, "fermenter-a",
                               false, 300101U);
  assert(!tracker.isConditionActive(GatewayAlarmType::CuckooOffline,
                                    "fermenter-a"));
  assert(tracker.isConditionActive(GatewayAlarmType::CuckooBeerDeviation,
                                   "fermenter-a"));

  tracker.setExternalCondition(GatewayAlarmType::CuckooBeerDeviation,
                               "fermenter-a", false, 300102U);
  assert(!tracker.isConditionActive(GatewayAlarmType::CuckooBeerDeviation,
                                    "fermenter-a"));
}

}  // namespace

int main() {
  testTemperatureHistory();
  testSingleEventSoundsEveryTenSecondsForFiveMinutes();
  testTemperatureClearBoundaryIsInclusive();
  testIdsCapacityRetainAndWraparound();
  testOffline();
  testGatewayLoopSequenceAndPerTypeDeduplication();
  testIndependentScheduleBoundaryWindows();
  testPeriodicScheduleAndExactTokens();
  testOfflineDismissedMarker();
  testExternalAlarmConditionsRemainQueryablePerDeviceUntilCleared();
  puts("gateway alarm schedule tests passed");
  return 0;
}
