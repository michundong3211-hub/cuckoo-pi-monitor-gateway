#pragma once
#include <stddef.h>
#include <stdint.h>
#include "psram_budget.hpp"

namespace psram_lvgl {
constexpr size_t kExtraBytes = 256U * 1024U;
constexpr size_t kMinimumInitialLargest = 1024U;
enum Status : uint32_t { NotAttempted, Registered, NoPsram, InitialPoolLow, AllocFailed, Unaligned, AddFailed, BudgetLow };
struct Monitor { uint32_t total, free, largest; };
struct Snapshot {
  uint32_t status=NotAttempted, requested=kExtraBytes;
  uint32_t internalBefore=0, internalAfter=0;
  uint32_t psramTotal=0, psramBefore=0, psramLargestBefore=0, psramAfter=0, psramLargestAfter=0;
  Monitor lvBefore{}, lvAfter{};
};
struct Context { bool attempted=false; void* buffer=nullptr; void* pool=nullptr; Snapshot snapshot{}; };

// Called exactly in single-threaded startup, after lv_init and before objects/tasks.
// Api::add returns NULL only when the TLSF pool was not added. Its subsequent
// pool-list allocation is protected by the initial 1 KiB largest-block check.
template<class Api> void initialize(Context& context, Api& api) {
  if(context.attempted) return;
  context.attempted=true;
  Snapshot& s=context.snapshot;
  s.internalBefore=api.internalFree();
  s.psramTotal=api.psramTotal();
  s.psramBefore=api.psramFree();
  s.psramLargestBefore=api.psramLargest();
  s.lvBefore=api.monitor();
  if(!s.psramTotal) s.status=NoPsram;
  else if(s.lvBefore.free < kMinimumInitialLargest || s.lvBefore.largest < kMinimumInitialLargest) s.status=InitialPoolLow;
  else {
    s.status=BudgetLow;
    const size_t candidates[] = {kExtraBytes,128U*1024U,64U*1024U};
    for(size_t bytes : candidates) {
      const size_t available=api.psramFree(), largest=api.psramLargest();
      if(!psram_budget::allows(s.psramTotal,available,largest,bytes)) continue;
      s.requested=bytes;
      void* memory=api.allocate(bytes);
      if(!memory) { s.status=AllocFailed; continue; }
      if((reinterpret_cast<uintptr_t>(memory) & 7U)!=0) { api.release(memory); s.status=Unaligned; continue; }
      void* pool=api.add(memory,bytes);
      if(!pool) { api.release(memory); s.status=AddFailed; continue; }
      context.buffer=memory; context.pool=pool; s.status=Registered; break;
    }
  }
  s.lvAfter=api.monitor();
  s.internalAfter=api.internalFree();
  s.psramAfter=api.psramFree();
  s.psramLargestAfter=api.psramLargest();
}
}
