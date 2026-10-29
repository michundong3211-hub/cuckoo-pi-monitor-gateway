#include "gateway_config_storage.hpp"

#include <cassert>
#include <cctype>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

class CorruptibleFakeConfigFileSystem : public FakeConfigFileSystem {
 public:
  bool isValidJson(const std::string& path) const override {
    if (path.size() >= 4 && path.compare(path.size() - 4, 4, ".tmp") == 0)
      return FakeConfigFileSystem::isValidJson(path);
    if (path.size() >= 4 && path.compare(path.size() - 4, 4, ".bak") == 0)
      return backup_file != "corrupt" && !backup_file.empty();
    return main_file != "corrupt" && !main_file.empty();
  }
};

class JsonValidatingFakeConfigFileSystem : public FakeConfigFileSystem {
 private:
  static bool hasSuffix(const std::string& path, const char* suffix) {
    const size_t suffix_size = std::strlen(suffix);
    return path.size() >= suffix_size &&
           path.compare(path.size() - suffix_size, suffix_size, suffix) == 0;
  }

  static bool isJsonObjectDocument(const std::string& json) {
    size_t first = 0;
    while (first < json.size() &&
           std::isspace(static_cast<unsigned char>(json[first]))) {
      ++first;
    }
    size_t last = json.size();
    while (last > first &&
           std::isspace(static_cast<unsigned char>(json[last - 1]))) {
      --last;
    }
    if (first == last || json[first] != '{' || json[last - 1] != '}')
      return false;

    int object_depth = 0;
    int array_depth = 0;
    size_t colon_count = 0;
    bool in_string = false;
    bool escaped = false;
    for (size_t i = first; i < last; ++i) {
      const char c = json[i];
      if (in_string) {
        if (escaped) {
          escaped = false;
        } else if (c == '\\') {
          escaped = true;
        } else if (c == '"') {
          in_string = false;
        }
        continue;
      }
      if (c == '"') {
        in_string = true;
      } else if (c == '{') {
        ++object_depth;
      } else if (c == '}') {
        if (--object_depth < 0) return false;
      } else if (c == '[') {
        ++array_depth;
      } else if (c == ']') {
        if (--array_depth < 0) return false;
      } else if (c == ':') {
        ++colon_count;
      }
    }
    if (in_string || escaped || object_depth != 0 || array_depth != 0)
      return false;

    bool has_content = false;
    for (size_t i = first + 1; i + 1 < last; ++i) {
      if (!std::isspace(static_cast<unsigned char>(json[i]))) {
        has_content = true;
        break;
      }
    }
    return !has_content || colon_count != 0;
  }

  const std::string& fileForPath(const std::string& path) const {
    if (hasSuffix(path, ".tmp")) return temp_file;
    if (hasSuffix(path, ".bak")) return backup_file;
    return main_file;
  }

 public:
  bool isValidJson(const std::string& path) const override {
    if (hasSuffix(path, ".tmp") && fail_temp_parse) return false;
    return isJsonObjectDocument(fileForPath(path));
  }
};

static std::string read_file(const char* path) {
  std::ifstream input(path);
  std::ostringstream contents;
  contents << input.rdbuf();
  return contents.str();
}

int main() {
  {
    JsonValidatingFakeConfigFileSystem validator;
    validator.main_file = "{}";
    assert(validator.isValidJson("/gravitymon-gw.json"));
    validator.main_file = "[]";
    assert(!validator.isValidJson("/gravitymon-gw.json"));
    validator.main_file = "1";
    assert(!validator.isValidJson("/gravitymon-gw.json"));
    validator.main_file = "{broken-json";
    assert(!validator.isValidJson("/gravitymon-gw.json"));
  }

  {
    CorruptibleFakeConfigFileSystem fs;
    fs.main_file = "corrupt";
    fs.backup_file = "valid-backup";
    VerifiedConfigStorage storage(fs, "/gravitymon-gw.json");

    const ConfigStorageResult result = storage.recover();
    assert(result.ok());
    assert(result.committed);
    assert(fs.main_file == "valid-backup");
    assert(fs.backup_file.empty());
  }

  {
    FakeConfigFileSystem fs;
    fs.temp_file = "valid-temporary-config";
    VerifiedConfigStorage storage(fs, "/gravitymon-gw.json");

    const ConfigStorageResult result = storage.recover();
    assert(result.ok());
    assert(result.committed);
    assert(fs.main_file == "valid-temporary-config");
    assert(fs.temp_file.empty());
  }

  {
    FakeConfigFileSystem fs;
    fs.main_file = "old-valid-json";
    fs.fail_remove = true;
    VerifiedConfigStorage storage(fs, "/gravitymon-gw.json");

    const ConfigStorageResult result =
        storage.replaceWithVerifiedJson("new-valid-json");
    assert(!result.ok());
    assert(result.committed);
    assert(result.error == ConfigStorageError::BackupCleanupFailed);
    assert(fs.main_file == "new-valid-json");
    assert(fs.backup_file == "old-valid-json");
  }

  {
    const std::string source = read_file("src/config_gateway.cpp");
    assert(source.find("CFG : Invalid device ID") != std::string::npos);
    assert(source.find("CFG : Device settings capacity full") !=
           std::string::npos);
    assert(source.find("!result.ok() && !result.committed") !=
           std::string::npos);
    assert(source.find("return result.committed") != std::string::npos);
  }

  FakeConfigFileSystem fs;
  fs.main_file = "original";
  VerifiedConfigStorage storage(fs, "/gravitymon-gw.json");

  ConfigStorageResult result =
      storage.replaceWithVerifiedJson("new-valid-json");
  assert(result.ok());
  assert(fs.main_file == "new-valid-json");
  assert(fs.backup_file.empty());
  assert(fs.temp_file.empty());

  fs.fail_short_write = true;
  result = storage.replaceWithVerifiedJson("short-write");
  assert(!result.ok());
  assert(result.error == ConfigStorageError::ShortWrite);
  assert(fs.main_file == "new-valid-json");
  fs.fail_short_write = false;

  fs.fail_temp_readback = true;
  result = storage.replaceWithVerifiedJson("malformed");
  assert(!result.ok());
  assert(result.error == ConfigStorageError::TemporaryReadbackFailed);
  assert(fs.main_file == "new-valid-json");
  fs.fail_temp_readback = false;

  {
    FakeConfigFileSystem invalid_temp_fs;
    invalid_temp_fs.main_file = "{\"version\":1}";
    invalid_temp_fs.fail_temp_parse = true;
    VerifiedConfigStorage invalid_temp_storage(
        invalid_temp_fs, "/gravitymon-gw.json");

    const ConfigStorageResult invalid_temp_result =
        invalid_temp_storage.replaceWithVerifiedJson("{broken-json");
    if (invalid_temp_result.ok() ||
        invalid_temp_result.error != ConfigStorageError::TemporaryJsonInvalid ||
        invalid_temp_result.committed ||
        invalid_temp_fs.main_file != "{\"version\":1}" ||
        !invalid_temp_fs.temp_file.empty()) {
      std::cerr << "invalid temporary JSON was promoted\n";
      return 1;
    }
  }

  {
    JsonValidatingFakeConfigFileSystem invalid_cleanup_fs;
    invalid_cleanup_fs.main_file = "{\"version\":1}";
    invalid_cleanup_fs.fail_temp_parse = true;
    invalid_cleanup_fs.fail_remove = true;
    VerifiedConfigStorage invalid_cleanup_storage(
        invalid_cleanup_fs, "/gravitymon-gw.json");

    const ConfigStorageResult invalid_cleanup_result =
        invalid_cleanup_storage.replaceWithVerifiedJson("{broken-json");
    if (invalid_cleanup_result.ok() ||
        invalid_cleanup_result.error !=
            ConfigStorageError::TemporaryCleanupFailed ||
        invalid_cleanup_result.committed ||
        invalid_cleanup_fs.main_file != "{\"version\":1}" ||
        invalid_cleanup_fs.temp_file != "{broken-json") {
      std::cerr << "invalid temporary cleanup failure was not preserved\n";
      return 1;
    }
  }

  result = storage.replaceWithVerifiedJson("{\"device_settings\":[]}");
  assert(result.ok());
  assert(fs.main_file == "{\"device_settings\":[]}");
  assert(fs.temp_file.empty());

  fs.fail_temp_readback = true;
  result = storage.replaceWithVerifiedJson("{\"device_settings\":[1]}");
  assert(!result.ok());
  assert(result.error == ConfigStorageError::TemporaryReadbackFailed);
  assert(fs.main_file == "{\"device_settings\":[]}");
  fs.fail_temp_readback = false;

  fs.fail_backup = true;
  result = storage.replaceWithVerifiedJson("backup-rename-failure");
  assert(!result.ok());
  assert(result.error == ConfigStorageError::BackupRenameFailed);
  assert(fs.main_file == "{\"device_settings\":[]}");
  fs.fail_backup = false;

  fs.fail_promote = true;
  result = storage.replaceWithVerifiedJson("rename-failure");
  assert(!result.ok());
  assert(result.error == ConfigStorageError::PromoteRenameFailed);
  assert(fs.main_file == "{\"device_settings\":[]}");
  assert(fs.backup_file.empty());
  fs.fail_promote = false;

  fs.main_file = "only-valid-copy";
  fs.fail_promote = true;
  fs.fail_restore = true;
  result = storage.replaceWithVerifiedJson("candidate");
  assert(!result.ok());
  assert(result.error == ConfigStorageError::RollbackRenameFailed);
  assert(fs.main_file.empty());
  assert(fs.backup_file == "only-valid-copy");

  fs.fail_promote = false;
  result = storage.replaceWithVerifiedJson("later-candidate");
  assert(!result.ok());
  assert(result.error == ConfigStorageError::RecoveryRestoreFailed);
  assert(fs.backup_file == "only-valid-copy");
  fs.fail_restore = false;

  fs.main_file.clear();
  fs.backup_file = "recovered";
  fs.temp_file = "stale";
  result = storage.recover();
  assert(result.ok());
  assert(fs.main_file == "recovered");
  assert(fs.backup_file.empty());
  assert(fs.temp_file.empty());

  fs.main_file = "valid-main";
  fs.backup_file = "stale-backup";
  fs.temp_file = "stale-temp";
  result = storage.recover();
  assert(result.ok());
  assert(fs.main_file == "valid-main");
  assert(fs.backup_file == "stale-backup");
  assert(fs.temp_file.empty());
  return 0;
}
