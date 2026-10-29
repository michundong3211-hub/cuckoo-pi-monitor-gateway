#pragma once

#if defined(GMT154_06)
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// Task-context logging only. One lock for every format type and output sink.
// Static storage avoids spending the already constrained heap on another lock.
inline SemaphoreHandle_t gmt154LoggingMutex() {
  static StaticSemaphore_t storage{};
  static SemaphoreHandle_t mutex = xSemaphoreCreateRecursiveMutexStatic(&storage);
  return mutex;
}

class Gmt154LogGuard {
 public:
  Gmt154LogGuard() { xSemaphoreTakeRecursive(gmt154LoggingMutex(), portMAX_DELAY); }
  ~Gmt154LogGuard() { xSemaphoreGiveRecursive(gmt154LoggingMutex()); }
  Gmt154LogGuard(const Gmt154LogGuard&) = delete;
  Gmt154LogGuard& operator=(const Gmt154LogGuard&) = delete;
};
#endif
