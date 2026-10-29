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

#include <WiFi.h>

#include <config_gateway.hpp>
#include <helper.hpp>
#include <main.hpp>
#include <main_gateway.hpp>
#include <push_gateway.hpp>
#include <pushtarget.hpp>

namespace {
constexpr auto TPL_GATEWAY_IP_ADDRESS = "${ip-address}";
constexpr auto TPL_GATEWAY_RUN_MODE = "${run-mode}";
constexpr auto TPL_HTTP_POST_SLEEP_INTERVAL = "${http-post-sleep-interval}";
constexpr auto TPL_BLE_SLEEP_INTERVAL = "${ble-sleep-interval}";
}  // namespace

void setupTemplateEngineGravityGateway(GravmonGatewayConfig* config,
                                       TemplatingEngine& engine, float angle,
                                       float gravitySG, float tempC,
                                       float voltage, int interval,
                                       const char* id, const char* token,
                                       const char* name,
                                       int httpPostSleepInterval,
                                       int bleSleepInterval,
                                       const GatewayDeviceMetadata&
                                           deviceMetadata) {
  float corrGravitySG = gravitySG;

  // Names
  engine.setVal(TPL_MDNS, strlen(name) ? name : config->getMDNS());
  engine.setVal(TPL_ID, id);
  engine.setVal(TPL_TOKEN, strlen(token) ? token : config->getToken());
  engine.setVal(TPL_TOKEN2, strlen(token) ? token : config->getToken());

  // Temperature
  if (config->isTempFormatC()) {
    engine.setVal(TPL_TEMP, tempC, DECIMALS_TEMP);
  } else {
    engine.setVal(TPL_TEMP, convertCtoF(tempC), DECIMALS_TEMP);
  }

  engine.setVal(TPL_TEMP_C, tempC, DECIMALS_TEMP);
  engine.setVal(TPL_TEMP_F, convertCtoF(tempC), DECIMALS_TEMP);
  engine.setVal(TPL_TEMP_UNITS, config->getTempFormat());

  // Battery & Timer
  engine.setVal(TPL_BATTERY, voltage, DECIMALS_BATTERY);
  engine.setVal(TPL_SLEEP_INTERVAL, interval);

  int charge = 0;

  if (voltage > 4.15)
    charge = 100;
  else if (voltage > 4.05)
    charge = 90;
  else if (voltage > 3.97)
    charge = 80;
  else if (voltage > 3.91)
    charge = 70;
  else if (voltage > 3.86)
    charge = 60;
  else if (voltage > 3.81)
    charge = 50;
  else if (voltage > 3.78)
    charge = 40;
  else if (voltage > 3.76)
    charge = 30;
  else if (voltage > 3.73)
    charge = 20;
  else if (voltage > 3.67)
    charge = 10;
  else if (voltage > 3.44)
    charge = 5;

  engine.setVal(TPL_BATTERY_PERCENT, charge);

  // Performance metrics
  engine.setVal(TPL_RUN_TIME, deviceMetadata.runTime(), DECIMALS_RUNTIME);
  engine.setVal(TPL_RSSI, WiFi.RSSI());

  // Angle/Tilt
  engine.setVal(TPL_TILT, angle, DECIMALS_TILT);
  engine.setVal(TPL_ANGLE, angle, DECIMALS_TILT);
  engine.setVal(TPL_VELOCITY, 0, 1);

  // Gravity options
  if (config->isGravitySG()) {
    engine.setVal(TPL_GRAVITY, gravitySG, DECIMALS_SG);
    engine.setVal(TPL_GRAVITY_CORR, corrGravitySG, DECIMALS_SG);
  } else {
    engine.setVal(TPL_GRAVITY, convertToPlato(gravitySG), DECIMALS_PLATO);
    engine.setVal(TPL_GRAVITY_CORR, convertToPlato(corrGravitySG),
                  DECIMALS_PLATO);
  }

  engine.setVal(TPL_GRAVITY_G, gravitySG, DECIMALS_SG);
  engine.setVal(TPL_GRAVITY_P, convertToPlato(gravitySG), DECIMALS_PLATO);
  engine.setVal(TPL_GRAVITY_CORR_G, corrGravitySG, DECIMALS_SG);
  engine.setVal(TPL_GRAVITY_CORR_P, convertToPlato(corrGravitySG),
                DECIMALS_PLATO);
  engine.setVal(TPL_GRAVITY_UNIT, config->getGravityUnit());

  engine.setVal(TPL_APP_VER, deviceMetadata.appVersion());
  engine.setVal(TPL_APP_BUILD, CFG_GITREV);
  engine.setVal(TPL_GATEWAY_IP_ADDRESS, deviceMetadata.ipAddress());
  engine.setVal(TPL_GATEWAY_RUN_MODE, deviceMetadata.runMode());
  engine.setVal(TPL_HTTP_POST_SLEEP_INTERVAL, httpPostSleepInterval);
  engine.setVal(TPL_BLE_SLEEP_INTERVAL, bleSleepInterval);
}

void setupTemplateEnginePressureGateway(GravmonGatewayConfig* config,
                                        TemplatingEngine& engine,
                                        float pressurePsi, float pressurePsi1,
                                        float tempC, float voltage,
                                        int interval, const char* id,
                                        const char* token, const char* name,
                                        int httpPostSleepInterval,
                                        int bleSleepInterval) {
  float runTime = 0;

  // Names
  engine.setVal(TPL_MDNS, strlen(name) ? name : config->getMDNS());
  engine.setVal(TPL_ID, id);
  engine.setVal(TPL_TOKEN, strlen(token) ? token : config->getToken());
  engine.setVal(TPL_TOKEN2, strlen(token) ? token : config->getToken());

  // Temperature
  if (config->isTempFormatC()) {
    engine.setVal(TPL_TEMP, tempC, DECIMALS_TEMP);
  } else {
    engine.setVal(TPL_TEMP, convertCtoF(tempC), DECIMALS_TEMP);
  }

  engine.setVal(TPL_TEMP_C, tempC, DECIMALS_TEMP);
  engine.setVal(TPL_TEMP_F, convertCtoF(tempC), DECIMALS_TEMP);
  engine.setVal(TPL_TEMP_UNITS, config->getTempFormat());

  // Battery & Timer
  engine.setVal(TPL_BATTERY, voltage, DECIMALS_BATTERY);
  engine.setVal(TPL_SLEEP_INTERVAL, interval);

  int charge = 0;

  if (voltage > 4.15)
    charge = 100;
  else if (voltage > 4.05)
    charge = 90;
  else if (voltage > 3.97)
    charge = 80;
  else if (voltage > 3.91)
    charge = 70;
  else if (voltage > 3.86)
    charge = 60;
  else if (voltage > 3.81)
    charge = 50;
  else if (voltage > 3.78)
    charge = 40;
  else if (voltage > 3.76)
    charge = 30;
  else if (voltage > 3.73)
    charge = 20;
  else if (voltage > 3.67)
    charge = 10;
  else if (voltage > 3.44)
    charge = 5;

  engine.setVal(TPL_BATTERY_PERCENT, charge);

  // Performance metrics
  engine.setVal(TPL_RUN_TIME, runTime, DECIMALS_RUNTIME);
  engine.setVal(TPL_RSSI, WiFi.RSSI());

  engine.setVal(TPL_PRESSURE_PSI, pressurePsi, DECIMALS_PRESSURE);
  engine.setVal(TPL_PRESSURE1_PSI, pressurePsi1, DECIMALS_PRESSURE);
  engine.setVal(TPL_PRESSURE_BAR, convertPsiPressureToBar(pressurePsi),
                DECIMALS_PRESSURE);
  engine.setVal(TPL_PRESSURE1_BAR, convertPsiPressureToBar(pressurePsi1),
                DECIMALS_PRESSURE);
  engine.setVal(TPL_PRESSURE_KPA, convertPsiPressureToKPa(pressurePsi),
                DECIMALS_PRESSURE);
  engine.setVal(TPL_PRESSURE_KPA, convertPsiPressureToKPa(pressurePsi1),
                DECIMALS_PRESSURE);

  if (config->isPressureBar()) {
    engine.setVal(TPL_PRESSURE, convertPsiPressureToBar(pressurePsi),
                  DECIMALS_PRESSURE);
    engine.setVal(TPL_PRESSURE1, convertPsiPressureToBar(pressurePsi1),
                  DECIMALS_PRESSURE);
  } else if (config->isPressureKpa()) {
    engine.setVal(TPL_PRESSURE, convertPsiPressureToKPa(pressurePsi),
                  DECIMALS_PRESSURE);
    engine.setVal(TPL_PRESSURE1, convertPsiPressureToKPa(pressurePsi1),
                  DECIMALS_PRESSURE);
  } else {
    engine.setVal(TPL_PRESSURE, pressurePsi, DECIMALS_PRESSURE);
    engine.setVal(TPL_PRESSURE1, pressurePsi1, DECIMALS_PRESSURE);
  }

  engine.setVal(TPL_PRESSURE_UNIT, config->getPressureUnit());

  engine.setVal(TPL_APP_VER, CFG_APPVER);
  engine.setVal(TPL_APP_BUILD, CFG_GITREV);
  engine.setVal(TPL_GATEWAY_IP_ADDRESS, gatewayIpAddress());
  engine.setVal(TPL_GATEWAY_RUN_MODE, gatewayRunModeName());
  engine.setVal(TPL_HTTP_POST_SLEEP_INTERVAL, httpPostSleepInterval);
  engine.setVal(TPL_BLE_SLEEP_INTERVAL, bleSleepInterval);
}

#endif  // GATEWAY

// EOF
