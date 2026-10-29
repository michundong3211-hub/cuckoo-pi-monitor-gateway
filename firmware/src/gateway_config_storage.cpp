#include "gateway_config_storage.hpp"

#include <cstring>
#include <utility>

namespace {
ConfigStorageResult success(bool committed = false) {
  ConfigStorageResult result;
  result.committed = committed;
  return result;
}
ConfigStorageResult failure(ConfigStorageError error, bool committed = false) {
  ConfigStorageResult result;
  result.error = error;
  result.committed = committed;
  return result;
}

bool readbackMatches(const ConfigFileOperations& file_system,
                     const std::string& path, const char* expected,
                     size_t expected_size) {
  if (!expected || expected_size == 0 || !file_system.exists(path) ||
      file_system.fileSize(path) != expected_size)
    return false;
  constexpr size_t kChunkSize = 128;
  char actual[kChunkSize];
  for (size_t offset = 0; offset < expected_size; offset += kChunkSize) {
    const size_t chunk =
        (expected_size - offset) < kChunkSize ? expected_size - offset
                                              : kChunkSize;
    if (file_system.read(path, offset, actual, chunk) != chunk ||
        std::memcmp(actual, expected + offset, chunk) != 0) {
      return false;
    }
  }
  return true;
}
}

const char* config_storage_error_name(ConfigStorageError error) {
  switch (error) {
    case ConfigStorageError::None:
      return "none";
    case ConfigStorageError::RecoveryRestoreFailed:
      return "recovery restore rename failed";
    case ConfigStorageError::TemporaryWriteFailed:
      return "temporary write failed";
    case ConfigStorageError::ShortWrite:
      return "temporary short write";
    case ConfigStorageError::TemporaryReadbackFailed:
      return "temporary readback verification failed";
    case ConfigStorageError::TemporaryJsonInvalid:
      return "temporary JSON verification failed";
    case ConfigStorageError::TemporaryCleanupFailed:
      return "temporary cleanup failed";
    case ConfigStorageError::ExistingMainInvalid:
      return "existing main JSON invalid";
    case ConfigStorageError::BackupCleanupFailed:
      return "backup cleanup failed";
    case ConfigStorageError::BackupRenameFailed:
      return "main to backup rename failed";
    case ConfigStorageError::PromoteRenameFailed:
      return "temporary promotion rename failed";
    case ConfigStorageError::RollbackRenameFailed:
      return "backup rollback rename failed";
  }
  return "unknown storage error";
}

VerifiedConfigStorage::VerifiedConfigStorage(ConfigFileOperations& file_system,
                                             const std::string& main_path)
    : _file_system(file_system),
      _main_path(main_path),
      _temp_path(main_path + ".tmp"),
      _backup_path(main_path + ".bak") {}

ConfigStorageResult VerifiedConfigStorage::cleanupTemporary() {
  if (_file_system.exists(_temp_path) && !_file_system.remove(_temp_path))
    return failure(ConfigStorageError::TemporaryCleanupFailed);
  return success();
}

ConfigStorageResult VerifiedConfigStorage::recover() {
  const bool main_exists = _file_system.exists(_main_path);
  const bool main_valid =
      main_exists && _file_system.isValidJson(_main_path);
  const bool temp_exists = _file_system.exists(_temp_path);
  const bool temp_valid =
      temp_exists && _file_system.isValidJson(_temp_path);
  const bool backup_exists = _file_system.exists(_backup_path);
  const bool backup_valid =
      backup_exists && _file_system.isValidJson(_backup_path);

  if (main_valid) {
    const ConfigStorageResult cleanup = cleanupTemporary();
    if (!cleanup.ok()) {
      return failure(cleanup.error, true);
    }
    return success(true);
  }

  if (backup_valid) {
    if (main_exists && !_file_system.remove(_main_path))
      return failure(ConfigStorageError::RecoveryRestoreFailed);
    if (!_file_system.rename(_backup_path, _main_path))
      return failure(ConfigStorageError::RecoveryRestoreFailed);

    const ConfigStorageResult cleanup = cleanupTemporary();
    if (!cleanup.ok()) {
      return failure(cleanup.error, true);
    }
    return success(true);
  }

  if (temp_valid) {
    if (main_exists && !_file_system.remove(_main_path))
      return failure(ConfigStorageError::RecoveryRestoreFailed);
    if (!_file_system.rename(_temp_path, _main_path))
      return failure(ConfigStorageError::RecoveryRestoreFailed);

    ConfigStorageResult result = success(true);
    if (backup_exists && !_file_system.remove(_backup_path)) {
      result.error = ConfigStorageError::BackupCleanupFailed;
      return result;
    }
    return result;
  }

  if (!main_exists && !temp_exists && !backup_exists) return success();

  if (temp_exists && !_file_system.remove(_temp_path))
    return failure(ConfigStorageError::TemporaryCleanupFailed);
  return failure(ConfigStorageError::ExistingMainInvalid);
}

ConfigStorageResult VerifiedConfigStorage::replaceWithVerifiedJson(
    const std::string& json) {
  return replaceWithVerifiedJson(json.data(), json.size());
}

ConfigStorageResult VerifiedConfigStorage::replaceWithVerifiedJson(
    const char* json, size_t size) {
  const ConfigStorageResult recovered = recover();
  if (!recovered.ok() && !recovered.committed) return recovered;

  const size_t written = _file_system.write(_temp_path, json, size);
  if (written == 0 && size != 0)
    return failure(ConfigStorageError::TemporaryWriteFailed);
  if (written != size) {
    cleanupTemporary();
    return failure(ConfigStorageError::ShortWrite);
  }
  if (!readbackMatches(_file_system, _temp_path, json, size)) {
    cleanupTemporary();
    return failure(ConfigStorageError::TemporaryReadbackFailed);
  }
  if (!_file_system.isValidJson(_temp_path)) {
    const ConfigStorageResult cleanup = cleanupTemporary();
    if (!cleanup.ok()) return failure(cleanup.error);
    return failure(ConfigStorageError::TemporaryJsonInvalid);
  }

  const bool had_main = _file_system.exists(_main_path);
  if (had_main && !_file_system.isValidJson(_main_path))
    return failure(ConfigStorageError::ExistingMainInvalid);
  if (had_main && _file_system.exists(_backup_path) &&
      !_file_system.remove(_backup_path))
    return failure(ConfigStorageError::BackupCleanupFailed);
  if (had_main && !_file_system.rename(_main_path, _backup_path))
    return failure(ConfigStorageError::BackupRenameFailed);

  if (!_file_system.rename(_temp_path, _main_path)) {
    if (had_main && !_file_system.rename(_backup_path, _main_path))
      return failure(ConfigStorageError::RollbackRenameFailed);
    cleanupTemporary();
    return failure(ConfigStorageError::PromoteRenameFailed);
  }

  ConfigStorageResult result = success(true);
  if (had_main && _file_system.exists(_backup_path) &&
      !_file_system.remove(_backup_path)) {
    result.error = ConfigStorageError::BackupCleanupFailed;
  }
  return result;
}

bool FakeConfigFileSystem::isTempPath(const std::string& path) {
  return path.size() >= 4 && path.compare(path.size() - 4, 4, ".tmp") == 0;
}

bool FakeConfigFileSystem::isBackupPath(const std::string& path) {
  return path.size() >= 4 && path.compare(path.size() - 4, 4, ".bak") == 0;
}

std::string& FakeConfigFileSystem::fileFor(const std::string& path) {
  if (isTempPath(path)) return temp_file;
  if (isBackupPath(path)) return backup_file;
  return main_file;
}

const std::string& FakeConfigFileSystem::fileFor(
    const std::string& path) const {
  if (isTempPath(path)) return temp_file;
  if (isBackupPath(path)) return backup_file;
  return main_file;
}

bool FakeConfigFileSystem::exists(const std::string& path) const {
  return !fileFor(path).empty();
}

size_t FakeConfigFileSystem::write(const std::string& path, const char* data,
                                   size_t size) {
  const size_t bytes = fail_short_write && size ? size - 1 : size;
  fileFor(path).assign(data, bytes);
  return bytes;
}

bool FakeConfigFileSystem::remove(const std::string& path) {
  if (fail_remove) return false;
  fileFor(path).clear();
  return true;
}

bool FakeConfigFileSystem::rename(const std::string& from,
                                  const std::string& to) {
  if (fail_promote && isTempPath(from) && !isBackupPath(to) &&
      !isTempPath(to))
    return false;
  if (fail_restore && isBackupPath(from) && !isBackupPath(to) &&
      !isTempPath(to))
    return false;
  if (fail_backup && !isTempPath(from) && !isBackupPath(from) &&
      isBackupPath(to))
    return false;
  std::string& source = fileFor(from);
  if (source.empty()) return false;
  fileFor(to) = std::move(source);
  source.clear();
  return true;
}

size_t FakeConfigFileSystem::fileSize(const std::string& path) const {
  return fileFor(path).size();
}

size_t FakeConfigFileSystem::read(const std::string& path, size_t offset,
                                  char* data, size_t size) const {
  if (!data || offset >= fileFor(path).size()) return 0;
  const std::string& file = fileFor(path);
  const size_t available = file.size() - offset;
  const size_t count = available < size ? available : size;
  std::memcpy(data, file.data() + offset, count);
  if (fail_temp_readback && isTempPath(path)) return count ? count - 1 : 0;
  return count;
}

bool FakeConfigFileSystem::isValidJson(const std::string& path) const {
  if (isTempPath(path) && fail_temp_parse) return false;
  return !fileFor(path).empty();
}
