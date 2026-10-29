#include "gateway35_touch.hpp"

#include "waveshare_35_profile.hpp"

namespace gateway35 {
namespace {
constexpr uint8_t kFt6336TouchStatusRegister = 0x02;
constexpr size_t kFt6336ReportLength = 5;
}  // namespace

bool probeTouch(TwoWire& wire) {
  wire.beginTransmission(kFt6336Address);
  return wire.endTransmission() == 0;
}

TouchResult parseTouchReport(const uint8_t* report, size_t length,
                             TouchPoint* point) {
  if (report == nullptr || length < kFt6336ReportLength) {
    return TouchResult::kInvalidReport;
  }

  const uint8_t touch_count = report[0] & 0x0F;
  if (touch_count == 0) return TouchResult::kNoTouch;
  if (touch_count > 2 || point == nullptr) return TouchResult::kInvalidReport;

  const uint16_t x = ((report[1] & 0x0F) << 8) | report[2];
  const uint16_t y = ((report[3] & 0x0F) << 8) | report[4];
  if (x >= kDisplayWidth || y >= kDisplayHeight) {
    return TouchResult::kInvalidReport;
  }

  point->x = x;
  point->y = y;
  return TouchResult::kValid;
}

TouchResult readTouch(TwoWire& wire, TouchPoint* point) {
  wire.beginTransmission(kFt6336Address);
  wire.write(kFt6336TouchStatusRegister);
  if (wire.endTransmission(false) != 0) return TouchResult::kIoError;
  if (wire.requestFrom(kFt6336Address, kFt6336ReportLength) !=
      kFt6336ReportLength) {
    return TouchResult::kIoError;
  }
  if (wire.available() < static_cast<int>(kFt6336ReportLength)) {
    return TouchResult::kIoError;
  }

  uint8_t report[kFt6336ReportLength];
  for (size_t i = 0; i < kFt6336ReportLength; ++i) {
    const int value = wire.read();
    if (value < 0) return TouchResult::kIoError;
    report[i] = static_cast<uint8_t>(value);
  }
  return parseTouchReport(report, sizeof(report), point);
}

}  // namespace gateway35
