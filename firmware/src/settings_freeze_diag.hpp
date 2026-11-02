#pragma once
#include <Arduino.h>
#include <atomic>
#include <stdint.h>
#include <stddef.h>
namespace freeze_diag {
enum Stage { Gateway, Handler, Touch, Action, Rebuild, Flush, FlushWait, Refresh, Render, IndevRead, IndevCore, IndevReset, IndevPointer, IndevRelease, IndevShortClick, IndevEvent, RefrArea, DrawFlush, DrawWait, DrawDispatch, CuckooAction, CuckooRebuild, Count };
enum MemorySlot { ActionEnter, ActionExit, RebuildEnter, RebuildExit, MemoryCount };
struct Counter { std::atomic<uint32_t> in{0}, out{0}, start{0}, end{0}; };
struct Memory { std::atomic<uint32_t> free{0}, largest{0}, sampled{0}; };
constexpr unsigned PoolFieldCount=16;
struct State { std::atomic<uint32_t> pool[PoolFieldCount]{}; std::atomic<uint32_t> drawBufferBytes{0}, drawSource{0}, runtime[5]{}; Counter c[Count]; Memory memory[MemoryCount]; std::atomic<uint32_t> lastAction{0}, cuckooLastAction{0}, inputState{0}, inputContinue{0}, inputEvent{0}; std::atomic<uint32_t> allocCount[3]{}, allocLastOp{0}, allocLastSize{0}, allocLastTime{0}; std::atomic<int32_t> renderPhase{0}, renderMaxRow{0}, renderW{0}, renderH{0}, renderX1{0}, renderY1{0}, renderX2{0}, renderY2{0}; };
extern State state;
inline void enter(Stage s) { state.c[s].start.store(millis(),std::memory_order_relaxed); state.c[s].in.fetch_add(1,std::memory_order_relaxed); }
inline void leave(Stage s) { state.c[s].end.store(millis(),std::memory_order_relaxed); state.c[s].out.fetch_add(1,std::memory_order_relaxed); }
struct Scope { Stage s; explicit Scope(Stage x):s(x){enter(s);} ~Scope(){leave(s);} };
bool formatJson(char* output, size_t size, uint32_t now);
}
