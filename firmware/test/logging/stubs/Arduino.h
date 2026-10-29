#pragma once
#include <windows.h>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <string>
#include <type_traits>

class __FlashStringHelper;
inline uint32_t testMillis = 0;
inline unsigned long millis() { return testMillis; }
class Print;
class Printable {
 public:
  virtual size_t printTo(Print&) const = 0;
};
constexpr int DEC = 10, HEX = 16, BIN = 2;
template <typename T> T constrain(T value, T low, T high) {
  return std::min(high, std::max(low, value));
}
class Print {
 public:
  virtual ~Print() = default;
  virtual size_t write(uint8_t) = 0;
  virtual size_t write(const uint8_t* data, size_t count) {
    for (size_t i = 0; i < count; ++i) write(data[i]);
    return count;
  }
  virtual void flush() {}
  size_t print(const char* text) {
    return write(reinterpret_cast<const uint8_t*>(text), std::strlen(text));
  }
  size_t print(const __FlashStringHelper* text) {
    return print(reinterpret_cast<const char*>(text));
  }
  size_t print(char c) { return write(static_cast<uint8_t>(c)); }
  size_t print(const Printable& value) { return value.printTo(*this); }
  template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
  size_t print(T value, int base = DEC) {
    std::ostringstream out;
    if (base == HEX) out << std::hex << std::uppercase;
    out << value;
    return print(out.str().c_str());
  }
};
