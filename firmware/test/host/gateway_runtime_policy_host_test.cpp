#include <assert.h>

#include "../../src/gateway_runtime_policy.hpp"

int main() {
  static_assert(GATEWAY_MIN_PUSH_FREE_HEAP_BYTES == 30000);
  assert(!gatewayPushResourcesAvailable(false, 50000));
  assert(!gatewayPushResourcesAvailable(true, 29999));
  assert(gatewayPushResourcesAvailable(true, 30000));
  assert(gatewayPushResourcesAvailable(true, 43076));
  return 0;
}
