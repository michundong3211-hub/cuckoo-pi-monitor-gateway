#pragma once

#include <gateway_orientation.hpp>

using GatewayRotationStep = void (*)(void* context);
using GatewayRotationCleanupStep = bool (*)(void* context);
using GatewayLvglHandlerStep = bool (*)(void* context);

struct GatewayRotationTeardown {
  GatewayRotationStep reset_all_indev = nullptr;
  GatewayRotationCleanupStep cleanup_delete = nullptr;
  GatewayRotationStep rebuild = nullptr;
  void* context = nullptr;
};

bool runGatewayRotationTeardown(const GatewayRotationTeardown& teardown);

struct GatewayLvglLoopIteration {
  GatewayLvglHandlerStep lvgl_handler = nullptr;
  GatewayRotationStep orientation_poll = nullptr;
  void* context = nullptr;
};

bool runGatewayLvglLoopIteration(const GatewayLvglLoopIteration& iteration);

class GatewayRotationGuard {
 public:
  void pointerPressed();
  void pointerReleased();
  void pointerLost();
  void settingsOpened();
  void settingsClosed();
  void releaseLocks();

  bool isBlocked() const;
  bool settingsOpen() const { return settings_open_; }

  void requestOrientation(GatewayOrientation orientation);
  bool takePendingOrientation(GatewayOrientation* orientation);
  bool consumeFreshSampleRequest();

 private:
  bool pointer_pressed_ = false;
  bool settings_open_ = false;
  bool fresh_sample_required_ = false;
  bool has_pending_orientation_ = false;
  GatewayOrientation pending_orientation = GatewayOrientation::Landscape;
};

GatewayRotationGuard& gatewayRotationGuard();

inline void gatewayRotationPointerPressed() {
  gatewayRotationGuard().pointerPressed();
}

inline void gatewayRotationPointerReleased() {
  gatewayRotationGuard().pointerReleased();
}

inline void gatewayRotationPointerLost() {
  gatewayRotationGuard().pointerLost();
}

inline void gatewayRotationSettingsOpened() {
  gatewayRotationGuard().settingsOpened();
}

inline void gatewayRotationSettingsClosed() {
  gatewayRotationGuard().settingsClosed();
}

inline void gatewayRotationReleaseLocks() {
  gatewayRotationGuard().releaseLocks();
}
