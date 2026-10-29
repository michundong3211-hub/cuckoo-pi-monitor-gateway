#pragma once
#include <stdint.h>
inline bool gmt154MeasurementOffline(uint32_t age, uint32_t timeout) { return age >= timeout; }
inline bool gmt154ShowOfflineFrame(bool has_device, bool online) { return has_device && !online; }
#include <string.h>
inline bool gmt154MatchingMeasurementOffline(const char* displayed, const char* sampled, bool online) { return displayed && sampled && displayed[0] && strcmp(displayed, sampled) == 0 && !online; }
