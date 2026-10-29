#include <assert.h>

#include "../../src/gateway_orientation.hpp"

int main() {
  assert(GATEWAY_DEFAULT_ORIENTATION == GatewayOrientation::Portrait);
  assert(gatewayInitialOrientationForLayout(2) ==
         GatewayOrientation::Portrait);
  assert(gatewayInitialOrientationForLayout(0) ==
         GatewayOrientation::Landscape);
  assert(gatewayInitialOrientationForLayout(1) ==
         GatewayOrientation::Landscape);
  assert(gatewayAutoRotationEnabledForLayout(2));
  assert(!gatewayAutoRotationEnabledForLayout(0));
  assert(!gatewayAutoRotationEnabledForLayout(1));
  assert(!gatewayAutoRotationEnabledForLayout(3));

  const GatewayLayoutTransitionPlan rejected =
      gatewayPlanLayoutTransition(2, 0, false);
  assert(rejected.result == GatewayLayoutTransitionResult::Rejected);
  assert(rejected.target_layout == 0);
  assert(rejected.target_orientation == GatewayOrientation::Landscape);

  const GatewayLayoutTransitionPlan accepted_plan =
      gatewayPlanLayoutTransition(2, 0, true);
  assert(accepted_plan.result == GatewayLayoutTransitionResult::Accepted);
  assert(accepted_plan.target_layout == 0);
  assert(accepted_plan.target_orientation == GatewayOrientation::Landscape);

  const GatewayLayoutTransitionPlan noop =
      gatewayPlanLayoutTransition(0, 0, true);
  assert(noop.result == GatewayLayoutTransitionResult::Noop);
  assert(noop.target_layout == 0);
  const GatewayLayoutTransitionPlan invalid =
      gatewayPlanLayoutTransition(2, 99, true);
  assert(invalid.result == GatewayLayoutTransitionResult::Accepted);
  assert(invalid.target_layout == 0);
  assert(invalid.target_orientation == GatewayOrientation::Landscape);

  const GatewayLayoutTransitionPlan layout2 =
      gatewayPlanLayoutTransition(0, 2, true);
  assert(layout2.result == GatewayLayoutTransitionResult::Accepted);
  assert(layout2.target_orientation == GatewayOrientation::Portrait);
  assert(gatewayCommitLayoutTransition(accepted_plan, true) ==
         GatewayLayoutTransitionResult::Accepted);
  assert(gatewayCommitLayoutTransition(accepted_plan, false) ==
         GatewayLayoutTransitionResult::Rejected);
  assert(gatewayCommitLayoutTransition(rejected, true) ==
         GatewayLayoutTransitionResult::Rejected);
  assert(gatewayCommitLayoutTransition(noop, true) ==
         GatewayLayoutTransitionResult::Noop);

  GatewayOrientationDebouncer state(GATEWAY_DEFAULT_ORIENTATION);
  GatewayOrientation accepted = GATEWAY_DEFAULT_ORIENTATION;
  const GatewayDisplayRotation landscape_rotation =
      gatewayDisplayRotation(GatewayOrientation::Landscape);
  const GatewayDisplayRotation portrait_rotation =
      gatewayDisplayRotation(GatewayOrientation::Portrait);

  assert(landscape_rotation.tft_rotation == 1);
  assert(landscape_rotation.lvgl_rotation_degrees == 90);
  assert(portrait_rotation.tft_rotation == 0);
  assert(portrait_rotation.lvgl_rotation_degrees == 0);

  const GatewayAccelerationSample gateway35_portrait_sample =
      gateway35OrientationSample({0.95f, 0.02f, 0.03f});
  assert(gateway35_portrait_sample.x_g == 0.95f);
  assert(gateway35_portrait_sample.y_g == 0.02f);
  assert(gateway35_portrait_sample.z_g == 0.03f);
  assert(gatewayOrientationCandidate(gateway35_portrait_sample) ==
         GatewayOrientationCandidate::Portrait);

  assert(gatewayOrientationCandidate({0.95f, 0.02f, 0.03f}) ==
         GatewayOrientationCandidate::Portrait);
  assert(gatewayOrientationCandidate({0.02f, -0.95f, 0.03f}) ==
         GatewayOrientationCandidate::Landscape);
  assert(gatewayOrientationCandidate({0.02f, 0.02f, 0.98f}) ==
         GatewayOrientationCandidate::Ignore);
  assert(gatewayOrientationCandidate({0.75f, 0.02f, 0.60f}) ==
         GatewayOrientationCandidate::Ignore);
  assert(!state.update({0.02f, -0.95f, 0.03f}, 0, &accepted));
  assert(!state.update({0.02f, -0.95f, 0.03f}, 799, &accepted));
  assert(state.update({0.02f, -0.95f, 0.03f}, 800, &accepted));
  assert(accepted == GatewayOrientation::Landscape);
  assert(!state.update({0.95f, 0.02f, 0.03f}, 2300, &accepted));
  assert(state.update({0.95f, 0.02f, 0.03f}, 3100, &accepted));
  assert(accepted == GatewayOrientation::Portrait);
}
