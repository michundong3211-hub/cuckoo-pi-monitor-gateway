#ifndef SRC_GATEWAY35_STORAGE_COMPAT_HPP_
#define SRC_GATEWAY35_STORAGE_COMPAT_HPP_

// The 3.5 panel has no SD/MMC feature in this migration.  Several shared
// gateway translation units still reference the legacy storage facade even
// when ENABLE_SD/ENABLE_MMC is absent.  Inject this 3.5-only no-op facade at
// compile time so those references remain harmless without enabling a card
// driver or changing the shared business sources.
#if defined(GATEWAY_35)

#include <FS.h>
#include <LittleFS.h>

class Gateway35NoopStorage {
 public:
  bool hasCard() const { return false; }
  bool begin(...) { return false; }
  void end() {}
  File open(const String&, const char* = FILE_READ, bool = false) {
    return File();
  }
  bool exists(const String&) const { return false; }
  bool remove(const String&) { return false; }
  bool rename(const String&, const String&) { return false; }
  uint64_t totalBytes() const { return 0; }
  uint64_t usedBytes() const { return 0; }
  fs::FS& getFS() const { return LittleFS; }
};

static Gateway35NoopStorage mySdStorage;

#endif  // defined(GATEWAY_35)

#endif  // SRC_GATEWAY35_STORAGE_COMPAT_HPP_
