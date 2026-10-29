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
    std::cerr << "forbidden token: " << what << "\n";
    ++failures;
  }
}

std::string section_after(const std::string& source, const std::string& header) {
  const size_t start = source.find(header);
  if (start == std::string::npos) {
    std::cerr << "cannot find section " << header << "\n";
    ++failures;
    return {};
  }
  const size_t end = source.find("\n[", start + header.size());
  return source.substr(start, end == std::string::npos ? std::string::npos
                                                        : end - start);
}

void require_pair(const std::string& variant, const std::string& profile,
                  const char* variant_value, const char* profile_value,
                  const char* what) {
  const bool variant_matches = variant.find(variant_value) != std::string::npos;
  const bool profile_matches = profile.find(profile_value) != std::string::npos;
  if (!variant_matches || !profile_matches) {
    std::cerr << "profile/variant mismatch: " << what << "\n";
    ++failures;
  }
}

}  // namespace

int main() {
  const std::string platform = read_file("platformio.ini");
  const std::string board = read_file("boards/waveshare_s3_tft_35.json");
  const std::string variant =
      read_file("boards/waveshare_s3_tft_35/pins_arduino.h");
  const std::string profile = read_file("src/waveshare_35_profile.hpp");

  const std::string gateway35 =
      section_after(platform, "[env:gateway35s3wave-tft-sd]");
  const std::string gateway35_lvgl_flags =
      section_after(platform, "build_flags_lvgl_gateway35 =");
  require(gateway35, "[env:gateway35s3wave-tft-sd]", "3.5 environment exists");
  require(gateway35, "-D GATEWAY_35=1", "3.5 marker");
  require(gateway35, "-D PIN_VOLT=4", "3.5 voltage pin fallback");
  require(gateway35,
          "-D CFG_FILENAMEBIN=\"\\\"firmware35s3wave-tft.bin\\\"\"",
          "3.5 firmware filename fallback");
  require(gateway35, "-D TFT_WIDTH=320", "3.5 width");
  require(gateway35, "-D TFT_HEIGHT=480", "3.5 height");
  require(gateway35, "board = waveshare_s3_tft_35", "3.5 board");
  require(gateway35, "Arduino_GFX.git#v1.5.5", "pinned Arduino_GFX");
  require(gateway35, "lvgl#v9.4.0", "pinned LVGL");
  require(gateway35_lvgl_flags, "-D ENABLE_LVGL=1", "3.5 LVGL enabled");
  require(gateway35_lvgl_flags, "-D LV_COLOR_DEPTH=16",
          "3.5 RGB565 color depth");
  require(gateway35_lvgl_flags, "-D LV_FONT_MONTSERRAT_40=1",
          "3.5 existing fonts");
  reject(gateway35, "ENABLE_MMC", "3.5 SD/MMC disabled");
  reject(gateway35, "ENABLE_SD", "3.5 SD disabled");
  require(gateway35, "-D ENABLE_GATEWAY_AUDIO=1", "3.5 audio enabled");
  reject(gateway35, "LV_USE_TFT_ESPI", "3.5 TFT_eSPI integration disabled");
  reject(gateway35_lvgl_flags, "LV_USE_TFT_ESPI",
         "3.5 LVGL flags exclude TFT_eSPI");

  const std::string gateway28 =
      section_after(platform, "[env:gateway32s3wave-tft-sd]");
  require(gateway28, "-D TFT_WIDTH=240", "2.8 width preserved");
  require(gateway28, "-D TFT_HEIGHT=320", "2.8 height preserved");
  require(gateway28, "-D WAVESHARE_CST3530=1", "2.8 CST3530 preserved");
  require(gateway28, "-D ENABLE_MMC=1", "2.8 MMC preserved");
  require(gateway28, "-D ENABLE_GATEWAY_AUDIO=1", "2.8 audio preserved");
  reject(gateway28, "GATEWAY_35", "2.8 isolated from 3.5 marker");

  require(board, "SKU30733", "board SKU30733 name");
  require(board, "\"flash_size\": \"16MB\"", "board 16 MB flash");
  require(board, "\"maximum_size\": 16777216", "board 16 MB maximum size");
  require(board, "\"memory_type\": \"qio_opi\"", "board OPI PSRAM mode");
  require(board, "-DBOARD_HAS_PSRAM", "board PSRAM flag");
  require(board, "-DBOARD_PSRAM_SIZE=8388608", "board 8 MB PSRAM size");
  require(board, "\"variant\": \"waveshare_s3_tft_35\"", "board variant");

  require(variant, "static const uint8_t SDA = 8;", "variant SDA");
  require(variant, "static const uint8_t SCL = 7;", "variant SCL");
  require(variant, "static const uint8_t MOSI = 1;", "variant MOSI");
  require(variant, "static const uint8_t MISO = 2;", "variant MISO");
  require(variant, "static const uint8_t SCK = 5;", "variant SCK");
  reject(variant, "LED_BUILTIN", "variant must not invent a physical LED pin");

  require(gateway35, "-D TOUCH_CS=-1", "3.5 legacy touch log placeholder");
  require(gateway35, "-D TFT_BL=6", "3.5 legacy backlight log value");
  require(gateway35, "-D TFT_DC=3", "3.5 legacy DC log value");
  require(gateway35, "-D TFT_MISO=2", "3.5 legacy MISO log value");
  require(gateway35, "-D TFT_MOSI=1", "3.5 legacy MOSI log value");
  require(gateway35, "-D TFT_SCLK=5", "3.5 legacy SCLK log value");
  require(gateway35, "-D TFT_RST=-1", "3.5 legacy reset log value");
  require(gateway35, "-D TFT_CS=-1", "3.5 legacy CS log value");
  require(gateway35, "-D LED_BUILTIN=-1", "3.5 OTA LED pin disabled");
  require(gateway35, "-D ESPFWK_DISABLE_LED=1", "3.5 framework LED disabled");
  require(read_file("src/sdcard_sd.hpp"), "gateway35_storage_compat.hpp",
          "3.5 storage facade is connected through sdcard_sd.hpp");

  require(profile, "kSku[] = \"30733\"", "profile SKU");
  require(profile, "kDisplayWidth = 320", "profile display width");
  require(profile, "kDisplayHeight = 480", "profile display height");
  require(profile, "kLcdBacklightPin = 6", "profile backlight");
  require(profile, "kSpiMisoPin = 2", "profile MISO");
  require(profile, "kSpiMosiPin = 1", "profile MOSI");
  require(profile, "kSpiSclkPin = 5", "profile SCLK");
  require(profile, "kLcdDcPin = 3", "profile DC");
  require(profile, "kLcdCsPin = -1", "profile CS");
  require(profile, "kLcdResetPin = -1", "profile reset");
  require(profile, "kI2cSdaPin = 8", "profile SDA");
  require(profile, "kI2cSclPin = 7", "profile SCL");
  require(profile, "kFt6336Address = 0x38", "profile FT6336 address");
  require(profile, "kTca9554Address = 0x20", "profile TCA9554 address");
  require(profile, "kTca9554Exio1Mask = 0x02", "profile EXIO1 reset mask");
  require(profile, "EXIO1", "profile EXIO1 name");

  require_pair(variant, profile, "SDA = 8", "kI2cSdaPin = 8",
               "SDA must match profile");
  require_pair(variant, profile, "SCL = 7", "kI2cSclPin = 7",
               "SCL must match profile");
  require_pair(variant, profile, "MOSI = 1", "kSpiMosiPin = 1",
               "MOSI must match profile");
  require_pair(variant, profile, "MISO = 2", "kSpiMisoPin = 2",
               "MISO must match profile");
  require_pair(variant, profile, "SCK = 5", "kSpiSclkPin = 5",
               "SCK must match profile");

  return failures == 0 ? 0 : 1;
}
