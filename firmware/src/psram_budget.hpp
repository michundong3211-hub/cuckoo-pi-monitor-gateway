#pragma once
#include <stddef.h>
#include <stdint.h>
namespace psram_budget {
constexpr size_t metadata_margin=4096;
inline size_t reserve(size_t total) { size_t quarter=total/4; return quarter>1024U*1024U?quarter:1024U*1024U; }
inline bool allows(size_t total,size_t free,size_t largest,size_t bytes) {
 const size_t keep=reserve(total);
 return total && largest>=bytes && free>=keep && free-keep>=metadata_margin && free-keep-metadata_margin>=bytes;
}
}
