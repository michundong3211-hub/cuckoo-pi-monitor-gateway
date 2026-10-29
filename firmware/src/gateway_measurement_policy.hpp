#pragma once

#include <stddef.h>
#include <stdint.h>

constexpr size_t GATEWAY_MEASUREMENT_MAX_ENTRIES = 20;

constexpr bool gatewayMeasurementCanStore(size_t currentCount,
                                          bool updatesExisting) {
  return updatesExisting || currentCount < GATEWAY_MEASUREMENT_MAX_ENTRIES;
}

constexpr uint32_t gatewayNextMeasurementGeneration(uint32_t current) {
  return current + 1U;
}
