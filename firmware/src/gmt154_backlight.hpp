#pragma once

namespace gmt154_backlight {

template <typename Io>
class Controller {
 public:
  Controller(Io& io, int pin) : io_(io), pin_(pin) {}

  void initialize_off() {
    io_.pin_mode(pin_);
    io_.digital_write(pin_, true);
  }

  void turn_on() { io_.digital_write(pin_, false); }

 private:
  Io& io_;
  int pin_;
};

}  // namespace gmt154_backlight
