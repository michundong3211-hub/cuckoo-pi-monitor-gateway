#pragma once
#include "settings_freeze_diag.hpp"
#include <lvgl.h>
namespace freeze_diag {
inline void recordDisplayEvent(lv_event_code_t code) {
  switch(code) {
    case LV_EVENT_FLUSH_START: enter(Flush); break;
    case LV_EVENT_FLUSH_FINISH: leave(Flush); break;
    case LV_EVENT_FLUSH_WAIT_START: enter(FlushWait); break;
    case LV_EVENT_FLUSH_WAIT_FINISH: leave(FlushWait); break;
    case LV_EVENT_REFR_START: enter(Refresh); break;
    case LV_EVENT_REFR_READY: leave(Refresh); break;
    case LV_EVENT_RENDER_START: enter(Render); break;
    case LV_EVENT_RENDER_READY: leave(Render); break;
    default: break;
  }
}
}
