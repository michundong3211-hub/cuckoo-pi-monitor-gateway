#include "gmt154_backlight.hpp"

#include <cassert>
#include <string>
#include <vector>

namespace {

struct FakeIo {
  std::vector<std::string> events;

  void pin_mode(int pin) { events.push_back("mode:" + std::to_string(pin)); }

  void digital_write(int pin, bool high) {
    events.push_back("write:" + std::to_string(pin) + ":" +
                     (high ? "1" : "0"));
  }
};

void testActiveLowInitializationAndEnable() {
  FakeIo io;
  gmt154_backlight::Controller<FakeIo> backlight(io, 2);

  backlight.initialize_off();
  backlight.turn_on();

  assert((io.events == std::vector<std::string>{
                             "mode:2", "write:2:1", "write:2:0"}));
}

}  // namespace

int main() {
  testActiveLowInitializationAndEnable();
  return 0;
}
