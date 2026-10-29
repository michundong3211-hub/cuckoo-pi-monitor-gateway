#include <ArduinoLog.hpp>
#include <serialws.hpp>
#include <gmt154_heap_diagnostics.hpp>
#include <esp_heap_caps.h>
#include <atomic>
#include <cassert>
#include <iostream>
#include <string>
#include <vector>
#include <windows.h>

class Capture : public Print {
 public:
  std::string text;
  bool bounded = false;
  CRITICAL_SECTION mutex;
  Capture() { InitializeCriticalSection(&mutex); }
  ~Capture() { DeleteCriticalSection(&mutex); }
  size_t write(uint8_t c) override {
    EnterCriticalSection(&mutex);
    text += static_cast<char>(c);
    SwitchToThread();
    LeaveCriticalSection(&mutex);
    return 1;
  }
  size_t write(const uint8_t* data, size_t count) override {
    if (bounded && count > 40) throw std::runtime_error("serial buffer read out of bounds");
    return Print::write(data, count);
  }
};
class BoundaryProbe : public SerialWebSocket {
 public:
  void fillToBoundary() { memset(_buf, 'A', sizeof(_buf)); _bufSize = sizeof(_buf); }
};

// Generated from the real display.cpp log call, with known diagnostic values.
#include "display_log_case.inc"

int main(int argc, char** argv) {
  if (argc != 2) return 2;
  Capture capture;
  Log.begin(6, &capture, false);
  const std::string which = argv[1];
  if (which == "heap") {
    testMillis = 29999;
    gmt154PollHeapDiagnostics();
    if (!capture.text.empty()) return 1;
    testMillis = 30000;
    gmt154PollHeapDiagnostics();
    const std::string expected = "HEAP: stage=runtime uptime_ms=30000 internal_free=4732 "
        "internal_largest=2048 internal_min=4000 integrity=ok wifi=3 rssi=-57\n";
    if (capture.text != expected) return 1;
    gmt154PollHeapDiagnostics();
    if (capture.text != expected) return 1;
    capture.text.clear();
    testHeapIntact = false;
    testMillis = 60000;
    gmt154PollHeapDiagnostics();
    if (capture.text.find("integrity=FAILED") == std::string::npos) return 1;
    testMillis = 0xfffffff0;
    gmt154PollHeapDiagnostics();
    capture.text.clear();
    testMillis = 29983;  // 29,999 ms since last sample, across wrap.
    gmt154PollHeapDiagnostics();
    if (!capture.text.empty()) return 1;
    testMillis = 29984;
    gmt154PollHeapDiagnostics();
    if (capture.text.empty()) return 1;
  } else if (which == "format") {
    runDisplayLogCase();
    const std::string expected = "DIAG: GMT154 task result=1 internal_free_before=57008 "
        "internal_largest_before=31732 internal_free_after=46000 "
        "internal_largest_after=20000 dt=123 ms.\n";
    if (capture.text != expected) {
      std::cerr << "Incorrect diagnostic values: " << capture.text;
      return 1;
    }
  } else if (which == "boundary") {
    capture.bounded = true;
    AsyncWebServer server;
    BoundaryProbe sink;
    sink.begin(&server, &capture);
    capture.text.clear();
    sink.fillToBoundary();
    sink.write('!');
    sink.flush();
    if (capture.text != std::string(40, 'A') + "!") {
      std::cerr << "Full buffer was not drained before next byte";
      return 1;
    }
  } else if (which == "threads") {
    capture.bounded = true;
    AsyncWebServer server;
    SerialWebSocket sink;
    sink.begin(&server, &capture);
    capture.text.clear();
    Log.begin(6, &sink, false);
    std::vector<HANDLE> threads;
    std::atomic<bool> start{false};
    struct Worker { int t; std::atomic<bool>* start; } workers[4];
    for (int t = 0; t < 4; ++t) {
      workers[t] = {t, &start};
      threads.push_back(CreateThread(nullptr, 0, [](LPVOID arg) -> DWORD {
      const auto& worker = *static_cast<Worker*>(arg);
      const int t = worker.t;
      while (!worker.start->load()) SwitchToThread();
      for (int i = 0; i < 500; ++i) {
        // Mix RAM and flash formats to catch one-lock-per-template mistakes.
        if (t % 2) Log.notice(F("worker=%d item=%d\n"), t, i);
        else Log.notice("worker=%d item=%d\n", t, i);
      }
      return 0;
      }, &workers[t], 0, nullptr));
    }
    start = true;
    for (auto thread : threads) {
      if (!thread || WaitForSingleObject(thread, 15000) != WAIT_OBJECT_0) return 1;
      CloseHandle(thread);
    }
    sink.flush();
    std::istringstream lines(capture.text);
    std::string line;
    bool seen[4][500]{};
    int count = 0;
    while (std::getline(lines, line)) {
      int t = -1, i = -1, end = 0;
      if (std::sscanf(line.c_str(), "worker=%d item=%d%n", &t, &i, &end) != 2 ||
          end != static_cast<int>(line.size()) || t < 0 || t >= 4 || i < 0 ||
          i >= 500 || seen[t][i]) {
        std::cerr << "Interleaved/duplicate log record: " << line;
        return 1;
      }
      seen[t][i] = true;
      ++count;
    }
    if (count != 2000) return 1;
  } else return 2;
  return 0;
}
