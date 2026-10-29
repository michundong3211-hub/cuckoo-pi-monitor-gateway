#pragma once

#if defined(GMT154_WIFI_SCAN_DIAG) && defined(ESP32)
#include <Arduino.h>
#include <esp_event.h>
#include <esp_wifi.h>
#include <log.hpp>

namespace gmt154_wifi_diag {
struct Record {
  uint32_t ms;
  uint32_t sequence;
  uint8_t scan_id;
  uint32_t status;
  uint8_t number;
};
struct Buffer {
  portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
  Record records[16] = {};
  uint8_t read = 0;
  uint8_t count = 0;
  uint32_t sequence = 0;
  uint32_t overflow = 0;
};
inline Buffer &buffer() {
  static Buffer value;
  return value;
}
inline void on_scan_done(void *, esp_event_base_t, int32_t, void *event_data) {
  if (!event_data) return;
  const auto *event = static_cast<const wifi_event_sta_scan_done_t *>(event_data);
  Buffer &q = buffer();
  const uint32_t ms = millis();
  portENTER_CRITICAL(&q.mux);
  const uint32_t sequence = ++q.sequence;
  if (q.count == 16) {
    ++q.overflow;
  } else {
    q.records[(q.read + q.count) % 16] =
        {ms, sequence, event->scan_id, event->status, event->number};
    ++q.count;
  }
  portEXIT_CRITICAL(&q.mux);
}
inline void drain() {
  static bool registered = false;
  if (!registered) {
    const esp_err_t result = esp_event_handler_register(
        WIFI_EVENT, WIFI_EVENT_SCAN_DONE, on_scan_done, nullptr);
    registered = (result == ESP_OK);
    Log.notice(F("WSCAN: event register result=%d" CR), static_cast<int>(result));
  }
  Buffer &q = buffer();
  for (;;) {
    Record record;
    uint32_t overflow;
    bool has_record;
    portENTER_CRITICAL(&q.mux);
    has_record = q.count != 0;
    if (has_record) {
      record = q.records[q.read];
      q.read = (q.read + 1) % 16;
      --q.count;
    }
    overflow = q.overflow;
    q.overflow = 0;
    portEXIT_CRITICAL(&q.mux);
    if (overflow) Log.notice(F("WSCAN: event overflow=%u" CR), overflow);
    if (!has_record) break;
    Log.notice(F("WSCAN: event ms=%u seq=%u scan_id=%u status=%u number=%u" CR),
               record.ms, record.sequence, record.scan_id, record.status,
               record.number);
  }
}
}  // namespace gmt154_wifi_diag
#endif
