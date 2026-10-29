#ifndef SRC_GATEWAY_DEFAULT_TEMPLATES_HPP_
#define SRC_GATEWAY_DEFAULT_TEMPLATES_HPP_

#include <stddef.h>

constexpr char GATEWAY_HTTP_POST1_TEMPLATE_PATH[] = "/http-1.tpl";
constexpr char GATEWAY_CUCKOO_TILT_HTTP_POST_FORMAT[] =
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
    "\"run-mode\":\"${run-mode}\",\"run-time\":${run-time}}";

enum class GatewayDefaultTemplateResult {
  KeptExisting,
  Created,
  WriteFailed
};

template <typename HasContentFn, typename WriteFn>
GatewayDefaultTemplateResult ensureGatewayHttpPost1DefaultTemplate(
    HasContentFn hasContent, WriteFn write) {
  if (hasContent(GATEWAY_HTTP_POST1_TEMPLATE_PATH)) {
    return GatewayDefaultTemplateResult::KeptExisting;
  }

  const size_t size = sizeof(GATEWAY_CUCKOO_TILT_HTTP_POST_FORMAT) - 1;
  return write(GATEWAY_HTTP_POST1_TEMPLATE_PATH,
               GATEWAY_CUCKOO_TILT_HTTP_POST_FORMAT, size)
             ? GatewayDefaultTemplateResult::Created
             : GatewayDefaultTemplateResult::WriteFailed;
}

#endif  // SRC_GATEWAY_DEFAULT_TEMPLATES_HPP_
