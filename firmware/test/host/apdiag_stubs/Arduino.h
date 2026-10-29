#pragma once
#include <cstdint>
#include <cstddef>
#define F(s) s
#define CR "\n"
inline uint32_t fakeMillis = 0;
inline uint32_t millis() { return fakeMillis; }
