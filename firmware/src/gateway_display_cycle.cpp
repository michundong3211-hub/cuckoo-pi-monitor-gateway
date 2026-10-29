#include "gateway_display_cycle.hpp"
#include "cuckoo_pi_model.hpp"

namespace {
bool hasCuckooDevice(const CuckooPiConfig& config, size_t index) {
  if (!config.enabled || index >= config.device_count ||
      index >= kMaxCuckooPiDevices) {
    return false;
  }
  char normalized_id[13] = {};
  return normalizeCuckooPiId(config.devices[index].id, normalized_id);
}
}

size_t gatewayDisplayItemCount(size_t measurement_count,
                               const CuckooPiConfig& config) {
  size_t count = measurement_count;
  for (size_t index = 0; index < kMaxCuckooPiDevices; ++index) {
    if (hasCuckooDevice(config, index)) ++count;
  }
  return count;
}

bool gatewayDisplayItemAt(size_t position, size_t measurement_count,
                          const CuckooPiConfig& config,
                          GatewayDisplayItem* output) {
  if (!output) return false;
  if (position < measurement_count) {
    output->kind = GatewayDisplayItemKind::Measurement;
    output->index = position;
    return true;
  }

  size_t cuckoo_index = 0;
  for (size_t index = 0; index < kMaxCuckooPiDevices; ++index) {
    if (!hasCuckooDevice(config, index)) continue;
    if (position == measurement_count + cuckoo_index) {
      output->kind = GatewayDisplayItemKind::CuckooPi;
      output->index = index;
      return true;
    }
    ++cuckoo_index;
  }
  return false;
}
