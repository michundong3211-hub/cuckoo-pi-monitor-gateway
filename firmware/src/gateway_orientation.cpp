#include "gateway_orientation.hpp"

#include <cmath>

namespace {
constexpr float kMinDominantAxisG = 0.70f;
constexpr float kMinDominantMarginG = 0.20f;
constexpr float kMaxAbsZForOrientationG = 0.55f;
constexpr uint32_t kStableCandidateMs = 800;
constexpr uint32_t kRotationLockoutMs = 1500;

float absf(float value) { return std::fabs(value); }
}  // namespace

uint8_t gatewayNormalizeLayoutId(uint8_t layout_id) {
#if defined(GMT154_06)
  (void)layout_id;
  return 2;
#else
  return layout_id < GATEWAY_LAYOUT_COUNT ? layout_id : 0;
#endif
}

GatewayOrientation gatewayInitialOrientationForLayout(uint8_t layout_id) {
#if defined(GMT154_06)
  (void)layout_id;
  return GatewayOrientation::Landscape;
#else
  return gatewayNormalizeLayoutId(layout_id) == 2
             ? GATEWAY_DEFAULT_ORIENTATION
             : GatewayOrientation::Landscape;
#endif
}

bool gatewayAutoRotationEnabledForLayout(uint8_t layout_id) {
#if defined(GMT154_06)
  (void)layout_id;
  return false;
#else
  return gatewayNormalizeLayoutId(layout_id) == 2;
#endif
}

GatewayLayoutTransitionPlan gatewayPlanLayoutTransition(
    uint8_t current_layout, uint8_t requested_layout, bool can_teardown) {
  const uint8_t target_layout = gatewayNormalizeLayoutId(requested_layout);
  const GatewayOrientation target_orientation =
      gatewayInitialOrientationForLayout(target_layout);
  if (gatewayNormalizeLayoutId(current_layout) == target_layout) {
    return {target_layout, target_orientation,
            GatewayLayoutTransitionResult::Noop};
  }
  if (!can_teardown) {
    return {target_layout, target_orientation,
            GatewayLayoutTransitionResult::Rejected};
  }
  return {target_layout, target_orientation,
          GatewayLayoutTransitionResult::Accepted};
}

GatewayLayoutTransitionResult gatewayCommitLayoutTransition(
    const GatewayLayoutTransitionPlan& plan, bool cleanup_succeeded) {
  if (plan.result == GatewayLayoutTransitionResult::Noop) {
    return GatewayLayoutTransitionResult::Noop;
  }
  if (plan.result != GatewayLayoutTransitionResult::Accepted ||
      !cleanup_succeeded) {
    return GatewayLayoutTransitionResult::Rejected;
  }
  return GatewayLayoutTransitionResult::Accepted;
}

GatewayOrientationCandidate gatewayOrientationCandidate(
    const GatewayAccelerationSample& sample) {
  if (absf(sample.z_g) > kMaxAbsZForOrientationG) {
    return GatewayOrientationCandidate::Ignore;
  }

  const float abs_x = absf(sample.x_g);
  const float abs_y = absf(sample.y_g);

  if (abs_x >= kMinDominantAxisG && abs_x >= abs_y + kMinDominantMarginG) {
    return GatewayOrientationCandidate::Portrait;
  }

  if (abs_y >= kMinDominantAxisG && abs_y >= abs_x + kMinDominantMarginG) {
    return GatewayOrientationCandidate::Landscape;
  }

  return GatewayOrientationCandidate::Ignore;
}

GatewayAccelerationSample gateway35OrientationSample(
    const GatewayAccelerationSample& sample) {
  // SKU 30733 currently uses the QMI8658 native axis order. Keep this board
  // seam explicit so physical validation can adjust the 3.5 mapping without
  // changing the established 2.8 classifier.
  return sample;
}

GatewayDisplayRotation gatewayDisplayRotation(
    GatewayOrientation orientation) {
#if defined(GMT154_06)
  (void)orientation;
  return GatewayDisplayRotation{0, 0};
#else
  if (orientation == GatewayOrientation::Landscape) {
    return GatewayDisplayRotation{1, 90};
  }

  return GatewayDisplayRotation{0, 0};
#endif
}

GatewayOrientationDebouncer::GatewayOrientationDebouncer(
    GatewayOrientation initial)
    : current_(initial),
      candidate_(GatewayOrientationCandidate::Ignore),
      candidate_since_ms_(0),
      last_accepted_ms_(0),
      has_accepted_(false) {}

void GatewayOrientationDebouncer::resetForFreshSample() {
  candidate_ = GatewayOrientationCandidate::Ignore;
  candidate_since_ms_ = 0;
}

bool GatewayOrientationDebouncer::update(
    const GatewayAccelerationSample& sample, uint32_t now_ms,
    GatewayOrientation* accepted) {
  const GatewayOrientationCandidate next_candidate =
      gatewayOrientationCandidate(sample);

  if (next_candidate == GatewayOrientationCandidate::Ignore) {
    candidate_ = GatewayOrientationCandidate::Ignore;
    candidate_since_ms_ = now_ms;
    return false;
  }

  if (next_candidate != candidate_) {
    candidate_ = next_candidate;
    candidate_since_ms_ = now_ms;
  }

  if (now_ms - candidate_since_ms_ < kStableCandidateMs) {
    return false;
  }

  const GatewayOrientation next_orientation =
      candidate_ == GatewayOrientationCandidate::Landscape
          ? GatewayOrientation::Landscape
          : GatewayOrientation::Portrait;

  if (next_orientation == current_) {
    return false;
  }

  if (has_accepted_ && now_ms - last_accepted_ms_ <= kRotationLockoutMs) {
    return false;
  }

  current_ = next_orientation;
  candidate_ = GatewayOrientationCandidate::Ignore;
  candidate_since_ms_ = now_ms;
  last_accepted_ms_ = now_ms;
  has_accepted_ = true;

  if (accepted != nullptr) {
    *accepted = next_orientation;
  }
  return true;
}
