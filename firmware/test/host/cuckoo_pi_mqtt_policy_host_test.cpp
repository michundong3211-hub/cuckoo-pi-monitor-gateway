#include "cuckoo_pi_mqtt_policy.hpp"

#include <cassert>
#include <cstring>

int main() {
  const uint32_t expected[] = {1000U, 2000U, 5000U, 10000U,
                               30000U, 30000U, 30000U};
  for (uint8_t failure = 0; failure < 7; ++failure) {
    assert(cuckooPiReconnectDelayMs(failure) == expected[failure]);
  }

  assert(!cuckooPiShouldConnect(false, true, 1, 1000U, 0U));
  assert(!cuckooPiShouldConnect(true, false, 1, 1000U, 0U));
  assert(!cuckooPiShouldConnect(true, true, 0, 1000U, 0U));
  assert(!cuckooPiShouldConnect(true, true, 1, 999U, 1000U));
  assert(cuckooPiShouldConnect(true, true, 1, 1000U, 1000U));
  assert(cuckooPiShouldConnect(true, true, 1, 1001U, 1000U));
  assert(!cuckooPiShouldConnect(true, true, 1, 0xfffffff0U, 16U));
  assert(cuckooPiShouldConnect(true, true, 1, 16U, 16U));

  CuckooPiConfig no_endpoint = defaultCuckooPiConfig();
  no_endpoint.device_count = 1;
  no_endpoint.devices[0] = defaultCuckooPiDeviceConfig("001122334455");
  assert(!cuckooPiHasMqttEndpoint(no_endpoint));
  no_endpoint.port = 1884;
  assert(!cuckooPiHasMqttEndpoint(no_endpoint));
  std::strcpy(no_endpoint.broker, "broker.example");
  assert(cuckooPiHasMqttEndpoint(no_endpoint));
  no_endpoint.port = 0;
  assert(!cuckooPiHasMqttEndpoint(no_endpoint));

  CuckooPiConfig first = defaultCuckooPiConfig();
  first.enabled = true;
  first.device_count = 1;
  first.devices[0] = defaultCuckooPiDeviceConfig("001122334455");
  CuckooPiConfig same_subscription = first;
  same_subscription.devices[0].offline_timeout_seconds = 900;
  assert(cuckooPiSubscriptionConfigEqual(first, same_subscription));

  CuckooPiConfig changed = first;
  std::strcpy(changed.broker, "mqtt.example");
  assert(!cuckooPiSubscriptionConfigEqual(first, changed));
  changed = first;
  changed.port = 1883;
  assert(!cuckooPiSubscriptionConfigEqual(first, changed));
  changed = first;
  std::strcpy(changed.client_id, "client-changed");
  assert(!cuckooPiSubscriptionConfigEqual(first, changed));
  changed = first;
  std::strcpy(changed.username, "user-changed");
  assert(!cuckooPiSubscriptionConfigEqual(first, changed));
  changed = first;
  std::strcpy(changed.password, "password-changed");
  assert(!cuckooPiSubscriptionConfigEqual(first, changed));
  changed = first;
  changed.devices[0] = defaultCuckooPiDeviceConfig("aabbccddeeff");
  assert(!cuckooPiSubscriptionConfigEqual(first, changed));
  changed = first;
  changed.device_count = 0;
  assert(!cuckooPiSubscriptionConfigEqual(first, changed));

  first.device_count = 2;
  first.devices[1] = defaultCuckooPiDeviceConfig("aabbccddeeff");
  assert(cuckooPiFindStatusTopicDeviceIndex(
             first, "/cuckoo/wenkong/001122334455") == 0);
  assert(cuckooPiFindStatusTopicDeviceIndex(
             first, "/cuckoo/wenkong/aabbccddeeff") == 1);
  assert(cuckooPiFindStatusTopicDeviceIndex(
             first, "/cuckoo/mode/001122334455") == -1);
  assert(cuckooPiFindStatusTopicDeviceIndex(
             first, "/cuckoo/wenkong/001122334455/extra") == -1);
  assert(cuckooPiFindStatusTopicDeviceIndex(first, nullptr) == -1);

  return 0;
}
