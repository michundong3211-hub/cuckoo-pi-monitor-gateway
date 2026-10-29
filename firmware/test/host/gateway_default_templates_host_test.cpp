#include <assert.h>
#include <stddef.h>
#include <string.h>

#include "../../src/gateway_default_templates.hpp"

int main() {
  bool wrote = false;
  const char* written_path = nullptr;
  const char* written_data = nullptr;
  size_t written_size = 0;

  const GatewayDefaultTemplateResult created =
      ensureGatewayHttpPost1DefaultTemplate(
          [](const char*) { return false; },
          [&](const char* path, const char* data, size_t size) {
            wrote = true;
            written_path = path;
            written_data = data;
            written_size = size;
            return true;
          });

  assert(created == GatewayDefaultTemplateResult::Created);
  assert(wrote);
  assert(strcmp(written_path, "/http-1.tpl") == 0);
  assert(written_size == strlen(written_data));
  assert(strcmp(
             written_data,
             "{\"name\":\"${mdns}\",\"ID\":\"${id}\",\"token\":\"${token}\"," 
             "\"interval\":${sleep-interval},"
             "\"http-post-sleep-interval\":${http-post-sleep-interval},"
             "\"ble-sleep-interval\":${ble-sleep-interval},"
             "\"temperature\":${temp},\"temp_units\":\"${temp-unit}\"," 
             "\"gravity\":${gravity},\"velocity\":${velocity},"
             "\"angle\":${angle},\"battery\":${battery},\"RSSI\":${rssi},"
             "\"corr-gravity\":${corr-gravity},"
             "\"gravity-unit\":\"${gravity-unit}\"," 
             "\"ip-address\":\"${ip-address}\",\"app-ver\":\"${app-ver}\"," 
             "\"run-mode\":\"${run-mode}\",\"run-time\":${run-time}}") == 0);

  wrote = false;
  const GatewayDefaultTemplateResult kept =
      ensureGatewayHttpPost1DefaultTemplate(
          [](const char*) { return true; },
          [&](const char*, const char*, size_t) {
            wrote = true;
            return true;
          });

  assert(kept == GatewayDefaultTemplateResult::KeptExisting);
  assert(!wrote);

  const GatewayDefaultTemplateResult failed =
      ensureGatewayHttpPost1DefaultTemplate(
          [](const char*) { return false; },
          [](const char*, const char*, size_t) { return false; });
  assert(failed == GatewayDefaultTemplateResult::WriteFailed);
  return 0;
}
