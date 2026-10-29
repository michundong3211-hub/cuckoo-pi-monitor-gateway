#pragma once

#include <cstdint>

enum class GatewayOrientation : uint8_t { Landscape, Portrait };

constexpr GatewayOrientation GATEWAY_DEFAULT_ORIENTATION =
    GatewayOrientation::Portrait;
constexpr uint8_t GATEWAY_LAYOUT_COUNT = 3;

uint8_t gatewayNormalizeLayoutId(uint8_t layout_id);
GatewayOrientation gatewayInitialOrientationForLayout(uint8_t layout_id);
bool gatewayAutoRotationEnabledForLayout(uint8_t layout_id);

enum class GatewayOrientationCandidate : uint8_t {
  Ignore,
  Landscape,
  Portrait
};

struct GatewayAccelerationSample {
  float x_g;
  float y_g;
  float z_g;
};

struct GatewayDisplayRotation {
  uint8_t tft_rotation;
  uint16_t lvgl_rotation_degrees;
};

enum class GatewayLayoutTransitionResult : uint8_t {
  Noop,
  Rejected,
  Accepted
};

struct GatewayLayoutTransitionPlan {
  uint8_t target_layout;
  GatewayOrientation target_orientation;
  GatewayLayoutTransitionResult result;
};

GatewayOrientationCandidate gatewayOrientationCandidate(
    const GatewayAccelerationSample& sample);
GatewayAccelerationSample gateway35OrientationSample(
    const GatewayAccelerationSample& sample);
GatewayDisplayRotation gatewayDisplayRotation(GatewayOrientation orientation);
GatewayLayoutTransitionPlan gatewayPlanLayoutTransition(
    uint8_t current_layout, uint8_t requested_layout, bool can_teardown);
GatewayLayoutTransitionResult gatewayCommitLayoutTransition(
    const GatewayLayoutTransitionPlan& plan, bool cleanup_succeeded);

class GatewayOrientationDebouncer {
 public:
  explicit GatewayOrientationDebouncer(GatewayOrientation initial);

  void resetForFreshSample();

  bool update(const GatewayAccelerationSample& sample, uint32_t now_ms,
              GatewayOrientation* accepted);

 private:
  GatewayOrientation current_;
  GatewayOrientationCandidate candidate_;
  uint32_t candidate_since_ms_;
  uint32_t last_accepted_ms_;
  bool has_accepted_;
};
