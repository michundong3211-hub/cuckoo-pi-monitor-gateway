#include "cuckoo_pi_screen.hpp"

#include <cassert>
#include <cstring>
#include <utility>

namespace {
bool inside(const GatewaySettingsRect& rect, int width, int height) {
  return rect.x >= 0 && rect.y >= 0 && rect.width > 0 && rect.height > 0 &&
         rect.x + rect.width <= width && rect.y + rect.height <= height;
}

void test_gmt154_square_thermostat_layout() {
  const CuckooPiScreenLayout layout =
      calculateCuckooPiScreenLayout(240, 240);

  assert(layout.screen.width == 240);
  assert(layout.screen.height == 240);
  assert(layout.header.height == 32);
  assert(layout.title.x == 12);
  assert(layout.title.y == 5);
  assert(layout.title.width == 144);
  assert(layout.index.width == 0);
  assert(layout.index.height == 0);
  assert(layout.online.x == 178);
  assert(layout.online.width == 58);
  assert(layout.beer_heading.y == 43);
  assert(layout.beer_heading.height == 18);
  assert(layout.beer.y == 58);
  assert(layout.beer.width == 208);
  assert(layout.beer.height == 64);
  assert(layout.beer_set.x == 8);
  assert(layout.beer_set.y == 122);
  assert(layout.beer_set.width == 112);
  assert(layout.beer_set.height == 58);
  assert(layout.fridge_set.x == 120);
  assert(layout.fridge_set.width == 112);
  assert(layout.mode_card.x == 8);
  assert(layout.mode_card.y == 180);
  assert(layout.mode_card.width == 112);
  assert(layout.mode_card.height == 60);
  assert(layout.mode.x == 8);
  assert(layout.mode.y == 180);
  assert(layout.mode.width == 112);
  assert(layout.mode.height == 60);
  assert(layout.mode.y + layout.mode.height == layout.screen.height);
  assert(layout.state.x == 120);
  assert(layout.state.y == 180);
  assert(layout.state.width == 112);
  assert(layout.state.height == 60);
  assert(layout.state.y + layout.state.height == layout.screen.height);
  assert(layout.touch.y == 32);
  assert(layout.touch.height == 208);
  assert(layout.runtime.width == 0);
  assert(layout.runtime.height == 0);
  assert(layout.fridge.width == 0);
  assert(layout.fridge.height == 0);
  assert(layout.chiller.width == 0);
  assert(layout.chiller.height == 0);
}
}  // namespace

int main() {
  char runtime[32] = {};
  assert(formatCuckooPiRuntime(0, runtime, sizeof(runtime)));
  assert(std::strcmp(runtime, "0s") == 0);
  assert(formatCuckooPiRuntime(59, runtime, sizeof(runtime)));
  assert(std::strcmp(runtime, "59s") == 0);
  assert(formatCuckooPiRuntime(60, runtime, sizeof(runtime)));
  assert(std::strcmp(runtime, "1m 0s") == 0);
  assert(formatCuckooPiRuntime(15387, runtime, sizeof(runtime)));
  assert(std::strcmp(runtime, "4h 16m") == 0);
  assert(formatCuckooPiRuntime(90061, runtime, sizeof(runtime)));
  assert(std::strcmp(runtime, "1d 1h") == 0);
  assert(!formatCuckooPiRuntime(10, nullptr, sizeof(runtime)));

  char text[96] = {};
  assert(formatCuckooPiStateLabel(
      false, "Cooling Minimum Time", text, sizeof(text)));
  assert(std::strcmp(text, "STATE Cooling Minimum Time") == 0);
  assert(formatCuckooPiStateLabel(true, "Cooling Minimum Time", text,
                                  sizeof(text)));
  assert(std::strcmp(text, "STATE\nCooling Minimum Time") == 0);
  assert(formatCuckooPiStateLabel(false, nullptr, text, sizeof(text)));
  assert(std::strcmp(text, "STATE --") == 0);

  assert(formatCuckooPiRuntimeLabel(false, true, 1740, text, sizeof(text)));
  assert(std::strcmp(text, "RUN TIME 29m 0s") == 0);
  assert(formatCuckooPiRuntimeLabel(true, true, 1740, text, sizeof(text)));
  assert(std::strcmp(text, "RUN TIME\n29m 0s") == 0);
  assert(formatCuckooPiRuntimeLabel(false, false, 0, text, sizeof(text)));
  assert(std::strcmp(text, "RUN TIME --") == 0);

  assert(formatCuckooPiMetricLabel("FRIDGE TEMP", true, true, -5.5f, "F",
                                   text, sizeof(text)));
  assert(std::strcmp(text, "FRIDGE TEMP\n-5.50 F") == 0);
  assert(formatCuckooPiMetricLabel("FRIDGE\nTEMP", true, false, 0.0f, "C",
                                   text, sizeof(text)));
  assert(std::strcmp(text, "FRIDGE\nTEMP\n--") == 0);
  assert(formatCuckooPiMetricLabel("FRIDGE SET", false, true, -12.25f, "C",
                                   text, sizeof(text)));
  assert(std::strcmp(text, "FRIDGE SET  -12.25 C") == 0);
  assert(formatCuckooPiMetricLabel("FRIDGE SET", false, false, 0.0f, "C",
                                   text, sizeof(text)));
  assert(std::strcmp(text, "FRIDGE SET  --") == 0);
  assert(formatCuckooPiMetricLabel("CHILLER TEMP", true, true, 31.31299973f,
                                   "C", text, sizeof(text)));
  assert(std::strcmp(text, "CHILLER TEMP\n31.31 C") == 0);
  assert(formatCuckooPiMetricLabel("CHILLER TEMP", true, true, 88.36339569f,
                                   "F", text, sizeof(text)));
  assert(std::strcmp(text, "CHILLER TEMP\n88.36 F") == 0);
  assert(formatCuckooPiMetricLabel("CHILLER\nTEMP", true, false, 0.0f, "F",
                                   text, sizeof(text)));
  assert(std::strcmp(text, "CHILLER\nTEMP\n--") == 0);
  assert(formatCuckooPiMetricLabel("CHILLER\nTEMP", true, true,
                                   31.31299973f, "C", text, sizeof(text)));
  assert(std::strcmp(text, "CHILLER\nTEMP\n31.31 C") == 0);
  assert(formatCuckooPiMetricLabel("CHILLER\nTEMP", true, true,
                                   88.36339569f, "F", text, sizeof(text)));
  assert(std::strcmp(text, "CHILLER\nTEMP\n88.36 F") == 0);
  assert(formatCuckooPiMetricLabel("CHILLER\nTEMP", true, true,
                                   31.31299973f, "C", text, sizeof(text)));
  assert(std::strcmp(text, "CHILLER\nTEMP\n31.31 C") == 0);

  const GatewaySettingsRect temperature_region{16, 94, 288, 110};
  const CuckooPiTextMetrics number_metrics{220, 80, 10};
  const CuckooPiTextMetrics unit_metrics{14, 20, 5};
  const CuckooPiTemperatureGroup group = layoutCuckooPiTemperatureGroup(
      temperature_region, number_metrics, unit_metrics, true, 6);
  assert(group.fits);
  assert(group.number.x >= temperature_region.x);
  assert(group.number.x + group.number.width + 6 == group.unit.x);
  assert(group.unit.x + group.unit.width <=
         temperature_region.x + temperature_region.width);
  assert(group.number.y + number_metrics.height - number_metrics.base_line ==
         group.unit.y + unit_metrics.height - unit_metrics.base_line);

  const CuckooPiTemperatureGroup overflow_group =
      layoutCuckooPiTemperatureGroup(
          temperature_region, CuckooPiTextMetrics{320, 80, 10}, unit_metrics,
          true, 6);
  assert(!overflow_group.fits);
  assert(overflow_group.number.x >= temperature_region.x);
  assert(overflow_group.unit.x + overflow_group.unit.width <=
         temperature_region.x + temperature_region.width);
  assert(overflow_group.unit.x -
             (overflow_group.number.x + overflow_group.number.width) ==
         6);

  const CuckooPiTemperatureGroup baseline_overflow_group =
      layoutCuckooPiTemperatureGroup(
          temperature_region, CuckooPiTextMetrics{120, 80, 60},
          CuckooPiTextMetrics{40, 80, 0}, true, 6);
  assert(!baseline_overflow_group.fits);

  const CuckooPiTemperatureGroup impossible_unit_group =
      layoutCuckooPiTemperatureGroup(
          temperature_region, number_metrics,
          CuckooPiTextMetrics{288, 20, 5}, true, 6);
  assert(!impossible_unit_group.fits);
  assert(impossible_unit_group.number.width == 0);
  assert(impossible_unit_group.unit.width == 0);

  const CuckooPiFontMeasurement font_candidates[] = {
      {80, {300, 80, 10}}, {72, {300, 72, 9}},
      {48, {180, 48, 6}}, {40, {150, 40, 5}}};
  assert(selectCuckooPiFontSize(font_candidates,
                                sizeof(font_candidates) /
                                    sizeof(font_candidates[0]),
                                80, 288, 110) == 48);
  assert(selectCuckooPiFontSize(font_candidates,
                                sizeof(font_candidates) /
                                    sizeof(font_candidates[0]),
                                72, 120, 35) == 40);

  for (const auto& size : {std::pair<int, int>{480, 320},
                           std::pair<int, int>{320, 480}}) {
    const CuckooPiScreenLayout layout =
        calculateCuckooPiScreenLayout(size.first, size.second);
    assert(layout.screen.width == size.first);
    assert(layout.screen.height == size.second);
    assert(inside(layout.header, size.first, size.second));
    assert(inside(layout.title, size.first, size.second));
    assert(inside(layout.index, size.first, size.second));
    assert(inside(layout.online, size.first, size.second));
    assert(inside(layout.temperature_card, size.first, size.second));
    assert(inside(layout.beer_heading, size.first, size.second));
    assert(inside(layout.mode_card, size.first, size.second));
    assert(inside(layout.mode, size.first, size.second));
    assert(inside(layout.state, size.first, size.second));
    assert(inside(layout.beer, size.first, size.second));
    assert(inside(layout.beer_unit, size.first, size.second));
    assert(inside(layout.beer_set, size.first, size.second));
    assert(inside(layout.fridge, size.first, size.second));
    assert(inside(layout.fridge_set, size.first, size.second));
    assert(inside(layout.chiller, size.first, size.second));
    assert(inside(layout.runtime, size.first, size.second));
    assert(inside(layout.touch, size.first, size.second));
    if (size.first == 320) {
      assert(layout.temperature_card.x == 8);
      assert(layout.temperature_card.y == 48);
      assert(layout.temperature_card.height == 416);
      assert(layout.beer_heading.x == 16);
      assert(layout.beer_heading.width == 140);
      assert(layout.beer.y == 94);
      assert(layout.beer.height == 110);
      assert(layout.mode_card.x == 166);
      assert(layout.mode_card.y == 62);
      assert(layout.mode_card.height == 32);
      assert(layout.mode.x == 170);
      assert(layout.mode.y == 66);
      assert(layout.mode.width == 130);
      assert(layout.mode.height == 24);
      assert(layout.beer_set.y == 214);
      assert(layout.fridge_set.x == 16);
      assert(layout.fridge_set.y == 248);
      assert(layout.fridge_set.width == 288);
      assert(layout.fridge_set.height == 26);
      assert(layout.state.y == 284);
      assert(layout.state.height == 52);
      assert(layout.runtime.y == 342);
      assert(layout.runtime.height == 36);
      assert(layout.fridge.y == 392);
      assert(layout.chiller.x == 168);
      assert(layout.chiller.y == 392);
      assert(layout.fridge.width == 136);
      assert(layout.chiller.width == 136);
      assert(layout.fridge.height == 72);
      assert(layout.chiller.height == 72);
    } else {
      assert(layout.header.height == 36);
      assert(layout.temperature_card.y == 38);
      assert(layout.temperature_card.height == 198);
      assert(layout.mode_card.x == 330);
      assert(layout.mode_card.y == 54);
      assert(layout.mode_card.width == 138);
      assert(layout.mode_card.height == 40);
      assert(layout.mode.x == 334);
      assert(layout.mode.y == 58);
      assert(layout.mode.width == 130);
      assert(layout.mode.height == 32);
      assert(layout.online.x == 384);
      assert(layout.online.width == 84);
      assert(layout.fridge_set.x == 286);
      assert(layout.fridge_set.y == 151);
      assert(layout.fridge_set.width == 174);
      assert(layout.fridge_set.height == 30);
      assert(layout.state.y == 186);
      assert(layout.runtime.y == 186);
      assert(layout.fridge.y == 248);
      assert(layout.chiller.x == 254);
      assert(layout.chiller.y == 248);
      assert(layout.fridge.height == 62);
      assert(layout.chiller.height == 62);
    }
    assert(layout.fridge.y == layout.chiller.y);
    assert(layout.fridge.x + layout.fridge.width < layout.chiller.x);
    assert(layout.runtime.y + layout.runtime.height < layout.fridge.y);
    assert(layout.beer_set.x + layout.beer_set.width < layout.fridge_set.x ||
           layout.fridge_set.x + layout.fridge_set.width < layout.beer_set.x ||
           layout.beer_set.y + layout.beer_set.height <= layout.fridge_set.y ||
           layout.fridge_set.y + layout.fridge_set.height <= layout.beer_set.y);
    assert(layout.online.x + layout.online.width <= size.first - 8);
    assert(layout.mode.y >= layout.mode_card.y);
    assert(layout.state.y >= layout.mode_card.y);
    assert(layout.title.x + layout.title.width + 4 <= layout.online.x);
    if (size.first == 480) {
      assert(layout.title.x == 12);
      assert(layout.title.width == 180);
    } else {
      assert(layout.title.x == 8);
      assert(layout.title.width == 180);
    }
  }
  test_gmt154_square_thermostat_layout();
  return 0;
}
