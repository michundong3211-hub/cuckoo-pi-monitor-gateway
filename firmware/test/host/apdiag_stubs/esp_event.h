#pragma once
#include "freertos/portmacro.h"
using esp_err_t=int;
using esp_event_base_t=const char*;
using esp_event_handler_instance_t=void*;
using Handler=void(*)(void*,esp_event_base_t,int32_t,void*);
inline constexpr esp_event_base_t WIFI_EVENT="wifi";
constexpr int ESP_EVENT_ANY_ID=-1, ESP_OK=0, ESP_ERR_NOT_FOUND=261;
inline Handler fakeHandler=nullptr;
inline int registrationCalls=0, registrationError=0;
inline esp_err_t esp_event_handler_instance_register(esp_event_base_t,int,Handler h,void*,esp_event_handler_instance_t*) {assert(!criticalDepth);++registrationCalls;fakeHandler=h;return registrationError;}
