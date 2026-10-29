#include <windows.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// Build contract for the periodic alarm gateway. Never relies on assert()
// being enabled: every failure is counted explicitly and reported. All file
// opens are checked for failure. This contract deliberately has NO dependency
// on platformio.ascii-build.ini, script/source_build_id.py, the settings
// entry/layout tests, the gateway32-unit fallback, or any historical plan
// text.

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

std::vector<std::string> list_src_cpp() {
  std::vector<std::string> files;
  WIN32_FIND_DATAA data;
  HANDLE handle = FindFirstFileA("src\\*.cpp", &data);
  if (handle == INVALID_HANDLE_VALUE) {
    std::cerr << "cannot enumerate src\\*.cpp\n";
    ++failures;
    return files;
  }
  do {
    files.push_back(data.cFileName);
  } while (FindNextFileA(handle, &data));
  FindClose(handle);
  return files;
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
      std::cerr << "missing ordered requirement: " << what << " ("
                << token << ")\n";
      ++failures;
      return;
    }
    offset = found + token.size();
  }
}

size_t count_occurrences(const std::string& source,
                         const std::string& needle) {
  if (needle.empty()) return 0;
  size_t count = 0;
  size_t offset = 0;
  while ((offset = source.find(needle, offset)) != std::string::npos) {
    ++count;
    offset += needle.size();
  }
  return count;
}

std::string section_after(const std::string& source, const std::string& header,
                          const std::string& next_marker) {
  const size_t start = source.find(header);
  if (start == std::string::npos) return {};
  const size_t end = source.find(next_marker, start + 1);
  if (end == std::string::npos) return source.substr(start);
  return source.substr(start, end - start);
}

}  // namespace

int main() {
  const std::string platform = read_file("platformio.ini");
  const std::string gmt_board = read_file("boards/gmt154_06.json");
  const std::string gmt_patch_script =
      read_file("script/patch_tft_espi_gmt154.py");
  const std::string gmt_logging_patch_script =
      read_file("script/patch_espframework_gmt154_logging.py");
  const std::string gmt_sync_wifi_patch_script =
      read_file("script/patch_espframework_gmt154_sync_wifi_diag.py");
  const std::string copy = read_file("script/copy_firmware.py");
  const std::string display_source = read_file("src/display.cpp");
  const std::string gmt_adapter_source =
      read_file("src/gmt154_display_adapter.hpp");
  const std::string orientation_source =
      read_file("src/gateway_orientation.cpp");
  const std::string cuckoo_screen_source =
      read_file("src/cuckoo_pi_screen.cpp");
  const std::string main_gateway = read_file("src/main_gateway.cpp");
  const std::string ui_gateway = read_file("src/ui_gravitymon_gateway.cpp");
  const std::string alarm_header = read_file("src/gateway_alarm.hpp");
  const std::string alarm_source = read_file("src/gateway_alarm.cpp");
  const std::string audio_header = read_file("src/gateway_audio.hpp");
  const std::string audio_source = read_file("src/gateway_audio.cpp");
  const std::string lifecycle_source =
      read_file("src/gateway_alarm_lifecycle.cpp");
  const std::string alert_source = read_file("src/gateway_alert_state.cpp");

  // --- Waveshare environment and audio enablement ---
  const std::string wave_section =
      section_after(platform, "[env:gateway32s3wave-tft-sd]", "\n[");
  require(wave_section, "-D WAVESHARE_CST3530=1",
          "Waveshare env must enable the CST3530 touch controller");
  require(wave_section, "-D ENABLE_GATEWAY_AUDIO=1",
          "Waveshare env must enable the gateway audio module");
  require(platform, "-D LV_FONT_MONTSERRAT_40=1",
          "LVGL Montserrat 40 font must be enabled");

  // --- Independent GMT154-06 1.54-inch square environment ---
  const std::string gmt_section =
      section_after(platform, "[env:gmt154-06]", "\n[");
  require(gmt_section, "board = gmt154_06", "GMT154 env board identity");
  require(gmt_section, "${gmt154_env_data.build_flags}",
          "GMT154 production environment uses shared flags and owns its filename");
  require(gmt_section, "pre:script/patch_tft_espi_gmt154.py",
          "GMT154 must run the TFT_eSPI patch as a pre script");
  reject(gmt_section, "script/copy_firmware.py",
         "GMT154 must not inherit the firmware copy post action");
  reject(gmt_section, "script/create_versionjson.py",
         "GMT154 must not inherit the version JSON post action");
  if (count_occurrences(platform, "pre:script/patch_tft_espi_gmt154.py") != 4) {
    std::cerr << "TFT_eSPI patch must be mounted on GMT154, C2b, C2c, and the inherited sync diagnostic\n";
    ++failures;
  }
  const std::string common_section =
      section_after(platform, "[common_env_data]", "\n[env:");
  reject(common_section, "patch_tft_espi_gmt154.py",
         "TFT_eSPI patch must not be inherited by common environments");
  const std::string gmt154_build_data =
      section_after(platform, "[gmt154_env_data]", "\n[");
  const std::string gmt_effective_section =
      gmt_section + "\n" + gmt154_build_data;
  require(gmt154_build_data, "${common_env_data.build_flags}",
          "GMT154 shared flags retain the global base flags");
  require(gmt154_build_data, "${common_env_data.build_flags_lvgl}",
          "GMT154 shared flags retain the LVGL base flags");
  require(gmt154_build_data, "-D GMT154_06=1",
          "GMT154 shared flags retain the board feature macro");
  require(gmt154_build_data, "-D ST7789_2_DRIVER=1",
          "GMT154 shared flags select the ST7789 V2 driver");
  reject(gmt_effective_section, "-D ST7789_DRIVER=1",
         "GMT154 must use only the ST7789 V2 driver");
  require(gmt154_build_data, "-D TFT_WIDTH=240",
          "GMT154 shared flags set square width");
  require(gmt154_build_data, "-D TFT_HEIGHT=240",
          "GMT154 shared flags set square height");
  require(gmt154_build_data, "-D TFT_DC=17",
          "GMT154 shared flags set LCD DC pin");
  require(gmt154_build_data, "-D TFT_SCLK=18",
          "GMT154 shared flags set LCD SCL pin");
  require(gmt154_build_data, "-D TFT_MOSI=1",
          "GMT154 shared flags set LCD SDA pin");
  require(gmt154_build_data, "-D TFT_RST=6",
          "GMT154 shared flags set LCD reset pin");
  require(gmt154_build_data, "-D GMT154_06_BACKLIGHT_PIN=2",
          "GMT154 shared flags set active-low backlight pin");
  reject(gmt_effective_section, "-D TFT_SDA_READ=1",
         "GMT154 must not enable TFT SDA readback");
  reject(gmt_effective_section, "-D CGRAM_OFFSET=1",
         "GMT154 must not apply the legacy CGRAM offset");
  reject(gmt_effective_section, "TFT_BL",
         "GMT154 must not configure the unverified backlight pin");
  require(gmt154_build_data, "-D TFT_CS=-1",
          "GMT154 shared flags keep LCD CS grounded");
  require(gmt154_build_data, "-D TOUCH_CS=-1",
          "GMT154 shared flags disable touch controller CS");
  require(gmt154_build_data, "-D TFT_SPI_MODE=SPI_MODE3",
          "GMT154 shared flags select SPI mode 3");
  require(gmt154_build_data, "-D SPI_FREQUENCY=20000000",
          "GMT154 shared flags set 20MHz bringup SPI frequency");
  reject(gmt154_build_data, "CFG_FILENAMEBIN",
         "GMT154 shared flags must not hardcode an environment filename");
  const std::string wave_section_after_gmt =
      section_after(platform, "[env:gateway32s3wave-tft-sd]", "\n[env:");
  reject(wave_section_after_gmt, "patch_tft_espi_gmt154.py",
         "TFT_eSPI patch must not mount on the Waveshare environment");
  reject(gmt_effective_section, "TFT_BACKLIGHT_ON",
         "GMT154 must not configure TFT_eSPI to drive the backlight");
  require(gmt_section, "board_build.partitions = part32_16mb_coredump.csv",
          "GMT154 existing 16MB partition table");
  reject(gmt_effective_section, "-D ENABLE_SD=1",
         "GMT154 must not add SD SPI CS");
  reject(gmt_effective_section, "-D ENABLE_MMC=1",
         "GMT154 must not add MMC SPI use");
  require(gmt_patch_script, "\"gmt154-06-sync-wifi-scan-diag\"",
          "TFT patch whitelist must include the synchronous scan baseline");
  require(gmt_patch_script, "\"gmt154-06-sync-wifi-scan-diag-ble-init-off\"",
          "TFT patch whitelist must include the BLE-init-off diagnostic");
  require(gmt_patch_script, "AddBuildMiddleware",
          "patch script must run before library source compilation");
  reject(gmt_patch_script, "AddPostAction(\"buildprog\"",
         "patch script must not use a buildprog post action");
  require(gmt_patch_script, "GMT154_TFT_ESPI_PATCH:",
          "patch script must emit an auditable build marker");
  require(gmt_logging_patch_script, "\"gmt154-06-sync-wifi-scan-diag\"",
          "logging patch whitelist must include the synchronous scan baseline");
  require(gmt_logging_patch_script, "\"gmt154-06-sync-wifi-scan-diag-ble-init-off\"",
          "logging patch whitelist must include the BLE-init-off diagnostic");
  require(gmt_logging_patch_script,
          "f\"{pioenv}/espframework/src\"",
          "logging patch uses the active environment dependency directory");
  require(gmt_sync_wifi_patch_script, "TARGET_ENVS = frozenset({",
          "synchronous Wi-Fi patch must use a bounded environment whitelist");
  require(gmt_sync_wifi_patch_script, "'gmt154-06-sync-wifi-scan-diag'",
          "synchronous Wi-Fi patch whitelist must include its baseline");
  require(gmt_sync_wifi_patch_script,
          "'gmt154-06-sync-wifi-scan-diag-ble-init-off'",
          "synchronous Wi-Fi patch whitelist must include BLE-init-off");
  require(gmt_sync_wifi_patch_script, "pioenv not in TARGET_ENVS",
          "synchronous Wi-Fi patch must fail closed for other environments");
  require(gmt_sync_wifi_patch_script,
          "/ pioenv / 'espframework/src/basewebserver.cpp'",
          "synchronous Wi-Fi patch must use the active environment dependency path");
  const std::string c2b_section =
      section_after(platform, "[env:gmt154-06-c2b-diag]", "\n[env:");
  require(c2b_section, "pre:script/board.py",
          "C2b runs normal board setup");
  require(c2b_section, "pre:script/patch_tft_espi_gmt154.py",
          "C2b runs production TFT no-MISO patch");
  require(c2b_section, "pre:script/patch_espframework_gmt154_logging.py",
          "C2b runs production logging guard patch");
  require(c2b_section, "firmware-gmt154-06-c2b-diag.bin",
          "C2b has a unique CFG_FILENAMEBIN runtime identity");
  require(c2b_section, "GMT154_AUDIO_STD_INIT_DIAG=1",
          "C2b environment enables only its standard-init diagnostic");
  const std::string c2c_section =
      section_after(platform, "[env:gmt154-06-c2c-diag]", "\n[env:");
  require(c2c_section, "extends = env:gmt154-06",
          "C2c inherits the production GMT154 environment");
  require(c2c_section, "pre:script/board.py",
          "C2c runs normal board setup");
  require(c2c_section, "pre:script/patch_tft_espi_gmt154.py",
          "C2c runs production TFT no-MISO patch");
  require(c2c_section, "pre:script/patch_espframework_gmt154_logging.py",
          "C2c runs production logging guard patch");
  require(c2c_section, "firmware-gmt154-06-c2c-diag.bin",
          "C2c has a unique CFG_FILENAMEBIN runtime identity");
  require(c2c_section, "GMT154_AUDIO_STD_ENABLE_DIAG=1",
          "C2c environment enables only the standard-init-and-enable diagnostic");
  const std::string sync_wifi_section = section_after(
      platform, "[env:gmt154-06-sync-wifi-scan-diag]", "\n[");
  require(sync_wifi_section, "extends = env:gmt154-06",
          "synchronous Wi-Fi diagnostic inherits the complete GMT154 environment");
  require(sync_wifi_section, "pre:script/board.py",
          "synchronous Wi-Fi diagnostic runs normal board setup");
  require(sync_wifi_section, "pre:script/patch_tft_espi_gmt154.py",
          "synchronous Wi-Fi diagnostic applies the GMT154 TFT patch");
  require(sync_wifi_section,
          "pre:script/patch_espframework_gmt154_logging.py",
          "synchronous Wi-Fi diagnostic applies the GMT154 logging patch");
  require(sync_wifi_section,
          "pre:script/patch_espframework_gmt154_sync_wifi_diag.py",
          "synchronous Wi-Fi diagnostic applies the synchronous scan patch");
  require(sync_wifi_section, "${gmt154_env_data.build_flags}",
          "synchronous Wi-Fi diagnostic uses filename-free GMT154 shared flags");
  reject(sync_wifi_section, "-U CFG_FILENAMEBIN",
         "synchronous Wi-Fi diagnostic must not pass a malformed multi-token undef");
  require(sync_wifi_section,
          "firmware-gmt154-06-sync-wifi-scan-diag.bin",
          "synchronous Wi-Fi diagnostic defines its filename exactly once");
  reject(sync_wifi_section, "pre:script/patch_espframework_gmt154_wifi_scan.py",
         "synchronous Wi-Fi diagnostic must retain synchronous scan behavior");
  const std::string ble_init_off_section = section_after(
      platform, "[env:gmt154-06-sync-wifi-scan-diag-ble-init-off]", "\n[");
  require(ble_init_off_section,
          "extends = env:gmt154-06-sync-wifi-scan-diag",
          "BLE-init-off environment inherits the synchronous Wi-Fi diagnostic");
  reject(ble_init_off_section, "extra_scripts =",
         "BLE-init-off environment must inherit the complete patch script list");
  require(ble_init_off_section, "${gmt154_env_data.build_flags}",
          "BLE-init-off environment uses filename-free GMT154 shared flags");
  require(ble_init_off_section, "GMT154_06_BLE_INIT_OFF_DIAG=1",
          "BLE-init-off environment selects its single diagnostic macro");
  reject(ble_init_off_section, "-U CFG_FILENAMEBIN",
         "BLE-init-off environment must not pass a malformed multi-token undef");
  require(ble_init_off_section,
          "firmware-gmt154-06-sync-wifi-scan-diag-ble-init-off.bin",
          "BLE-init-off environment has a unique firmware filename");
  reject(sync_wifi_section, "GMT154_06_BLE_INIT_OFF_DIAG=1",
         "synchronous Wi-Fi baseline must not disable BLE initialization");
  require_ordered(
      main_gateway,
      {"bleScanner.setScanTime(myConfig.getBleScanTime());",
       "bleScanner.setAllowActiveScan(myConfig.getBleActiveScan());",
       "#if defined(GMT154_06) && defined(GMT154_06_BLE_INIT_OFF_DIAG) && \\\n    GMT154_06_BLE_INIT_OFF_DIAG == 1",
       "#else", "bleScanner.init();", "#endif"},
      "BLE configuration stays intact and only GMT154 diagnostic builds skip scanner init");
  require(gmt_board, "GMT154-06", "GMT154 board name");
  require(gmt_board, "BOARD_HAS_PSRAM", "GMT154 board PSRAM support");
  require(gmt_board, "BOARD_PSRAM_SIZE=8388608", "GMT154 8MB PSRAM");
  require(gmt_board, "\"flash_size\": \"16MB\"",
          "GMT154 16MB flash");
  require(display_source, "#if defined(GMT154_06)",
          "display source must have an explicit GMT154 branch");
  reject(display_source, "applyGmt154PostInitOverrides",
         "GMT154 must remove the post-V2 override helper");
  reject(display_source, "0xB2", "GMT154 must remove vendor B2 overrides");
  reject(display_source, "0xE7", "GMT154 must remove vendor E7 overrides");
  require(display_source, "#elif defined(GMT154_06)",
          "display setup must explicitly bypass CST328 for GMT154");
  require(display_source, "#if !defined(GMT154_06)",
          "GMT154 must not register a LVGL pointer indev");
  require_ordered(
      display_source,
      {"_display = lv_display_create(TFT_WIDTH, TFT_HEIGHT);",
       "lv_display_set_user_data(_display, _tft);",
       "lv_display_set_flush_cb(_display, gmt154_display::flush);"},
      "GMT154 LVGL flush must reuse the initialized TFT instance");
  require(display_source, "_display = lv_tft_espi_create",
          "non-GMT154 targets must retain the LVGL TFT_eSPI helper path");
  require(display_source, "heap_caps_get_free_size(MALLOC_CAP_SPIRAM)",
          "GMT154 must log PSRAM free heap");
  require(gmt_adapter_source, "BufferAllocation allocateDrawBuffer(size_t bytes)",
          "GMT154 must expose the runtime buffer allocation policy");
  require_ordered(
      gmt_adapter_source,
      {"void* buffer = ps_malloc(bytes);",
       "heap_caps_malloc(bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)"},
      "GMT154 allocation must prefer PSRAM before internal 8-bit heap");
  require(gmt_adapter_source, "BufferSource::Internal",
          "GMT154 fallback must report internal buffer ownership");
  require(display_source, "DIAG: DISP LVGL buffer source=",
          "GMT154 must log the selected LVGL buffer source");
  require(display_source, "if (_display == nullptr)",
          "display creation must be null-guarded");
  require(display_source, "free(draw_buf);",
          "display creation failure must release the draw buffer");
  require_ordered(
      display_source,
      {"DIAG: DISP stage=setup.begin.",
       "DIAG: DISP stage=panel.init.begin.",
       "DIAG: DISP stage=panel.init.done.",
       "DIAG: DISP stage=rotation.begin.",
       "DIAG: DISP stage=rotation.done.",
       "DIAG: DISP stage=clear.begin.",
       "DIAG: DISP stage=clear.done.",
       "DIAG: DISP stage=touch.init.begin.",
       "DIAG: DISP stage=touch.init.done.",
       "DIAG: DISP stage=setup.done."},
      "display initialization markers must be present and ordered");
  require_ordered(
      display_source,
      {"gmt154Backlight.initialize_off();",
       "_tft->init();",
       "clear(TFT_BLACK);",
       "gmt154Backlight.turn_on();"},
      "GMT154 backlight must bracket panel init and first clear");
  require_ordered(
      display_source,
      {"DIAG: DISP stage=rotation.begin.",
       "DIAG: DISP stage=rotation.done."},
      "display rotation markers must bracket rotation");
  require_ordered(
      display_source,
      {"DIAG: DISP stage=clear.begin.",
       "DIAG: DISP stage=clear.done."},
      "display clear markers must bracket clear");
  require(orientation_source, "#if defined(GMT154_06)",
          "GMT154 orientation must be fixed");
  require(orientation_source, "return GatewayOrientation::Landscape;",
          "GMT154 must not enable orientation rotation");
  require(cuckoo_screen_source, "width == 240 && height == 240",
          "Cuckoo UI must define a square layout");

  // --- Isolated build support: no legacy build-config coupling ---
  reject(platform, "script/source_build_id.py",
         "platformio.ini must not wire in script/source_build_id.py");
  reject(platform, "platformio.ascii-build.ini",
         "platformio.ini must not reference an ASCII build config");
  reject(copy, ".pio/build",
         "copy_firmware.py must not hard-code .pio/build");
  require(copy, "SKIP_FIRMWARE_COPY",
          "firmware copy must be disableable via SKIP_FIRMWARE_COPY");
  require(copy, "os.environ.get(\"SKIP_FIRMWARE_COPY\") == \"1\"",
          "firmware copy must honor the SKIP_FIRMWARE_COPY environment");
  require(copy, "env.subst(\"$BUILD_DIR\")",
          "copy_firmware.py must use $BUILD_DIR for artifact paths");

  // --- Single audio task and single I2S writer across all src files ---
  const std::vector<std::string> src_files = list_src_cpp();
  if (src_files.empty()) return 1;
  for (const std::string& name : src_files) {
    const std::string source = read_file(("src/" + name).c_str());
    if (name == "gateway_audio.cpp") {
      if (count_occurrences(source, "i2s_channel_write(") == 0) {
        std::cerr << "gateway_audio.cpp must own the I2S writer\n";
        ++failures;
      }
      if (count_occurrences(source, "xTaskCreatePinnedToCore(") != 1) {
        std::cerr << "gateway_audio.cpp must create exactly one audio task\n";
        ++failures;
      }
    } else {
      reject(source, "i2s_channel_write(",
             (name + " must not write I2S directly").c_str());
    }
    reject(source, "erase_flash",
           (name + " must not erase flash").c_str());
    reject(source, "LittleFS.format",
           (name + " must not format LittleFS").c_str());
  }

  // --- Alarm runtime files must not write LittleFS/config state ---
  for (const char* what :
       {"gateway_alarm.cpp", "gateway_audio.cpp", "gateway_alert_state.cpp",
        "gateway_alarm_lifecycle.cpp"}) {
    const std::string source = read_file((std::string("src/") + what).c_str());
    reject(source, "LittleFS", "alarm runtime must not touch LittleFS");
    reject(source, "myConfig", "alarm runtime must not touch myConfig");
    reject(source, "upload(", "alarm runtime must not upload");
  }
  reject(lifecycle_source, "i2s",
         "alarm lifecycle must remain state/action-only");
  reject(lifecycle_source, "I2S",
         "alarm lifecycle must remain state/action-only");
  reject(lifecycle_source, "lvgl",
         "alarm lifecycle must not reference LVGL");
  reject(lifecycle_source, "lv_",
         "alarm lifecycle must not reference LVGL");
  reject(lifecycle_source, "malloc",
         "alarm lifecycle must not allocate");
  reject(lifecycle_source, "calloc",
         "alarm lifecycle must not allocate");
  reject(lifecycle_source, "realloc",
         "alarm lifecycle must not allocate");
  reject(lifecycle_source, "free(",
         "alarm lifecycle must not allocate");
  reject(lifecycle_source, "new ",
         "alarm lifecycle must not allocate");

  // --- Main queue/cancel and UI exact-CLOSE integration ---
  require(main_gateway, "gatewayAudio.queueAlarm(action.event)",
          "main must queue alarm events through GatewayAudio");
  require(main_gateway, "gatewayAudio.cancelByKeyResult(",
          "main must cancel alarms by key through GatewayAudio");
  require(main_gateway,
          "#if defined(ENABLE_MMC) || defined(ENABLE_SD)\n  if (!mySdStorage.hasCard())",
          "alarm trace storage must be conditional for no-SD boards");
  require_ordered(
      main_gateway,
      {"DIAG: MAIN stage=setup.begin.",
       "DIAG: MAIN stage=display.init.begin.",
       "DIAG: MAIN stage=display.init.done.",
       "DIAG: MAIN stage=filesystem.init.begin.",
       "DIAG: MAIN stage=filesystem.init.done.",
       "DIAG: MAIN stage=wifi.init.begin.",
       "DIAG: MAIN stage=wifi.init.done.",
       "DIAG: MAIN stage=config.load.begin.",
       "DIAG: MAIN stage=config.load.done.",
       "DIAG: MAIN stage=ui.init.begin.",
       "DIAG: MAIN stage=ui.init.done.",
       "DIAG: MAIN stage=setup.done."},
      "startup stage markers must be present and ordered");
  require(main_gateway, "DIAG: MAIN stage=runmode.result=wifi_setup.",
          "runMode wifi-setup result marker");
  require(main_gateway, "DIAG: MAIN stage=runmode.result=measurement.",
          "runMode measurement result marker");
  require(main_gateway, "DIAG: MAIN stage=ap.entry=wifi_setup.",
          "wifi-setup AP entry marker");
  require(main_gateway, "DIAG: MAIN stage=ap.entry=direct_ssid.",
          "direct-SSID AP entry marker");
  if (count_occurrences(main_gateway, "DIAG: MAIN stage=wifi.connect.begin.") !=
      2 ||
      count_occurrences(main_gateway, "DIAG: MAIN stage=wifi.connect.done.") !=
      2) {
    std::cerr << "wifi connect must have before/after markers for both paths\n";
    ++failures;
  }
  if (count_occurrences(main_gateway, "DIAG: MAIN stage=wifi.startAP.begin.") !=
      4 ||
      count_occurrences(main_gateway, "DIAG: MAIN stage=wifi.startAP.done.") !=
      4) {
    std::cerr << "startAP marker count must include fallback and target branches\n";
    ++failures;
  }
  require_ordered(
      main_gateway,
      {"DIAG: MAIN stage=ap.entry=wifi_setup.",
       "DIAG: MAIN stage=wifi.startAP.begin.",
       "myWifi.startAP();",
       "DIAG: MAIN stage=wifi.startAP.done."},
      "wifi-setup AP entry must bracket startAP");
  require_ordered(
      main_gateway,
      {"DIAG: MAIN stage=ap.entry=direct_ssid.",
       "DIAG: MAIN stage=wifi.connect.begin.",
       "myWifi.connect(false, WIFI_AP_STA);",
       "DIAG: MAIN stage=wifi.connect.done.",
       "DIAG: MAIN stage=wifi.startAP.begin.",
       "myWifi.startAP(WIFI_AP_STA);",
       "DIAG: MAIN stage=wifi.startAP.done."},
      "direct-SSID AP entry must bracket connect and startAP");
  require_ordered(
      main_gateway,
      {"if (!wifiConnected)", "DIAG: MAIN stage=wifi.fallback=setup_portal.",
       "DIAG: MAIN stage=wifi.startAP.begin.", "myWifi.startAP();",
       "DIAG: MAIN stage=wifi.startAP.done."},
      "C3 initial station failure must bracket fallback setup AP");
  require(ui_gateway, "gatewayAudio.cancelExactResult(local)",
          "UI must cancel alarms by exact token through GatewayAudio");
  require(alarm_header, "event_token",
          "alarm events must carry an exact CLOSE token");
  require(alarm_header, "GatewayAlarmCloseResult closeExact(",
          "alarm tracker must expose exact-token close");
  require(alarm_source, "GatewayAlarmTracker::closeExact(",
          "alarm tracker must implement exact-token close");
  require(audio_header, "bool queueAlarm(const GatewayAlarmEvent& event)",
          "audio module must accept alarm events");
  require(audio_header, "cancelExactResult(",
          "audio module must cancel by exact token");

  return failures == 0 ? 0 : 1;
}
