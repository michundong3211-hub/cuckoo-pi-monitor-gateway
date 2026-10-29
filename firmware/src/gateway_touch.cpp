#include "gateway_touch.hpp"

namespace {
uint16_t clampTouchCoordinate(int32_t value, int32_t logical_size) {
  if (value < 0) return 0;
  if (value >= logical_size) return static_cast<uint16_t>(logical_size - 1);
  return static_cast<uint16_t>(value);
}
}  // namespace

GatewayTouchPoint gatewayTouchTransform(uint16_t raw_x, uint16_t raw_y,
                                        GatewayOrientation orientation) {
  return gatewayTouchTransform(raw_x, raw_y, orientation, 240, 320);
}

GatewayTouchPoint gatewayTouchTransform(uint16_t raw_x, uint16_t raw_y,
                                        GatewayOrientation orientation,
                                        uint16_t raw_width,
                                        uint16_t raw_height) {
  if (raw_width == 0 || raw_height == 0) return GatewayTouchPoint{0, 0};
  const int32_t width = raw_width;
  const int32_t height = raw_height;
  if (orientation == GatewayOrientation::Portrait) {
    return GatewayTouchPoint{clampTouchCoordinate(raw_x, width),
                             clampTouchCoordinate(raw_y, height)};
  }

  // Landscape retains the established dual-axis inversion mapping.
  const int32_t x = width - 1 - raw_x;
  const int32_t y = height - 1 - raw_y;
  return GatewayTouchPoint{clampTouchCoordinate(x, width),
                            clampTouchCoordinate(y, height)};
}

GatewayTouchPoint gateway35NativeTouchPoint(uint16_t raw_x, uint16_t raw_y) {
  return gateway35NativeTouchPoint(raw_x, raw_y,
                                   GatewayOrientation::Portrait);
}

GatewayTouchPoint gateway35NativeTouchPoint(uint16_t raw_x, uint16_t raw_y,
                                            GatewayOrientation orientation) {
  constexpr uint16_t kNativeWidth = 320;
  constexpr uint16_t kNativeHeight = 480;
  // Portrait reports already match LVGL's native panel space. In landscape,
  // SKU30733's mounted FT6336 axes are 180 degrees opposite that native
  // space, so invert both axes before LVGL applies the display's 90-degree
  // pointer rotation.
  return gatewayTouchTransform(raw_x, raw_y, orientation, kNativeWidth,
                               kNativeHeight);
}

GatewayTouchPoint gatewayLvglPointerDiagnosticPoint(
    GatewayTouchPoint input, GatewayOrientation orientation,
    uint16_t native_width, uint16_t native_height) {
  if (native_width == 0 || native_height == 0) {
    return GatewayTouchPoint{0, 0};
  }
  if (orientation == GatewayOrientation::Portrait) {
    return GatewayTouchPoint{
        clampTouchCoordinate(input.x, native_width),
        clampTouchCoordinate(input.y, native_height)};
  }

  // Mirrors LVGL v9 indev_pointer_proc() for LV_DISPLAY_ROTATION_90.
  const int32_t x = native_height - 1 - static_cast<int32_t>(input.y);
  const int32_t y = input.x;
  return GatewayTouchPoint{clampTouchCoordinate(x, native_height),
                           clampTouchCoordinate(y, native_width)};
}

GatewayOrientation gateway35TouchOrientationForLayout(uint8_t layout_id) {
  (void)layout_id;
  return GatewayOrientation::Portrait;
}
