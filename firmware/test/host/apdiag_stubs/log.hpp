#pragma once
#include <cassert>
#include <string>
#include <vector>
#include "freertos/portmacro.h"
inline std::vector<std::string> fakeLines;
struct FakeLog { void notice(const char* s) {assert(!criticalDepth);fakeLines.emplace_back(s);} void notice(const char*,const char* s) {assert(!criticalDepth);fakeLines.emplace_back(s);} void warning(const char*,const char* s) {notice("",s);} };
inline FakeLog Log;
