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

// #define CREATE_TESTDATA

#include <WiFi.h>
#include <LittleFS.h>
#include <esp_core_dump.h>

#include <battery.hpp>
#include <ble_gateway.hpp>
#include <config_gateway.hpp>
#include <cuckoo_pi_alarm.hpp>
#include <cuckoo_pi_mqtt.hpp>
#include <gateway_display_cycle.hpp>
#include <cstdio>
#include <deque>
#include <display.hpp>
#include <gateway_alarm.hpp>
#include <gateway_alarm_lifecycle.hpp>
#include <gateway_alert_state.hpp>
#include <gateway_audio.hpp>
#include <gateway_default_templates.hpp>
#include <gateway_runtime_policy.hpp>
#include <gmt154_build_version.hpp>
#include <waveshare_boot_identity.hpp>
#include <gmt154_heap_diagnostics.hpp>
#include <gmt154_ap_diagnostics_policy.hpp>
#include <helper.hpp>
#include <led.hpp>
#include <log.hpp>
#include <looptimer.hpp>
#include <main.hpp>
#include <main_gateway.hpp>
#include <mdns_discovery.hpp>
#include <measurement.hpp>
#include "gmt154_offline_frame.hpp"
#include <memory>
#include <push_gateway.hpp>
#include <pushtarget.hpp>
#include <sdcard_mmc.hpp>
#include <sdcard_sd.hpp>
#include <serialws.hpp>
#include <stdarg.h>
#include <uptime.hpp>
#include <utils.hpp>
#include <web_gateway.hpp>
#include <wificonnection.hpp>
#include <ui_gravitymon_gateway.hpp>

constexpr auto CFG_FILENAME = "/gravitymon-gw.json";
constexpr auto CFG_AP_SSID = "Gateway";
constexpr auto CFG_AP_PASS = "password";

#if !defined(USER_SSID)
#define USER_SSID ""
#define USER_PASS ""
#endif

void controller();
void updateDisplayStatus();
void updateDisplayLogs();
void checkSleepMode(float angle, float volt);
void gestureLeft();
void gestureRight();
void processGatewayAlarms();

#if defined(ARDUINO_ARCH_ESP32)
void traceAlarmEvent(const char* format, ...) {
#if defined(ENABLE_MMC) || defined(ENABLE_SD)
  if (!mySdStorage.hasCard()) return;
  char line[160];
  va_list args;
  va_start(args, format);
  vsnprintf(line, sizeof(line), format, args);
  va_end(args);
  char out[200];
  snprintf(out, sizeof(out), "%lu %s\r\n",
           (unsigned long)(millis() / 1000UL), line);
  fs::File f = mySdStorage.open("/alarm_trace.log", "a");
  if (f) {
    f.print(out);
    f.close();
  }
#else
  (void)format;
#endif
}
#endif

void checkCrashReason();

void ensureGatewayDefaultTemplates() {
  const GatewayDefaultTemplateResult result =
      ensureGatewayHttpPost1DefaultTemplate(
          [](const char* path) {
            File file = LittleFS.open(path, "r");
            if (!file) return false;
            const bool hasContent = file.size() > 0;
            file.close();
            return hasContent;
          },
          [](const char* path, const char* data, size_t size) {
            File file = LittleFS.open(path, "w");
            if (!file) return false;
            const size_t written = file.write(
                reinterpret_cast<const uint8_t*>(data), size);
            file.flush();
            file.close();
            if (written != size) LittleFS.remove(path);
            return written == size;
          });

  if (result == GatewayDefaultTemplateResult::Created) {
    Log.notice(F("CFG : Installed CuckooTilt HTTP Post #1 template." CR));
  } else if (result == GatewayDefaultTemplateResult::WriteFailed) {
    Log.error(F("CFG : Failed to install CuckooTilt HTTP Post #1 template."
                CR));
  }
}

SerialDebug mySerial;
GravmonGatewayConfig myConfig(CFG_APPNAME, CFG_FILENAME);
WifiConnection myWifi(&myConfig, CFG_AP_SSID, CFG_AP_PASS, CFG_APPNAME,
                      USER_SSID, USER_PASS);
GatewayWebServer myWebServer(&myConfig);
SerialWebSocket mySerialWebSocket;
Display myDisplay;
BatteryVoltage myBatteryVoltage(
    &myConfig);  // Needs to be defined but not used in gateway
MdnsScanner myMdnsScanner(300000);  // Scan every 5 minutes (blocking ~9s; loop WDT unsubscribed during scan)
MeasurementList myMeasurementList;  // Data recevied from http or bluetooth
LoopTimer controllerTimer(20 *
                          1000);  // For handling push and other periodic tasks
LoopTimer cycleTimer(4 * 1000);   // Cycle through the devices on the display
LoopTimer displayTimer(100);      // Process text updates for display
LoopTimer sdTimer(30 * 1000);     // Check if there is an SD card attached

bool sleepModeAlwaysSkip =
    false;  // Needs to be defined but not used in gateway
RunMode runMode = RunMode::measurementMode;

const char* gatewayBuildDateTime() {
  static char timestamp[17]{};
  waveshare_boot::formatBuildDateTime(timestamp, __DATE__, __TIME__);
  return timestamp;
}

const char* gatewayRunModeName() {
  switch (runMode) {
    case RunMode::measurementMode:
      return "measurement";
    case RunMode::wifiSetupMode:
      return "wifi_setup";
  }
  return "unknown";
}

String gatewayIpAddress() {
  if (runMode == RunMode::wifiSetupMode) return WiFi.softAPIP().toString();
  return WiFi.localIP().toString();
}

const auto MAX_LOG_ENTRIES =
    5;  // Max number of events to save (what can be displayed)
std::deque<String> logEntryList;  // Last number of events
bool logUpdated = true;           // If the history log should be updated
int displayMeasurementIndex =
    0;  // What entry is shown on the top of the display

struct ReadingGenerationStamp {
  bool in_use = false;
  char device_id[GRAVITY_DEVICE_ID_CAPACITY] = {};
  uint32_t generation = 0;
};

struct CuckooAlarmStamp {
  bool in_use = false;
  char device_id[13] = {};
  uint32_t configured_since_ms = 0;
  CuckooPiAlarmState state{};
};

GatewayAlarmTracker gatewayAlarmTracker;
GatewayAlertState gatewayAlertState;
ReadingGenerationStamp alarmReadingGenerations[MAX_GRAVITY_DEVICE_SETTINGS] =
    {};
CuckooAlarmStamp cuckooAlarmStamps[kMaxCuckooPiDevices] = {};

static CuckooPiConfig currentDisplayCuckooConfig() {
  CuckooPiConfig config = defaultCuckooPiConfig();
  myConfig.copyCuckooPiConfig(&config);
  return config;
}

static bool currentCuckooDeviceOnline(const CuckooPiDeviceConfig& settings,
                                      const CuckooPiRuntimeDevice& runtime,
                                      uint32_t now_ms) {
  if (!runtime.has_snapshot) return false;
  const uint64_t timeout_ms =
      static_cast<uint64_t>(settings.offline_timeout_seconds) * 1000ULL;
  return static_cast<uint64_t>(static_cast<uint32_t>(now_ms - runtime.last_seen_ms)) <
         timeout_ms;
}
#if defined(ENABLE_MMC)
SdCardMMC mySdStorage;
#elif defined(ENABLE_SD)
SdCardSD mySdStorage;
#endif

void setup() {
  // Main startup
  Log.notice(F("DIAG: MAIN stage=setup.begin." CR));
  delay(2000);

  Log.notice(F("Main: Started setup for %s." CR), myConfig.getID());
#if defined(CFG_SOURCE_BUILD_ID)
  Log.notice(F("Main: Source build ID %s." CR), CFG_SOURCE_BUILD_ID);
#endif
  printBuildOptions();
  detectChipRevision();

#if defined(ENABLE_TFT)
  Log.notice(F("Main: TOUCH_CS %d." CR), TOUCH_CS);
#ifdef TFT_BL
  Log.notice(F("Main: TFT_BL %d." CR), TFT_BL);
#endif
  Log.notice(F("Main: TFT_DC %d." CR), TFT_DC);
  Log.notice(F("Main: TFT_MISO %d." CR), TFT_MISO);
  Log.notice(F("Main: TFT_MOSI %d." CR), TFT_MOSI);
  Log.notice(F("Main: TFT_SCLK %d." CR), TFT_SCLK);
  Log.notice(F("Main: TFT_RST %d." CR), TFT_RST);
  Log.notice(F("Main: TFT_CS %d." CR), TFT_CS);
#endif

  Log.notice(F("Main: Initialize display." CR));
  Log.notice(F("DIAG: MAIN stage=display.init.begin." CR));
  myDisplay.setup();
  Log.notice(F("DIAG: MAIN stage=display.init.done." CR));
#if defined(GATEWAY_35)
  myDisplay.setFont(FontSize::FONT_18);
#else
  myDisplay.setFont(FontSize::FONT_12);
#endif
#if defined(WAVESHARE_S3_TFT) || defined(WAVESHARE_S3_TFT_35) || \
    (defined(GMT154_06) && !defined(GMT154_06_C3))
  constexpr int kStartupStatusLineOffset = 1;
  char buildDateTime[17]{};
  waveshare_boot::formatBuildDateTime(buildDateTime, __DATE__, __TIME__);
  buildDateTime[10] = '\0';
#if defined(WAVESHARE_S3_TFT_35)
  myDisplay.printLineCentered(1, "WAVESHARE 3.5");
#elif defined(WAVESHARE_S3_TFT)
  myDisplay.printLineCentered(1, "WAVESHARE 2.8");
#else
  myDisplay.printLineCentered(1, "GMT154-06 S3");
#endif
  myDisplay.printLineCentered(2, buildDateTime);
  myDisplay.printLineCentered(3, buildDateTime + 11);
#else
  constexpr int kStartupStatusLineOffset = 0;
  myDisplay.printLineCentered(1, "GravityMon Gateway");
#endif
  myDisplay.printLineCentered(3 + kStartupStatusLineOffset, "Starting");
#if defined(GMT154_AUDIO_STD_INIT_DIAG) && GMT154_AUDIO_STD_INIT_DIAG
  Log.warning(F("DIAG: C2B standard-mode I2S init diagnostic firmware." CR));
  myDisplay.printLineCentered(5 + kStartupStatusLineOffset, "C2B DIAGNOSTIC");
#endif
#if defined(GMT154_AUDIO_STD_ENABLE_DIAG) && GMT154_AUDIO_STD_ENABLE_DIAG
  Log.warning(F("DIAG: C2C standard-mode I2S enable diagnostic firmware." CR));
  myDisplay.printLineCentered(5 + kStartupStatusLineOffset, "C2C DIAGNOSTIC");
#endif
#if defined(GMT154_06)
  const std::string gmt154BuildVersion =
      gmt154::formatBuildVersion(__DATE__, __TIME__);
  Log.notice(F("Main: Firmware version %s." CR), gmt154BuildVersion.c_str());
#if defined(GMT154_06_C3)
  myDisplay.setFont(FontSize::FONT_9);
  myDisplay.printLineCentered(6, gmt154BuildVersion.c_str());
  myDisplay.setFont(FontSize::FONT_12);
#endif
#endif

  Log.notice(F("DIAG: MAIN stage=filesystem.init.begin." CR));
  myConfig.checkFileSystem();
  ensureGatewayDefaultTemplates();
  Log.notice(F("DIAG: MAIN stage=filesystem.init.done." CR));
  Log.notice(F("DIAG: MAIN stage=wifi.init.begin." CR));
  myWifi.init();  // double reset check
  Log.notice(F("DIAG: MAIN stage=wifi.init.done." CR));
  checkResetReason();
  checkCrashReason();
  Log.notice(F("DIAG: MAIN stage=config.load.begin." CR));
  if (!myConfig.recoverConfigFile()) {
    Log.error(F("CFG : Config recovery failed; load/save disabled." CR));
  } else {
    myConfig.loadFile();
  }
  Log.notice(F("DIAG: MAIN stage=config.load.done." CR));
  myConfig.setWifiScanAP(true);

#if defined(ENABLE_MMC)
  myDisplay.printLineCentered(3 + kStartupStatusLineOffset, "Mounting SD (SD_MMC) card");
  Log.notice(F("Main: MMC_CLK %d." CR), MMC_CLK);
  Log.notice(F("Main: MMC_CMD %d." CR), MMC_CMD);
  Log.notice(F("Main: MMC_D0 %d." CR), MMC_D0);
  mySdStorage.begin(MMC_CLK, MMC_CMD, MMC_D0);
#endif

#if defined(ENABLE_SD)
  myDisplay.printLineCentered(3 + kStartupStatusLineOffset, "Mounting SD (SD) card");
#if defined(ENABLE_TFT)
  mySdStorage.begin(SD_CS, myDisplay.getSPI());
#else
  mySdStorage.begin(SD_CS, SPI);
#endif  // ENABLE_TFT
  mySdStorage.remove("/alarm_trace.log");
#endif  // ENABLE_SD

  // No stored config, move to portal
  if (!myWifi.hasConfig()) {
    Log.notice(
        F("Main: No wifi configuration detected, entering wifi setup." CR));
    runMode = RunMode::wifiSetupMode;
  }

  // Double reset, go to portal.
  if (myWifi.isDoubleResetDetected()) {
    Log.notice(F("Main: Double reset detected, entering wifi setup." CR));
    runMode = RunMode::wifiSetupMode;
  }

  if (runMode == RunMode::wifiSetupMode) {
    Log.notice(F("DIAG: MAIN stage=runmode.result=wifi_setup." CR));
  } else {
    Log.notice(F("DIAG: MAIN stage=runmode.result=measurement." CR));
  }

#if defined(GMT154_06_C3)
  bool wifiConnected = false;
#endif

  // Do this setup for all modes exect wifi setup
  switch (runMode) {
    case RunMode::wifiSetupMode:
      Log.notice(F("Main: Entering WIFI Setup." CR));
      myDisplay.printLineCentered(3 + kStartupStatusLineOffset, "Entering WIFI Setup");
      myWifi.enableImprov(true);
      Log.notice(F("DIAG: MAIN stage=ap.entry=wifi_setup." CR));
      Log.notice(F("DIAG: MAIN stage=wifi.startAP.begin." CR));
      myWifi.startAP();
      Log.notice(F("DIAG: MAIN stage=wifi.startAP.done." CR));
      break;

    case RunMode::measurementMode:
      Log.notice(F("Main: Connecting to WIFI." CR));
      myDisplay.printLineCentered(3 + kStartupStatusLineOffset, "Connecting to WIFI");

      if (strlen(myConfig.getWifiDirectSSID())) {
        myDisplay.printLineCentered(4 + kStartupStatusLineOffset, "Creating AP");
        Log.notice(F("Main: Connecting to WIFI and creating AP." CR));
        Log.notice(F("DIAG: MAIN stage=ap.entry=direct_ssid." CR));
        Log.notice(F("DIAG: MAIN stage=wifi.connect.begin." CR));
#if defined(GMT154_06_C3)
        myWifi.connect(false, WIFI_AP_STA);
        wifiConnected = myWifi.isConnected();
#else
        myWifi.connect(false, WIFI_AP_STA);
#endif
        Log.notice(F("DIAG: MAIN stage=wifi.connect.done." CR));
#if defined(GMT154_06_C3)
        if (wifiConnected) {
          myWifi.setAP(myConfig.getWifiDirectSSID(),
                       myConfig.getWifiDirectPass());
          Log.notice(F("DIAG: MAIN stage=wifi.startAP.begin." CR));
          myWifi.startAP(WIFI_AP_STA);
          Log.notice(F("DIAG: MAIN stage=wifi.startAP.done." CR));
        }
#else
        myWifi.setAP(myConfig.getWifiDirectSSID(),
                     myConfig.getWifiDirectPass());
        Log.notice(F("DIAG: MAIN stage=wifi.startAP.begin." CR));
        myWifi.startAP(WIFI_AP_STA);
        Log.notice(F("DIAG: MAIN stage=wifi.startAP.done." CR));
#endif
      } else {
        Log.notice(F("DIAG: MAIN stage=wifi.connect.begin." CR));
#if defined(GMT154_06_C3)
        myWifi.connect(false, WIFI_AP);
        wifiConnected = myWifi.isConnected();
#else
        myWifi.connect(false, WIFI_AP);
#endif
        Log.notice(F("DIAG: MAIN stage=wifi.connect.done." CR));
      }
#if defined(GMT154_06_C3)
      if (!wifiConnected) {
        Log.error(F("Main: Initial WIFI connection failed; entering setup portal." CR));
        runMode = RunMode::wifiSetupMode;
        myDisplay.printLineCentered(3 + kStartupStatusLineOffset, "Entering WIFI Setup");
        myWifi.enableImprov(true);
        Log.notice(F("DIAG: MAIN stage=wifi.fallback=setup_portal." CR));
        Log.notice(F("DIAG: MAIN stage=wifi.startAP.begin." CR));
        myWifi.startAP();
        Log.notice(F("DIAG: MAIN stage=wifi.startAP.done." CR));
      } else {
        myDisplay.calibrateTouch();
      }
#else
      myDisplay.calibrateTouch();
#endif
      break;
  }

  gmt154LogHeapSnapshot("after_wifi");
  gmt154StartApDiagnostics();

  // Do this setup for configuration mode
  switch (runMode) {
    case RunMode::measurementMode:
      if (myWifi.isConnected()) {
        Log.notice(F("Main: Activating web server." CR));
        ledOn(LedColor::BLUE);  // Blue or slow flashing to indicate config mode
        Log.notice(F("Main: Synchronizing time." CR));
        myDisplay.printLineCentered(3 + kStartupStatusLineOffset, "Synchronizing time");
        myDisplay.printLineCentered(4 + kStartupStatusLineOffset, "");
        myWifi.timeSync(myConfig.getTimezone());

        case RunMode::wifiSetupMode:
          Log.notice(F("Main: Initializing the web server." CR));
          myWebServer.setupWebServer(
              "gravitymon-gateway");  // Takes less than 4ms, so skip
                                      // this measurement
          mySerialWebSocket.begin(myWebServer.getWebServer(), &Serial);
          mySerial.begin(&mySerialWebSocket);
      } else {
        Log.error(F("Main: Failed to connect with WIFI." CR));
        ledOn(LedColor::RED);  // Red or fast flashing to indicate connection
                               // error
        myDisplay.printLineCentered(5 + kStartupStatusLineOffset, "Failed to connect with WIFI");
      }
      break;

    default:
      break;
  }

  gmt154LogHeapSnapshot("before_ble");
  if (runMode == RunMode::measurementMode && myConfig.isBleEnable()) {
    myDisplay.printLineCentered(3 + kStartupStatusLineOffset, "Setting up BLE scanner");
    Log.notice(F("Main: Initialize ble scanner." CR));
    bleScanner.setScanTime(myConfig.getBleScanTime());
    bleScanner.setAllowActiveScan(myConfig.getBleActiveScan());
#if defined(GMT154_06) && defined(GMT154_06_BLE_INIT_OFF_DIAG) && \
    GMT154_06_BLE_INIT_OFF_DIAG == 1
    Log.notice(F("DIAG: GMT154 BLE scanner init skipped." CR));
#else
    bleScanner.init();
#endif
  }

  gmt154LogHeapSnapshot("after_ble");
  myMdnsScanner.setup();

  Log.notice(F("Main: Startup completed." CR));
#if defined(ENABLE_TFT)
  Log.notice(F("DIAG: MAIN stage=ui.init.begin." CR));
  myDisplay.printLineCentered(3 + kStartupStatusLineOffset, "Startup completed");
  myDisplay.setFont(FontSize::FONT_9);
  delay(1000);
  myDisplay.createUI(myConfig.getDisplayLayoutId());
  Log.notice(F("DIAG: MAIN stage=ui.init.done." CR));
#endif
#if defined(ENABLE_GATEWAY_AUDIO)
  gmt154LogHeapSnapshot("before_audio");
  gatewayAudio.begin();
  gmt154LogHeapSnapshot("after_audio");
#endif
  cuckooPiMqttService.begin();

  // Restore persisted offline-alarm dismissals (CLOSE) after reboot so a
  // still-offline device does not re-alarm. Cleared when the device reports
  // data again.
  {
    const uint8_t deviceCount = myConfig.getDeviceSettingsCount();
    for (uint8_t i = 0; i < deviceCount; ++i) {
      GravityDeviceSettings settings{};
      if (myConfig.copyDeviceSettingsByIndex(i, &settings) &&
          settings.offline_alarm_dismissed) {
        gatewayAlarmTracker.restoreOfflineDismissed(settings.device_id);
        Log.notice(F("ALM : restored offline dismissal dev=%s." CR),
                   settings.device_id);
        traceAlarmEvent("restore-dismissed dev=%s", settings.device_id);
      }
    }
  }

#if defined(CREATE_TESTDATA)
  std::unique_ptr<MeasurementBaseData> gravityData1;
  gravityData1.reset(new GravityData(MeasurementSource::HttpPost, "GRAV001",
                                     "grav-1", "token1", 22.1, 1.045, 45.2,
                                     3.78, 0, -67, 900));
  myMeasurementList.updateData(gravityData1);

  std::unique_ptr<MeasurementBaseData> pressureData1;
  pressureData1.reset(new PressureData(MeasurementSource::HttpPost, "PRESS01",
                                       "press-1", "token", 10.2, 15.13, 16.12,
                                       3.58, 0, -78, 900));
  myMeasurementList.updateData(pressureData1);

  std::unique_ptr<MeasurementBaseData> gravityData2;
  gravityData2.reset(new RaptData(MeasurementSource::BleBeacon, "RAPT01", 10.2,
                                  1.025, 3.2, 40, 30.1, 50, -78));
  myMeasurementList.updateData(gravityData2);

  std::unique_ptr<MeasurementBaseData> gravityData3;
  gravityData3.reset(new GravityData(MeasurementSource::HttpPost, "GRAV002",
                                     "grav-3", "token3", 14.2, 1.085, 67.2,
                                     4.08, 0, -74, 600));
  myMeasurementList.updateData(gravityData3);

  std::unique_ptr<MeasurementBaseData> tiltData1;
  tiltData1.reset(new TiltData(MeasurementSource::BleBeacon, TiltColor::Red,
                               14.2, 1.085, 10, -72, false));
  myMeasurementList.updateData(tiltData1);

  std::unique_ptr<MeasurementBaseData> tiltData2;
  tiltData2.reset(new TiltData(MeasurementSource::BleBeacon, TiltColor::Blue,
                               14.2, 1.085, 10, -72, true));
  myMeasurementList.updateData(tiltData2);

  std::unique_ptr<MeasurementBaseData> chamberData1;
  chamberData1.reset(new ChamberData(MeasurementSource::BleBeacon, "CHAM01",
                                     "Chamber01", 14.2, 18.3, 10, -72));
  myMeasurementList.updateData(chamberData1);

  std::unique_ptr<MeasurementBaseData> raptData1;
  raptData1.reset(new RaptData(MeasurementSource::BleBeacon, "RAPT02", 15.2,
                               1.030, 1.2, 35.33, 40, 10, -72));
  myMeasurementList.updateData(raptData1);

  myDisplay.updateHistory("Line 1", 0);
  myDisplay.updateHistory("Line 2", 1);
  myDisplay.updateHistory("Line 3", 2);
  myDisplay.updateHistory("Line 4", 3);
  myDisplay.updateHistory("Line 5", 4);
#endif

  updateDisplayStatus();
  Log.notice(F("DIAG: MAIN stage=setup.done." CR));
  gmt154LogHeapSnapshot("setup_done");
}

void loop() {
  gmt154PollHeapDiagnostics();
  myUptime.calculate();
  myWebServer.loop();
  mySerialWebSocket.loop();
  myWifi.loop();
  CuckooPiConfig cuckoo_config{};
  if (myConfig.copyCuckooPiConfig(&cuckoo_config)) {
    cuckooPiMqttService.applyConfig(cuckoo_config);
    cuckooPiMqttService.tick(millis(), WiFi.status() == WL_CONNECTED);
  }
  bleScanner.loop(myConfig.getSdLogMinTime());
#if defined(GMT154_06_C3)
  if (gmt154::shouldDiscoverMdns(true, runMode == RunMode::measurementMode,
                                myWifi.isConnected())) {
    myMdnsScanner.loop();
  }
#else
  myMdnsScanner.loop();
#endif

  switch (runMode) {
    case RunMode::measurementMode:
      if (!myWifi.isConnected()) {
        Log.warning(F("Loop: Wifi was disconnected, trying to reconnect." CR));
        myWifi.connect();
      }
      controller();
      break;

    case RunMode::wifiSetupMode:
      break;
  }

  if (logUpdated) {
    updateDisplayLogs();
    logUpdated = false;
  }

  if (sdTimer.hasExpired()) {
    sdTimer.reset();
#if defined(ENABLE_MMC)
    if (!mySdStorage.hasCard()) {
      Log.notice(F("Loop: SD card not mounted, retry mounting." CR));
      mySdStorage.end();
      mySdStorage.begin(MMC_CLK, MMC_CMD, MMC_D0);
    }
#endif

#if defined(ENABLE_SD)
    if (!mySdStorage.hasCard()) {
      Log.notice(F("Loop: SD card not mounted, retry mounting." CR));
      mySdStorage.end();
#if defined(ENABLE_TFT)
      mySdStorage.begin(SD_CS, myDisplay.getSPI());
#else
      mySdStorage.begin(SD_CS, SPI);
#endif  // ENABLE_TFT
    }
#endif  // ENABLE_SD
  }

  cycleTimer.setInterval(10 * 1000UL);
  if (!myDisplay.isSettingsOpen() && cycleTimer.hasExpired()) {
    cycleTimer.reset();
    displayMeasurementIndex++;

    const CuckooPiConfig displayConfig = currentDisplayCuckooConfig();
    const size_t displayCount = gravitymon_gateway_get_layout() == 2
                                    ? gatewayDisplayItemCount(
                                          myMeasurementList.size(), displayConfig)
                                    : myMeasurementList.size();
    if (displayCount > 0 &&
        static_cast<size_t>(displayMeasurementIndex) >= displayCount) {
      displayMeasurementIndex = 0;
    }

#if defined(ENABLE_MMC) || defined(ENABLE_SD)
    // --- Log file rotation: allow up to 4 log files (log.txt, log1.txt,
    // log2.txt, log3.txt, log4.txt) ---
    const char* logBase = "/data";
    const char* logExt = ".csv";
    constexpr size_t maxLogFileSize =
        16 * 1024;  // bytes, can be changed at runtime
    char logFileName[40];
    snprintf(logFileName, sizeof(logFileName), "%s%s", logBase,
             logExt);  // /data.csv
    if (mySdStorage.hasCard()) {
      fs::File logFile = mySdStorage.open(logFileName, "r");
      if (logFile) {
        size_t logSize = logFile.size();
        logFile.close();
        if (logSize > maxLogFileSize) {
          // Rotate: data3.csv->data4.csv, data2.csv->data3.csv,
          // data1.csv->data2.csv, data.csv->data1.csv
          for (int i = myConfig.getSdLogFiles() - 1; i >= 1; --i) {
            char oldName[24], newName[24];
            snprintf(oldName, sizeof(oldName), "%s%d%s", logBase, i,
                     logExt);  // /data1.csv, /data2.csv, ...
            snprintf(newName, sizeof(newName), "%s%d%s", logBase, i + 1,
                     logExt);  // /data2.csv, /data3.csv, ...
            if (mySdStorage.exists(oldName)) {
              mySdStorage.remove(newName);  // Remove if exists
              if (mySdStorage.rename(oldName, newName)) {
                Log.notice(F("Loop: Log rotation: %s -> %s" CR), oldName,
                           newName);
              } else {
                Log.error(F("Loop: Log rotation failed: %s -> %s" CR), oldName,
                          newName);
              }
            }
          }
          // data.csv -> data1.csv
          char firstRotated[40];
          snprintf(firstRotated, sizeof(firstRotated), "%s1%s", logBase,
                   logExt);
          mySdStorage.remove(firstRotated);  // Remove if exists
          if (mySdStorage.rename(logFileName, firstRotated)) {
            Log.notice(F("Loop: Log rotation: %s -> %s" CR), logFileName,
                       firstRotated);
          } else {
            Log.error(F("Loop: Log rotation failed: %s -> %s" CR), logFileName,
                      firstRotated);
          }
        }
      }
    }
#endif
  }

  if (displayTimer.hasExpired()) {
  displayTimer.reset();

    processGatewayAlarms();
    updateDisplayStatus();

    const CuckooPiConfig displayConfig = currentDisplayCuckooConfig();
    const size_t displayCount = gravitymon_gateway_get_layout() == 2
                                    ? gatewayDisplayItemCount(
                                          myMeasurementList.size(), displayConfig)
                                    : myMeasurementList.size();

    if (displayCount == 0) {  // No data to display
      myDisplay.updateEmpty();

    } else {
      if (displayMeasurementIndex < 0 ||
          static_cast<size_t>(displayMeasurementIndex) >= displayCount)
        displayMeasurementIndex = 0;

      GatewayDisplayItem displayItem{};
      gatewayDisplayItemAt(static_cast<size_t>(displayMeasurementIndex),
                           myMeasurementList.size(), displayConfig,
                           &displayItem);
      if (displayItem.kind == GatewayDisplayItemKind::CuckooPi) {
        CuckooPiRuntimeDevice runtime{};
        CuckooPiSnapshot snapshot{};
        const CuckooPiDeviceConfig& device =
            displayConfig.devices[displayItem.index];
        snprintf(snapshot.device_id, sizeof(snapshot.device_id), "%s",
                 device.id);
        const bool found = cuckooPiMqttService.copyDeviceRuntime(
            device.id, &runtime);
        if (found && runtime.has_snapshot) snapshot = runtime.snapshot;
        myDisplay.updateCuckooPi(
            snapshot, static_cast<uint8_t>(displayMeasurementIndex + 1),
            static_cast<uint8_t>(displayCount),
            found && currentCuckooDeviceOnline(device, runtime, millis()));
      } else {
      MeasurementEntry* entry =
          myMeasurementList.getMeasurementEntry(displayItem.index);

      GravityDeviceSettings displaySettings{};
      myConfig.copyDeviceSettings(entry->getId().c_str(), &displaySettings);
      const bool displayOnline = !gmt154MeasurementOffline(
          entry->getUpdateAge(), displaySettings.offline_timeout_seconds);

      char t[20], v1[20], v2[20], v3[20], s[20];
      strftime(t, sizeof(t), "%Y-%m-%d %H:%M:%S", entry->getTimeinfoUpdated());

      switch (entry->getType()) {
        case MeasurementType::Gravitymon: {
          const GravityData* gd = entry->getGravityData();
          if (gd) {
            float temp = myConfig.isTempFormatF() ? convertCtoF(gd->getTempC())
                                                  : gd->getTempC();
            float gravity = myConfig.isGravityPlato()
                                ? convertToPlato(gd->getGravity())
                                : gd->getGravity();

            String name = strlen(gd->getName()) ? gd->getName()
                                                : myMdnsScanner.findDeviceByTxt(
                                                      "id", gd->getId(), true);
            if (!name.length()) name = gd->getId();

            myDisplay.updateGravity(
                gd->getId(), name.c_str(), displayMeasurementIndex + 1,
                myMeasurementList.size(), gd->getTypeAsString(),
                gd->getSourceAsString(), gd->getCreatedAsString(), gravity,
                myConfig.getGravityUnit(), temp, myConfig.getTempUnit(),
                gd->getBattery(),
                getBatteryPercentage(gd->getBattery(), BatteryType::LithiumIon),
                gd->getRssi(), displayOnline);
          }
        } break;
        case MeasurementType::Pressuremon: {
          const PressureData* pd = entry->getPressureData();
          if (pd) {
            float temp = NAN, pressure = NAN, pressure1 = NAN;

            if (!isnan(pd->getPressure())) {
              temp = myConfig.isTempFormatF() ? convertCtoF(pd->getTempC())
                                              : pd->getTempC();
            }

            if (!isnan(pd->getPressure())) {
              pressure = myConfig.isPressureBar()
                             ? convertPsiPressureToBar(pd->getPressure())
                         : myConfig.isPressureKpa()
                             ? convertPsiPressureToKPa(pd->getPressure())
                             : pd->getPressure();
            }

            if (!isnan(pd->getPressure1())) {
              pressure1 = myConfig.isPressureBar()
                              ? convertPsiPressureToBar(pd->getPressure1())
                          : myConfig.isPressureKpa()
                              ? convertPsiPressureToKPa(pd->getPressure1())
                              : pd->getPressure1();
            }

            String name = strlen(pd->getName()) ? pd->getName()
                                                : myMdnsScanner.findDeviceByTxt(
                                                      "id", pd->getId(), true);
            if (!name.length()) name = pd->getId();

            myDisplay.updatePressure(
                pd->getId(), name.c_str(), displayMeasurementIndex + 1,
                myMeasurementList.size(), pd->getTypeAsString(),
                pd->getSourceAsString(), pd->getCreatedAsString(), pressure,
                pressure1, myConfig.getPressureUnit(), temp,
                myConfig.getTempUnit(), pd->getBattery(),
                getBatteryPercentage(pd->getBattery(), BatteryType::LiPo),
                pd->getRssi());
          }
        } break;
        case MeasurementType::Chamber: {
          const ChamberData* cd = entry->getChamberData();
          if (cd) {
            float chamberTemp = NAN, beerTemp = NAN;

            if (!isnan(cd->getChamberTempC())) {
              chamberTemp = myConfig.isTempFormatF()
                                ? convertCtoF(cd->getChamberTempC())
                                : cd->getChamberTempC();
            }

            if (!isnan(cd->getBeerTempC())) {
              beerTemp = myConfig.isTempFormatF()
                             ? convertCtoF(cd->getBeerTempC())
                             : cd->getBeerTempC();
            }

            String name =
                myMdnsScanner.findDeviceByTxt("id", cd->getId(), true);
            if (!name.length()) name = cd->getId();

            myDisplay.updateTemperature(
                cd->getId(), name.c_str(), displayMeasurementIndex + 1,
                myMeasurementList.size(), cd->getTypeAsString(),
                cd->getSourceAsString(), cd->getCreatedAsString(), chamberTemp,
                beerTemp, myConfig.getTempUnit(), cd->getRssi());
          }
        } break;
        case MeasurementType::TiltPro:
        case MeasurementType::Tilt: {
          const TiltData* td = entry->getTiltData();
          if (td) {
            float temp = myConfig.isTempFormatF() ? convertCtoF(td->getTempC())
                                                  : td->getTempC();
            float gravity = myConfig.isGravityPlato()
                                ? convertToPlato(td->getGravity())
                                : td->getGravity();

            myDisplay.updateGravity(
                td->getId(), td->getId(), displayMeasurementIndex + 1,
                myMeasurementList.size(), td->getTypeAsString(),
                td->getSourceAsString(), td->getCreatedAsString(), gravity,
                myConfig.getGravityUnit(), temp, myConfig.getTempUnit(), NAN,
                NAN, td->getRssi(), displayOnline);
          }
        } break;
        case MeasurementType::Rapt: {
          const RaptData* rd = entry->getRaptData();
          if (rd) {
            float temp = myConfig.isTempFormatF() ? convertCtoF(rd->getTempC())
                                                  : rd->getTempC();
            float gravity = myConfig.isGravityPlato()
                                ? convertToPlato(rd->getGravity())
                                : rd->getGravity();

            String name =
                myMdnsScanner.findDeviceByTxt("id", rd->getId(), true);
            if (!name.length()) name = rd->getId();

            myDisplay.updateGravity(
                rd->getId(), name.c_str(), displayMeasurementIndex + 1,
                myMeasurementList.size(), rd->getTypeAsString(),
                rd->getSourceAsString(), rd->getCreatedAsString(), gravity,
                myConfig.getGravityUnit(), temp, myConfig.getTempUnit(), NAN,
                rd->getBatteryPercent(), rd->getRssi(), displayOnline);
          }
        } break;
      }
      }
    }
  }
#if defined(GMT154_06_C3)
  // The C3 loop task has higher priority than the LVGL task. A one-tick
  // block lets the lower-priority renderer run even when this loop is busy.
  delay(1);
#endif
}

static bool snapshotContainsId(const GatewayMeasurementSnapshot* snapshots,
                               size_t count, const char* deviceId) {
  for (size_t i = 0; i < count; ++i) {
    if (strcmp(snapshots[i].id, deviceId) == 0) return true;
  }
  return false;
}

static bool cuckooConfigContainsId(const CuckooPiConfig& config,
                                   const char* deviceId) {
  if (!config.enabled || !deviceId) return false;
  for (uint8_t index = 0; index < config.device_count &&
                          index < kMaxCuckooPiDevices;
       ++index) {
    if (strcmp(config.devices[index].id, deviceId) == 0) return true;
  }
  return false;
}

static CuckooAlarmStamp* findOrCreateCuckooAlarmStamp(const char* deviceId,
                                                       uint32_t nowMs) {
  for (CuckooAlarmStamp& stamp : cuckooAlarmStamps) {
    if (stamp.in_use && strcmp(stamp.device_id, deviceId) == 0) return &stamp;
  }
  for (CuckooAlarmStamp& stamp : cuckooAlarmStamps) {
    if (!stamp.in_use) {
      stamp = CuckooAlarmStamp{};
      stamp.in_use = true;
      memcpy(stamp.device_id, deviceId, strlen(deviceId) + 1);
      stamp.configured_since_ms = nowMs;
      return &stamp;
    }
  }
  return nullptr;
}

static ReadingGenerationStamp* findOrCreateGenerationStamp(
    const char* deviceId) {
  for (ReadingGenerationStamp& stamp : alarmReadingGenerations) {
    if (stamp.in_use && strcmp(stamp.device_id, deviceId) == 0) return &stamp;
  }
  for (ReadingGenerationStamp& stamp : alarmReadingGenerations) {
    if (!stamp.in_use) {
      stamp = ReadingGenerationStamp{};
      stamp.in_use = true;
      memcpy(stamp.device_id, deviceId, strlen(deviceId) + 1);
      return &stamp;
    }
  }
  return nullptr;
}

static void requestGatewayAlarmCleanup(GatewayAlarmType type,
                                       const char* deviceId) {
  if (!gatewayAlarmLifecycle.requestCleanup(type, deviceId)) {
    Log.warning(F("ALM : cleanup request rejected; type=%d device=%s." CR),
                static_cast<int>(type), deviceId == nullptr ? "" : deviceId);
  }
}

static GatewayAlarmLifecycleActionResult gatewayLifecycleResult(
    GatewayAudioCancelResult result) {
  if (result == GatewayAudioCancelResult::Retry)
    return GatewayAlarmLifecycleActionResult::Retry;
  if (result == GatewayAudioCancelResult::NotFound)
    return GatewayAlarmLifecycleActionResult::NotFound;
  return GatewayAlarmLifecycleActionResult::Completed;
}

static GatewayAlarmLifecycleActionResult gatewayLifecycleResult(
    GatewayAlertClearResult result) {
  if (result == GatewayAlertClearResult::Retry)
    return GatewayAlarmLifecycleActionResult::Retry;
  if (result == GatewayAlertClearResult::NotFound)
    return GatewayAlarmLifecycleActionResult::NotFound;
  return GatewayAlarmLifecycleActionResult::Completed;
}

static void serviceGatewayAlarmLifecycleActions() {
  for (size_t slot = 0; slot < GATEWAY_ALARM_LIFECYCLE_CAPACITY; ++slot) {
    GatewayAlarmLifecycleAction action{};
    if (!gatewayAlarmLifecycle.nextMainAction(slot, &action)) continue;

    GatewayAlarmLifecycleActionResult result =
        GatewayAlarmLifecycleActionResult::Rejected;
    switch (action.type) {
      case GatewayAlarmLifecycleActionType::ResetSession:
        result = gatewayLifecycleResult(gatewayAudio.resetKeySession(
            action.alarm_type, action.device_id,
            action.event.condition_token));
        break;
      case GatewayAlarmLifecycleActionType::PublishAlert:
#if defined(GMT154_06)
        result = GatewayAlarmLifecycleActionResult::Completed;
#else
        result = gatewayAlertState.publish(action.event)
                     ? GatewayAlarmLifecycleActionResult::Completed
                     : GatewayAlarmLifecycleActionResult::Rejected;
#endif
        break;
      case GatewayAlarmLifecycleActionType::QueueAudio:
        result = gatewayAudio.queueAlarm(action.event)
                     ? GatewayAlarmLifecycleActionResult::Completed
                     : GatewayAlarmLifecycleActionResult::Rejected;
        if (result == GatewayAlarmLifecycleActionResult::Rejected) {
          Log.warning(F("ALM : audio queue rejected; type=%d device=%s." CR),
                      static_cast<int>(action.alarm_type), action.device_id);
        }
        break;
      case GatewayAlarmLifecycleActionType::ClearAlertKey:
        result = gatewayLifecycleResult(gatewayAlertState.clearByKeyResult(
            action.alarm_type, action.device_id));
        break;
      case GatewayAlarmLifecycleActionType::CancelAudioKey:
        result = gatewayLifecycleResult(gatewayAudio.cancelByKeyResult(
            action.alarm_type, action.device_id));
        break;
      case GatewayAlarmLifecycleActionType::RetireExact:
        result = gatewayLifecycleResult(
            gatewayAudio.retireExact(action.event));
        break;
      case GatewayAlarmLifecycleActionType::None:
      case GatewayAlarmLifecycleActionType::CloseTrackerExact:
      case GatewayAlarmLifecycleActionType::CancelAudioExact:
        continue;
    }
    if (!gatewayAlarmLifecycle.applyResult(action, result)) {
      Log.warning(F("ALM : lifecycle result rejected; action=%d device=%s."
                    CR),
                  static_cast<int>(action.type), action.device_id);
    }
    traceAlarmEvent("lifecycle action=%d result=%d dev=%s", (int)action.type,
                    (int)result, action.device_id);
  }
}

static bool canScheduleGatewayAlarm(
    GatewayAlarmLifecycleReserveResult result) {
  return result == GatewayAlarmLifecycleReserveResult::ReservedNew ||
         result == GatewayAlarmLifecycleReserveResult::ReservedExisting;
}

static void scheduleCuckooAlarm(GatewayAlarmType type, const char* deviceId,
                                uint32_t nowMs) {
  const GatewayAlarmLifecycleReserveResult reservation =
      gatewayAlarmLifecycle.reserveScheduleKey(type, deviceId);
  if (!canScheduleGatewayAlarm(reservation)) {
    if (reservation == GatewayAlarmLifecycleReserveResult::Full) {
      Log.warning(F("ALM : Cuckoo lifecycle capacity full type=%d dev=%s." CR),
                  static_cast<int>(type), deviceId);
    }
    return;
  }
  const GatewayAlarmDecision decision =
      gatewayAlarmTracker.schedule(type, deviceId, nowMs);
  if (decision.should_play) {
    if (!gatewayAlarmLifecycle.retainScheduledEvent(decision.event)) {
      Log.warning(F("ALM : Cuckoo event not retained type=%d dev=%s." CR),
                  static_cast<int>(type), deviceId);
    }
  } else if (reservation == GatewayAlarmLifecycleReserveResult::ReservedNew) {
    gatewayAlarmLifecycle.releaseUnusedReservation(type, deviceId);
  }
}

void processGatewayAlarms() {
#if defined(GMT154_06) && defined(ENABLE_GATEWAY_AUDIO)
  struct AudibleUpdateGuard {
    bool active;
    AudibleUpdateGuard() : active(gatewayAlarmTracker.beginAudibleUpdate()) {}
    ~AudibleUpdateGuard() { if (active) gatewayAlarmTracker.endAudibleUpdate(); }
  } audibleUpdateGuard;
#endif
  static uint32_t lastAlarmPassMs = 0;
  const uint32_t passNow = millis();
  if (lastAlarmPassMs != 0 && passNow - lastAlarmPassMs > 3000U) {
    Log.warning(F("ALM : pipeline stall %d ms." CR),
                (int)(passNow - lastAlarmPassMs));
  }
  lastAlarmPassMs = passNow;

  serviceGatewayAlarmLifecycleActions();
  GatewayMeasurementSnapshot snapshots[GATEWAY_MEASUREMENT_MAX_ENTRIES] = {};
  const size_t count = myMeasurementList.copySnapshot(
      snapshots, GATEWAY_MEASUREMENT_MAX_ENTRIES);
  CuckooPiConfig cuckooConfig = defaultCuckooPiConfig();
  myConfig.copyCuckooPiConfig(&cuckooConfig);
  const char* retainedIds[GATEWAY_MEASUREMENT_MAX_ENTRIES +
                          kMaxCuckooPiDevices] = {};
  size_t retainedCount = 0;
  for (size_t i = 0; i < count; ++i) retainedIds[retainedCount++] = snapshots[i].id;
  if (cuckooConfig.enabled) {
    for (uint8_t index = 0; index < cuckooConfig.device_count &&
                            index < kMaxCuckooPiDevices;
         ++index) {
      retainedIds[retainedCount++] = cuckooConfig.devices[index].id;
    }
  }
  gatewayAlarmTracker.retainDevices(retainedIds, retainedCount);

  for (ReadingGenerationStamp& stamp : alarmReadingGenerations) {
    if (stamp.in_use &&
        !snapshotContainsId(snapshots, count, stamp.device_id)) {
      stamp = ReadingGenerationStamp{};
    }
  }
  for (CuckooAlarmStamp& stamp : cuckooAlarmStamps) {
    if (stamp.in_use &&
        !cuckooConfigContainsId(cuckooConfig, stamp.device_id)) {
      stamp = CuckooAlarmStamp{};
    }
  }

  for (size_t slot = 0; slot < GATEWAY_ALARM_LIFECYCLE_CAPACITY; ++slot) {
    GatewayAlarmType type = GatewayAlarmType::TemperatureChange;
    char deviceId[GATEWAY_ALARM_LIFECYCLE_DEVICE_ID_CAPACITY] = {};
    if (!gatewayAlarmLifecycle.copyKeyAt(slot, &type, deviceId,
                                         sizeof(deviceId))) {
      continue;
    }
    const bool isCuckooType = type == GatewayAlarmType::CuckooOffline ||
                              type == GatewayAlarmType::CuckooBeerDeviation;
    if (!(isCuckooType ? cuckooConfigContainsId(cuckooConfig, deviceId)
                       : snapshotContainsId(snapshots, count, deviceId))) {
      requestGatewayAlarmCleanup(type, deviceId);
    }
  }

  const uint32_t nowMs = millis();
  for (size_t i = 0; i < count; ++i) {
    const GatewayMeasurementSnapshot& snapshot = snapshots[i];
    GravityDeviceSettings settings{};
    const bool hasSettings =
        myConfig.copyDeviceSettings(snapshot.id, &settings);
    ReadingGenerationStamp* stamp = findOrCreateGenerationStamp(snapshot.id);
    const bool isNewReading =
        stamp != nullptr && stamp->generation != snapshot.generation;

    if (isNewReading) {
      stamp->generation = snapshot.generation;
      if (gatewayAlarmTracker.markOnline(snapshot.id)) {
        Log.notice(F("ALM : device back online dev=%s; offline cleanup." CR),
                   snapshot.id);
        traceAlarmEvent("online dev=%s", snapshot.id);
        requestGatewayAlarmCleanup(GatewayAlarmType::Offline, snapshot.id);
      }
      // Device reported data: clear any persisted offline-alarm dismissal so a
      // future offline episode can alarm again.
      if (hasSettings && settings.offline_alarm_dismissed) {
        settings.offline_alarm_dismissed = false;
        myConfig.saveDeviceSettings(settings);
        gatewayAlarmTracker.setOfflineDismissed(snapshot.id, false);
      }
      if (snapshot.has_temperature) {
        const GatewayAlarmDecision temperatureDecision =
            gatewayAlarmTracker.recordTemperature(
                snapshot.id, snapshot.temperature_c,
                hasSettings && settings.temperature_change_alarm_enabled,
                settings.temperature_compare_minutes,
                settings.temperature_change_threshold_c, nowMs);
        if (temperatureDecision.should_clear)
          requestGatewayAlarmCleanup(GatewayAlarmType::TemperatureChange,
                                     snapshot.id);
      }
    }

    if (!hasSettings || !settings.temperature_change_alarm_enabled) {
      const GatewayAlarmDecision temperatureDecision =
          gatewayAlarmTracker.recordTemperature(
              snapshot.id, NAN, false, 1, 1.0f, nowMs);
      if (temperatureDecision.should_clear)
        requestGatewayAlarmCleanup(GatewayAlarmType::TemperatureChange,
                                   snapshot.id);
    }

    const GatewayAlarmLifecycleReserveResult temperatureReservation =
        gatewayAlarmLifecycle.reserveScheduleKey(
            GatewayAlarmType::TemperatureChange, snapshot.id);
    if (canScheduleGatewayAlarm(temperatureReservation)) {
      const GatewayAlarmDecision decision =
          gatewayAlarmTracker.schedule(GatewayAlarmType::TemperatureChange,
                                       snapshot.id, nowMs);
      if (decision.should_play) {
        char cond[24], tok[24];
        snprintf(cond, sizeof(cond), "%08lx:%08lx",
                 (unsigned long)(uint32_t)(decision.event.condition_token >>
                                           32),
                 (unsigned long)(uint32_t)(decision.event.condition_token &
                                           0xffffffffUL));
        snprintf(tok, sizeof(tok), "%08lx:%08lx",
                 (unsigned long)(uint32_t)(decision.event.event_token >> 32),
                 (unsigned long)(uint32_t)(decision.event.event_token &
                                           0xffffffffUL));
        Log.notice(F("ALM : temp play dev=%s win=%d rep=%d cond=%s tok=%s."
                     CR),
                   snapshot.id, (int)decision.event.window_index,
                   (int)decision.event.repeat_index, cond, tok);
        traceAlarmEvent("temp-play dev=%s win=%d rep=%d tok=%s", snapshot.id,
                        (int)decision.event.window_index,
                        (int)decision.event.repeat_index, tok);
        if (!gatewayAlarmLifecycle.retainScheduledEvent(decision.event)) {
          Log.warning(F("ALM : scheduled temperature event not retained; "
                        "device=%s." CR),
                      snapshot.id);
        }
      } else if (temperatureReservation ==
                 GatewayAlarmLifecycleReserveResult::ReservedNew) {
        gatewayAlarmLifecycle.releaseUnusedReservation(
            GatewayAlarmType::TemperatureChange, snapshot.id);
      }
    } else if (temperatureReservation ==
               GatewayAlarmLifecycleReserveResult::Full) {
      Log.warning(F("ALM : lifecycle capacity rejected before temperature "
                    "schedule; device=%s." CR),
                  snapshot.id);
    }

    const GatewayAlarmDecision offlineDecision =
        gatewayAlarmTracker.evaluateOffline(
            snapshot.id, snapshot.update_age_seconds,
            hasSettings && settings.offline_alarm_enabled,
            settings.offline_timeout_seconds);
    if (offlineDecision.should_play) {
      Log.notice(F("ALM : offline condition start dev=%s age=%u sec." CR),
                 snapshot.id, snapshot.update_age_seconds);
      traceAlarmEvent("offline-start dev=%s age=%d", snapshot.id,
                      (int)snapshot.update_age_seconds);
    }
    if (offlineDecision.should_clear) {
      Log.notice(F("ALM : offline condition clear dev=%s." CR), snapshot.id);
      traceAlarmEvent("offline-clear dev=%s", snapshot.id);
      requestGatewayAlarmCleanup(GatewayAlarmType::Offline, snapshot.id);
    }

    const GatewayAlarmLifecycleReserveResult offlineReservation =
        gatewayAlarmLifecycle.reserveScheduleKey(GatewayAlarmType::Offline,
                                                  snapshot.id);
    if (canScheduleGatewayAlarm(offlineReservation)) {
      const GatewayAlarmDecision decision =
          gatewayAlarmTracker.schedule(GatewayAlarmType::Offline, snapshot.id,
                                       nowMs);
      if (decision.should_play) {
        char cond[24], tok[24];
        snprintf(cond, sizeof(cond), "%08lx:%08lx",
                 (unsigned long)(uint32_t)(decision.event.condition_token >>
                                           32),
                 (unsigned long)(uint32_t)(decision.event.condition_token &
                                           0xffffffffUL));
        snprintf(tok, sizeof(tok), "%08lx:%08lx",
                 (unsigned long)(uint32_t)(decision.event.event_token >> 32),
                 (unsigned long)(uint32_t)(decision.event.event_token &
                                           0xffffffffUL));
        Log.notice(F("ALM : offline play dev=%s win=%d rep=%d cond=%s "
                     "tok=%s." CR),
                   snapshot.id, (int)decision.event.window_index,
                   (int)decision.event.repeat_index, cond, tok);
        traceAlarmEvent("offline-play dev=%s win=%d rep=%d tok=%s",
                        snapshot.id, (int)decision.event.window_index,
                        (int)decision.event.repeat_index, tok);
        if (!gatewayAlarmLifecycle.retainScheduledEvent(decision.event)) {
          Log.warning(F("ALM : scheduled offline event not retained; "
                        "device=%s." CR),
                      snapshot.id);
        }
      } else if (offlineReservation ==
                 GatewayAlarmLifecycleReserveResult::ReservedNew) {
        gatewayAlarmLifecycle.releaseUnusedReservation(
            GatewayAlarmType::Offline, snapshot.id);
      }
    } else if (offlineReservation ==
               GatewayAlarmLifecycleReserveResult::Full) {
      Log.warning(F("ALM : lifecycle capacity rejected before offline "
                    "schedule; device=%s." CR),
                  snapshot.id);
    }

    // Diagnostic: while a device stays past the offline timeout, report
    // whether the tracker is producing a play decision or suppressing it
    // (closed/quiet window). Throttled to avoid serial flooding.
    if (!offlineDecision.should_play && snapshot.update_age_seconds >= 60U) {
      static uint32_t lastSuppressLogMs = 0;
      if (passNow - lastSuppressLogMs >= 10000U) {
        lastSuppressLogMs = passNow;
        Log.notice(F("ALM : offline suppressed dev=%s age=%d sec." CR),
                   snapshot.id, (int)snapshot.update_age_seconds);
      }
    }
  }

  if (cuckooConfig.enabled) {
    for (uint8_t index = 0; index < cuckooConfig.device_count &&
                            index < kMaxCuckooPiDevices;
         ++index) {
      CuckooPiDeviceConfig& settings = cuckooConfig.devices[index];
      CuckooAlarmStamp* stamp =
          findOrCreateCuckooAlarmStamp(settings.id, nowMs);
      if (!stamp) continue;
      CuckooPiRuntimeDevice runtime{};
      const bool hasRuntime =
          cuckooPiMqttService.copyDeviceRuntime(settings.id, &runtime);
      const bool hasSnapshot = hasRuntime && runtime.has_snapshot;
      const uint32_t updateAgeMs = hasSnapshot ? nowMs - runtime.last_seen_ms : 0;
      const CuckooPiAlarmEvaluation evaluation = evaluateCuckooPiAlarm(
          settings, hasSnapshot ? &runtime.snapshot : nullptr, hasSnapshot,
          runtime.last_seen_ms, nowMs, stamp->configured_since_ms,
          &stamp->state);

      if (hasSnapshot && shouldClearCuckooPiOfflineDismissal(
                             settings, runtime.snapshot, true, evaluation) &&
          myConfig.saveCuckooPiOfflineDismissal(settings.id, false)) {
        settings.offline_alarm_dismissed = false;
        settings.offline_dismissed_status_time_seconds = 0U;
        Log.notice(F("ALM : cleared Cuckoo offline dismissal dev=%s." CR),
                   settings.id);
      }

#if defined(GMT154_06)
      const bool offlineActive = evaluation.offline_active;
#else
      const bool offlineActive =
          evaluation.offline_active && !evaluation.offline_suppressed;
#endif
      const GatewayAlarmDecision offlineCondition =
          gatewayAlarmTracker.setExternalCondition(
              GatewayAlarmType::CuckooOffline, settings.id, offlineActive,
              nowMs, evaluation.offline_suppressed);
      if (offlineCondition.should_clear || evaluation.offline_cleared) {
        requestGatewayAlarmCleanup(GatewayAlarmType::CuckooOffline,
                                   settings.id);
      }
      const GatewayAlarmDecision deviationCondition =
          gatewayAlarmTracker.setExternalCondition(
              GatewayAlarmType::CuckooBeerDeviation, settings.id,
              evaluation.beer_deviation_active, nowMs);
      if (deviationCondition.should_clear ||
          evaluation.beer_deviation_cleared) {
        requestGatewayAlarmCleanup(GatewayAlarmType::CuckooBeerDeviation,
                                   settings.id);
      }
      scheduleCuckooAlarm(GatewayAlarmType::CuckooOffline, settings.id,
                          nowMs);
      scheduleCuckooAlarm(GatewayAlarmType::CuckooBeerDeviation, settings.id,
                          nowMs);
    }
  }
  serviceGatewayAlarmLifecycleActions();
}

void gestureLeft() {
  displayTimer.reset();
  displayMeasurementIndex--;

  const CuckooPiConfig config = currentDisplayCuckooConfig();
  const size_t count = gravitymon_gateway_get_layout() == 2
                           ? gatewayDisplayItemCount(myMeasurementList.size(),
                                                     config)
                           : myMeasurementList.size();
  if (displayMeasurementIndex < 0 && count)
    displayMeasurementIndex = static_cast<int>(count - 1);
  else if (displayMeasurementIndex < 0)
    displayMeasurementIndex = 0;
}

void gestureRight() {
  displayTimer.reset();
  displayMeasurementIndex++;

  const CuckooPiConfig config = currentDisplayCuckooConfig();
  const size_t count = gravitymon_gateway_get_layout() == 2
                           ? gatewayDisplayItemCount(myMeasurementList.size(),
                                                     config)
                           : myMeasurementList.size();
  if (count && static_cast<size_t>(displayMeasurementIndex) >= count)
    displayMeasurementIndex = 0;
}

void addGravityLogEntry(const char* id, const tm* timeinfo, float gravitySG,
                        float tempC) {
  float temp = myConfig.isTempFormatF() ? convertCtoF(tempC) : tempC;
  float gravity =
      myConfig.isGravityPlato() ? convertToPlato(gravitySG) : gravitySG;

  char s[60];
  snprintf(s, sizeof(s), "%02d:%02d ID:%s Gravity:%.3f%s Temp: %.1f%s",
           timeinfo->tm_hour, timeinfo->tm_min, id, gravity,
           myConfig.isGravitySG() ? "SG" : "P", temp,
           myConfig.isTempFormatC() ? "C" : "F");

  logEntryList.push_back(s);
  logUpdated = true;
}

void addPressureLogEntry(const char* id, const tm* timeinfo, float pressurePSI,
                         float pressure1PSI, float tempC) {
  float temp = myConfig.isTempFormatF() ? convertCtoF(tempC) : tempC;
  float pressure =
      myConfig.isPressureBar()   ? convertPsiPressureToBar(pressurePSI)
      : myConfig.isPressureKpa() ? convertPsiPressureToKPa(pressurePSI)
                                 : pressurePSI;
  float pressure1 =
      myConfig.isPressureBar()   ? convertPsiPressureToBar(pressure1PSI)
      : myConfig.isPressureKpa() ? convertPsiPressureToKPa(pressure1PSI)
                                 : pressure1PSI;

  char s[60];
  snprintf(s, sizeof(s), "%02d:%02d ID:%s Pressure:%.3f%s Temp:%.1f%s",
           timeinfo->tm_hour, timeinfo->tm_min, id, pressure,
           myConfig.getPressureUnit(), temp,
           myConfig.isTempFormatC() ? "C" : "F");

  logEntryList.push_back(s);
  logUpdated = true;
}

void addChamberLogEntry(const char* id, const tm* timeinfo, float chamberTempC,
                        float beerTempC) {
  float chamberTemp =
      myConfig.isTempFormatF() ? convertCtoF(chamberTempC) : chamberTempC;
  float beerTemp =
      myConfig.isTempFormatF() ? convertCtoF(beerTempC) : beerTempC;

  char s[60];
  snprintf(s, sizeof(s), "%02d:%02d ID:%s Chamber:%.1f%s Beer:%.1f%s",
           timeinfo->tm_hour, timeinfo->tm_min, id, chamberTemp,
           myConfig.isTempFormatC() ? "C" : "F", beerTemp,
           myConfig.isTempFormatC() ? "C" : "F");

  logEntryList.push_back(s);
  logUpdated = true;
}

void controller() {
  // Scan for ble beacons
  if (myConfig.isBleEnable()) {
    bleScanner.setScanTime(myConfig.getBleScanTime());
    bleScanner.setAllowActiveScan(myConfig.getBleActiveScan());
    bleScanner.scan();
    yield();  // Reset watchdog after BLE scan
  }

  if (controllerTimer.hasExpired()) {
    if (gatewayPushResourcesAvailable(WiFi.status() == WL_CONNECTED,
                                      ESP.getFreeHeap())) {
      BrewingPush push(&myConfig);

      for (int i = 0; i < myMeasurementList.size(); i++) {
        MeasurementEntry* entry = myMeasurementList.getMeasurementEntry(i);

        switch (entry->getType()) {
          case MeasurementType::Gravitymon: {
            Log.notice("Loop: Processing Gravitymon data %d." CR, i);

            if (entry->isUpdated() &&
                (entry->getPushAge() > myConfig.getPushResendTime())) {
              const GravityData* gd = entry->getGravityData();

              addGravityLogEntry(gd->getId(), entry->getTimeinfoUpdated(),
                                 gd->getGravity(), gd->getTempC());

              TemplatingEngine engine;

              setupTemplateEngineGravityGateway(
                  &myConfig, engine, gd->getAngle(), gd->getGravity(),
                  gd->getTempC(), gd->getBattery(), gd->getInterval(),
                  gd->getId(), gd->getToken(), gd->getName(),
                  gd->getHttpPostSleepInterval(), gd->getBleSleepInterval(),
                  gd->getDeviceMetadata());

              if (gatewayPushResourcesAvailable(
                      WiFi.status() == WL_CONNECTED, ESP.getFreeHeap())) {
                push.sendAll(engine, BrewingPush::MeasurementType::GRAVITY,
                             myConfig.isHttpPostGravityEnable(),
                             myConfig.isHttpPost2GravityEnable(),
                             myConfig.isHttpGetGravityEnable(),
                             myConfig.isInfluxdb2GravityEnable(),
                             myConfig.isMqttGravityEnable());
                entry->setPushed();
              } else {
                Log.warning(F("PUSH: Skipped due to WiFi not connected or low "
                              "heap (%d, minimum %d)" CR),
                            ESP.getFreeHeap(),
                            GATEWAY_MIN_PUSH_FREE_HEAP_BYTES);
              }

              yield();
            }
          } break;

          case MeasurementType::Pressuremon: {
            Log.notice("Loop: Processing Pressuremon data %d." CR, i);

            if (entry->isUpdated() &&
                (entry->getPushAge() > myConfig.getPushResendTime())) {
              const PressureData* pd = entry->getPressureData();

              addPressureLogEntry(pd->getId(), entry->getTimeinfoUpdated(),
                                  pd->getPressure(), pd->getPressure1(),
                                  pd->getTempC());

              TemplatingEngine engine;

              setupTemplateEnginePressureGateway(
                  &myConfig, engine, pd->getPressure(), pd->getPressure1(),
                  pd->getTempC(), pd->getBattery(), pd->getInterval(),
                  pd->getId(), pd->getToken(), pd->getName(),
                  pd->getHttpPostSleepInterval(), pd->getBleSleepInterval());
              if (gatewayPushResourcesAvailable(
                      WiFi.status() == WL_CONNECTED, ESP.getFreeHeap())) {
                push.sendAll(engine, BrewingPush::MeasurementType::PRESSURE,
                             myConfig.isHttpPostPressureEnable(),
                             myConfig.isHttpPost2PressureEnable(),
                             myConfig.isHttpGetPressureEnable(),
                             myConfig.isInfluxdb2PressureEnable(),
                             myConfig.isMqttPressureEnable());
                entry->setPushed();
              } else {
                Log.warning(F("PUSH: Skipped due to WiFi not connected or low "
                              "heap (%d, minimum %d)" CR),
                            ESP.getFreeHeap(),
                            GATEWAY_MIN_PUSH_FREE_HEAP_BYTES);
              }
              yield();
            }
          } break;

          case MeasurementType::Tilt:
          case MeasurementType::TiltPro: {
            Log.notice("Loop: Processing Tilt data %d." CR, i);

            // #define ENABLE_TILT_SCANNING

#if defined(ENABLE_TILT_SCANNING)
            //  This part is for testing / debugging only, use Tiltbridge if you
            //  use Tilt
            //  as BLE transmission, will show detected tilt devices but dont
            //  send data.

            if (entry->isUpdated() &&
                (entry->getPushAge() > myConfig.getPushResendTime())) {
              const TiltData* pd = entry->getTiltData();

              addGravityLogEntry(pd->getId(), entry->getTimeinfoUpdated(),
                                 pd->getGravity(), pd->getTempC());

              // TODO: Add push support

              entry->setPushed();
            }
#endif
          } break;

          case MeasurementType::Chamber: {
            Log.notice("Loop: Processing Chamber data %d." CR, i);
          } break;

          case MeasurementType::Rapt: {
            Log.notice("Loop: Processing Rapt data %d." CR, i);

            if (entry->isUpdated() &&
                (entry->getPushAge() > myConfig.getPushResendTime())) {
              const RaptData* rd = entry->getRaptData();

              addGravityLogEntry(rd->getId(), entry->getTimeinfoUpdated(),
                                 rd->getGravity(), rd->getTempC());

              // NOTE: Push support not posible

              entry->setPushed();
            }
          } break;
        }
      }
    } else {
      Log.warning(F("Loop: Push skipped due WiFi %s, heap %d, minimum %d" CR),
                  WiFi.status() == WL_CONNECTED ? "connected" : "disconnected",
                  ESP.getFreeHeap(), GATEWAY_MIN_PUSH_FREE_HEAP_BYTES);
    }

    controllerTimer.reset();
  }
}

void updateDisplayStatus() {
  char info[80];

  /*
      if (!myWifi.isConnected()) {
        snprintf(statusBar, sizeof(statusBar), "Not connected");
      } else {
        snprintf(statusBar, sizeof(statusBar), "ssid: %s ip: %s, rssi: %d",
                 WiFi.SSID().c_str(), WiFi.localIP().toString().c_str(),
                 WiFi.RSSI());
      }
  */

  switch (runMode) {
    case RunMode::measurementMode:
      if (strlen(myConfig.getWifiDirectSSID())) {
        snprintf(info, sizeof(info), "ip: %s direct: %s rssi: %d",
                 WiFi.localIP().toString().c_str(),
                 myConfig.getWifiDirectSSID(), WiFi.RSSI());
      } else {
        snprintf(info, sizeof(info), "ssid: %s ip: %s rssi: %d",
                 WiFi.SSID().c_str(), WiFi.localIP().toString().c_str(),
                 WiFi.RSSI());
      }
      myDisplay.updateNetworkStatus(
          WiFi.localIP().toString().c_str(),
          WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0);
      break;

    case RunMode::wifiSetupMode:
      snprintf(info, sizeof(info), "Wifi Setup - 192.168.4.1");
      myDisplay.updateNetworkStatus("192.168.4.1", 0);
      break;
  }

  myDisplay.updateStatus(info);
  myDisplay.updateDarkmode(myConfig.getDarkMode());
  myDisplay.setLayout(myConfig.getDisplayLayoutId());
}

void updateDisplayLogs() {
  int idx = 0;

  while (logEntryList.size() > MAX_LOG_ENTRIES) logEntryList.pop_front();

  for (const auto& entry : logEntryList) {
    if (idx >= MAX_LOG_ENTRIES) break;
    Log.notice("Loop: Updating log entry %s (%d)." CR, entry.c_str(), idx);
    myDisplay.updateHistory(entry.c_str(), idx++);
    yield();  // Reset watchdog after updating each log entry
  }
}

#endif  // GATEWAY

// EOF
