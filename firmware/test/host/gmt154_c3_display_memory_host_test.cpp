#include "gmt154_display_adapter.hpp"

#include <iostream>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << "\n";
    ++failures;
  }
}

void testUsesInternalHeapWithoutProbingPsram() {
  fakeGmt154HeapReset();
  const gmt154_display::BufferAllocation allocation =
      gmt154_display::allocateDrawBuffer(11520);

  expect(allocation.buffer != nullptr, "C3 internal allocation succeeds");
  expect(allocation.source == gmt154_display::BufferSource::Internal,
         "C3 reports internal buffer source");
  expect(allocation.bytes == 11520, "C3 keeps the full draw buffer");
  expect(g_fake_gmt154_heap.psram_calls == 0,
         "C3 does not probe or allocate from PSRAM");
  expect(g_fake_gmt154_heap.internal_calls == 1,
         "C3 allocates from internal 8-bit heap");
  expect(g_fake_gmt154_heap.last_internal_caps ==
             (MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
         "C3 requests internal 8-bit memory");
  delete[] static_cast<uint8_t*>(allocation.buffer);
}

void testInternalFallbackCanReduceTheBuffer() {
  fakeGmt154HeapReset();
  g_fake_gmt154_heap.internal_fails = true;
  const gmt154_display::BufferAllocation allocation =
      gmt154_display::allocateDrawBuffer(11520);

  expect(allocation.buffer == nullptr,
         "reduced C3 buffer remains unavailable when internal heap fails");
  expect(g_fake_gmt154_heap.psram_calls == 0,
         "C3 fallback still avoids PSRAM");
  expect(g_fake_gmt154_heap.internal_calls == 2,
         "C3 retries with a reduced internal buffer");
}

}  // namespace

int main() {
  testUsesInternalHeapWithoutProbingPsram();
  testInternalFallbackCanReduceTheBuffer();
  return failures == 0 ? 0 : 1;
}
