#include <assert.h>
#include <string.h>

#include "../../src/gateway_device_metadata.hpp"

int main() {
  GatewayDeviceMetadata bleMetadata;
  assert(strcmp(bleMetadata.ipAddress(), "") == 0);
  assert(strcmp(bleMetadata.appVersion(), "") == 0);
  assert(strcmp(bleMetadata.runMode(), "") == 0);
  assert(bleMetadata.runTime() == 0.0f);

  GatewayDeviceMetadata httpMetadata("192.168.4.2", "3.3.4", "0", 3.0f);
  assert(strcmp(httpMetadata.ipAddress(), "192.168.4.2") == 0);
  assert(strcmp(httpMetadata.appVersion(), "3.3.4") == 0);
  assert(strcmp(httpMetadata.runMode(), "0") == 0);
  assert(httpMetadata.runTime() == 3.0f);
  return 0;
}
