#include <iostream>
#include <string>

#if __has_include("gmt154_build_version.hpp")
#include "gmt154_build_version.hpp"
#else
// Keep the test executable runnable before the production helper exists so
// the RED run reports a failed behavior assertion rather than a compile error.
namespace gmt154 {
inline std::string formatBuildVersion(const char*, const char*) { return {}; }
inline std::string formatBuildVersion(unsigned year, unsigned month,
                                      unsigned day, unsigned hour,
                                      unsigned minute) {
  (void)year;
  (void)month;
  (void)day;
  (void)hour;
  (void)minute;
  return {};
}
}  // namespace gmt154
#endif

int main() {
  const std::string actual =
      gmt154::formatBuildVersion(2026, 10, 11, 9, 7);
  const std::string expected = "2026.10.11.0907-GMT154";
  if (actual != expected) {
    std::cerr << "FAIL: expected " << expected << ", got " << actual << '\n';
    return 1;
  }
  const std::string fromCompilerDate =
      gmt154::formatBuildVersion("Oct  3 2026", "09:07:55");
  if (fromCompilerDate != "2026.10.03.0907-GMT154") {
    std::cerr << "FAIL: compiler date/time conversion expected "
              << "2026.10.03.0907-GMT154, got " << fromCompilerDate << '\n';
    return 1;
  }
  std::cout << "PASS GMT154 build version format and zero padding\n";
  return 0;
}
