#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {
int failures = 0;

std::string readFile(const char* path) {
  std::ifstream input(path);
  if (!input.good()) {
    std::cerr << "cannot open " << path << "\n";
    ++failures;
    return {};
  }
  std::ostringstream contents;
  contents << input.rdbuf();
  return contents.str();
}

void require(const std::string& source, const char* token) {
  if (source.find(token) == std::string::npos) {
    std::cerr << "missing requirement: " << token << "\n";
    ++failures;
  }
}

void reject(const std::string& source, const char* token) {
  if (source.find(token) != std::string::npos) {
    std::cerr << "forbidden unsafe pattern remains: " << token << "\n";
    ++failures;
  }
}

std::string functionBody(const std::string& source, const char* signature) {
  const std::string::size_type signature_pos = source.find(signature);
  if (signature_pos == std::string::npos) return {};
  const std::string::size_type body_start = source.find('{', signature_pos);
  if (body_start == std::string::npos) return {};
  int depth = 0;
  for (std::string::size_type i = body_start; i < source.size(); ++i) {
    if (source[i] == '{') ++depth;
    if (source[i] == '}' && --depth == 0) {
      return source.substr(body_start, i - body_start + 1);
    }
  }
  return {};
}
}  // namespace

int main() {
  const std::string ui = readFile("src/ui_gravitymon_gateway.cpp");
  const std::string display = readFile("src/display.cpp");
  const std::string teardown = readFile("src/gateway_rotation_teardown.hpp");

  require(ui, "gatewayRotationSettingsOpened();");
  require(ui, "gatewayRotationSettingsClosed();");
  require(ui, "gatewayRotationSettingsClosed();\n    return;");
  require(ui, "gatewayRotationSettingsClosed();");
  require(ui, "gravitymon_gateway_cleanup(void)");
  require(ui, "gatewayRotationReleaseLocks();");
  require(display, "consumeFreshSampleRequest");
  require(teardown, "class GatewayRotationGuard");
  require(teardown, "GatewayOrientation pending_orientation");
  reject(ui, "GatewayRotationUiContext");
  reject(ui, "const GatewayRotationUiContext*");

  const std::string setter =
      functionBody(ui, "void gravitymon_gateway_set_device_id(");
  require(setter, "requestClose()");
  reject(setter, "settings_ui");
  reject(setter, "close_settings_overlay");
  reject(setter, "gatewayRotation");
  reject(setter, "lv_obj_");

  const std::string loop =
      functionBody(ui, "void gravitymon_gateway_loop(");
  require(loop, "consumeCloseRequest()");
  require(loop, "g_state.settings_ui.clear()");
  require(loop, "close_settings_overlay()");

  const std::string is_open =
      functionBody(ui, "bool gravitymon_gateway_is_settings_open(");
  require(is_open, "isOpen()");
  reject(is_open, "g_state.settings_ui");

  return failures == 0 ? 0 : 1;
}
