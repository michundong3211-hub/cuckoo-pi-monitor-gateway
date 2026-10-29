#include "lvglbudget_stubs/lvgl.h"
#define GMT154_06_C3 1
#include "gmt154_lvgl_budget_probe.hpp"
#include <cassert>
#include <cstring>
#include <cstdint>
int main() {
  using namespace gmt154;
  LvglBudgetSampler actual;
  LvglBudgetSnapshot native{};
  assert(!captureLvglBudget(actual,false,0,native));
  assert(fakeReadyQueries==0 && fakeMonitors==0 && fakeStackQueries==0);
  fakeLocked=true;
  assert(!captureLvglBudget(actual,true,0,native) && !native.valid);
  fakeReady=true;
  assert(captureLvglBudget(actual,true,1,native));
  assert(native.total==32768 && native.free==12345 && native.biggest==10000 && native.maxUsed==22000);
  assert(native.stackMinimumBytes==4321 && fakeMonitors==1 && fakeStackQueries==1);
  assert(!captureLvglBudget(actual,true,30000,native));
  fakeLocked=false;
  assert(!captureLvglBudget(actual,false,30001,native) && !native.valid);
  fakeLocked=true;
  assert(captureLvglBudget(actual,true,30002,native));
  fakeLocked=false;

  LvglBudgetSampler sampler;
  LvglBudgetSnapshot out{};
  int calls=0;
  auto sample=[&](LvglBudgetSnapshot& s) { ++calls;s.total=32768;s.free=100;s.biggest=80;s.maxUsed=30000;s.stackMinimumBytes=4321; };
  assert(!sampler.trySample(false,true,true,0,out,sample) && calls==0);
  assert(!sampler.trySample(true,false,true,0,out,sample) && !out.valid);
  assert(!sampler.trySample(true,true,false,0,out,sample) && !out.valid);
  assert(sampler.trySample(true,true,true,1,out,sample) && out.valid && calls==1);
  assert(!sampler.trySample(true,true,true,30000,out,sample) && calls==1);
  assert(!sampler.trySample(true,true,false,30001,out,sample));
  assert(sampler.trySample(true,true,true,30002,out,sample) && calls==2);
  LvglBudgetSampler rollover;
  assert(rollover.trySample(true,true,true,UINT32_MAX-9,out,sample));
  assert(!rollover.trySample(true,true,true,10,out,sample));
  assert(rollover.trySample(true,true,true,30000,out,sample));
  char line[lvglBudgetLineCapacity];
  formatLvglBudget(line,sizeof(line),{});
  assert(std::strstr(line,"unknown") && !std::strstr(line,"total_bytes=0"));
  out={true,UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX};
  int len=formatLvglBudget(line,sizeof(line),out);
  assert(len>0 && static_cast<unsigned>(len)<sizeof(line));
  assert(std::strstr(line,"stack_min_free_bytes=4294967295"));
  out.stackMinimumBytes=4321;formatLvglBudget(line,sizeof(line),out);
  assert(std::strstr(line,"stack_min_free_bytes=4321"));
}
