#pragma once
#include <stdint.h>

// One gateway-wide cycle; skipped cycles are never queued for later playback.
class Gmt154MergedAudioCycle {
 public:
  void playbackCancelled() { running_ = false; }
  bool update(bool active, uint32_t now) {
    if (!active) { running_ = false; return false; }
    if (!running_) { running_ = true; start_ = now; return true; }
    const uint32_t elapsed = now - start_;
    if (elapsed < 300000UL) return false;
    start_ = now;  // Delayed service starts a fresh cycle, without catch-up.
    return true;
  }
 private:
  bool running_ = false;
  uint32_t start_ = 0;
};
