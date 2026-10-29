#ifndef SRC_GATEWAY_RUNTIME_POLICY_HPP_
#define SRC_GATEWAY_RUNTIME_POLICY_HPP_

#include <stdint.h>

// A real HTTP test succeeded with roughly 43 KB free. Keep 30 KB as a hard
// floor so automatic forwarding can run while retaining headroom for the
// template engine and HTTP client.
constexpr uint32_t GATEWAY_MIN_PUSH_FREE_HEAP_BYTES = 30000;

inline bool gatewayPushResourcesAvailable(bool wifiConnected,
                                          uint32_t freeHeapBytes) {
  return wifiConnected && freeHeapBytes >= GATEWAY_MIN_PUSH_FREE_HEAP_BYTES;
}

#endif  // SRC_GATEWAY_RUNTIME_POLICY_HPP_
