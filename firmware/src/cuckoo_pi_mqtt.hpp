#ifndef SRC_CUCKOO_PI_MQTT_HPP_
#define SRC_CUCKOO_PI_MQTT_HPP_

#if defined(GATEWAY)

#include "cuckoo_pi_config.hpp"
#include "cuckoo_pi_model.hpp"

#include <MQTTClient.h>
#include <WiFiClient.h>

struct CuckooPiRuntimeDevice {
  CuckooPiSnapshot snapshot;
  bool has_snapshot;
  uint32_t last_seen_ms;
  uint32_t parse_error_count;
};

struct CuckooPiMqttDiagnostics {
  bool connected;
  uint8_t failure_count;
  uint32_t next_attempt_ms;
  uint32_t connect_attempt_count;
  uint32_t successful_connection_count;
  int last_error;
  uint32_t dropped_message_count;
};

class CuckooPiMqttService {
 public:
  CuckooPiMqttService();
  void begin();
  void applyConfig(const CuckooPiConfig& config);
  void tick(uint32_t now_ms, bool wifi_connected);
  bool copyDeviceRuntime(const char* device_id,
                         CuckooPiRuntimeDevice* output) const;
  void copyDiagnostics(CuckooPiMqttDiagnostics* output) const;

 private:
  static CuckooPiMqttService* active_instance_;
  static void onMessage(MQTTClient* client, char topic[], char bytes[],
                        int length);
  void handleMessage(const char* topic, const char* bytes, int length,
                     uint32_t now_ms);
  void scheduleFailure(uint32_t now_ms);
  void setConnected(bool connected);
  void lock() const;
  void unlock() const;

  WiFiClient network_;
  MQTTClient client_;
  CuckooPiConfig config_;
  CuckooPiRuntimeDevice devices_[kMaxCuckooPiDevices];
  CuckooPiMqttDiagnostics diagnostics_;
  mutable SemaphoreHandle_t mutex_;
  char client_id_[40];
  bool begun_;
};

extern CuckooPiMqttService cuckooPiMqttService;

#endif  // GATEWAY
#endif  // SRC_CUCKOO_PI_MQTT_HPP_
