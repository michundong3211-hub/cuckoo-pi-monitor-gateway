#pragma once
#include <cassert>
using portMUX_TYPE=int;
#define portMUX_INITIALIZER_UNLOCKED 0
inline int criticalDepth=0, criticalEntries=0;
inline void enter(int*) { assert(criticalDepth==0);++criticalDepth;++criticalEntries; }
inline void leave(int*) { assert(criticalDepth==1);--criticalDepth; }
#define portENTER_CRITICAL(p) enter(p)
#define portEXIT_CRITICAL(p) leave(p)
