#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {

int failures = 0;

std::string read_file(const char* path) {
  std::ifstream input(path);
  if (!input.good()) {
    std::cerr << "cannot open " << path << "\n";
    ++failures;
    return {};
  }
  std::ostringstream contents;
  contents << input.rdbuf();
  return contents.str();
}

void require(const std::string& source, const std::string& token,
             const char* what) {
  if (source.find(token) == std::string::npos) {
    std::cerr << "missing requirement: " << what << "\n";
    ++failures;
  }
}

void reject(const std::string& source, const std::string& token,
            const char* what) {
  if (source.find(token) != std::string::npos) {
    std::cerr << "forbidden behavior: " << what << "\n";
    ++failures;
  }
}

size_t count_occurrences(const std::string& source,
                         const std::string& needle) {
  size_t count = 0;
  size_t offset = 0;
  while ((offset = source.find(needle, offset)) != std::string::npos) {
    ++count;
    offset += needle.size();
  }
  return count;
}

}  // namespace

int main() {
  const std::string push = read_file("src/push_gateway.cpp");
  const std::string main = read_file("src/main_gateway.cpp");
  const std::string header = read_file("src/main_gateway.hpp");
  const std::string measurement = read_file("src/measurement.hpp");
  const std::string ble = read_file("src/ble_gateway.cpp");
  const std::string web = read_file("src/web_gateway.cpp");

  require(push, "${ip-address}",
          "gateway HTTP templates must expose the ip-address placeholder");
  require(push, "${run-mode}",
          "gateway HTTP templates must expose the run-mode placeholder");
  require(push, "${http-post-sleep-interval}",
          "gateway HTTP templates must expose the HTTP post sleep interval placeholder");
  require(push, "${ble-sleep-interval}",
          "gateway HTTP templates must expose the BLE sleep interval placeholder");
  require(push, "httpPostSleepInterval",
          "template setup must receive the HTTP post sleep interval value");
  require(push, "bleSleepInterval",
          "template setup must receive the BLE sleep interval value");
  require(web, "GRAVMON_HTTP_POST_SLEEP_INTERVAL",
          "HTTP input must parse the HTTP post sleep interval field");
  require(web, "GRAVMON_BLE_SLEEP_INTERVAL",
          "HTTP input must parse the BLE sleep interval field");
  require(web, "GRAVMON_IP_ADDRESS",
          "HTTP input must parse the device IP address");
  require(web, "GRAVMON_APP_VERSION",
          "HTTP input must parse the device app version");
  require(web, "GRAVMON_RUN_MODE",
          "HTTP input must parse the device run mode");
  require(web, "GRAVMON_RUN_TIME",
          "HTTP input must parse the device run time");
  require(measurement, "GatewayDeviceMetadata _deviceMetadata",
          "gravity measurements must own device metadata");
  require(push, "deviceMetadata.ipAddress()",
          "gravity templates must use the received device IP address");
  require(push, "deviceMetadata.appVersion()",
          "gravity templates must use the received device app version");
  require(push, "deviceMetadata.runMode()",
          "gravity templates must use the received device run mode");
  require(push, "deviceMetadata.runTime()",
          "gravity templates must use the received device run time");
  if (count_occurrences(
          ble,
          "GatewayDeviceMetadata bleMetadata(\"\", \"\", \"BLE\", 0.0f)") !=
      0) {
    std::cerr << "BLE gravity sources must not send text in integer run-mode\n";
    ++failures;
  }
  require(push, "gatewayRunModeName()",
          "template setup must use the gateway run mode accessor");
  require(main, "WiFi.localIP()",
          "gateway IP accessor must use the gateway Wi-Fi address");
  require(main, "gatewayRunModeName()",
          "main gateway must define the run mode accessor");
  require(main, "measurement",
          "measurement mode must have a stable public string");
  require(main, "wifi_setup",
          "Wi-Fi setup mode must have a stable public string");
  require(header, "gatewayRunModeName()",
          "run mode accessor must be declared for HTTP template code");
  require(web, "copyLatestPushTestSample",
          "push tests must use the latest real measurement");
  require(web, "getString()",
          "HTTP push tests must read the response body on failure");
  require(web, "HTTP post failed, response=%d, body=%s",
          "HTTP push tests must log the response body on failure");
  reject(web, "\"012345\"",
         "push tests must not send the hard-coded device id");
  require(web, "sample.token, sample.name",
          "push tests must forward the real measurement token and name");
  reject(web, "\"grav-test\"",
         "push tests must not send a synthetic gravity device");
  reject(web, "\"press-test\"",
         "push tests must not send a synthetic pressure device");

  if (count_occurrences(push, "${ip-address}") != 1) {
    std::cerr << "ip-address placeholder must be defined once in gateway template code\n";
    ++failures;
  }
  if (count_occurrences(push, "${run-mode}") != 1) {
    std::cerr << "run-mode placeholder must be defined once in gateway template code\n";
    ++failures;
  }
  if (count_occurrences(push, "gatewayIpAddress()") != 1) {
    std::cerr << "only pressure templates may use the gateway IP address\n";
    ++failures;
  }
  if (count_occurrences(push, "gatewayRunModeName()") != 1) {
    std::cerr << "only pressure templates may use the gateway run mode\n";
    ++failures;
  }
  if (count_occurrences(push, "engine.setVal(TPL_APP_VER, CFG_APPVER)") != 1) {
    std::cerr << "only pressure templates may use the gateway app version\n";
    ++failures;
  }

  return failures == 0 ? 0 : 1;
}
