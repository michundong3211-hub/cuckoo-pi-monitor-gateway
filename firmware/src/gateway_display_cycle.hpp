#pragma once

#include <cstddef>
#include <cstdint>

#include "cuckoo_pi_config.hpp"

enum class GatewayDisplayItemKind : uint8_t {
  Measurement,
  CuckooPi,
};

struct GatewayDisplayItem {
  GatewayDisplayItemKind kind = GatewayDisplayItemKind::Measurement;
  size_t index = 0;
};

size_t gatewayDisplayItemCount(size_t measurement_count,
                               const CuckooPiConfig& config);
bool gatewayDisplayItemAt(size_t position, size_t measurement_count,
                          const CuckooPiConfig& config,
                          GatewayDisplayItem* output);
