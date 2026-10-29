#include <assert.h>
#include <stdint.h>

#include "../../src/gateway_push_test_policy.hpp"

int main() {
  assert(gatewayPushCandidateIsNewer(false, 0, 500));
  assert(gatewayPushCandidateIsNewer(true, 500, 100));
  assert(!gatewayPushCandidateIsNewer(true, 100, 500));

  // When millisecond ages tie, prefer the later list entry so a deterministic
  // result is returned even when two samples arrive in the same tick.
  assert(gatewayPushCandidateIsNewer(true, 100, 100));
  return 0;
}
