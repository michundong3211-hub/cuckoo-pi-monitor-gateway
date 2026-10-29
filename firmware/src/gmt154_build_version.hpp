#pragma once

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace gmt154 {

inline std::string formatBuildVersion(unsigned year, unsigned month,
                                      unsigned day, unsigned hour,
                                      unsigned minute) {
  char version[32]{};
  std::snprintf(version, sizeof(version), "%04u.%02u.%02u.%02u%02u-GMT154",
                year, month, day, hour, minute);
  return version;
}

inline std::string formatBuildVersion(const char* buildDate,
                                      const char* buildTime) {
  static constexpr const char* months[] = {
      "Jan", "Feb", "Mar", "Apr", "May", "Jun",
      "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  if (buildDate == nullptr || buildTime == nullptr) return {};

  unsigned month = 0;
  for (unsigned i = 0; i < 12; ++i) {
    if (std::strncmp(buildDate, months[i], 3) == 0) {
      month = i + 1;
      break;
    }
  }
  if (month == 0) return {};

  const unsigned day = static_cast<unsigned>(std::atoi(buildDate + 4));
  const unsigned year = static_cast<unsigned>(std::atoi(buildDate + 7));
  const unsigned hour = static_cast<unsigned>(std::atoi(buildTime));
  const unsigned minute = static_cast<unsigned>(std::atoi(buildTime + 3));
  return formatBuildVersion(year, month, day, hour, minute);
}

}  // namespace gmt154
