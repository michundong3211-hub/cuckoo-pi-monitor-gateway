#pragma once
#if defined(GMT154_06_C3)
#include "gmt154_lvgl_budget.hpp"
#include <lvgl.h>
#include <freertos/task.h>
namespace gmt154 {
// LVGL task context only. Caller holds UI mutex; this never logs or allocates.
inline bool captureLvglBudget(LvglBudgetSampler& sampler, bool locked,
                              uint32_t now, LvglBudgetSnapshot& out) {
  if (!locked) { out.valid = false; return false; }
  return sampler.trySample(true, lv_is_initialized(), locked, now, out,
      [](LvglBudgetSnapshot& snapshot) {
        lv_mem_monitor_t monitor{};
        lv_mem_monitor(&monitor);
        snapshot.total = static_cast<uint32_t>(monitor.total_size);
        snapshot.free = static_cast<uint32_t>(monitor.free_size);
        snapshot.biggest = static_cast<uint32_t>(monitor.free_biggest_size);
        snapshot.maxUsed = static_cast<uint32_t>(monitor.max_used);
        // ESP-IDF task.h explicitly returns BYTES, unlike vanilla FreeRTOS.
        snapshot.stackMinimumBytes = static_cast<uint32_t>(uxTaskGetStackHighWaterMark(nullptr));
      });
}
}  // namespace gmt154
#endif
