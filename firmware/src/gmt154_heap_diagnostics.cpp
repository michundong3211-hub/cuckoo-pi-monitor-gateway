#include "gmt154_heap_diagnostics.hpp"

#if defined(GMT154_06)
#include <Arduino.h>
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <log.hpp>
#include <cstdio>

#if defined(GMT154_06_C3)
#include "gmt154_ap_diagnostics_policy.hpp"
#include <esp_event.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
namespace {
portMUX_TYPE apMux = portMUX_INITIALIZER_UNLOCKED;
gmt154::ApEventState apEvents;
esp_event_handler_instance_t apHandlerInstance = nullptr;
bool apRegistrationAttempted = false;
void apEventHandler(void*, esp_event_base_t base, int32_t id, void* data) {
  if (base != WIFI_EVENT) return;
  gmt154::ApEvent event;
  bool reasonKnown = false;
  int32_t reason = -1;
  switch (id) {
    case WIFI_EVENT_AP_START: event = gmt154::ApEvent::start; break;
    case WIFI_EVENT_AP_STOP: event = gmt154::ApEvent::stop; break;
    case WIFI_EVENT_AP_STACONNECTED: event = gmt154::ApEvent::associated; break;
    case WIFI_EVENT_AP_STADISCONNECTED:
      event = gmt154::ApEvent::apDisconnected;
      if (data != nullptr) {
        reason = static_cast<const wifi_event_ap_stadisconnected_t*>(data)->reason;
        reasonKnown = true;
      }
      break;
    case WIFI_EVENT_STA_DISCONNECTED:
      event = gmt154::ApEvent::staDisconnected;
      if (data != nullptr) {
        reason = static_cast<const wifi_event_sta_disconnected_t*>(data)->reason;
        reasonKnown = true;
      }
      break;
    default: return;
  }
  const uint32_t now = millis();
  portENTER_CRITICAL(&apMux);
  gmt154::recordApEvent(apEvents, event, now, reasonKnown, reason);
  portEXIT_CRITICAL(&apMux);
}
void logApSnapshot(const char* stage) {
  gmt154::ApEventState snapshot;
  portENTER_CRITICAL(&apMux);
  snapshot = apEvents;
  portEXIT_CRITICAL(&apMux);
  wifi_mode_t mode = WIFI_MODE_NULL;
  const esp_err_t modeErr = esp_wifi_get_mode(&mode);
  wifi_sta_list_t stations{};
  const esp_err_t clientsErr = esp_wifi_ap_get_sta_list(&stations);
  esp_netif_ip_info_t info{};
  esp_netif_t* ap = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
  const esp_err_t ipErr = ap ? esp_netif_get_ip_info(ap, &info) : ESP_ERR_NOT_FOUND;
  char line[gmt154::apLineCapacity];
  gmt154::formatApSnapshot(line, sizeof(line), stage, snapshot, modeErr,
                          static_cast<int>(mode), stations.num, clientsErr,
                          info.ip.addr, ipErr);
  Log.notice(F("%s" CR), line);
}
}  // namespace
void gmt154StartApDiagnostics() {
  if (apRegistrationAttempted) return;
  apRegistrationAttempted = true;
  const esp_err_t error = esp_event_handler_instance_register(
      WIFI_EVENT, ESP_EVENT_ANY_ID, apEventHandler, nullptr, &apHandlerInstance);
  if (error != ESP_OK) {
    char line[72];
    std::snprintf(line, sizeof(line), "APDIAG: registration_failed err=%d", static_cast<int>(error));
    Log.warning(F("%s" CR), line);
  } else {
    Log.notice(F("APDIAG: registration_ok err=0" CR));
  }
  // AP_START may have happened before registration; the current state is queried.
  logApSnapshot("startup");
}
#endif

void gmt154LogHeapSnapshot(const char* stage) {
  constexpr uint32_t caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
  const size_t free = heap_caps_get_free_size(caps);
  const size_t largest = heap_caps_get_largest_free_block(caps);
  const size_t minimum = heap_caps_get_minimum_free_size(caps);
  const bool intact = heap_caps_check_integrity(caps, false);
  const int wifi = static_cast<int>(WiFi.status());
  // Format with snprintf, then pass just a string to ArduinoLog. Its %u/%l
  // consume 64-bit values and must not receive printf-style size_t arguments.
  char line[240];
  std::snprintf(line, sizeof(line),
                "HEAP: stage=%s uptime_ms=%lu internal_free=%lu "
                "internal_largest=%lu internal_min=%lu integrity=%s wifi=%d rssi=%d",
                stage, static_cast<unsigned long>(millis()),
                static_cast<unsigned long>(free), static_cast<unsigned long>(largest),
                static_cast<unsigned long>(minimum), intact ? "ok" : "FAILED",
                wifi, wifi == WL_CONNECTED ? WiFi.RSSI() : 0);
  Log.notice(F("%s" CR), line);
}

#if defined(GMT154_06_C3) && GMT154_06_C3 && defined(GMT154_C3_BLE_MEMORY_PROFILE) && GMT154_C3_BLE_MEMORY_PROFILE
void gmt154LogBleInitResult(const char* phase, int result) {
  char line[112];
  std::snprintf(line, sizeof(line), "BLEPROFILE: phase=%s result=%d", phase, result);
  Log.notice(F("%s" CR), line);
}
#endif

void gmt154PollHeapDiagnostics() {
  static uint32_t last = 0;
  const uint32_t now = millis();
  if (static_cast<uint32_t>(now - last) < 30000U) return;
  last = now;
  gmt154LogHeapSnapshot("runtime");
#if defined(GMT154_06_C3)
  logApSnapshot("runtime");
#endif
}
#endif
