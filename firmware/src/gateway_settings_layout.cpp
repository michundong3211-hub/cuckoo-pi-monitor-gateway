#include "gateway_settings_layout.hpp"

namespace {
GatewaySettingsRect makeGatewaySettingsRect(int16_t x, int16_t y,
                                            int16_t width, int16_t height) {
  GatewaySettingsRect rect;
  rect.x = x;
  rect.y = y;
  rect.width = width;
  rect.height = height;
  return rect;
}
}  // namespace

GatewaySettingsLayout calculateGatewaySettingsLayout(int16_t width,
                                                      int16_t height) {
  GatewaySettingsLayout layout{};
  layout.screen = makeGatewaySettingsRect(0, 0, width, height);
  const bool portrait = height > width;
  const int16_t baseWidth = portrait ? 240 : 320;
  const int16_t baseHeight = portrait ? 320 : 240;

  const auto scaleGatewaySettingsRect = [&](int16_t x, int16_t y,
                                             int16_t rectWidth,
                                             int16_t rectHeight) {
    const auto scaleX = [&](int16_t value) {
      return static_cast<int16_t>((static_cast<int32_t>(value) * width +
                                   baseWidth / 2) /
                                  baseWidth);
    };
    const auto scaleY = [&](int16_t value) {
      return static_cast<int16_t>((static_cast<int32_t>(value) * height +
                                   baseHeight / 2) /
                                  baseHeight);
    };
    return makeGatewaySettingsRect(scaleX(x), scaleY(y), scaleX(rectWidth),
                                   scaleY(rectHeight));
  };

  if (portrait) {
    layout.header = scaleGatewaySettingsRect(0, 0, 240, 40);
    layout.header_title = scaleGatewaySettingsRect(4, 0, 156, 40);
    layout.header_device = scaleGatewaySettingsRect(160, 0, 76, 40);
    layout.footer = scaleGatewaySettingsRect(0, 272, 240, 48);
    layout.content = scaleGatewaySettingsRect(0, 40, 240, 232);
    layout.home_cells[0] = scaleGatewaySettingsRect(4, 42, 114, 111);
    layout.home_cells[1] = scaleGatewaySettingsRect(122, 42, 114, 111);
    layout.home_cells[2] = scaleGatewaySettingsRect(4, 157, 114, 111);
    layout.home_cells[3] = scaleGatewaySettingsRect(122, 157, 114, 111);
    layout.home_back = scaleGatewaySettingsRect(4, 274, 232, 44);
    layout.detail_back = scaleGatewaySettingsRect(4, 274, 114, 44);
    layout.detail_save = scaleGatewaySettingsRect(122, 274, 114, 44);
    layout.toggle = scaleGatewaySettingsRect(88, 52, 64, 44);
    layout.minus_buttons[0] = scaleGatewaySettingsRect(20, 112, 52, 48);
    layout.plus_buttons[0] = scaleGatewaySettingsRect(168, 112, 52, 48);
    layout.value_fields[0] = scaleGatewaySettingsRect(76, 112, 88, 48);
    layout.row_hints[0] = scaleGatewaySettingsRect(48, 96, 144, 16);
    layout.minus_buttons[1] = scaleGatewaySettingsRect(20, 180, 52, 48);
    layout.plus_buttons[1] = scaleGatewaySettingsRect(168, 180, 52, 48);
    layout.value_fields[1] = scaleGatewaySettingsRect(76, 180, 88, 48);
    layout.row_hints[1] = scaleGatewaySettingsRect(48, 164, 144, 16);
    layout.og_hint = scaleGatewaySettingsRect(8, 44, 224, 20);
    const int16_t swatchX[4] = {20, 72, 124, 176};
    for (int i = 0; i < 8; ++i) {
      layout.ring_swatches[i] = scaleGatewaySettingsRect(
          swatchX[i % 4], static_cast<int16_t>(70 + (i / 4) * 60), 44, 44);
    }
    layout.ring_summary = scaleGatewaySettingsRect(8, 186, 224, 24);
  } else {
    layout.header = scaleGatewaySettingsRect(0, 0, 320, 36);
    layout.header_title = scaleGatewaySettingsRect(4, 0, 210, 36);
    layout.header_device = scaleGatewaySettingsRect(218, 0, 98, 36);
    layout.footer = scaleGatewaySettingsRect(0, 194, 320, 46);
    layout.content = scaleGatewaySettingsRect(0, 36, 320, 158);
    layout.home_cells[0] = scaleGatewaySettingsRect(4, 38, 154, 75);
    layout.home_cells[1] = scaleGatewaySettingsRect(162, 38, 154, 75);
    layout.home_cells[2] = scaleGatewaySettingsRect(4, 117, 154, 75);
    layout.home_cells[3] = scaleGatewaySettingsRect(162, 117, 154, 75);
    layout.home_back = scaleGatewaySettingsRect(4, 195, 312, 44);
    layout.detail_back = scaleGatewaySettingsRect(4, 195, 154, 44);
    layout.detail_save = scaleGatewaySettingsRect(162, 195, 154, 44);
    layout.toggle = scaleGatewaySettingsRect(128, 44, 64, 44);
    layout.minus_buttons[0] = scaleGatewaySettingsRect(24, 94, 56, 44);
    layout.plus_buttons[0] = scaleGatewaySettingsRect(240, 94, 56, 44);
    layout.value_fields[0] = scaleGatewaySettingsRect(84, 94, 152, 44);
    layout.row_hints[0] = scaleGatewaySettingsRect(84, 76, 152, 18);
    layout.minus_buttons[1] = scaleGatewaySettingsRect(24, 144, 56, 44);
    layout.plus_buttons[1] = scaleGatewaySettingsRect(240, 144, 56, 44);
    layout.value_fields[1] = scaleGatewaySettingsRect(84, 144, 152, 44);
    layout.row_hints[1] = scaleGatewaySettingsRect(84, 126, 152, 18);
    layout.og_hint = scaleGatewaySettingsRect(8, 40, 304, 20);
    const int16_t swatchX[4] = {50, 108, 166, 224};
    for (int i = 0; i < 8; ++i) {
      layout.ring_swatches[i] = scaleGatewaySettingsRect(
          swatchX[i % 4], static_cast<int16_t>(65 + (i / 4) * 58), 44, 44);
    }
    layout.ring_summary = scaleGatewaySettingsRect(8, 171, 304, 18);

    if (width == 480 && height == 320) {
      // The 3.5-inch landscape canvas scales the legacy row geometry enough
      // for the first hint to overlap the OFF toggle. Keep the touch targets
      // unchanged and place only the centered hint/value regions below it.
      layout.row_hints[0].y = 120;
      layout.row_hints[0].height = 20;
      layout.value_fields[0].y = 140;
      layout.value_fields[0].height = 36;
      layout.row_hints[1].y = 178;
      layout.row_hints[1].height = 20;
      layout.value_fields[1].y = 198;
      layout.value_fields[1].height = 40;
    }
  }
  return layout;
}
