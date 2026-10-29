#ifndef SRC_GATEWAY_DEVICE_METADATA_HPP_
#define SRC_GATEWAY_DEVICE_METADATA_HPP_

#include <stdio.h>

class GatewayDeviceMetadata {
 private:
  char _ipAddress[40] = {};
  char _appVersion[32] = {};
  char _runMode[32] = {};
  float _runTime = 0.0f;

  static void copy(char* target, size_t targetSize, const char* value) {
    snprintf(target, targetSize, "%s", value == nullptr ? "" : value);
  }

 public:
  GatewayDeviceMetadata() = default;

  GatewayDeviceMetadata(const char* ipAddress, const char* appVersion,
                        const char* runMode, float runTime)
      : _runTime(runTime) {
    copy(_ipAddress, sizeof(_ipAddress), ipAddress);
    copy(_appVersion, sizeof(_appVersion), appVersion);
    copy(_runMode, sizeof(_runMode), runMode);
  }

  const char* ipAddress() const { return _ipAddress; }
  const char* appVersion() const { return _appVersion; }
  const char* runMode() const { return _runMode; }
  float runTime() const { return _runTime; }
};

#endif  // SRC_GATEWAY_DEVICE_METADATA_HPP_
