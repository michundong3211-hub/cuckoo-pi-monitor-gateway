#pragma once
#include <cstdint>
#include <cstdio>
namespace gmt154 {
struct LvglBudgetSnapshot {
  bool valid = false;
  uint32_t total = 0, free = 0, biggest = 0, maxUsed = 0, stackMinimumBytes = 0;
};
class LvglBudgetSampler {
 public:
  template <typename Sample>
  bool trySample(bool c3, bool ready, bool locked, uint32_t now,
                 LvglBudgetSnapshot& out, Sample sample) {
    out.valid = false;
    if (!c3 || !ready || !locked ||
        (sampled_ && static_cast<uint32_t>(now - last_) < 30000U)) return false;
    sample(out);
    out.valid = true;
    last_ = now;
    sampled_ = true;
    return true;
  }
 private:
  uint32_t last_ = 0;
  bool sampled_ = false;
};
constexpr unsigned lvglBudgetLineCapacity = 220;
inline int formatLvglBudget(char* out, unsigned capacity, const LvglBudgetSnapshot& s) {
  if (!s.valid) return std::snprintf(out, capacity, "LVGLBUDGET: sample=unknown");
  return std::snprintf(out, capacity,
      "LVGLBUDGET: total_bytes=%lu free_bytes=%lu biggest_bytes=%lu max_used_bytes=%lu stack_min_free_bytes=%lu",
      static_cast<unsigned long>(s.total), static_cast<unsigned long>(s.free),
      static_cast<unsigned long>(s.biggest), static_cast<unsigned long>(s.maxUsed),
      static_cast<unsigned long>(s.stackMinimumBytes));
}
}  // namespace gmt154
