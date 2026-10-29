#include <assert.h>
#include <stdint.h>

#include "../../lib/qmi8658/Qmi8658.hpp"

TwoWire Wire;

int main() {
  const uint8_t raw[6] = {0x00, 0x20, 0x00, 0xE0, 0x00, 0x00};
  const GatewayAccelerationSample value = Qmi8658::decodeAcceleration(raw);

  assert(value.x_g > 0.99f && value.x_g < 1.01f);
  assert(value.y_g < -0.99f && value.y_g > -1.01f);
  assert(value.z_g == 0.0f);

  Qmi8658 qmi;
  const uint8_t probe_results[] = {0, 0, 0, 0};
  Wire.setEndTransmissionResults(probe_results, sizeof(probe_results));
  assert(qmi.begin(Wire));
  assert(Wire.begin_calls.size() == 1);
  assert(Wire.begin_calls[0].sda == 11);
  assert(Wire.begin_calls[0].scl == 10);
  assert(Wire.begin_calls[0].frequency == 400000);
  assert(Wire.transmissions.size() == 4);
  assert(Wire.transmissions[0].address == 0x6A);
  assert(Wire.transmissions[0].bytes.empty());
  assert(Wire.transmissions[1].address == 0x6A);
  assert(Wire.transmissions[1].bytes.size() == 2);
  assert(Wire.transmissions[1].bytes[0] == 0x02);
  assert(Wire.transmissions[1].bytes[1] == 0x40);
  assert(Wire.transmissions[2].bytes[0] == 0x03);
  assert(Wire.transmissions[2].bytes[1] == 0x16);
  assert(Wire.transmissions[3].bytes[0] == 0x08);
  assert(Wire.transmissions[3].bytes[1] == 0x41);

  const uint8_t sample_bytes[] = {0x00, 0x20, 0x00, 0xE0, 0x00, 0x00};
  Wire.setReadBuffer(sample_bytes, sizeof(sample_bytes));
  GatewayAccelerationSample sample{};
  assert(qmi.readAcceleration(&sample));
  assert(Wire.transmissions.size() == 5);
  assert(Wire.transmissions[4].address == 0x6A);
  assert(Wire.transmissions[4].bytes.size() == 1);
  assert(Wire.transmissions[4].bytes[0] == 0x35);
  assert(Wire.request_history.size() == 1);
  assert(Wire.request_history[0].address == 0x6A);
  assert(Wire.request_history[0].quantity == 6);
  assert(sample.x_g > 0.99f && sample.x_g < 1.01f);
  assert(sample.y_g < -0.99f && sample.y_g > -1.01f);
  assert(sample.z_g == 0.0f);

  Wire = TwoWire();
  Qmi8658 shared_bus_qmi;
  const uint8_t shared_bus_results[] = {0, 0, 0, 0};
  Wire.setEndTransmissionResults(shared_bus_results,
                                 sizeof(shared_bus_results));
  assert(shared_bus_qmi.beginOnInitializedBus(Wire));
  assert(Wire.begin_calls.empty());
  assert(Wire.transmissions.size() == 4);
  assert(Wire.transmissions[0].address == 0x6A);
  assert(Wire.transmissions[1].bytes[0] == 0x02);
  assert(Wire.transmissions[2].bytes[0] == 0x03);
  assert(Wire.transmissions[3].bytes[0] == 0x08);

  return 0;
}
