#pragma once

#include <cstddef>
#include <string>

enum class ConfigStorageError {
  None,
  RecoveryRestoreFailed,
  TemporaryWriteFailed,
  ShortWrite,
  TemporaryReadbackFailed,
  TemporaryJsonInvalid,
  TemporaryCleanupFailed,
  ExistingMainInvalid,
  BackupCleanupFailed,
  BackupRenameFailed,
  PromoteRenameFailed,
  RollbackRenameFailed,
};

struct ConfigStorageResult {
  ConfigStorageError error = ConfigStorageError::None;
  bool committed = false;
  bool ok() const { return error == ConfigStorageError::None; }
};

const char* config_storage_error_name(ConfigStorageError error);

class ConfigFileOperations {
 public:
  virtual ~ConfigFileOperations() = default;
  virtual bool exists(const std::string& path) const = 0;
  virtual size_t write(const std::string& path, const char* data,
                       size_t size) = 0;
  virtual bool remove(const std::string& path) = 0;
  virtual bool rename(const std::string& from, const std::string& to) = 0;
  virtual size_t fileSize(const std::string& path) const = 0;
  virtual size_t read(const std::string& path, size_t offset, char* data,
                      size_t size) const = 0;
  virtual bool isValidJson(const std::string& path) const = 0;
};

class VerifiedConfigStorage {
 public:
  VerifiedConfigStorage(ConfigFileOperations& file_system,
                        const std::string& main_path);

  ConfigStorageResult replaceWithVerifiedJson(const std::string& json);
  ConfigStorageResult replaceWithVerifiedJson(const char* json, size_t size);
  ConfigStorageResult recover();

 private:
  ConfigStorageResult cleanupTemporary();

  ConfigFileOperations& _file_system;
  std::string _main_path;
  std::string _temp_path;
  std::string _backup_path;
};

class FakeConfigFileSystem : public ConfigFileOperations {
 public:
  std::string main_file;
  std::string temp_file;
  std::string backup_file;
  bool fail_short_write = false;
  bool fail_temp_parse = false;
  bool fail_temp_readback = false;
  bool fail_promote = false;
  bool fail_restore = false;
  bool fail_backup = false;
  bool fail_remove = false;

  bool exists(const std::string& path) const override;
  size_t write(const std::string& path, const char* data,
               size_t size) override;
  bool remove(const std::string& path) override;
  bool rename(const std::string& from, const std::string& to) override;
  size_t fileSize(const std::string& path) const override;
  size_t read(const std::string& path, size_t offset, char* data,
              size_t size) const override;
  bool isValidJson(const std::string& path) const override;

 private:
  static bool isTempPath(const std::string& path);
  static bool isBackupPath(const std::string& path);
  std::string& fileFor(const std::string& path);
  const std::string& fileFor(const std::string& path) const;
};
