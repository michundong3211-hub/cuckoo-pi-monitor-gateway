#include "gateway_config_boot_state.hpp"

#include <cassert>

int main() {
  GatewayConfigBootState state;
  assert(!state.storageReady());
  assert(!state.markRecoveryResult(false));
  assert(!state.storageReady());
  assert(state.markRecoveryResult(true));
  assert(state.storageReady());
  state.disableStorage();
  assert(!state.storageReady());
  return 0;
}
