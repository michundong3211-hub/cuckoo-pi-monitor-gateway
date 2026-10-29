#pragma once
#include "esp_event.h"
using esp_netif_t=int;
struct esp_netif_ip_info_t {struct {uint32_t addr;} ip;};
inline bool fakeNetifExists=true;
inline int fakeIpError=0;
inline esp_netif_t* esp_netif_get_handle_from_ifkey(const char*) {assert(!criticalDepth);static int x;return fakeNetifExists?&x:nullptr;}
inline esp_err_t esp_netif_get_ip_info(esp_netif_t*,esp_netif_ip_info_t* p) {assert(!criticalDepth);p->ip.addr=0x0104a8c0U;return fakeIpError;}
