#pragma once
#include <cstddef>
#include <memory>
class AsyncWebSocket {
 public:
  explicit AsyncWebSocket(const char*) {}
  size_t count() const { return 0; }
  void cleanupClients() {}
  void textAll(const char*, size_t) {}
};
class AsyncWebServer {
 public:
  void addHandler(AsyncWebSocket* socket) { socket_.reset(socket); }
 private:
  std::unique_ptr<AsyncWebSocket> socket_;
};
