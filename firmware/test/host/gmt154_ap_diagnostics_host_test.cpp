#include "gmt154_ap_diagnostics_policy.hpp"
#include <cassert>
#include <cstring>
#include <climits>
#include <initializer_list>
int main() {
  using namespace gmt154;
  for (bool measurement : {false, true}) for (bool connected : {false, true}) {
    assert(shouldDiscoverMdns(true, measurement, connected) == (measurement && connected));
    assert(shouldDiscoverMdns(false, measurement, connected));
  }
  ApEventState s{};
  recordApEvent(s, ApEvent::start, 1, false, 99);
  recordApEvent(s, ApEvent::associated, 2, false, 99);
  recordApEvent(s, ApEvent::apDisconnected, 3, true, 7);
  recordApEvent(s, ApEvent::staDisconnected, 4, true, 201);
  recordApEvent(s, ApEvent::stop, 5, false, 99);
  assert(s.starts == 1 && s.stops == 1 && s.associations == 1);
  assert(s.apDisconnects == 1 && s.staDisconnects == 1);
  assert(s.apReason == 7 && s.staReason == 201 && s.lastMs == 5);
  recordApEvent(s, ApEvent::apDisconnected, 6, false, 0);
  assert(s.apReason == -1 && s.apDisconnects == 2);
  const auto copy = s; recordApEvent(s, ApEvent::start, 7, false, 0);
  assert(copy.starts == 1 && s.starts == 2);
  s = {UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX,
       UINT32_MAX, UINT32_MAX, INT32_MIN, INT32_MIN};
  char line[apLineCapacity];
  int len = formatApSnapshot(line, sizeof(line), "runtime", s, INT32_MIN,
                            INT32_MIN, UINT32_MAX, INT32_MIN, UINT32_MAX, INT32_MIN);
  assert(len > 0 && static_cast<unsigned>(len) < sizeof(line));
  assert(std::strstr(line, "clients=unknown") && std::strstr(line, "ap_ip=unknown"));
  assert(std::strstr(line, "mode=unknown"));
  len = formatApSnapshot(line, sizeof(line), "startup", {}, 0, 3, 0, 0,
                        0x0104a8c0U, 0);
  assert(std::strstr(line, "ap_ip=192.168.4.1") && std::strstr(line, "clients=0"));
  assert(std::strstr(line, "mode=3"));
  static_assert(sizeof(ApEventState) == 36, "bounded state");
}
