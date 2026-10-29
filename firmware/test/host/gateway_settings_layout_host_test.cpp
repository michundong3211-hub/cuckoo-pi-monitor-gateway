#include <assert.h>
#include <string.h>

#include "../../src/gateway_settings_layout.hpp"

namespace {

bool overlaps(const GatewaySettingsRect& a, const GatewaySettingsRect& b) {
  return a.x < b.x + b.width && a.x + a.width > b.x &&
         a.y < b.y + b.height && a.y + a.height > b.y;
}

void assertInBounds(const GatewaySettingsRect& rect, int width, int height) {
  assert(rect.x >= 0);
  assert(rect.y >= 0);
  assert(rect.width > 0);
  assert(rect.height > 0);
  assert(rect.x + rect.width <= width);
  assert(rect.y + rect.height <= height);
}

void assertTouchTarget(const GatewaySettingsRect& rect, int width,
                       int height) {
  assertInBounds(rect, width, height);
  assert(rect.width >= 44);
  assert(rect.height >= 44);
}

void assertLayout(int width, int height, int expectedHeader,
                  int expectedFooter, int minimumCellWidth,
                  int minimumCellHeight) {
  const GatewaySettingsLayout layout =
      calculateGatewaySettingsLayout(width, height);
  assert(layout.screen.width == width);
  assert(layout.screen.height == height);
  assert(layout.header.x == 0 && layout.header.y == 0);
  assert(layout.header.width == width);
  assert(layout.header.height == expectedHeader);
  assert(layout.footer.x == 0);
  assert(layout.footer.y == height - expectedFooter);
  assert(layout.footer.width == width);
  assert(layout.footer.height == expectedFooter);

  for (int i = 0; i < 4; ++i) {
    assertInBounds(layout.home_cells[i], width, height);
    assert(layout.home_cells[i].width >= minimumCellWidth);
    assert(layout.home_cells[i].height >= minimumCellHeight);
    assert(!overlaps(layout.home_cells[i], layout.header));
    assert(!overlaps(layout.home_cells[i], layout.footer));
    for (int j = i + 1; j < 4; ++j) {
      assert(!overlaps(layout.home_cells[i], layout.home_cells[j]));
    }
  }

  assertTouchTarget(layout.home_back, width, height);
  assertTouchTarget(layout.detail_back, width, height);
  assertTouchTarget(layout.detail_save, width, height);
  assertTouchTarget(layout.toggle, width, height);
  for (int i = 0; i < 2; ++i) {
    assertTouchTarget(layout.minus_buttons[i], width, height);
    assertTouchTarget(layout.plus_buttons[i], width, height);
    assertInBounds(layout.value_fields[i], width, height);
    assert(!overlaps(layout.minus_buttons[i], layout.footer));
    assert(!overlaps(layout.plus_buttons[i], layout.footer));
  }
  for (int i = 0; i < 8; ++i) {
    assertTouchTarget(layout.ring_swatches[i], width, height);
    assert(!overlaps(layout.ring_swatches[i], layout.footer));
    for (int j = i + 1; j < 8; ++j) {
      assert(!overlaps(layout.ring_swatches[i], layout.ring_swatches[j]));
    }
  }
  assert(!overlaps(layout.detail_back, layout.detail_save));
  assert(!overlaps(layout.home_back, layout.header));
  assert(!overlaps(layout.detail_back, layout.header));
  assert(!overlaps(layout.detail_save, layout.header));
  assert(!overlaps(layout.toggle, layout.header));
  assert(!overlaps(layout.toggle, layout.footer));
  for (int i = 0; i < 2; ++i) {
    assert(!overlaps(layout.minus_buttons[i], layout.plus_buttons[i]));
    assert(!overlaps(layout.minus_buttons[i], layout.value_fields[i]));
    assert(!overlaps(layout.plus_buttons[i], layout.value_fields[i]));
  }
}

void testLandscape() { assertLayout(320, 240, 36, 46, 154, 74); }

void testPortrait() { assertLayout(240, 320, 40, 48, 112, 105); }

void testWaveshare35PortraitUsesFullCanvas() {
  const GatewaySettingsLayout layout =
      calculateGatewaySettingsLayout(320, 480);
  assertLayout(320, 480, 60, 72, 152, 167);

  // The 3.5-inch portrait page is the 240x320 design scaled to the native
  // 320x480 canvas. These exact anchors keep the settings page from silently
  // falling back to the legacy 2.8-inch geometry.
  assert(layout.home_cells[0].x == 5);
  assert(layout.home_cells[0].y == 63);
  assert(layout.home_cells[0].width == 152);
  assert(layout.home_cells[0].height == 167);
  assert(layout.home_back.x == 5);
  assert(layout.home_back.y == 411);
  assert(layout.home_back.width == 309);
  assert(layout.home_back.height == 66);
  assert(layout.toggle.x == 117);
  assert(layout.toggle.y == 78);
  assert(layout.toggle.width == 85);
  assert(layout.toggle.height == 66);
  assert(layout.header_title.x == 5);
  assert(layout.header_title.y == 0);
  assert(layout.header_title.width == 208);
  assert(layout.header_title.height == 60);
  assert(layout.header_device.x == 213);
  assert(layout.header_device.y == 0);
  assert(layout.header_device.width == 101);
  assert(layout.header_device.height == 60);
  assert(layout.og_hint.x == 11);
  assert(layout.og_hint.y == 66);
  assert(layout.og_hint.width == 299);
  assert(layout.og_hint.height == 30);
  assert(layout.row_hints[0].x == 64);
  assert(layout.row_hints[0].y == 144);
  assert(layout.row_hints[0].width == 192);
  assert(layout.row_hints[0].height == 24);
  assert(layout.row_hints[1].x == 64);
  assert(layout.row_hints[1].y == 246);
  assert(layout.row_hints[1].width == 192);
  assert(layout.row_hints[1].height == 24);
  assert(layout.ring_summary.x == 11);
  assert(layout.ring_summary.y == 279);
  assert(layout.ring_summary.width == 299);
  assert(layout.ring_summary.height == 36);
  assert(layout.ring_swatches[7].x == 235);
  assert(layout.ring_swatches[7].y == 195);
  assert(layout.ring_swatches[7].width == 59);
  assert(layout.ring_swatches[7].height == 66);

  assertInBounds(layout.header_title, 320, 480);
  assertInBounds(layout.header_device, 320, 480);
  assertInBounds(layout.og_hint, 320, 480);
  assertInBounds(layout.row_hints[0], 320, 480);
  assertInBounds(layout.row_hints[1], 320, 480);
  assertInBounds(layout.ring_summary, 320, 480);
  assert(!overlaps(layout.header_title, layout.header_device));
}

void testWaveshare35LandscapeKeepsDetailTextBelowToggle() {
  const GatewaySettingsLayout layout =
      calculateGatewaySettingsLayout(480, 320);

  // Temperature and offline pages share row 0. Its hint must begin below the
  // OFF toggle; the temperature page also uses row 1 and must stay above the
  // footer. These hand-checked boundaries reproduce the photographed overlap.
  assert(layout.toggle.y + layout.toggle.height <= layout.row_hints[0].y);
  assert(layout.row_hints[0].y + layout.row_hints[0].height <=
         layout.value_fields[0].y);
  assert(layout.value_fields[0].y + layout.value_fields[0].height <=
         layout.row_hints[1].y);
  assert(layout.row_hints[1].y + layout.row_hints[1].height <=
         layout.value_fields[1].y);
  assert(layout.value_fields[1].y + layout.value_fields[1].height <=
         layout.footer.y);
}

void testRapidAlternationIsDeterministic() {
  const GatewaySettingsLayout landscape =
      calculateGatewaySettingsLayout(320, 240);
  const GatewaySettingsLayout portrait =
      calculateGatewaySettingsLayout(240, 320);
  for (int i = 0; i < 100; ++i) {
    const GatewaySettingsLayout nextLandscape =
        calculateGatewaySettingsLayout(320, 240);
    const GatewaySettingsLayout nextPortrait =
        calculateGatewaySettingsLayout(240, 320);
    assert(memcmp(&landscape, &nextLandscape, sizeof(landscape)) == 0);
    assert(memcmp(&portrait, &nextPortrait, sizeof(portrait)) == 0);
  }
}

}  // namespace

int main() {
  testLandscape();
  testPortrait();
  testWaveshare35PortraitUsesFullCanvas();
  testWaveshare35LandscapeKeepsDetailTextBelowToggle();
  testRapidAlternationIsDeterministic();
  return 0;
}
