#include <assert.h>

#include "../../src/gateway_settings_thread_state.hpp"

int main() {
  GatewaySettingsThreadState state;
  assert(!state.isOpen());
  assert(!state.consumeCloseRequest());

  state.publishOpen(true);
  assert(state.isOpen());

  state.requestClose();
  assert(state.consumeCloseRequest());
  assert(!state.consumeCloseRequest());
  assert(state.isOpen());

  state.publishOpen(false);
  assert(!state.isOpen());

  for (int i = 0; i < 100; ++i) {
    state.requestClose();
    assert(state.consumeCloseRequest());
    assert(!state.consumeCloseRequest());
  }
  return 0;
}
