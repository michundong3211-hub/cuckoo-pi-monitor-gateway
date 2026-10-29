#pragma once
#include <cassert>
#include <cstdint>
inline int fakeStackQueries=0;
inline uint32_t uxTaskGetStackHighWaterMark(void* task) {assert(fakeLocked && task==nullptr);++fakeStackQueries;return 4321;}
