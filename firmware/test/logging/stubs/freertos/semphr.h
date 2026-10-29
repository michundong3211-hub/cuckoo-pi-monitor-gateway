#pragma once
#include "FreeRTOS.h"
struct StaticSemaphore_t { CRITICAL_SECTION section; };
using SemaphoreHandle_t = StaticSemaphore_t*;
inline SemaphoreHandle_t xSemaphoreCreateRecursiveMutexStatic(StaticSemaphore_t* storage) {
  InitializeCriticalSection(&storage->section);
  return storage;
}
inline BaseType_t xSemaphoreTakeRecursive(SemaphoreHandle_t mutex, unsigned) {
  EnterCriticalSection(&mutex->section);
  return pdTRUE;
}
inline BaseType_t xSemaphoreGiveRecursive(SemaphoreHandle_t mutex) {
  LeaveCriticalSection(&mutex->section);
  return pdTRUE;
}
