#include "settings_freeze_diag.hpp"
#include <stdarg.h>
#include <stdio.h>
namespace freeze_diag {
State state;
namespace {
bool append(char* out, size_t size, size_t& used, const char* format, ...) {
 if (used >= size) return false;
 va_list args; va_start(args,format);
 int n=vsnprintf(out+used,size-used,format,args);
 va_end(args);
 if(n<0 || static_cast<size_t>(n)>=size-used) return false;
 used+=static_cast<size_t>(n); return true;
}
}
bool formatJson(char* output, size_t size, uint32_t now) {
 if (!output || !size) return false;
 size_t used=0;
 if(!append(output,size,used,"{\"version\":6,\"now_ms\":%lu,\"stages\":{",static_cast<unsigned long>(now))) return false;
 const char* names[Count]={"gateway","handler","touch","action","rebuild","flush","flush_wait","refresh","render","indev_read","indev_core","indev_reset","indev_pointer","indev_release","indev_short_click","indev_event","refr_area","draw_flush","draw_wait","draw_dispatch","cuckoo_action","cuckoo_rebuild"};
 for(int i=0;i<Count;++i) {
  auto& c=state.c[i];
  if(!append(output,size,used,"%s\"%s\":[%lu,%lu,%lu,%lu]",i?",":"",names[i],
   static_cast<unsigned long>(c.in.load(std::memory_order_relaxed)),
   static_cast<unsigned long>(c.out.load(std::memory_order_relaxed)),
   static_cast<unsigned long>(c.start.load(std::memory_order_relaxed)),
   static_cast<unsigned long>(c.end.load(std::memory_order_relaxed)))) return false;
 }
 if(!append(output,size,used,"},\"last_action\":%lu,\"cuckoo_last_action\":%lu,\"memory\":{",static_cast<unsigned long>(state.lastAction.load(std::memory_order_relaxed)),static_cast<unsigned long>(state.cuckooLastAction.load(std::memory_order_relaxed)))) return false;
 const char* memNames[MemoryCount]={"action_enter","action_exit","rebuild_enter","rebuild_exit"};
 for(int i=0;i<MemoryCount;++i) {
  auto& m=state.memory[i];
  if(!append(output,size,used,"%s\"%s\":[%lu,%lu,%lu]",i?",":"",memNames[i],
   static_cast<unsigned long>(m.free.load(std::memory_order_relaxed)),
   static_cast<unsigned long>(m.largest.load(std::memory_order_relaxed)),
   static_cast<unsigned long>(m.sampled.load(std::memory_order_relaxed)))) return false;
 }
 if(!append(output,size,used,"},\"input\":[%lu,%lu,%lu],\"alloc\":[%lu,%lu,%lu,%lu,%lu,%lu],\"render_progress\":[%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld]",
 static_cast<unsigned long>(state.inputState.load(std::memory_order_relaxed)),static_cast<unsigned long>(state.inputContinue.load(std::memory_order_relaxed)),static_cast<unsigned long>(state.inputEvent.load(std::memory_order_relaxed)),
 static_cast<unsigned long>(state.allocCount[0].load(std::memory_order_relaxed)),static_cast<unsigned long>(state.allocCount[1].load(std::memory_order_relaxed)),static_cast<unsigned long>(state.allocCount[2].load(std::memory_order_relaxed)),static_cast<unsigned long>(state.allocLastOp.load(std::memory_order_relaxed)),static_cast<unsigned long>(state.allocLastSize.load(std::memory_order_relaxed)),static_cast<unsigned long>(state.allocLastTime.load(std::memory_order_relaxed)),
 static_cast<long>(state.renderPhase.load(std::memory_order_relaxed)),static_cast<long>(state.renderMaxRow.load(std::memory_order_relaxed)),static_cast<long>(state.renderW.load(std::memory_order_relaxed)),static_cast<long>(state.renderH.load(std::memory_order_relaxed)),static_cast<long>(state.renderX1.load(std::memory_order_relaxed)),static_cast<long>(state.renderY1.load(std::memory_order_relaxed)),static_cast<long>(state.renderX2.load(std::memory_order_relaxed)),static_cast<long>(state.renderY2.load(std::memory_order_relaxed)))) return false;
 if(!append(output,size,used,",\"psram_pool\":[")) return false;
 for(unsigned i=0;i<PoolFieldCount;++i) {
  if(!append(output,size,used,"%s%lu",i?",":"",static_cast<unsigned long>(state.pool[i].load(std::memory_order_acquire)))) return false;
 }
 if(!append(output,size,used,"],\"draw_buffer_bytes\":%lu,\"draw_source\":%lu,\"runtime_memory\":[%lu,%lu,%lu,%lu,%lu]}",static_cast<unsigned long>(state.drawBufferBytes.load(std::memory_order_acquire)),static_cast<unsigned long>(state.drawSource.load(std::memory_order_acquire)),static_cast<unsigned long>(state.runtime[0].load()),static_cast<unsigned long>(state.runtime[1].load()),static_cast<unsigned long>(state.runtime[2].load()),static_cast<unsigned long>(state.runtime[3].load()),static_cast<unsigned long>(state.runtime[4].load(std::memory_order_acquire)))) return false;
 return true;
}
}

extern "C" void freeze_indev_enter(unsigned id) { if(id < freeze_diag::Count) freeze_diag::enter(static_cast<freeze_diag::Stage>(id)); }
extern "C" void freeze_indev_leave(unsigned id) { if(id < freeze_diag::Count) freeze_diag::leave(static_cast<freeze_diag::Stage>(id)); }
extern "C" void freeze_indev_input(unsigned state, unsigned more) { freeze_diag::state.inputState.store(state,std::memory_order_relaxed); freeze_diag::state.inputContinue.store(more,std::memory_order_relaxed); }
extern "C" void freeze_indev_event(unsigned code) { freeze_diag::state.inputEvent.store(code,std::memory_order_relaxed); }

extern "C" void freeze_alloc_failure(unsigned op, unsigned size) {
 auto &s=freeze_diag::state; if(op>=1 && op<=3) s.allocCount[op-1].fetch_add(1,std::memory_order_relaxed);
 s.allocLastOp.store(op,std::memory_order_relaxed); s.allocLastSize.store(size,std::memory_order_relaxed); s.allocLastTime.store(millis(),std::memory_order_relaxed);
}
extern "C" void freeze_render_progress(int phase,int maxrow,int w,int h,int x1,int y1,int x2,int y2) {
 auto &s=freeze_diag::state; s.renderMaxRow.store(maxrow,std::memory_order_relaxed); s.renderW.store(w,std::memory_order_relaxed); s.renderH.store(h,std::memory_order_relaxed); s.renderX1.store(x1,std::memory_order_relaxed); s.renderY1.store(y1,std::memory_order_relaxed); s.renderX2.store(x2,std::memory_order_relaxed); s.renderY2.store(y2,std::memory_order_relaxed); s.renderPhase.store(phase,std::memory_order_relaxed);
}
