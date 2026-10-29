#include <freertos/FreeRTOS.h>

class BaseWebServer {
  String _wifiScanData;
  volatile bool _wifiScanTask = false;
};
