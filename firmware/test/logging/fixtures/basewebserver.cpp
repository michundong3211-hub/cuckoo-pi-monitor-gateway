#include <basewebserver.hpp>

void BaseWebServer::loop() {
  if (_wifiScanTask) {
    JsonDocument doc;

    doc[PARAM_STATUS] = false;
    doc[PARAM_SUCCESS] = true;
    doc[PARAM_MESSAGE] = "";

    JsonArray networks = doc[PARAM_NETWORKS].to<JsonArray>();

    Log.notice(F("WEB : Scanning for wifi networks." CR));
    int noNetwork = WiFi.scanNetworks(false, false);

    for (int i = 0; i < noNetwork; i++) {
      networks[i][PARAM_SSID] = WiFi.SSID(i);
      networks[i][PARAM_RSSI] = WiFi.RSSI(i);
      networks[i][PARAM_CHANNEL] = WiFi.channel(i);
#ifdef ESP8266
      networks[i][PARAM_ENCRYPTION] = WiFi.encryptionType(i);
#else
      networks[i][PARAM_ENCRYPTION] = WiFi.encryptionType(i);
#endif
    }

    unsigned int ret = serializeJson(doc, _wifiScanData);
    Log.notice(F("WEB : Scan complete %d, %s." CR), ret, _wifiScanData.c_str());
    _wifiScanTask = false;
  }
}

void BaseWebServer::webHandleUploadFirmware() {}

void BaseWebServer::webHandleWifiScan(AsyncWebServerRequest *request) {
  _wifiScanTask = true;
  _wifiScanData = "";
  AsyncJsonResponse *response = new AsyncJsonResponse(false);
  JsonObject obj = response->getRoot().as<JsonObject>();
  obj[PARAM_SUCCESS] = true;
  obj[PARAM_MESSAGE] = "Scheduled wifi scanning";
  response->setLength();
  request->send(response);
}

void BaseWebServer::webHandleWifiScanStatus(AsyncWebServerRequest *request) {
  if (_wifiScanTask || !_wifiScanData.length()) {
    AsyncJsonResponse *response = new AsyncJsonResponse(false);
    JsonObject obj = response->getRoot().as<JsonObject>();
    obj[PARAM_STATUS] = static_cast<bool>(_wifiScanTask);
    obj[PARAM_SUCCESS] = false;
    obj[PARAM_MESSAGE] =
        _wifiScanTask ? "Wifi scanning running" : "No scanning running";
    response->setLength();
    request->send(response);
  } else {
    request->send(200, "application/json", _wifiScanData);
  }
}
