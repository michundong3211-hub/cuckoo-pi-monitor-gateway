#pragma once

#include <deque>
#include <vector>

#include <stddef.h>
#include <stdint.h>

class TwoWire {
 public:
  struct BeginCall {
    int sda;
    int scl;
    uint32_t frequency;
  };

  bool begin(int sda, int scl, uint32_t frequency) {
    begin_calls.push_back({sda, scl, frequency});
    return begin_result;
  }

  void beginTransmission(uint8_t address) {
    current_address = address;
    current_write.clear();
  }

  size_t write(uint8_t value) {
    current_write.push_back(value);
    return 1;
  }

  uint8_t endTransmission(bool send_stop = true) {
    transmissions.push_back({current_address, current_write, send_stop});
    if (!end_transmission_results.empty()) {
      const uint8_t result = end_transmission_results.front();
      end_transmission_results.pop_front();
      return result;
    }
    return 0;
  }

  size_t requestFrom(uint8_t address, size_t quantity) {
    request_history.push_back({address, quantity});
    if (!queued_reads.empty()) {
      const QueuedRead queued = queued_reads.front();
      queued_reads.pop_front();
      read_buffer.assign(queued.bytes.begin(), queued.bytes.end());
      available_count = static_cast<int>(read_buffer.size());
      return queued.return_count;
    }
    available_count = static_cast<int>(read_buffer.size());
    return quantity;
  }

  int available() { return available_count; }

  int read() {
    if (!read_failures.empty()) {
      const bool should_fail = read_failures.front();
      read_failures.pop_front();
      if (should_fail) return -1;
    }
    if (read_buffer.empty()) {
      available_count = 0;
      return -1;
    }

    const uint8_t value = read_buffer.front();
    read_buffer.pop_front();
    available_count = static_cast<int>(read_buffer.size());
    return value;
  }

  void setReadBuffer(const uint8_t* data, size_t length) {
    read_buffer.assign(data, data + length);
    available_count = static_cast<int>(read_buffer.size());
  }

  void setEndTransmissionResults(const uint8_t* data, size_t length) {
    end_transmission_results.assign(data, data + length);
  }

  void queueReadResponse(const uint8_t* data, size_t length,
                         size_t return_count) {
    QueuedRead queued;
    queued.bytes.assign(data, data + length);
    queued.return_count = return_count;
    queued_reads.push_back(queued);
  }

  void queueReadFailure(bool should_fail) {
    read_failures.push_back(should_fail);
  }

  void setBeginResult(bool result) { begin_result = result; }

  struct Transmission {
    uint8_t address;
    std::vector<uint8_t> bytes;
    bool send_stop;
  };

  struct Request {
    uint8_t address;
    size_t quantity;
  };

  struct QueuedRead {
    std::vector<uint8_t> bytes;
    size_t return_count;
  };

  std::vector<BeginCall> begin_calls;
  std::vector<Transmission> transmissions;
  std::vector<Request> request_history;

 private:
  uint8_t current_address = 0;
  bool begin_result = true;
  std::vector<uint8_t> current_write;
  std::deque<uint8_t> end_transmission_results;
  std::deque<QueuedRead> queued_reads;
  std::deque<bool> read_failures;
  std::deque<uint8_t> read_buffer;
  int available_count = 0;
};

extern TwoWire Wire;
