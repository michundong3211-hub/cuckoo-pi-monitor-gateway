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

void require(const std::string& source, const char* token) {
  if (source.find(token) == std::string::npos) {
    std::cerr << "missing alignment requirement: " << token << "\n";
    ++failures;
  }
}

void reject(const std::string& source, const char* token) {
  if (source.find(token) != std::string::npos) {
    std::cerr << "forbidden asymmetric alignment remains: " << token << "\n";
    ++failures;
  }
}
}  // namespace

int main() {
  const std::string ui = readFile("src/ui_gravitymon_gateway.cpp");
  if (ui.empty()) return failures == 0 ? 1 : failures;

  const std::string centered_label =
      functionBody(ui, "static lv_obj_t* create_settings_centered_label(");
  require(centered_label, "lv_obj_set_size(region, rect.width, rect.height)");
  require(centered_label, "lv_obj_set_width(label, rect.width)");
  require(centered_label, "lv_obj_set_height(label, LV_SIZE_CONTENT)");
  require(centered_label, "lv_obj_center(label)");

  const std::string settings_button =
      functionBody(ui, "static lv_obj_t* create_settings_button(");
  require(settings_button, "lv_obj_set_width(label, rect.width)");
  require(settings_button, "lv_obj_set_height(label, LV_SIZE_CONTENT)");
  require(settings_button, "lv_obj_center(label)");
  reject(settings_button,
         "lv_obj_set_size(label, rect.width, rect.height)");

  const std::string home_cell =
      functionBody(ui, "static lv_obj_t* create_settings_home_cell(");
  require(home_cell, "lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER");
  reject(home_cell, "LV_TEXT_ALIGN_LEFT");
  reject(home_cell, "lv_obj_set_style_pad_left");
  reject(home_cell, "lv_obj_set_style_pad_top");

  const std::string settings_shell =
      functionBody(ui, "static void create_settings_shell(");
  require(settings_shell, "layout.header_title");
  require(settings_shell, "layout.header_device");
  require(settings_shell, "create_settings_centered_label");
  reject(settings_shell, "LV_TEXT_ALIGN_LEFT");
  reject(settings_shell, "LV_TEXT_ALIGN_RIGHT");

  const std::string settings_step_row =
      functionBody(ui, "static void create_settings_step_row(");
  require(settings_step_row, "create_settings_centered_label");
  require(settings_step_row, "layout.row_hints[row]");
  reject(settings_step_row, "lv_obj_set_style_pad_top");

  const std::string settings_og =
      functionBody(ui, "static void create_settings_og(");
  require(settings_og, "create_settings_centered_label");
  require(settings_og, "layout.og_hint");

  const std::string settings_ring_color =
      functionBody(ui, "static void create_settings_ring_color(");
  require(settings_ring_color, "create_settings_centered_label");
  require(settings_ring_color, "layout.ring_summary");

  const std::string alert = functionBody(
      ui, "static void gravitymon_gateway_create_alert_ui(");
  require(alert, "LV_TEXT_ALIGN_CENTER");
  require(alert, "layout_2_base_width() - 32");
  reject(alert, "224 - 16");

  require(ui, "dashboard_progress_font");
  require(ui, "dashboard_measurement_font");
  reject(ui, "lv_obj_set_style_transform_scale");

  const std::string landscape = functionBody(
      ui, "static void create_layout_2_dashboard_landscape(");
  require(landscape, "create_layout_2_dashboard_arc(scr, 24, 40, 120)");
  require(landscape,
          "lv_label_set_long_mode(g_state.lbl_name, LV_LABEL_LONG_CLIP)");
  require(landscape, "scr, \"--\", 156, 66, 152, 68");
  require(
      landscape,
      "lv_label_set_long_mode(g_state.lbl_measurement, LV_LABEL_LONG_CLIP)");

  const std::string portrait = functionBody(
      ui, "static void create_layout_2_dashboard_portrait(");
  require(portrait, "create_layout_2_dashboard_arc(scr, 48, 48, 144)");
  require(portrait,
          "lv_label_set_long_mode(g_state.lbl_name, LV_LABEL_LONG_CLIP)");
  require(portrait, "scr, \"--\", 20, 196, 200, 58");

  const std::string cuckoo =
      functionBody(ui, "static void create_cuckoo_dashboard(");
  require(cuckoo, "g_state.cuckoo_title");
  require(cuckoo, "g_state.cuckoo_index");
  require(cuckoo, "layout.index");
  require(cuckoo, "layout_2_font(20)");
  reject(cuckoo, "g_state.lbl_name = create_settings_centered_label");
  const std::string cuckoo_value =
      functionBody(ui, "static lv_obj_t* create_cuckoo_value(");
  require(cuckoo_value,
          "lv_obj_set_style_text_color(value, lv_color_hex(0xF4F7F7)");
  reject(cuckoo_value, "lv_label_set_recolor");
  require(ui, "create_cuckoo_colored_pair");
  require(ui, "g_state.cuckoo_label");
  require(ui,
          "lv_obj_set_style_text_color(label, lv_color_hex(0xF6B73C)");
  reject(ui, "LV_TEXT_FLAG_RECOLOR");
  const std::string cuckoo_loop = functionBody(
      ui, "static void gravitymon_gateway_loop_cuckoo(void)");
  require(cuckoo_loop, "g_state.cuckoo_index");
  require(cuckoo_loop, "g_state.data_index");
  require(cuckoo_loop, "lv_obj_set_style_text_color(g_state.cuckoo_value[3]");

  const std::string source_selector =
      functionBody(ui, "static void select_layout_2_source()");
  require(source_selector, "set_layout_2_gravity_header_visible(false)");
  require(source_selector, "set_layout_2_gravity_header_visible(true)");

  return failures;
}
