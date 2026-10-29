#include <stdint.h>

#include <Wire.h>

#include "../../src/gateway35_touch.hpp"

TwoWire Wire;

namespace {
struct TestState {
  int failures = 0;
  void expect(bool condition) {
    if (!condition) ++failures;
  }
};

void queueReport(TwoWire& wire, const uint8_t (&report)[5],
                 size_t return_count = 5) {
  wire.queueReadResponse(report, sizeof(report), return_count);
}

void testProbe(TestState* state) {
  Wire = TwoWire();
  state->expect(gateway35::probeTouch(Wire));
  state->expect(Wire.transmissions.size() == 1);
  state->expect(Wire.transmissions[0].address == 0x38);
  state->expect(Wire.transmissions[0].bytes.empty());
  state->expect(Wire.transmissions[0].send_stop);

  Wire = TwoWire();
  const uint8_t nack[] = {1};
  Wire.setEndTransmissionResults(nack, sizeof(nack));
  state->expect(!gateway35::probeTouch(Wire));
}

void expectReadShape(TestState* state) {
  state->expect(Wire.transmissions.size() == 1);
  state->expect(Wire.transmissions[0].address == 0x38);
  state->expect(Wire.transmissions[0].bytes.size() == 1 &&
                Wire.transmissions[0].bytes[0] == 0x02);
  state->expect(!Wire.transmissions[0].send_stop);
  state->expect(Wire.request_history.size() == 1);
  state->expect(Wire.request_history[0].address == 0x38);
  state->expect(Wire.request_history[0].quantity == 5);
}

void testReports(TestState* state) {
  const uint8_t no_touch[] = {0x00, 0, 0, 0, 0};
  Wire = TwoWire();
  queueReport(Wire, no_touch);
  gateway35::TouchPoint point{};
  state->expect(gateway35::readTouch(Wire, &point) ==
                gateway35::TouchResult::kNoTouch);
  expectReadShape(state);

  const uint8_t one_touch[] = {0x01, 0x01, 0x3F, 0x01, 0xDF};
  Wire = TwoWire();
  queueReport(Wire, one_touch);
  state->expect(gateway35::readTouch(Wire, &point) ==
                gateway35::TouchResult::kValid);
  state->expect(point.x == 319 && point.y == 479);
  expectReadShape(state);

  const uint8_t two_touches[] = {0x02, 0x01, 0x3F, 0x01, 0xDF};
  Wire = TwoWire();
  queueReport(Wire, two_touches);
  state->expect(gateway35::readTouch(Wire, &point) ==
                gateway35::TouchResult::kValid);
  state->expect(point.x == 319 && point.y == 479);

  const uint8_t many_touches[] = {0x03, 0, 0, 0, 0};
  Wire = TwoWire();
  queueReport(Wire, many_touches);
  state->expect(gateway35::readTouch(Wire, &point) ==
                gateway35::TouchResult::kInvalidReport);

  const uint8_t x_out_of_range[] = {0x01, 0x01, 0x40, 0x00, 0x00};
  Wire = TwoWire();
  queueReport(Wire, x_out_of_range);
  state->expect(gateway35::readTouch(Wire, &point) ==
                gateway35::TouchResult::kInvalidReport);

  const uint8_t y_out_of_range[] = {0x01, 0x00, 0x00, 0x01, 0xE0};
  Wire = TwoWire();
  queueReport(Wire, y_out_of_range);
  state->expect(gateway35::readTouch(Wire, &point) ==
                gateway35::TouchResult::kInvalidReport);
}

void testIoFailures(TestState* state) {
  gateway35::TouchPoint point{};
  const uint8_t report[] = {0x01, 0, 1, 0, 1};

  Wire = TwoWire();
  const uint8_t nack[] = {1};
  Wire.setEndTransmissionResults(nack, sizeof(nack));
  state->expect(gateway35::readTouch(Wire, &point) ==
                gateway35::TouchResult::kIoError);

  Wire = TwoWire();
  queueReport(Wire, report, 4);
  state->expect(gateway35::readTouch(Wire, &point) ==
                gateway35::TouchResult::kIoError);

  Wire = TwoWire();
  Wire.queueReadResponse(report, 4, 5);
  state->expect(gateway35::readTouch(Wire, &point) ==
                gateway35::TouchResult::kIoError);

  Wire = TwoWire();
  queueReport(Wire, report);
  Wire.queueReadFailure(true);
  state->expect(gateway35::readTouch(Wire, &point) ==
                gateway35::TouchResult::kIoError);
}
}  // namespace

int main() {
  TestState state;
  testProbe(&state);
  testReports(&state);
  testIoFailures(&state);
  return state.failures == 0 ? 0 : 1;
}
