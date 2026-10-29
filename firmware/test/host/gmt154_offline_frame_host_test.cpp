#include <cassert>
#include <fstream>
#include <iterator>
#include <string>
#include "gmt154_offline_frame.hpp"
#include "gmt154_cuckoo_temperature_policy.hpp"
static std::string read(const char* name) { std::ifstream f(name); assert(f); return {std::istreambuf_iterator<char>(f), {}}; }
static size_t count(const std::string& s, const std::string& needle) { size_t n=0, pos=0; while ((pos=s.find(needle,pos)) != std::string::npos) { ++n; pos+=needle.size(); } return n; }
int main() {
  assert(!gmt154MeasurementOffline(59, 60));
  assert(gmt154MeasurementOffline(60, 60));
  assert(gmt154MeasurementOffline(61, 60));
  assert(!gmt154MeasurementOffline(0, 3660));
  assert(!gmt154ShowOfflineFrame(false, false));
  assert(gmt154ShowOfflineFrame(true, false));
  assert(!gmt154ShowOfflineFrame(true, true));
  assert(gmt154MatchingMeasurementOffline("a", "a", false));
  assert(!gmt154MatchingMeasurementOffline("b", "a", false));
  assert(!gmt154MatchingMeasurementOffline("a", "a", true));
  assert(!gmt154MatchingMeasurementOffline("", "", false));
  for (int mode = 1; mode <= 3; ++mode) {
    auto p = gmt154_cuckoo_temperature_policy::selectTemperaturePresentation(
        true, true, mode, true, 8.0f, true, 18.0f, false);
    assert(p.display_temperature == (mode == 1 ? 8.0f : 18.0f));
    assert(gmt154ShowOfflineFrame(true, false));
    assert(!gmt154ShowOfflineFrame(true, true));
  }
  auto no_snapshot = gmt154_cuckoo_temperature_policy::selectTemperaturePresentation(
      true, false, 2, false, 0, false, 0, false);
  assert(!no_snapshot.has_temperature);
  assert(gmt154ShowOfflineFrame(true, false));
  const auto main = read("src/main_gateway.cpp");
  const auto display = read("src/display.cpp");
  const auto ui = read("src/ui_gravitymon_gateway.cpp");
  assert(count(main, "getRssi(), displayOnline)") == 3);
  assert(main.find("entry->getUpdateAge(), displaySettings.offline_timeout_seconds") != std::string::npos);
  assert(display.find("gravitymon_gateway_set_measurement_online(deviceId, online)") != std::string::npos);
  assert(ui.find("gmt154MatchingMeasurementOffline(g_state.data_device_id,") != std::string::npos);
  assert(ui.find("gmt154ShowOfflineFrame(device_id[0] != '\\0', cuckoo_online)") != std::string::npos);
  assert(ui.find("g_state.gravity_offline_frame = nullptr;") != std::string::npos);
  assert(ui.find("g_state.cuckoo_offline_frame = nullptr;") != std::string::npos);
}
