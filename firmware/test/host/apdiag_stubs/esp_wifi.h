#pragma once
#include "esp_event.h"
enum wifi_mode_t {WIFI_MODE_NULL=0, WIFI_MODE_APSTA=3};
enum {WIFI_EVENT_AP_START=1,WIFI_EVENT_AP_STOP,WIFI_EVENT_AP_STACONNECTED,WIFI_EVENT_AP_STADISCONNECTED,WIFI_EVENT_STA_DISCONNECTED};
struct wifi_event_ap_stadisconnected_t {uint16_t reason;};
struct wifi_event_sta_disconnected_t {uint8_t reason;};
struct wifi_sta_list_t {unsigned num;};
inline int fakeModeErr=0, fakeClientsErr=0;
inline esp_err_t esp_wifi_get_mode(wifi_mode_t* p) {assert(!criticalDepth);*p=WIFI_MODE_APSTA;return fakeModeErr;}
inline esp_err_t esp_wifi_ap_get_sta_list(wifi_sta_list_t* p) {assert(!criticalDepth);p->num=2;return fakeClientsErr;}
