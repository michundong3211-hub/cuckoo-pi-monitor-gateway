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
  const std::string source = read_file("src/config_gateway.cpp");
  const std::string start =
      "void GravmonGatewayConfig::createJsonWithDeviceSettings(";
  const std::string end = "void GravmonGatewayConfig::setDeviceSettings(";
  const size_t start_offset = source.find(start);
  const size_t end_offset = source.find(end, start_offset);
  if (start_offset == std::string::npos || end_offset == std::string::npos) {
    std::cerr << "cannot isolate createJsonWithDeviceSettings()\n";
    return 1;
  }

  const std::string body = source.substr(start_offset, end_offset - start_offset);
  if (body.find("const char* normalizedDeviceId = normalized.device_id;") ==
          std::string::npos ||
      body.find("device[\"id\"] = normalizedDeviceId;") ==
          std::string::npos) {
    std::cerr << "normalized device id must be assigned through const char*\n";
    return 1;
  }
  if (body.find("device[\"id\"] = normalized.device_id;") !=
      std::string::npos) {
    std::cerr << "normalized device id must not be passed as a local array\n";
    return 1;
  }
  return 0;
}
