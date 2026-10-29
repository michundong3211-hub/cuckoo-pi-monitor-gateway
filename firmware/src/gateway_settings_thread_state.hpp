#pragma once

#include <atomic>

class GatewaySettingsThreadState {
 public:
  void requestClose() {
    close_requested_.store(true, std::memory_order_release);
  }

  bool consumeCloseRequest() {
    return close_requested_.exchange(false, std::memory_order_acq_rel);
  }

  void publishOpen(bool open) {
    open_.store(open, std::memory_order_release);
  }

  bool isOpen() const {
    return open_.load(std::memory_order_acquire);
  }

 private:
  std::atomic<bool> close_requested_{false};
  std::atomic<bool> open_{false};
};
