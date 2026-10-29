#pragma once

#include <cstdint>

#include "gateway_orientation.hpp"

struct GatewayTouchPoint {
  uint16_t x;
  uint16_t y;
};

GatewayTouchPoint gatewayTouchTransform(uint16_t raw_x, uint16_t raw_y,
                                        GatewayOrientation orientation);
GatewayTouchPoint gatewayTouchTransform(uint16_t raw_x, uint16_t raw_y,
                                         GatewayOrientation orientation,
                                         uint16_t raw_width,
                                         uint16_t raw_height);
GatewayTouchPoint gateway35NativeTouchPoint(uint16_t raw_x, uint16_t raw_y);
GatewayTouchPoint gateway35NativeTouchPoint(uint16_t raw_x, uint16_t raw_y,
                                            GatewayOrientation orientation);
GatewayTouchPoint gatewayLvglPointerDiagnosticPoint(
    GatewayTouchPoint input, GatewayOrientation orientation,
    uint16_t native_width, uint16_t native_height);
GatewayOrientation gateway35TouchOrientationForLayout(uint8_t layout_id);
