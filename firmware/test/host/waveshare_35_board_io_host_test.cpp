#include <stdint.h>

#include <string>
#include <vector>

#include <Wire.h>

#include "../../src/gateway35_board_io.hpp"
#include "../../src/waveshare_35_profile.hpp"

TwoWire Wire;

namespace {
struct TestState {
  int failures = 0;
  std::vector<std::string> events;
  bool display_succeeds = true;

  void expect(bool condition) {
    if (!condition) ++failures;
  }
};

bool probe(void* context, TwoWire& wire) {
  static_cast<TestState*>(context)->events.push_back("probe");
  wire.beginTransmission(gateway35::kFt6336Address);
  return wire.endTransmission() == 0;
}

bool beginDisplay(void* context) {
  TestState* state = static_cast<TestState*>(context);
  state->events.push_back("display");
  return state->display_succeeds;
}

void fillBlack(void* context) {
  static_cast<TestState*>(context)->events.push_back("black");
}

void setBacklight(void* context, bool enabled) {
  static_cast<TestState*>(context)->events.push_back(enabled ? "backlight1"
                                                            : "backlight0");
}

void delayMs(void* context, uint32_t milliseconds) {
  static_cast<TestState*>(context)->events.push_back(
      std::string("delay") + std::to_string(milliseconds));
}

gateway35::StartupHooks hooksFor(TestState* state) {
  return {state, &probe, &beginDisplay, &fillBlack, &setBacklight, &delayMs};
}

void queueSuccessReads(TwoWire& wire) {
  const uint8_t config[] = {0xFB};
  const uint8_t output[] = {0xA4};
  wire.queueReadResponse(config, sizeof(config), 1);
  wire.queueReadResponse(output, sizeof(output), 1);
}

void verifySuccess(TestState* state, const TwoWire& wire) {
  state->expect(!state->events.empty() && state->events[0] == "backlight0");
  state->expect(wire.begin_calls.size() == 1);
  state->expect(wire.begin_calls.size() == 1 && wire.begin_calls[0].sda == 8 &&
                wire.begin_calls[0].scl == 7 &&
                wire.begin_calls[0].frequency == 100000);
  state->expect(wire.transmissions.size() == 7);
  state->expect(wire.transmissions.size() >= 1 &&
                wire.transmissions[0].address == 0x20 &&
                wire.transmissions[0].bytes == std::vector<uint8_t>{0x03} &&
                wire.transmissions[0].send_stop);
  state->expect(wire.transmissions.size() >= 2 &&
                wire.transmissions[1].bytes ==
                    std::vector<uint8_t>{0x03, 0xF9} &&
                wire.transmissions[1].send_stop);
  state->expect(wire.transmissions.size() >= 3 &&
                wire.transmissions[2].bytes == std::vector<uint8_t>{0x01} &&
                wire.transmissions[2].send_stop);
  state->expect(wire.transmissions.size() >= 4 &&
                wire.transmissions[3].bytes ==
                    std::vector<uint8_t>{0x01, 0xA6});
  state->expect(wire.transmissions.size() >= 5 &&
                wire.transmissions[4].bytes ==
                    std::vector<uint8_t>{0x01, 0xA4});
  state->expect(wire.transmissions.size() >= 6 &&
                wire.transmissions[5].bytes ==
                    std::vector<uint8_t>{0x01, 0xA6});
  state->expect(wire.transmissions.size() >= 7 &&
                wire.transmissions[6].address == 0x38 &&
                wire.transmissions[6].bytes.empty() &&
                wire.transmissions[6].send_stop);
  state->expect(wire.request_history.size() == 2);
  state->expect(wire.request_history.size() == 2 &&
                wire.request_history[0].address == 0x20 &&
                wire.request_history[0].quantity == 1 &&
                wire.request_history[1].address == 0x20 &&
                wire.request_history[1].quantity == 1);
  const std::vector<std::string> expected_events = {
      "backlight0", "delay10", "delay10", "delay200", "probe",
      "display", "black", "backlight1"};
  state->expect(state->events == expected_events);
}

void verifyFailureStopsBeforeBacklightOn(TestState* state, TwoWire& wire) {
  state->expect(!gateway35::initializePanelHardware(wire, hooksFor(state)));
  bool backlight_on = false;
  for (const std::string& event : state->events) {
    if (event == "backlight1") backlight_on = true;
  }
  state->expect(!backlight_on);
}

void merge(TestState* destination, const TestState& source) {
  destination->failures += source.failures;
}

void testSuccess(TestState* state) {
  Wire = TwoWire();
  queueSuccessReads(Wire);
  state->expect(gateway35::initializePanelHardware(Wire, hooksFor(state)));
  verifySuccess(state, Wire);
}

void testProbeFailure(TestState* state) {
  Wire = TwoWire();
  TestState fixture;
  queueSuccessReads(Wire);
  std::vector<uint8_t> results(7, 0);
  results[6] = 1;
  Wire.setEndTransmissionResults(results.data(), results.size());
  fixture.expect(gateway35::initializePanelHardware(Wire, hooksFor(&fixture)));
  fixture.expect(Wire.transmissions.size() == 7);
  fixture.expect(Wire.transmissions[6].address == gateway35::kFt6336Address);
  fixture.expect(fixture.events ==
                 std::vector<std::string>{"backlight0", "delay10", "delay10",
                                          "delay200", "probe", "display",
                                          "black", "backlight1"});
  merge(state, fixture);
}

void testI2cAndDisplayFailures(TestState* state) {
  Wire = TwoWire();
  TestState begin_failure;
  Wire.setBeginResult(false);
  verifyFailureStopsBeforeBacklightOn(&begin_failure, Wire);
  begin_failure.expect(Wire.begin_calls.size() == 1);
  begin_failure.expect(Wire.transmissions.empty());
  begin_failure.expect(begin_failure.events ==
                       std::vector<std::string>{"backlight0"});
  merge(state, begin_failure);

  for (const uint8_t failure_at : {1, 2, 3, 4, 5}) {
    Wire = TwoWire();
    TestState fixture;
    queueSuccessReads(Wire);
    std::vector<uint8_t> results(7, 0);
    results[failure_at] = 1;
    Wire.setEndTransmissionResults(results.data(), results.size());
    verifyFailureStopsBeforeBacklightOn(&fixture, Wire);
    merge(state, fixture);
  }

  Wire = TwoWire();
  TestState config_short;
  const uint8_t config[] = {0xFB};
  Wire.queueReadResponse(config, sizeof(config), 0);
  verifyFailureStopsBeforeBacklightOn(&config_short, Wire);
  merge(state, config_short);

  Wire = TwoWire();
  TestState output_short;
  Wire.queueReadResponse(config, sizeof(config), 1);
  Wire.queueReadResponse(config, sizeof(config), 0);
  verifyFailureStopsBeforeBacklightOn(&output_short, Wire);
  merge(state, output_short);

  Wire = TwoWire();
  TestState config_read_error;
  Wire.queueReadResponse(config, sizeof(config), 1);
  Wire.queueReadFailure(true);
  verifyFailureStopsBeforeBacklightOn(&config_read_error, Wire);
  merge(state, config_read_error);

  Wire = TwoWire();
  queueSuccessReads(Wire);
  TestState display_failure;
  display_failure.display_succeeds = false;
  verifyFailureStopsBeforeBacklightOn(&display_failure, Wire);
  state->expect(display_failure.events.back() == "display");
  merge(state, display_failure);
}
}  // namespace

int main() {
  TestState state;
  testSuccess(&state);
  testProbeFailure(&state);
  testI2cAndDisplayFailures(&state);
  return state.failures == 0 ? 0 : 1;
}
