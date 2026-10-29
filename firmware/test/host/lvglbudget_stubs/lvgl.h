#pragma once
#include <cassert>
#include <cstddef>
struct lv_mem_monitor_t { size_t total_size, free_size, free_biggest_size, max_used; };
inline bool fakeReady=false, fakeLocked=false;
inline int fakeReadyQueries=0, fakeMonitors=0;
inline bool lv_is_initialized() {assert(fakeLocked);++fakeReadyQueries;return fakeReady;}
inline void lv_mem_monitor(lv_mem_monitor_t* p) {assert(fakeLocked);++fakeMonitors;*p={32768,12345,10000,22000};}
