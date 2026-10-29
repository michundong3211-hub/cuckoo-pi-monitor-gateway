#pragma once

#if defined(GMT154_06)

#include <stddef.h>
#include <stdint.h>

#include <TFT_eSPI.h>
#include <esp_heap_caps.h>
#include <lvgl.h>

namespace gmt154_display {

enum class BufferSource { None, Psram, Internal };

struct BufferAllocation {
  void* buffer;
  BufferSource source;
  size_t bytes;
};

inline BufferAllocation allocateDrawBuffer(size_t bytes) {
#if defined(GMT154_06_C3)
  void* buffer = heap_caps_malloc(bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (buffer != nullptr) {
    return {buffer, BufferSource::Internal, bytes};
  }

  bytes /= 2;
  buffer = heap_caps_malloc(bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (buffer != nullptr) {
    return {buffer, BufferSource::Internal, bytes};
  }
#else
  void* buffer = ps_malloc(bytes);
  if (buffer != nullptr) {
    return {buffer, BufferSource::Psram, bytes};
  }

  bytes /= 2;
  buffer = heap_caps_malloc(bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (buffer != nullptr) {
    return {buffer, BufferSource::Internal, bytes};
  }
#endif

  return {nullptr, BufferSource::None, 0};
}

inline void registerDrawBuffer(lv_display_t* display,
                               const BufferAllocation& allocation) {
  if (display == nullptr || allocation.buffer == nullptr ||
      allocation.bytes == 0) {
    return;
  }

  lv_display_set_buffers(display, allocation.buffer, nullptr,
                         allocation.bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
}

inline const char* bufferSourceName(BufferSource source) {
  switch (source) {
    case BufferSource::Psram:
      return "psram";
    case BufferSource::Internal:
      return "internal";
    case BufferSource::None:
    default:
      return "none";
  }
}

inline void flush(lv_display_t* display, const lv_area_t* area,
                  uint8_t* px_map) {
  if (display == nullptr) return;

  TFT_eSPI* tft =
      static_cast<TFT_eSPI*>(lv_display_get_user_data(display));
  if (tft == nullptr || area == nullptr || px_map == nullptr) {
    lv_display_flush_ready(display);
    return;
  }

  const uint32_t width = static_cast<uint32_t>(area->x2 - area->x1 + 1);
  const uint32_t height = static_cast<uint32_t>(area->y2 - area->y1 + 1);
  tft->startWrite();
  tft->setAddrWindow(area->x1, area->y1, width, height);
  tft->pushColors(reinterpret_cast<uint16_t*>(px_map), width * height, true);
  tft->endWrite();
  lv_display_flush_ready(display);
}

}  // namespace gmt154_display

#endif  // defined(GMT154_06)
