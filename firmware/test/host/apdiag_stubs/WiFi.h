#pragma once
constexpr int WL_CONNECTED=3;
struct FakeWifi { int status() {return 0;} int RSSI() {return 0;} };
inline FakeWifi WiFi;
