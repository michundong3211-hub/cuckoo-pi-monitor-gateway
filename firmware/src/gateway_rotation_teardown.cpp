#include "gateway_rotation_teardown.hpp"

bool runGatewayRotationTeardown(const GatewayRotationTeardown& teardown) {
  if (teardown.reset_all_indev == nullptr ||
      teardown.cleanup_delete == nullptr || teardown.rebuild == nullptr) {
    return false;
  }
  teardown.reset_all_indev(teardown.context);
  if (!teardown.cleanup_delete(teardown.context)) return false;
  teardown.rebuild(teardown.context);
  return true;
}

bool runGatewayLvglLoopIteration(
    const GatewayLvglLoopIteration& iteration) {
  if (iteration.lvgl_handler == nullptr ||
      iteration.orientation_poll == nullptr) {
    return false;
  }
  if (!iteration.lvgl_handler(iteration.context)) return false;
  iteration.orientation_poll(iteration.context);
  return true;
}

void GatewayRotationGuard::pointerPressed() { pointer_pressed_ = true; }

void GatewayRotationGuard::pointerReleased() { pointer_pressed_ = false; }

void GatewayRotationGuard::pointerLost() { pointer_pressed_ = false; }

void GatewayRotationGuard::settingsOpened() {
  settings_open_ = true;
  has_pending_orientation_ = false;
  fresh_sample_required_ = true;
}

void GatewayRotationGuard::settingsClosed() {
  settings_open_ = false;
  has_pending_orientation_ = false;
  fresh_sample_required_ = true;
}

void GatewayRotationGuard::releaseLocks() {
  pointer_pressed_ = false;
  settings_open_ = false;
  has_pending_orientation_ = false;
}

bool GatewayRotationGuard::isBlocked() const {
  return pointer_pressed_ || settings_open_;
}

void GatewayRotationGuard::requestOrientation(GatewayOrientation orientation) {
  if (settings_open_) return;
  pending_orientation = orientation;
  has_pending_orientation_ = true;
}

bool GatewayRotationGuard::takePendingOrientation(
    GatewayOrientation* orientation) {
  if (isBlocked() || !has_pending_orientation_ || orientation == nullptr) {
    return false;
  }
  *orientation = pending_orientation;
  has_pending_orientation_ = false;
  return true;
}

bool GatewayRotationGuard::consumeFreshSampleRequest() {
  const bool required = fresh_sample_required_;
  fresh_sample_required_ = false;
  return required;
}

GatewayRotationGuard& gatewayRotationGuard() {
  static GatewayRotationGuard guard;
  return guard;
}
