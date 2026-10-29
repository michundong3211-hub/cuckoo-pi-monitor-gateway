#pragma once
constexpr int WL_CONNECTED = 3;
struct TestWiFi {
  int status() const { return WL_CONNECTED; }
  int RSSI() const { return -57; }
};
inline TestWiFi WiFi;
