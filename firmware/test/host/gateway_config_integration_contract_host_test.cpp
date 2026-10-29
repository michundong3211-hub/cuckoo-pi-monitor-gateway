#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

static std::string read_file(const char* path) {
  std::ifstream input(path);
  std::ostringstream contents;
  contents << input.rdbuf();
  return contents.str();
}

int main() {
  const std::string header = read_file("src/config_gateway.hpp");
  const std::string source = read_file("src/config_gateway.cpp");
  const std::string main_gateway = read_file("src/main_gateway.cpp");
  const std::string ui = read_file("src/ui_gravitymon_gateway.cpp");
  const std::string settings = read_file("src/gateway_device_settings.cpp");
  const std::string storage = read_file("src/gateway_config_storage.cpp");

  assert(header.find("#include \"gateway_device_settings.hpp\"") !=
         std::string::npos);
  assert(header.find("MAX_GRAVITY_DEVICE_SETTINGS = 8") == std::string::npos);
  assert(header.find("GravityDeviceSettings _deviceSettings") !=
         std::string::npos);
  assert(header.find("copyDeviceSettings(") != std::string::npos);
  assert(header.find("copyDeviceSettingsOrDefault(") != std::string::npos);
  assert(header.find("saveDeviceSettings(") != std::string::npos);
  assert(header.find("bool saveFile() override") != std::string::npos);
  assert(header.find("createJsonWithDeviceSettings(") != std::string::npos);
  assert(header.find("SemaphoreHandle_t") != std::string::npos);

  assert(source.find("LittleFsConfigFileOperations") != std::string::npos);
  assert(source.find("VerifiedConfigStorage") != std::string::npos);
  if (source.find("#include <log.hpp>") == std::string::npos) {
    std::cerr << "config_gateway.cpp must include the project log header\n";
    return 1;
  }
  assert(storage.find(".tmp") != std::string::npos);
  assert(storage.find(".bak") != std::string::npos);
  assert(source.find("? nullptr") == std::string::npos);
  assert(source.find("saveDeviceSettings") != std::string::npos);
  assert(source.find("recoverConfigFile") != std::string::npos);
  assert(source.find("#include <gateway_device_settings_json.hpp>") !=
         std::string::npos);
  assert(source.find("append_gateway_device_settings_json(devices, _deviceSettings,") !=
         std::string::npos);
  assert(main_gateway.find("if (!myConfig.recoverConfigFile())") !=
         std::string::npos);
  assert(main_gateway.find("CFG : Config recovery failed; load/save disabled") !=
         std::string::npos);
  assert(settings.find("strlen(device_id)") == std::string::npos);

  assert(ui.find("findDeviceSettings(") == std::string::npos);
  assert(main_gateway.find("findDeviceSettings(") == std::string::npos);
  assert(ui.find("getDeviceSettings(") == std::string::npos);
  assert(main_gateway.find("getDeviceSettings(") == std::string::npos);
  return 0;
}
