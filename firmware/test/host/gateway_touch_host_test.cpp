#include "../../src/gateway_touch.hpp"

namespace {
int g_failures = 0;

void expect(bool condition) {
  if (!condition) ++g_failures;
}
}  // namespace

int main() {
  const GatewayTouchPoint expected[] = {
      {239, 319}, {0, 319}, {239, 0}, {0, 0}, {119, 159}};
  const uint16_t raw[][2] = {
      {0, 0}, {239, 0}, {0, 319}, {239, 319}, {120, 160}};
  for (size_t i = 0; i < sizeof(raw) / sizeof(raw[0]); ++i) {
    const GatewayTouchPoint point = gatewayTouchTransform(
        raw[i][0], raw[i][1], GatewayOrientation::Landscape);
    expect(point.x == expected[i].x);
    expect(point.y == expected[i].y);
  }

  const GatewayTouchPoint portrait_compare = gatewayTouchTransform(
      184, 112, GatewayOrientation::Portrait);
  expect(portrait_compare.x == 184);
  expect(portrait_compare.y == 112);
  expect(portrait_compare.x >= 168 && portrait_compare.x < 168 + 52);
  expect(portrait_compare.y >= 112 && portrait_compare.y < 112 + 48);
  expect(!(portrait_compare.x >= 20 && portrait_compare.x < 20 + 52 &&
           portrait_compare.y >= 180 && portrait_compare.y < 180 + 48));

  const GatewayTouchPoint over_max =
      gatewayTouchTransform(500, 500, GatewayOrientation::Landscape);
  expect(over_max.x == 0);
  expect(over_max.y == 0);

  const GatewayTouchPoint under_min =
      gatewayTouchTransform(0, 0, GatewayOrientation::Portrait);
  expect(under_min.x == 0);
  expect(under_min.y == 0);

  const GatewayTouchPoint portrait_over_max =
      gatewayTouchTransform(500, 500, GatewayOrientation::Portrait);
  expect(portrait_over_max.x == 239);
  expect(portrait_over_max.y == 319);

  const GatewayTouchPoint sku30733_boundary = gatewayTouchTransform(
      319, 479, GatewayOrientation::Portrait, 320, 480);
  expect(sku30733_boundary.x == 319);
  expect(sku30733_boundary.y == 479);
  const GatewayTouchPoint sku30733_clamped = gatewayTouchTransform(
      320, 480, GatewayOrientation::Portrait, 320, 480);
  expect(sku30733_clamped.x == 319);
  expect(sku30733_clamped.y == 479);

  const GatewayTouchPoint sku30733_native_corner =
      gateway35NativeTouchPoint(319, 479);
  expect(sku30733_native_corner.x == 319);
  expect(sku30733_native_corner.y == 479);
  const GatewayTouchPoint sku30733_native_clamped =
      gateway35NativeTouchPoint(500, 500);
  expect(sku30733_native_clamped.x == 319);
  expect(sku30733_native_clamped.y == 479);

  // SKU30733's FT6336 landscape report is 180 degrees opposite LVGL's
  // rotated logical space. Invert both native axes before LVGL applies its
  // 90-degree display rotation.
  const GatewayTouchPoint sku30733_landscape_top_left =
      gateway35NativeTouchPoint(0, 0, GatewayOrientation::Landscape);
  expect(sku30733_landscape_top_left.x == 319);
  expect(sku30733_landscape_top_left.y == 479);
  const GatewayTouchPoint sku30733_landscape_top_right =
      gateway35NativeTouchPoint(319, 0, GatewayOrientation::Landscape);
  expect(sku30733_landscape_top_right.x == 0);
  expect(sku30733_landscape_top_right.y == 479);
  const GatewayTouchPoint sku30733_landscape_bottom_left =
      gateway35NativeTouchPoint(0, 479, GatewayOrientation::Landscape);
  expect(sku30733_landscape_bottom_left.x == 319);
  expect(sku30733_landscape_bottom_left.y == 0);
  const GatewayTouchPoint sku30733_landscape_bottom_right =
      gateway35NativeTouchPoint(319, 479, GatewayOrientation::Landscape);
  expect(sku30733_landscape_bottom_right.x == 0);
  expect(sku30733_landscape_bottom_right.y == 0);
  const GatewayTouchPoint sku30733_landscape_center =
      gateway35NativeTouchPoint(160, 240, GatewayOrientation::Landscape);
  expect(sku30733_landscape_center.x == 159);
  expect(sku30733_landscape_center.y == 239);

  // LVGL v9 rotates pointer input after the read callback. Diagnostics must
  // report this final point rather than only the callback's input point.
  const GatewayTouchPoint lvgl_landscape_from_native_top_left =
      gatewayLvglPointerDiagnosticPoint(
          GatewayTouchPoint{0, 0}, GatewayOrientation::Landscape, 320, 480);
  expect(lvgl_landscape_from_native_top_left.x == 479);
  expect(lvgl_landscape_from_native_top_left.y == 0);
  const GatewayTouchPoint lvgl_landscape_from_native_bottom_right =
      gatewayLvglPointerDiagnosticPoint(
          GatewayTouchPoint{319, 479}, GatewayOrientation::Landscape, 320,
          480);
  expect(lvgl_landscape_from_native_bottom_right.x == 0);
  expect(lvgl_landscape_from_native_bottom_right.y == 319);
  const GatewayTouchPoint lvgl_portrait_unchanged =
      gatewayLvglPointerDiagnosticPoint(
          GatewayTouchPoint{160, 240}, GatewayOrientation::Portrait, 320, 480);
  expect(lvgl_portrait_unchanged.x == 160);
  expect(lvgl_portrait_unchanged.y == 240);

  // Hand-derived centers from the 480x320 ring-color layout. These exact
  // points reproduce the reported yellow->default and purple->black faults
  // when the native inversion is missing.
  const struct {
    GatewayTouchPoint raw;
    GatewayTouchPoint expected_final;
  } sku30733_ring_color_centers[] = {
      {{203, 108}, {108, 116}},  // yellow, option 1
      {{203, 195}, {195, 116}},  // purple, option 2
      {{126, 282}, {282, 193}},  // black, option 7
      {{126, 369}, {369, 193}},  // BLE default, option 8
  };
  for (const auto& sample : sku30733_ring_color_centers) {
    const GatewayTouchPoint pre_lvgl = gateway35NativeTouchPoint(
        sample.raw.x, sample.raw.y, GatewayOrientation::Landscape);
    const GatewayTouchPoint final_point = gatewayLvglPointerDiagnosticPoint(
        pre_lvgl, GatewayOrientation::Landscape, 320, 480);
    expect(final_point.x == sample.expected_final.x);
    expect(final_point.y == sample.expected_final.y);
  }

  const struct {
    uint8_t layout_id;
    GatewayOrientation expected_orientation;
  } sku30733_layouts[] = {
      {0, GatewayOrientation::Portrait},
      {1, GatewayOrientation::Portrait},
      {2, GatewayOrientation::Portrait},
  };
  for (const auto& layout : sku30733_layouts) {
    expect(gateway35TouchOrientationForLayout(layout.layout_id) ==
           layout.expected_orientation);
  }
  return g_failures == 0 ? 0 : 1;
}
