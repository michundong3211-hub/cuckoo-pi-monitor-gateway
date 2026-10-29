#pragma once

#include <stddef.h>
#include <stdint.h>

constexpr uint32_t MALLOC_CAP_SPIRAM = 1u;
constexpr uint32_t MALLOC_CAP_INTERNAL = 2u;
constexpr uint32_t MALLOC_CAP_8BIT = 4u;

struct FakeGmt154HeapState {
  bool psram_fails = false;
  bool internal_fails = false;
  int psram_calls = 0;
  int internal_calls = 0;
  size_t last_psram_bytes = 0;
  size_t last_internal_bytes = 0;
  uint32_t last_internal_caps = 0;
};

inline FakeGmt154HeapState g_fake_gmt154_heap;

inline void fakeGmt154HeapReset() {
  g_fake_gmt154_heap = FakeGmt154HeapState{};
}

inline void* ps_malloc(size_t bytes) {
  ++g_fake_gmt154_heap.psram_calls;
  g_fake_gmt154_heap.last_psram_bytes = bytes;
  if (g_fake_gmt154_heap.psram_fails) return nullptr;
  return new uint8_t[bytes];
}

inline void* heap_caps_malloc(size_t bytes, uint32_t caps) {
  ++g_fake_gmt154_heap.internal_calls;
  g_fake_gmt154_heap.last_internal_bytes = bytes;
  g_fake_gmt154_heap.last_internal_caps = caps;
  if (g_fake_gmt154_heap.internal_fails) return nullptr;
  return new uint8_t[bytes];
}

inline size_t heap_caps_get_free_size(uint32_t) { return 0; }
inline size_t heap_caps_get_largest_free_block(uint32_t) { return 0; }
