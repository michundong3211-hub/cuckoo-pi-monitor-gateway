/*
MIT License

Copyright (c) 2021-2025 Magnus

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
 */
#if defined(GATEWAY)

#include <ArduinoJson.h>

#include <ble_gateway.hpp>
#include <config_gateway.hpp>
#include <main_gateway.hpp>
#include <mdns_discovery.hpp>
#include <measurement.hpp>
#include <memory>
#include <push_gateway.hpp>
#include <sdcard_mmc.hpp>
#include <sdcard_sd.hpp>
#include <uptime.hpp>
#include <utils.hpp>
#include "settings_freeze_diag.hpp"
#include <web_gateway.hpp>
#include <cuckoo_pi_mqtt.hpp>
#include <cuckoo_pi_model.hpp>

constexpr auto PARAM_GRAVITY_DEVICE = "gravity_device";
constexpr auto PARAM_PRESSURE_DEVICE = "pressure_device";
constexpr auto PARAM_TEMPERATURE_DEVICE = "temperature_device";
constexpr auto PARAM_RAPT_DEVICE = "rapt_device";
constexpr auto PARAM_DEVICE = "device";
constexpr auto PARAM_NAME = "name";
constexpr auto PARAM_TYPE = "type";
constexpr auto PARAM_SOURCE = "source";
constexpr auto PARAM_GRAVITY = "gravity";
constexpr auto PARAM_VELOCITY = "velocity";
constexpr auto PARAM_PRESSURE = "pressure";
constexpr auto PARAM_PRESSURE1 = "pressure1";
constexpr auto PARAM_TEMP = "temp";
constexpr auto PARAM_CHAMBER_TEMP = "chamber_temp";
constexpr auto PARAM_BEER_TEMP = "beer_temp";
constexpr auto PARAM_UPDATE_TIME = "update_time";
constexpr auto PARAM_PUSH_TIME = "push_time";
constexpr auto PARAM_UPTIME_SECONDS = "uptime_seconds";
constexpr auto PARAM_UPTIME_MINUTES = "uptime_minutes";
constexpr auto PARAM_UPTIME_HOURS = "uptime_hours";
constexpr auto PARAM_UPTIME_DAYS = "uptime_days";
constexpr auto PARAM_SD_MOUNTED = "sd_mounted";
constexpr auto PARAM_SD = "sd";
constexpr auto PARAM_TFT = "tft";

#if defined(ENABLE_MMC)
extern SdCardMMC mySdStorage;
#elif defined(ENABLE_SD)
extern SdCardSD mySdStorage;
#endif

extern MdnsScanner myMdnsScanner;

namespace {
constexpr size_t GATEWAY_HTTP_RESPONSE_LOG_LIMIT = 768;
constexpr int GATEWAY_PUSH_TEST_NO_MEASUREMENT = -2;

String gatewayHttpResponseForLog(String response, const char *measurementToken,
                                 const char *configuredToken) {
  if (measurementToken != nullptr && strlen(measurementToken) > 0)
    response.replace(measurementToken, "***");
  if (configuredToken != nullptr && strlen(configuredToken) > 0)
    response.replace(configuredToken, "***");
  response.replace("\r", "\\r");
  response.replace("\n", "\\n");
  if (response.length() == 0) response = "<empty>";
  if (response.length() > GATEWAY_HTTP_RESPONSE_LOG_LIMIT) {
    response = response.substring(0, GATEWAY_HTTP_RESPONSE_LOG_LIMIT);
    response += "...<truncated>";
  }
  return response;
}

class GatewayDiagnosticPush : public BrewingPush {
 public:
  explicit GatewayDiagnosticPush(GravmonGatewayConfig *config)
      : BrewingPush(config), _gatewayConfig(config) {}

  String sendHttpPostWithResponse(String &payload, const char *target,
                                  const char *header1, const char *header2,
                                  bool sendTcpFlag,
                                  const char *measurementToken) {
    if (sendTcpFlag) return sendTcp(payload, target);

    Log.notice(F("PUSH: Sending diagnostic values to HTTP post" CR));
    _lastResponseCode = 0;
    _lastSuccess = false;
    String response;

    if (isSecure(target)) {
      Log.notice(F("PUSH: HTTP, SSL enabled without validation." CR));
      allocateSecure();
      _wifiSecure->setInsecure();
      probeMFLN(target);
      _http->setTimeout(_config->getPushTimeout() * 1000);
      _http->begin(*_wifiSecure, target);
    } else {
      _http->setTimeout(_config->getPushTimeout() * 1000);
      _http->begin(*_wifi, target);
    }

    addHttpHeader(header1);
    addHttpHeader(header2);
    _lastResponseCode = _http->POST(payload);
    if (_lastResponseCode > 0) response = _http->getString();

    if (_lastResponseCode == 200) {
      _lastSuccess = true;
      Log.notice(F("PUSH: HTTP post successful, response=%d" CR),
                 _lastResponseCode);
    } else {
      const String responseForLog = gatewayHttpResponseForLog(
          response, measurementToken, _gatewayConfig->getToken());
      Log.error(F("PUSH: HTTP post failed, response=%d, body=%s" CR),
                _lastResponseCode, responseForLog.c_str());
    }

    _http->end();
    if (isSecure(target))
      _wifiSecure->stop();
    else
      _wifi->stop();
    tcp_cleanup();
    return response;
  }

 private:
  GravmonGatewayConfig *_gatewayConfig;
};

MeasurementType gatewayPushTestMeasurementType(
    const String &target, const GravmonGatewayConfig *config) {
  if ((!target.compareTo(PARAM_FORMAT_POST_GRAVITY) &&
       config->hasTargetHttpPost()) ||
      (!target.compareTo(PARAM_FORMAT_POST2_GRAVITY) &&
       config->hasTargetHttpPost2()) ||
      (!target.compareTo(PARAM_FORMAT_GET_GRAVITY) &&
       config->hasTargetHttpGet()) ||
      (!target.compareTo(PARAM_FORMAT_INFLUXDB_GRAVITY) &&
       config->hasTargetInfluxDb2()) ||
      (!target.compareTo(PARAM_FORMAT_MQTT_GRAVITY) &&
       config->hasTargetMqtt())) {
    return MeasurementType::Gravitymon;
  }
  if ((!target.compareTo(PARAM_FORMAT_POST_PRESSURE) &&
       config->hasTargetHttpPost()) ||
      (!target.compareTo(PARAM_FORMAT_POST2_PRESSURE) &&
       config->hasTargetHttpPost2()) ||
      (!target.compareTo(PARAM_FORMAT_GET_PRESSURE) &&
       config->hasTargetHttpGet()) ||
      (!target.compareTo(PARAM_FORMAT_INFLUXDB_PRESSURE) &&
       config->hasTargetInfluxDb2()) ||
      (!target.compareTo(PARAM_FORMAT_MQTT_PRESSURE) &&
       config->hasTargetMqtt())) {
    return MeasurementType::Pressuremon;
  }
  return MeasurementType::NoType;
}
}  // namespace

GatewayWebServer::GatewayWebServer(GravmonGatewayConfig *config)
    : BrewingWebServer(config), _gatewayConfig(config) {}

void GatewayWebServer::doWebFeature(JsonObject &obj) {
  obj["build_datetime"] = gatewayBuildDateTime();
  obj[PARAM_FIRMWARE_FILE] = CFG_FILENAMEBIN;
#if defined(ENABLE_MMC) || defined(ENABLE_SD)
  obj[PARAM_SD] = true;
#else
  obj[PARAM_SD] = false;
#endif

#if defined(ENABLE_TFT)
  obj[PARAM_TFT] = true;
#else
  obj[PARAM_TFT] = false;
#endif
}

void GatewayWebServer::doWebStatus(JsonObject &obj) {
  JsonArray gravityDevices = obj[PARAM_GRAVITY_DEVICE].to<JsonArray>();
  JsonArray pressureDevices = obj[PARAM_PRESSURE_DEVICE].to<JsonArray>();
  JsonArray temperatureDevices = obj[PARAM_TEMPERATURE_DEVICE].to<JsonArray>();
  JsonArray cuckooDevices = obj["cuckoo_pi_device"].to<JsonArray>();
  int gravIdx = 0, pressIdx = 0, tempIdx = 0;

  for (int i = 0; i < myMeasurementList.size(); i++) {
    MeasurementEntry *entry = myMeasurementList.getMeasurementEntry(i);

    switch (entry->getType()) {
      case MeasurementType::Gravitymon: {
        Log.notice("WEB: Processing Gravitymon data %d." CR, i);
        const GravityData *gd = entry->getGravityData();

        gravityDevices[gravIdx][PARAM_NAME] = gd->getName();
        gravityDevices[gravIdx][PARAM_DEVICE] = gd->getId();
        if (!isnan(gd->getGravity()))
          gravityDevices[gravIdx][PARAM_GRAVITY] = gd->getGravity();
        if (!isnan(gd->getTempC()))
          gravityDevices[gravIdx][PARAM_TEMP] = gd->getTempC();
        gravityDevices[gravIdx][PARAM_UPDATE_TIME] = entry->getUpdateAge();
        gravityDevices[gravIdx][PARAM_PUSH_TIME] = entry->getPushAge();
        gravityDevices[gravIdx][PARAM_SOURCE] = gd->getSourceAsString();
        gravityDevices[gravIdx][PARAM_TYPE] = gd->getTypeAsString();
        gravIdx++;
      } break;

      case MeasurementType::Pressuremon: {
        Log.notice("WEB: Processing Pressuremon data %d." CR, i);
        const PressureData *pd = entry->getPressureData();

        pressureDevices[pressIdx][PARAM_NAME] = pd->getName();
        pressureDevices[pressIdx][PARAM_DEVICE] = pd->getId();
        if (!isnan(pd->getPressure()))
          pressureDevices[pressIdx][PARAM_PRESSURE] = pd->getPressure();
        if (!isnan(pd->getPressure1()))
          pressureDevices[pressIdx][PARAM_PRESSURE1] = pd->getPressure1();
        if (!isnan(pd->getTempC()))
          pressureDevices[pressIdx][PARAM_TEMP] = pd->getTempC();
        pressureDevices[pressIdx][PARAM_UPDATE_TIME] = entry->getUpdateAge();
        pressureDevices[pressIdx][PARAM_PUSH_TIME] = entry->getPushAge();
        pressureDevices[pressIdx][PARAM_SOURCE] = pd->getSourceAsString();
        pressureDevices[pressIdx][PARAM_TYPE] = pd->getTypeAsString();
        pressIdx++;
      } break;

      case MeasurementType::Tilt:
      case MeasurementType::TiltPro: {
        Log.notice("WEB: Processing Tilt data %d." CR, i);
        const TiltData *td = entry->getTiltData();

        gravityDevices[gravIdx][PARAM_NAME] = td->getTiltColor();
        gravityDevices[gravIdx][PARAM_DEVICE] = td->getId();
        if (!isnan(td->getGravity()))
          gravityDevices[gravIdx][PARAM_GRAVITY] = td->getGravity();
        if (!isnan(td->getTempC()))
          gravityDevices[gravIdx][PARAM_TEMP] = td->getTempC();
        gravityDevices[gravIdx][PARAM_UPDATE_TIME] = entry->getUpdateAge();
        gravityDevices[gravIdx][PARAM_PUSH_TIME] = entry->getPushAge();
        gravityDevices[gravIdx][PARAM_SOURCE] = td->getSourceAsString();
        gravityDevices[gravIdx][PARAM_TYPE] = td->getTypeAsString();
        gravIdx++;
      } break;

      case MeasurementType::Chamber: {
        Log.notice("WEB: Processing Chamber data %d." CR, i);
        const ChamberData *cd = entry->getChamberData();

        temperatureDevices[tempIdx][PARAM_NAME] = cd->getName();
        temperatureDevices[tempIdx][PARAM_DEVICE] = cd->getId();
        if (!isnan(cd->getChamberTempC()))
          temperatureDevices[tempIdx][PARAM_CHAMBER_TEMP] =
              cd->getChamberTempC();
        if (!isnan(cd->getBeerTempC()))
          temperatureDevices[tempIdx][PARAM_BEER_TEMP] = cd->getBeerTempC();
        temperatureDevices[tempIdx][PARAM_UPDATE_TIME] = entry->getUpdateAge();
        temperatureDevices[tempIdx][PARAM_PUSH_TIME] = entry->getPushAge();
        temperatureDevices[tempIdx][PARAM_SOURCE] = cd->getSourceAsString();
        temperatureDevices[tempIdx][PARAM_TYPE] = cd->getTypeAsString();
        tempIdx++;
      } break;

      case MeasurementType::Rapt: {
        Log.notice("WEB: Processing Rapt data %d." CR, i);
        const RaptData *rd = entry->getRaptData();

        gravityDevices[gravIdx][PARAM_DEVICE] = rd->getId();
        if (!isnan(rd->getGravity()))
          gravityDevices[gravIdx][PARAM_GRAVITY] = rd->getGravity();
        if (!isnan(rd->getVelocity()))
          gravityDevices[gravIdx][PARAM_VELOCITY] = rd->getVelocity();
        if (!isnan(rd->getTempC()))
          gravityDevices[gravIdx][PARAM_TEMP] = rd->getTempC();
        gravityDevices[gravIdx][PARAM_UPDATE_TIME] = entry->getUpdateAge();
        gravityDevices[gravIdx][PARAM_PUSH_TIME] = entry->getPushAge();
        gravityDevices[gravIdx][PARAM_SOURCE] = rd->getSourceAsString();
        gravityDevices[gravIdx][PARAM_TYPE] = rd->getTypeAsString();
        gravIdx++;
      } break;
    }
  }

  CuckooPiConfig cuckooConfig = defaultCuckooPiConfig();
  CuckooPiMqttDiagnostics cuckooDiagnostics{};
  myConfig.copyCuckooPiConfig(&cuckooConfig);
  cuckooPiMqttService.copyDiagnostics(&cuckooDiagnostics);
  for (uint8_t index = 0; index < cuckooConfig.device_count &&
                          index < kMaxCuckooPiDevices;
       ++index) {
    const CuckooPiDeviceConfig& settings = cuckooConfig.devices[index];
    JsonObject device = cuckooDevices.add<JsonObject>();
    device["id"] = String(settings.id);
    device["enabled"] = cuckooConfig.enabled;
    device["mqtt_connected"] = cuckooDiagnostics.connected;
    CuckooPiRuntimeDevice runtime{};
    if (!cuckooPiMqttService.copyDeviceRuntime(settings.id, &runtime)) {
      device["online"] = false;
      device["update_time"] = 0;
      continue;
    }
    const uint32_t updateAgeSeconds =
        runtime.has_snapshot ? (millis() - runtime.last_seen_ms) / 1000UL : 0;
    device["update_time"] = updateAgeSeconds;
    device["online"] = runtime.has_snapshot &&
                       updateAgeSeconds < settings.offline_timeout_seconds;
    device["parse_errors"] = runtime.parse_error_count;
    if (!runtime.has_snapshot) continue;
    const CuckooPiSnapshot& snapshot = runtime.snapshot;
    if (snapshot.has_mode) {
      device["mode_raw"] = snapshot.mode_raw;
      device["mode"] = cuckooPiModeLabel(snapshot.mode_raw);
    }
    if (snapshot.has_state) {
      device["state_raw"] = snapshot.state_raw;
      device["state"] = cuckooPiStateLabel(snapshot.state_raw);
    }
    if (snapshot.has_beer_temp)
      device["beer_temp_c"] = snapshot.beer_temp_c;
    if (snapshot.has_beer_set) device["beer_set_c"] = snapshot.beer_set_c;
    if (snapshot.has_fridge_temp)
      device["fridge_temp_c"] = snapshot.fridge_temp_c;
    if (snapshot.has_fridge_set)
      device["fridge_set_c"] = snapshot.fridge_set_c;
    if (snapshot.has_status_time)
      device["status_time_seconds"] = snapshot.status_time_seconds;
  }

  // Add other params
  obj[CONFIG_GRAVITY_UNIT] = String(myConfig.getGravityUnit());
  obj[CONFIG_PRESSURE_UNIT] = myConfig.getPressureUnit();
  obj[PARAM_UPTIME_SECONDS] = myUptime.getSeconds();
  obj[PARAM_UPTIME_MINUTES] = myUptime.getMinutes();
  obj[PARAM_UPTIME_HOURS] = myUptime.getHours();
  obj[PARAM_UPTIME_DAYS] = myUptime.getDays();
#if defined(ENABLE_MMC) || defined(ENABLE_SD)
  obj[PARAM_SD_MOUNTED] = mySdStorage.hasCard();
#else
  obj[PARAM_SD_MOUNTED] = false;
#endif
}

bool GatewayWebServer::setupWebServer(const char *serviceName) {
  bool b = BrewingWebServer::setupWebServer(serviceName);

  _server->on(
      "/api/settings-freeze-diagnostic", (WebRequestMethodComposite)HTTP_GET,
      [](AsyncWebServerRequest *request) {
        char output[2304];
        if (!freeze_diag::formatJson(output, sizeof(output), millis())) {
          request->send(500, "application/json", "{\"error\":\"diagnostic snapshot too large\"}");
          return;
        }
        request->send(200, "application/json", output);
      });

  _server->on(
      "/api/factory", HTTP_POST,
      [this](AsyncWebServerRequest *request) {
        webHandleFactoryDefaults(request);
      });

  AsyncCallbackJsonWebHandler *handler;
  handler = new AsyncCallbackJsonWebHandler(
      "/post", std::bind(&GatewayWebServer::webHandleRemotePost, this,
                         std::placeholders::_1, std::placeholders::_2));
  _server->addHandler(handler);
  handler = new AsyncCallbackJsonWebHandler(
      "/api/sd", std::bind(&GatewayWebServer::webHandleSecureDigital, this,
                           std::placeholders::_1, std::placeholders::_2));
  _server->addHandler(handler);
  _server->on(
      "/api/mdns", (WebRequestMethodComposite)HTTP_GET,
      [this](AsyncWebServerRequest *request) { webHandleMdns(request); });
#if defined(ENABLE_MMC) || defined(ENABLE_SD)
  _server->serveStatic("/sd", mySdStorage.getFS(), "/");
#endif

  // SPA fallback: unknown GET routes (e.g. /alarm/<deviceId> in history
  // mode) are served with index.html instead of being redirected to "/".
  // API routes keep the default not-found handling.
  _server->onNotFound([](AsyncWebServerRequest *request) {
    if (request->method() == HTTP_GET && request->url().indexOf("/api") != 0) {
      Log.notice(F("WEB : SPA fallback for %s (Memory)." CR),
                 request->url().c_str());
      request->send(200, "text/html", (const uint8_t *)indexHtmlStart,
                    reinterpret_cast<uint32_t>(&indexHtmlEnd[0]) -
                        reinterpret_cast<uint32_t>(&indexHtmlStart[0]));
    } else {
      request->redirect("/");
    }
  });
  return b;
}

constexpr auto GRAVMON_NAME = "name";
constexpr auto GRAVMON_ID = "ID";
constexpr auto GRAVMON_TOKEN = "token";
constexpr auto GRAVMON_INTERVAL = "interval";
constexpr auto GRAVMON_TEMPERATURE = "temperature";
constexpr auto GRAVMON_TEMPERATURE_UNIT = "temp_units";
constexpr auto GRAVMON_GRAVITY = "gravity";
constexpr auto GRAVMON_ANGLE = "angle";
constexpr auto GRAVMON_BATTERY = "battery";
constexpr auto GRAVMON_RSSI = "RSSI";
constexpr auto GRAVMON_HTTP_POST_SLEEP_INTERVAL =
    "http-post-sleep-interval";
constexpr auto GRAVMON_BLE_SLEEP_INTERVAL = "ble-sleep-interval";
constexpr auto GRAVMON_IP_ADDRESS = "ip-address";
constexpr auto GRAVMON_APP_VERSION = "app-ver";
constexpr auto GRAVMON_RUN_MODE = "run-mode";
constexpr auto GRAVMON_RUN_TIME = "run-time";

constexpr auto PRESSMON_NAME = "name";
constexpr auto PRESSMON_ID = "id";
constexpr auto PRESSMON_TOKEN = "token";
constexpr auto PRESSMON_INTERVAL = "interval";
constexpr auto PRESSMON_TEMPERATURE = "temperature";
constexpr auto PRESSMON_TEMPERATURE_UNIT = "temperature-unit";
constexpr auto PRESSMON_PRESSURE = "pressure";
constexpr auto PRESSMON_PRESSURE1 = "pressure1";
constexpr auto PRESSMON_PRESSURE_UNIT = "pressure-unit";
constexpr auto PRESSMON_BATTERY = "battery";
constexpr auto PRESSMON_RSSI = "rssi";
constexpr auto PRESSMON_HTTP_POST_SLEEP_INTERVAL =
    "http-post-sleep-interval";
constexpr auto PRESSMON_BLE_SLEEP_INTERVAL = "ble-sleep-interval";

void GatewayWebServer::webHandleRemotePost(AsyncWebServerRequest *request,
                                           JsonVariant &json) {
  Log.notice(F("WEB : webServer callback for /post." CR));
  JsonObject obj = json.as<JsonObject>();

  /* Expected gravitymon format.
  {
    "name": "gravitymon-gwfa413c",
    "ID": "fa413c",
    "token": "",
    "interval": 900,
    "temperature": 20.1,
    "temp_units": "C",
    "gravity": 1.015,
    "angle": 35,
    "battery": 4,
    "RSSI": -79
  }*/

  /* Expected pressuremon format.
  {
    "name": "test",
    "id": "e17d44",
    "token": "",
    "interval": 300,
    "temperature": 22.3,
    "temperature-unit": "C",
    "pressure": 1.001,
    "pressure-unit": "PSI",
    "battery": 3.85,
    "rssi": -76.2
  }*/

  if (!obj[GRAVMON_GRAVITY].isNull() || !obj[PRESSMON_PRESSURE].isNull()) {
    String json;
    json.reserve(200);
    serializeJson(obj, json);
    _postData.push(json);
    request->send(200);
    return;
  }

  request->send(422);
}

void GatewayWebServer::loop() {
  BrewingWebServer::loop();

  if (_gatewayPushTestResultOverride) {
    _pushTestLastCode = _gatewayPushTestResultCode;
    _pushTestLastSuccess = _gatewayPushTestResultSuccess;
    Log.notice(F("WEB : Gateway push test result, success=%d, code=%d" CR),
               _pushTestLastSuccess, _pushTestLastCode);
    _gatewayPushTestResultOverride = false;
  }

  // Process data that was received in posts
  while (!_postData.empty()) {
    String data = _postData.front();

    Serial.printf("Processing data: %s\n", data.c_str());

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, data);
    JsonObject obj = doc.as<JsonObject>();

    // Process gravitymon data
    if (!obj[GRAVMON_GRAVITY].isNull()) {
      String id = !obj[GRAVMON_ID].isNull() ? obj[GRAVMON_ID].as<String>() : "";
      String token =
          !obj[GRAVMON_TOKEN].isNull() ? obj[GRAVMON_TOKEN].as<String>() : "";
      String name =
          !obj[GRAVMON_NAME].isNull() ? obj[GRAVMON_NAME].as<String>() : "";
      int interval =
          !obj[GRAVMON_INTERVAL].isNull() ? obj[GRAVMON_INTERVAL].as<int>() : 0;
      float temp = !obj[GRAVMON_TEMPERATURE].isNull()
                       ? obj[GRAVMON_TEMPERATURE].as<float>()
                       : 0.0;
      String tempUnits = !obj[GRAVMON_TEMPERATURE_UNIT].isNull()
                             ? obj[GRAVMON_TEMPERATURE_UNIT].as<String>()
                             : "";
      float gravity = !obj[GRAVMON_GRAVITY].isNull()
                          ? obj[GRAVMON_GRAVITY].as<float>()
                          : 0.0;
      float angle =
          !obj[GRAVMON_ANGLE].isNull() ? obj[GRAVMON_ANGLE].as<float>() : 0.0;
      float battery = !obj[GRAVMON_BATTERY].isNull()
                          ? obj[GRAVMON_BATTERY].as<float>()
                          : 0.0;
      int rssi = !obj[GRAVMON_RSSI].isNull() ? obj[GRAVMON_RSSI].as<int>() : 0;
      int httpPostSleepInterval =
          !obj[GRAVMON_HTTP_POST_SLEEP_INTERVAL].isNull()
              ? obj[GRAVMON_HTTP_POST_SLEEP_INTERVAL].as<int>()
              : 0;
      int bleSleepInterval =
          !obj[GRAVMON_BLE_SLEEP_INTERVAL].isNull()
              ? obj[GRAVMON_BLE_SLEEP_INTERVAL].as<int>()
              : 0;
      String ipAddress = !obj[GRAVMON_IP_ADDRESS].isNull()
                             ? obj[GRAVMON_IP_ADDRESS].as<String>()
                             : "";
      String appVersion = !obj[GRAVMON_APP_VERSION].isNull()
                              ? obj[GRAVMON_APP_VERSION].as<String>()
                              : "";
      String deviceRunMode = !obj[GRAVMON_RUN_MODE].isNull()
                                 ? obj[GRAVMON_RUN_MODE].as<String>()
                                 : "";
      float deviceRunTime = !obj[GRAVMON_RUN_TIME].isNull()
                                ? obj[GRAVMON_RUN_TIME].as<float>()
                                : 0.0f;
      GatewayDeviceMetadata deviceMetadata(
          ipAddress.c_str(), appVersion.c_str(), deviceRunMode.c_str(),
          deviceRunTime);

      std::unique_ptr<MeasurementBaseData> gravityData;
      gravityData.reset(new GravityData(MeasurementSource::HttpPost, id, name,
                                         token, temp, gravity, angle, battery, 0,
                                         rssi, interval, httpPostSleepInterval,
                                         bleSleepInterval, deviceMetadata));

      Log.info(F("WEB : Update data for gravitymon %s." CR),
               gravityData->getId());
      myMeasurementList.updateData(gravityData);
    }

    // Process pressuremon data
    if (!obj[PRESSMON_PRESSURE].isNull()) {
      String id =
          !obj[PRESSMON_ID].isNull() ? obj[PRESSMON_ID].as<String>() : "";
      String token =
          !obj[PRESSMON_TOKEN].isNull() ? obj[PRESSMON_TOKEN].as<String>() : "";
      String name =
          !obj[PRESSMON_NAME].isNull() ? obj[PRESSMON_NAME].as<String>() : "";
      int interval = !obj[PRESSMON_INTERVAL].isNull()
                         ? obj[PRESSMON_INTERVAL].as<int>()
                         : 0;
      float temp = !obj[PRESSMON_TEMPERATURE].isNull()
                       ? obj[PRESSMON_TEMPERATURE].as<float>()
                       : 0.0;
      String tempUnits = !obj[PRESSMON_PRESSURE_UNIT].isNull()
                             ? obj[PRESSMON_PRESSURE_UNIT].as<String>()
                             : "";
      float pressure = !obj[PRESSMON_PRESSURE].isNull()
                           ? obj[PRESSMON_PRESSURE].as<float>()
                           : 0.0;
      float pressure1 = !obj[PRESSMON_PRESSURE1].isNull()
                            ? obj[PRESSMON_PRESSURE1].as<float>()
                            : 0.0;
      float battery = !obj[PRESSMON_BATTERY].isNull()
                          ? obj[PRESSMON_BATTERY].as<float>()
                          : 0.0;
      int rssi =
          !obj[PRESSMON_RSSI].isNull() ? obj[PRESSMON_RSSI].as<int>() : 0;
      int httpPostSleepInterval =
          !obj[PRESSMON_HTTP_POST_SLEEP_INTERVAL].isNull()
              ? obj[PRESSMON_HTTP_POST_SLEEP_INTERVAL].as<int>()
              : 0;
      int bleSleepInterval =
          !obj[PRESSMON_BLE_SLEEP_INTERVAL].isNull()
              ? obj[PRESSMON_BLE_SLEEP_INTERVAL].as<int>()
              : 0;

      std::unique_ptr<MeasurementBaseData> pressureData;
      pressureData.reset(new PressureData(MeasurementSource::HttpPost, id, name,
                                          token, temp, pressure, pressure1,
                                          battery, 0, rssi, interval,
                                          httpPostSleepInterval,
                                          bleSleepInterval));

      Log.info(F("WEB : Update data for pressuremon %s." CR),
               pressureData->getId());
      myMeasurementList.updateData(pressureData);
    }

    _postData.pop();
  }
}

void GatewayWebServer::doTaskPushTestSetup(TemplatingEngine &engine,
                                           BrewingPush &push) {
  Log.notice(F("WEB : Running scheduled push test for %s" CR),
             _pushTestTarget.c_str());

  _gatewayPushTestResultOverride = false;
  const MeasurementType measurementType =
      gatewayPushTestMeasurementType(_pushTestTarget, _gatewayConfig);
  GatewayPushTestSample sample{};

  if (measurementType != MeasurementType::NoType) {
    if (!myMeasurementList.copyLatestPushTestSample(measurementType, &sample)) {
      Log.error(F("WEB : Push test failed: no real %s measurement available" CR),
                measurementType == MeasurementType::Gravitymon ? "gravity"
                                                               : "pressure");
      _pushTestEnabled = true;
      _gatewayPushTestResultCode = GATEWAY_PUSH_TEST_NO_MEASUREMENT;
      _gatewayPushTestResultSuccess = false;
      _gatewayPushTestResultOverride = true;
      engine.freeMemory();
      push.clearTemplate();
      return;
    }

    if (measurementType == MeasurementType::Gravitymon) {
      setupTemplateEngineGravityGateway(
          _gatewayConfig, engine, sample.angle, sample.gravity,
          sample.temperature_c, sample.battery, sample.interval, sample.id,
          sample.token, sample.name, sample.http_post_sleep_interval,
          sample.ble_sleep_interval, sample.device_metadata);
    } else {
      setupTemplateEnginePressureGateway(
          _gatewayConfig, engine, sample.pressure, sample.pressure1,
          sample.temperature_c, sample.battery, sample.interval, sample.id,
          sample.token, sample.name, sample.http_post_sleep_interval,
          sample.ble_sleep_interval);
    }
  }

  if (!_pushTestTarget.compareTo(PARAM_FORMAT_POST_GRAVITY) &&
      _gatewayConfig->hasTargetHttpPost()) {
    String tpl = push.getTemplate(BrewingPush::GRAVITY_TEMPLATE_HTTP1);
    String doc = engine.create(tpl.c_str());
    GatewayDiagnosticPush diagnosticPush(_gatewayConfig);
    diagnosticPush.sendHttpPostWithResponse(
        doc, _gatewayConfig->getTargetHttpPost(),
        _gatewayConfig->getHeader1HttpPost(),
        _gatewayConfig->getHeader2HttpPost(),
        _gatewayConfig->getTcpHttpPost(), sample.token);
    _gatewayPushTestResultCode = diagnosticPush.getLastCode();
    _gatewayPushTestResultSuccess = diagnosticPush.getLastSuccess();
    _gatewayPushTestResultOverride = true;
    _pushTestEnabled = true;
  } else if (!_pushTestTarget.compareTo(PARAM_FORMAT_POST2_GRAVITY) &&
             _gatewayConfig->hasTargetHttpPost2()) {
    String tpl = push.getTemplate(BrewingPush::GRAVITY_TEMPLATE_HTTP2);
    String doc = engine.create(tpl.c_str());
    GatewayDiagnosticPush diagnosticPush(_gatewayConfig);
    diagnosticPush.sendHttpPostWithResponse(
        doc, _gatewayConfig->getTargetHttpPost2(),
        _gatewayConfig->getHeader1HttpPost2(),
        _gatewayConfig->getHeader2HttpPost2(), false, sample.token);
    _gatewayPushTestResultCode = diagnosticPush.getLastCode();
    _gatewayPushTestResultSuccess = diagnosticPush.getLastSuccess();
    _gatewayPushTestResultOverride = true;
    _pushTestEnabled = true;
  } else if (!_pushTestTarget.compareTo(PARAM_FORMAT_GET_GRAVITY) &&
             _gatewayConfig->hasTargetHttpGet()) {
    String tpl = push.getTemplate(BrewingPush::GRAVITY_TEMPLATE_HTTP3);
    String doc = engine.create(tpl.c_str());
    push.sendHttpGet(doc);
    _pushTestEnabled = true;
  } else if (!_pushTestTarget.compareTo(PARAM_FORMAT_INFLUXDB_GRAVITY) &&
             _gatewayConfig->hasTargetInfluxDb2()) {
    String tpl = push.getTemplate(BrewingPush::GRAVITY_TEMPLATE_INFLUX);
    String doc = engine.create(tpl.c_str());
    push.sendInfluxDb2(doc);
    _pushTestEnabled = true;
  } else if (!_pushTestTarget.compareTo(PARAM_FORMAT_MQTT_GRAVITY) &&
             _gatewayConfig->hasTargetMqtt()) {
    String tpl = push.getTemplate(BrewingPush::GRAVITY_TEMPLATE_MQTT);
    String doc = engine.create(tpl.c_str());
    push.sendMqtt(doc);

    _pushTestEnabled = true;
  } else if (!_pushTestTarget.compareTo(PARAM_FORMAT_POST_PRESSURE) &&
             _gatewayConfig->hasTargetHttpPost()) {
    String tpl = push.getTemplate(BrewingPush::PRESSURE_TEMPLATE_HTTP1);
    String doc = engine.create(tpl.c_str());
    GatewayDiagnosticPush diagnosticPush(_gatewayConfig);
    diagnosticPush.sendHttpPostWithResponse(
        doc, _gatewayConfig->getTargetHttpPost(),
        _gatewayConfig->getHeader1HttpPost(),
        _gatewayConfig->getHeader2HttpPost(),
        _gatewayConfig->getTcpHttpPost(), sample.token);
    _gatewayPushTestResultCode = diagnosticPush.getLastCode();
    _gatewayPushTestResultSuccess = diagnosticPush.getLastSuccess();
    _gatewayPushTestResultOverride = true;
    _pushTestEnabled = true;
  } else if (!_pushTestTarget.compareTo(PARAM_FORMAT_POST2_PRESSURE) &&
             _gatewayConfig->hasTargetHttpPost2()) {
    String tpl = push.getTemplate(BrewingPush::PRESSURE_TEMPLATE_HTTP2);
    String doc = engine.create(tpl.c_str());
    GatewayDiagnosticPush diagnosticPush(_gatewayConfig);
    diagnosticPush.sendHttpPostWithResponse(
        doc, _gatewayConfig->getTargetHttpPost2(),
        _gatewayConfig->getHeader1HttpPost2(),
        _gatewayConfig->getHeader2HttpPost2(), false, sample.token);
    _gatewayPushTestResultCode = diagnosticPush.getLastCode();
    _gatewayPushTestResultSuccess = diagnosticPush.getLastSuccess();
    _gatewayPushTestResultOverride = true;
    _pushTestEnabled = true;
  } else if (!_pushTestTarget.compareTo(PARAM_FORMAT_GET_PRESSURE) &&
             _gatewayConfig->hasTargetHttpGet()) {
    String tpl = push.getTemplate(BrewingPush::PRESSURE_TEMPLATE_HTTP3);
    String doc = engine.create(tpl.c_str());
    push.sendHttpGet(doc);
    _pushTestEnabled = true;
  } else if (!_pushTestTarget.compareTo(PARAM_FORMAT_INFLUXDB_PRESSURE) &&
             _gatewayConfig->hasTargetInfluxDb2()) {
    String tpl = push.getTemplate(BrewingPush::PRESSURE_TEMPLATE_INFLUX);
    String doc = engine.create(tpl.c_str());
    push.sendInfluxDb2(doc);
    _pushTestEnabled = true;
  } else if (!_pushTestTarget.compareTo(PARAM_FORMAT_MQTT_PRESSURE) &&
             _gatewayConfig->hasTargetMqtt()) {
    String tpl = push.getTemplate(BrewingPush::PRESSURE_TEMPLATE_MQTT);
    String doc = engine.create(tpl.c_str());
    push.sendMqtt(doc);
    _pushTestEnabled = true;
  }

  engine.freeMemory();
  push.clearTemplate();
}

void GatewayWebServer::webHandleSecureDigital(AsyncWebServerRequest *request,
                                              JsonVariant &json) {
  if (!isAuthenticated(request)) {
    return;
  }

  Log.notice(F("WEB : webServer callback for /api/sd." CR));
#if defined(ENABLE_MMC) || defined(ENABLE_SD)
  JsonObject obj = json.as<JsonObject>();

  if (!obj[PARAM_COMMAND].isNull()) {
    if (obj[PARAM_COMMAND] == String("dir")) {
      Log.notice(F("WEB : File system listing requested." CR));
      AsyncJsonResponse *response = new AsyncJsonResponse(false);
      JsonObject obj = response->getRoot().as<JsonObject>();

      obj[PARAM_TOTAL] = mySdStorage.totalBytes();
      obj[PARAM_USED] = mySdStorage.usedBytes();
      obj[PARAM_FREE] = mySdStorage.totalBytes() - mySdStorage.usedBytes();

      File root = mySdStorage.open("/", FILE_READ);
      File f = root.openNextFile();
      int i = 0;

      JsonArray arr = obj[PARAM_FILES].to<JsonArray>();
      while (f) {
        Log.notice(F("WEB : %s." CR), f.name());
        if (!String(f.name()).startsWith(
                ".")) {  // Ignore files with . (hidden files)
          arr[i][PARAM_FILE] = "/" + String(f.name());
          arr[i][PARAM_SIZE] = static_cast<int>(f.size());
          i++;
        }
        f = root.openNextFile();
      }
      f.close();
      root.close();

      response->setLength();
      request->send(response);
    } else if (obj[PARAM_COMMAND] == String("del")) {
      Log.notice(F("WEB : File system delete requested." CR));

      if (!obj[PARAM_FILE].isNull()) {
        String f = obj[PARAM_FILE];
        mySdStorage.remove(f);
        request->send(200);
      } else {
        request->send(400);
      }
    } else {
      Log.warning(F("WEB : Unknown file system command." CR));
      request->send(400);
    }
  } else {
    Log.warning(F("WEB : Unknown file system command." CR));
    request->send(400);
  }
#endif
}

void GatewayWebServer::webHandleMdns(AsyncWebServerRequest *request) {
  if (!isAuthenticated(request)) {
    return;
  }

  Log.notice(F("WEB : webServer callback for /api/hardware." CR));
  AsyncJsonResponse *response = new AsyncJsonResponse(false);
  JsonObject obj = response->getRoot().as<JsonObject>();
  myMdnsScanner.populateJson(obj);
  response->setLength();
  request->send(response);
}

// void GatewayWebServer::doTaskHardwareScanning(JsonObject &obj) {
// }

#endif  // GATEWAY

// EOF
