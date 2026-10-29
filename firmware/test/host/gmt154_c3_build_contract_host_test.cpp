#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

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
    std::cerr << "forbidden token remains: " << what << "\n";
    ++failures;
  }
}

void require_ordered(const std::string& source,
                     const std::vector<std::string>& tokens,
                     const char* what) {
  size_t offset = 0;
  for (const std::string& token : tokens) {
    const size_t found = source.find(token, offset);
    if (found == std::string::npos) {
      std::cerr << "missing ordered requirement: " << what << " (" << token
                << ")\n";
      ++failures;
      return;
    }
    offset = found + token.size();
  }
}

std::string section_after(const std::string& source, const std::string& header,
                          const std::string& next_marker = "\n[") {
  const size_t start = source.find(header);
  if (start == std::string::npos) return {};
  const size_t end = source.find(next_marker, start + header.size());
  if (end == std::string::npos) return source.substr(start);
  return source.substr(start, end - start);
}

std::string trim(std::string value) {
  const auto not_space = [](unsigned char c) { return !std::isspace(c); };
  value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
  value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(),
              value.end());
  return value;
}

uint64_t read_json_integer(const std::string& source, const std::string& key) {
  const std::string marker = "\"" + key + "\"";
  const size_t key_pos = source.find(marker);
  if (key_pos == std::string::npos) return 0;
  const size_t colon = source.find(':', key_pos + marker.size());
  if (colon == std::string::npos) return 0;
  size_t value_pos = colon + 1;
  while (value_pos < source.size() &&
         std::isspace(static_cast<unsigned char>(source[value_pos]))) {
    ++value_pos;
  }
  try {
    return std::stoull(source.substr(value_pos));
  } catch (...) {
    return 0;
  }
}

uint64_t read_hex_define(const std::string& source, const std::string& key) {
  const size_t key_pos = source.rfind(key);
  if (key_pos == std::string::npos) return 0;
  const size_t value_pos = key_pos + key.size();
  try {
    return std::stoull(source.substr(value_pos), nullptr, 0);
  } catch (...) {
    return 0;
  }
}

struct Partition {
  std::string name;
  uint64_t offset;
  uint64_t size;
};

std::vector<Partition> read_partitions(const std::string& csv) {
  std::vector<Partition> rows;
  std::istringstream input(csv);
  std::string line;
  while (std::getline(input, line)) {
    line = trim(line);
    if (line.empty() || line[0] == '#') continue;
    std::vector<std::string> columns;
    std::stringstream row(line);
    std::string column;
    while (std::getline(row, column, ',')) columns.push_back(trim(column));
    if (columns.size() < 5 || columns[0] == "Name") continue;
    try {
      rows.push_back({columns[0], std::stoull(columns[3], nullptr, 0),
                      std::stoull(columns[4], nullptr, 0)});
    } catch (...) {
      std::cerr << "invalid partition offset/size: " << line << "\n";
      ++failures;
    }
  }
  return rows;
}

}  // namespace

int main() {
  const std::string platform = read_file("platformio.ini");
  const std::string c3_board = read_file("boards/gmt154_06_c3.json");
  const std::string c3_partitions =
      read_file("part32_gmt154_c3_single_app.csv");
  const std::string c3_env =
      section_after(platform, "[env:gmt154-06-c3]");
  const std::string s3_env = section_after(platform, "[env:gmt154-06]");
  const std::string gmt_data = section_after(platform, "[gmt154_env_data]");
  const std::string s3_effective = s3_env + "\n" + gmt_data;
  const std::string display_source = read_file("src/display.cpp");
  const std::string display_adapter =
      read_file("src/gmt154_display_adapter.hpp");
  const std::string main_gateway = read_file("src/main_gateway.cpp");
  const std::string c3_patch =
      read_file("script/patch_espframework_gmt154_c3_single_app.py");
  const std::string ui_gateway = read_file("src/ui_gravitymon_gateway.cpp");
  const std::string audio_codec_source = read_file("src/gmt154_audio_codec.cpp");
  const std::string audio_codec_header = read_file("src/gmt154_audio_codec.hpp");

  require(c3_board, "\"mcu\": \"esp32c3\"", "C3 board MCU identity");
  require(c3_board, "\"flash_size\": \"4MB\"", "C3 board flash size");
  require(c3_board, "\"maximum_size\": 3670016",
          "C3 board app limit matches the single factory partition");
  reject(c3_board, "BOARD_HAS_PSRAM", "C3 board must not declare PSRAM");
  require(c3_env, "board = gmt154_06_c3", "production C3 board selection");
  require(c3_env, "board_build.partitions = part32_gmt154_c3_single_app.csv",
          "production C3 partition selection");
  require(c3_env, "pre:script/patch_espframework_gmt154_c3_single_app.py",
          "C3-only framework updater rejection patch");
  require(c3_env, "${common_env_data.build_flags}",
          "C3 uses shared gateway base build flags");
  require(c3_env, "${common_env_data.build_flags_lvgl}",
          "C3 uses shared LVGL build flags");
  require(c3_env, "ESP32C3=1", "framework code selects the ESP32-C3 path");
  require(c3_env, "ENABLE_TFT=1", "C3 compiles the TFT display path");
  require(c3_env, "LCD_TFT=1", "C3 selects the shared LCD display path");
  require(c3_env, "USER_SETUP_LOADED=1",
          "C3 uses its explicit TFT_eSPI pin setup");
  require(c3_env, "LV_USE_FONT_COMPRESSED=1",
          "C3 enables compressed bitmaps used by the GMT154 fonts");
  require(c3_env, "${common_env_data.lib_deps}",
          "C3 retains Wi-Fi/web/BLE/MQTT gateway dependencies");
  require(c3_env, "${common_env_data.lib_deps_tft}",
          "C3 retains display dependencies");
  require(c3_env, "GMT154_06_C3=1", "C3-only target macro");
  require(c3_env, "GMT154_06=1", "shared GMT154 UI/display behavior branch");
  require(c3_env, "TFT_WIDTH=240", "C3 square display width");
  require(c3_env, "TFT_HEIGHT=240", "C3 square display height");
  require(c3_env, "TFT_DC=2", "C3 LCD DC pin");
  require(c3_env, "TFT_SCLK=3", "C3 LCD SCLK pin");
  require(c3_env, "TFT_MOSI=5", "C3 LCD MOSI pin");
  require(c3_env, "TFT_RST=6", "C3 LCD reset pin");
  require(c3_env, "GMT154_06_BACKLIGHT_PIN=1",
          "C3 active-low backlight pin");
  require(c3_patch, "TARGET_ENV = \"gmt154-06-c3\"",
          "framework updater patch is limited to the C3 environment");
  require(c3_patch, "request->send(403, \"text/plain\"",
          "C3 firmware route explicitly rejects upload requests");
  require(c3_patch, "webHandleUploadFirmware",
          "patch validates the original updater route before replacement");
  reject(c3_patch, "Update.begin(",
         "C3 updater patch must not call the flash updater");
  require(c3_env, "CONFIG_ASYNC_TCP_RUNNING_CORE=0",
          "C3 single-core AsyncTCP task affinity");
  require(c3_env, "MAX_SKETCH_SPACE=0x380000",
          "C3 application limit matches the single factory app partition");
  reject(c3_env, "MAX_SKETCH_SPACE=0x1C0000",
         "C3 must not retain the old OTA-slot app-size limit");
  require(c3_env, "TFT_CS=-1", "C3 grounded LCD CS");
  reject(c3_env, "TFT_MOSI=4", "C3 must not assign NC GPIO4 to LCD MOSI");
  reject(c3_env, "TFT_DC=17", "C3 must not inherit S3 LCD DC pin");
  reject(c3_env, "TFT_SCLK=18", "C3 must not inherit S3 LCD clock pin");
  reject(c3_env, "TFT_MOSI=1", "C3 must not inherit S3 LCD data pin");
  reject(c3_env, "GMT154_06_BACKLIGHT_PIN=2",
         "C3 must not inherit S3 backlight pin");
  reject(c3_env, "ENABLE_GATEWAY_AUDIO", "C3 audio product macro");
  reject(c3_env, "BOARD_HAS_PSRAM", "C3 PSRAM macro");
  reject(c3_env, "PSRAM", "C3 PSRAM configuration");
  reject(c3_env, "gateway_audio", "C3 audio-only dependency");
  require(audio_codec_source, "defined(ENABLE_GATEWAY_AUDIO)",
          "C3 does not compile the GMT154 ES8311 hardware adapter");
  require(audio_codec_header, "defined(ENABLE_GATEWAY_AUDIO)",
          "C3 codec adapter does not include ES8311 hardware declarations");
  reject(c3_env, "build_src_filter", "C3 must compile full product sources");
  reject(c3_env, "ESPFWK_DISABLE_WEBSERVER", "C3 web server disable flag");
  reject(c3_env, "ESPFWK_DISABLE_WIFI", "C3 Wi-Fi disable flag");
  require(display_source, "gmt154_backlight::Controller<Gmt154BacklightIo>",
          "GMT154 display uses the shared active-low backlight controller");
  require(display_source, "gmt154BacklightIo, GMT154_06_BACKLIGHT_PIN",
          "display controller receives target-selected backlight pin");
  require_ordered(display_source,
                  {"gmt154Backlight.initialize_off();", "_tft->init();",
                   "gmt154Backlight.turn_on();"},
                  "backlight stays off during panel initialization");
  require(display_source,
          "#if defined(GMT154_06) && !defined(GMT154_06_C3)",
          "PSRAM heap diagnostics are limited to the S3 target");
  require(display_adapter, "#if defined(GMT154_06_C3)",
          "C3 display buffers use an internal-heap-only path");
  require(main_gateway,
          "#if defined(ENABLE_GATEWAY_AUDIO)\n"
          "  gmt154LogHeapSnapshot(\"before_audio\");\n"
          "  gatewayAudio.begin();\n"
          "  gmt154LogHeapSnapshot(\"after_audio\");\n"
          "#endif",
          "audio startup is compiled only for audio-enabled targets");
  require(main_gateway, "cuckooPiMqttService.begin()",
          "C3 keeps the MQTT/Cuckoo service startup path");
  require(main_gateway, "settings.offline_alarm_dismissed",
          "C3 keeps persisted alarm dismissal behavior");
  require(main_gateway, "GatewayAlertState gatewayAlertState",
          "C3 keeps visual alert state");
  require(ui_gateway, "wid_alert_panel",
          "C3 keeps visual alarm UI");
  reject(ui_gateway, "gmt154_audio_profile.hpp",
         "UI must not depend on C3 audio hardware profile");
  reject(ui_gateway, "i2s_channel_", "UI must not initialize I2S");

  require(s3_env, "board = gmt154_06", "S3 board identity remains selected");
  require(s3_env, "board_build.partitions = part32_16mb_coredump.csv",
          "S3 partition selection remains intact");
  require(s3_effective, "-D TFT_DC=17", "S3 LCD DC pin is unchanged");
  require(s3_effective, "-D TFT_SCLK=18", "S3 LCD SCLK pin is unchanged");
  require(s3_effective, "-D TFT_MOSI=1", "S3 LCD MOSI pin is unchanged");
  require(s3_effective, "-D TFT_RST=6", "S3 LCD reset pin is unchanged");
  require(s3_effective, "-D GMT154_06_BACKLIGHT_PIN=2",
          "S3 backlight pin is unchanged");
  require(s3_effective, "-D ENABLE_GATEWAY_AUDIO=1",
          "S3 audio remains enabled");

  const std::vector<Partition> rows = read_partitions(c3_partitions);
  std::map<std::string, Partition> by_name;
  for (const Partition& row : rows) by_name[row.name] = row;
  for (const char* name : {"nvs", "factory", "spiffs", "coredump"}) {
    if (by_name.find(name) == by_name.end()) {
      std::cerr << "missing partition: " << name << "\n";
      ++failures;
    }
  }
  for (const char* name : {"otadata", "app0", "app1"}) {
    if (by_name.find(name) != by_name.end()) {
      std::cerr << "forbidden OTA partition: " << name << "\n";
      ++failures;
    }
  }
  const auto check_partition = [&](const char* name, uint64_t offset,
                                   uint64_t size) {
    const auto it = by_name.find(name);
    if (it != by_name.end() &&
        (it->second.offset != offset || it->second.size != size)) {
      std::cerr << "wrong partition geometry: " << name << "\n";
      ++failures;
    }
  };
  check_partition("nvs", 0x9000, 0x5000);
  check_partition("factory", 0x10000, 0x380000);
  check_partition("spiffs", 0x390000, 0x60000);
  check_partition("coredump", 0x3F0000, 0x10000);

  const auto factory = by_name.find("factory");
  const uint64_t board_app_limit = read_json_integer(c3_board, "maximum_size");
  const uint64_t environment_app_limit =
      read_hex_define(c3_env, "MAX_SKETCH_SPACE=");
  if (factory != by_name.end() &&
      (board_app_limit != factory->second.size ||
       environment_app_limit != factory->second.size ||
       factory->second.size != 0x380000)) {
    std::cerr << "C3 app size limits must agree: board=" << board_app_limit
              << " MAX_SKETCH_SPACE=" << environment_app_limit
              << " factory=" << factory->second.size << "\n";
    ++failures;
  }

  std::vector<Partition> sorted = rows;
  std::sort(sorted.begin(), sorted.end(), [](const Partition& a,
                                             const Partition& b) {
    return a.offset < b.offset;
  });
  uint64_t end = 0;
  for (const Partition& row : sorted) {
    if (row.offset < end) {
      std::cerr << "overlapping partition: " << row.name << "\n";
      ++failures;
    }
    end = row.offset + row.size;
  }
  if (end != 0x400000) {
    std::cerr << "partition table must end at 0x400000, got 0x" << std::hex
              << end << "\n";
    ++failures;
  }

  if (failures == 0) {
    std::cout << "GMT154 C3 build contract: PASS\n";
    return 0;
  }
  std::cerr << "GMT154 C3 build contract: " << failures << " failure(s)\n";
  return 1;
}
