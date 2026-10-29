#include "gateway_display_cycle.hpp"

#include <cassert>
#include <cstring>

int main() {
  CuckooPiConfig config = defaultCuckooPiConfig();
  config.enabled = true;
  config.device_count = 2;
  std::strcpy(config.devices[0].id, "001122334455");
  std::strcpy(config.devices[1].id, "001122334455");

  assert(gatewayDisplayItemCount(3, config) == 5);
  GatewayDisplayItem item{};
  assert(gatewayDisplayItemAt(0, 3, config, &item));
  assert(item.kind == GatewayDisplayItemKind::Measurement && item.index == 0);
  assert(gatewayDisplayItemAt(2, 3, config, &item));
  assert(item.kind == GatewayDisplayItemKind::Measurement && item.index == 2);
  assert(gatewayDisplayItemAt(3, 3, config, &item));
  assert(item.kind == GatewayDisplayItemKind::CuckooPi && item.index == 0);
  assert(gatewayDisplayItemAt(4, 3, config, &item));
  assert(item.kind == GatewayDisplayItemKind::CuckooPi && item.index == 1);
  assert(!gatewayDisplayItemAt(5, 3, config, &item));

  std::strcpy(config.devices[1].id, "invalid");
  assert(gatewayDisplayItemCount(3, config) == 4);
  assert(!gatewayDisplayItemAt(4, 3, config, &item));

  config.enabled = false;
  assert(gatewayDisplayItemCount(3, config) == 3);
  assert(!gatewayDisplayItemAt(3, 3, config, &item));
  return 0;
}
