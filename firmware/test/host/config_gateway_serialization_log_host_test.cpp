#include <cassert>
#include <config_gateway.hpp>
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

static size_t count_occurrences(const std::string& source,
                                const std::string& needle) {
  size_t count = 0;
  size_t offset = 0;
  while ((offset = source.find(needle, offset)) != std::string::npos) {
    ++count;
    offset += needle.size();
  }
  return count;
}

int main() {
  assert(gatewayConfigSavePreflight(false, false) ==
         GatewayConfigSavePreflightResult::Skip);
  assert(gatewayConfigSavePreflight(false, true) ==
         GatewayConfigSavePreflightResult::Skip);
  assert(gatewayConfigSavePreflight(true, false) ==
         GatewayConfigSavePreflightResult::Reject);
  assert(gatewayConfigSavePreflight(true, true) ==
         GatewayConfigSavePreflightResult::Proceed);
  bool storage_ready_queried = false;
  assert(gatewayConfigSavePreflight(false, [&storage_ready_queried]() {
           storage_ready_queried = true;
           return false;
         }) == GatewayConfigSavePreflightResult::Skip);
  assert(!storage_ready_queried);

  int backup_calls = 0;
  gatewayBackupAfterVerifiedCommit(false, [&backup_calls]() { ++backup_calls; });
  assert(backup_calls == 0);
  gatewayBackupAfterVerifiedCommit(true, [&backup_calls]() { ++backup_calls; });
  assert(backup_calls == 1);

  const std::string source = read_file("src/config_gateway.cpp");
  const std::string expected_arguments =
      "static_cast<uint64_t>(serialized), static_cast<uint64_t>(size));";

  if (source.find("#include <cstdint>") == std::string::npos) {
    std::cerr << "config_gateway.cpp must include <cstdint>\n";
    return 1;
  }
  if (count_occurrences(source, expected_arguments) != 2) {
    std::cerr << "both serialization failure logs must pass uint64_t values\n";
    return 1;
  }
  if (source.find("%llu") != std::string::npos ||
      source.find("%zu") != std::string::npos ||
      source.find("PRIu64") != std::string::npos) {
    std::cerr << "serialization logs changed ArduinoLog format semantics\n";
    return 1;
  }

  const size_t save_file_start =
      source.find("bool GravmonGatewayConfig::saveFile()");
  const size_t save_file_end =
      source.find("bool GravmonGatewayConfig::saveFileLocked()", save_file_start);
  const size_t preflight =
      source.find("gatewayConfigSavePreflight(", save_file_start);
  const size_t storage_ready = source.find("storageReady()", save_file_start);
  if (save_file_start == std::string::npos ||
      save_file_end == std::string::npos ||
      preflight == std::string::npos || storage_ready == std::string::npos ||
      preflight >= save_file_end || storage_ready >= save_file_end ||
      preflight > storage_ready) {
    std::cerr << "saveFile must short-circuit before storage-ready rejection\n";
    return 1;
  }

  if (count_occurrences(
          source, "backupWifiPreferencesAfterSuccessfulSave(") != 3 ||
      source.find("setPreference(\"ssid\"") == std::string::npos ||
      source.find("setPreference(\"pass\"") == std::string::npos) {
    std::cerr << "successful saves must share the Wi-Fi preference backup helper\n";
    return 1;
  }
  const size_t save_device_start =
      source.find("bool GravmonGatewayConfig::saveDeviceSettings(");
  const size_t save_file_locked_start =
      source.find("bool GravmonGatewayConfig::saveFileLocked()");
  const size_t save_device_commit =
      source.find("saveFileVerified", save_device_start);
  const size_t save_device_backup =
      source.find("gatewayBackupAfterVerifiedCommit", save_device_start);
  const size_t save_file_commit =
      source.find("saveFileVerified", save_file_locked_start);
  const size_t save_file_backup =
      source.find("gatewayBackupAfterVerifiedCommit", save_file_locked_start);
  if (save_device_start == std::string::npos ||
      save_file_locked_start == std::string::npos ||
      save_device_commit == std::string::npos ||
      save_device_backup == std::string::npos || save_file_commit == std::string::npos ||
      save_file_backup == std::string::npos || save_device_backup < save_device_commit ||
      save_file_backup < save_file_commit) {
    std::cerr << "Wi-Fi backup seam must follow verified commit in both save paths\n";
    return 1;
  }
  return 0;
}
