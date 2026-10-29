#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {

int failures = 0;

std::string readFile(const char* path) {
  std::ifstream input(path, std::ios::binary);
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
    std::cerr << "missing 3.5 UI resolution contract: " << token << "\n";
    ++failures;
  }
}

void reject(const std::string& source, const char* token) {
  if (source.find(token) != std::string::npos) {
    std::cerr << "legacy 3.5 UI geometry remains: " << token << "\n";
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
  const std::string settings = readFile("src/gateway_settings_layout.cpp");

  require(ui, "kGateway35PortraitWidth = 320");
  require(ui, "kGateway35PortraitHeight = 480");
  require(ui, "layout_2_screen_width()");
  require(ui, "layout_2_screen_height()");
  require(ui, "layout_2_scale_x(");
  require(ui, "layout_2_scale_y(");

  const std::string dashboard = functionBody(
      ui, "static void create_layout_2_dashboard_portrait(");
  require(dashboard, "layout_2_screen_width()");
  require(dashboard, "layout_2_screen_height()");
  require(dashboard, "layout_2_scale_x(");
  require(dashboard, "layout_2_scale_y(");
  reject(dashboard, "create_dashboard_rect(scr, 0, 0, 240, 34");
  reject(dashboard, "lv_obj_set_size(g_state.wid_dashboard_touch, 240, 258)");

  const std::string alert = functionBody(
      ui, "static void gravitymon_gateway_create_alert_ui(const GatewayAlertEvent& event) {");
  require(ui, "const int32_t screen_width = layout_2_screen_width();");
  require(ui, "const int32_t screen_height = layout_2_screen_height();");
  require(alert, "layout_2_scale_x(");
  require(alert, "layout_2_scale_y(");
  reject(alert, "const int32_t screen_width = portrait ? 240 : 320");

  const std::string rebuild = functionBody(
      ui, "static void gravitymon_gateway_rebuild_settings_page() {");
  require(rebuild, "layout_2_screen_width()");
  require(rebuild, "layout_2_screen_height()");
  reject(rebuild, "layout_2_is_portrait() ? 240 : 320");

  require(settings, "scaleGatewaySettingsRect");
  require(settings, "const int16_t baseWidth");
  require(settings, "const int16_t baseHeight");

  return failures == 0 ? 0 : 1;
}
