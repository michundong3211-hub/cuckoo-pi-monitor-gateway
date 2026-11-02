/*
MIT License

Copyright (c) 2024-2025 Magnus

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
 */
#if defined(ENABLE_LVGL)

#include <math.h>
#include <string.h>

#include <cstdio>
#include <ctime>
#include <lvgl.h>
#if defined(ARDUINO_ARCH_ESP32)
#include <freertos/FreeRTOS.h>
#endif
#include <config_gateway.hpp>
#include <cuckoo_pi_mqtt.hpp>
#include <gmt154_cuckoo_temperature_policy.hpp>
#include <gateway_alarm_lifecycle.hpp>
#include "settings_freeze_diag.hpp"
#include "gmt154_offline_frame.hpp"
#include <gateway_alert_state.hpp>
#include <gateway_display_cycle.hpp>
#include <gateway_audio.hpp>
#include <gateway_rotation_teardown.hpp>
#include <gateway_settings_layout.hpp>
#include "gravity_settings_compact_label.hpp"
#include <gateway_settings_state.hpp>
#include <gateway_settings_thread_state.hpp>
#include <gateway_ui_style.hpp>
#include <cuckoo_pi_screen.hpp>
#include <cuckoo_pi_settings_state.hpp>
#include <gravity_display_units.hpp>
#include <log.hpp>
#include <ui_gravitymon_gateway.hpp>

#if defined(GATEWAY_35)
LV_FONT_DECLARE(lv_font_montserrat_72_digits);
LV_FONT_DECLARE(lv_font_montserrat_80_digits);
LV_FONT_DECLARE(lv_font_montserrat_regular_12);
LV_FONT_DECLARE(lv_font_montserrat_regular_14);
LV_FONT_DECLARE(lv_font_montserrat_regular_16);
LV_FONT_DECLARE(lv_font_montserrat_regular_20);
#endif
#if defined(GMT154_06)
LV_FONT_DECLARE(lv_font_montserrat_54_digits);
#endif

/**
 * UI state structure - matching hardware layout
 */
typedef struct {
  // Display labels
  lv_obj_t* lbl_name;
  lv_obj_t* lbl_index;
  lv_obj_t* lbl_temp;
  lv_obj_t* lbl_measurement;  // Gravity or Pressure
  lv_obj_t* lbl_battery;
  lv_obj_t* lbl_time;
  lv_obj_t* lbl_status;
  lv_obj_t* lbl_rssi;
  lv_obj_t* lbl_type;
  lv_obj_t* lbl_source;
  lv_obj_t* lbl_progress;
  lv_obj_t* lbl_measurement_unit;
  lv_obj_t* lbl_gateway_ip;
  lv_obj_t* lbl_wifi_rssi;
  lv_obj_t* lbl_abv;
  lv_obj_t* lbl_header_battery;
  lv_obj_t* lbl_history[5];

  lv_obj_t* wid_battery_canvas;  // Battery indicator canvas
  lv_obj_t* wid_dashboard_arc;
  lv_obj_t* wid_dashboard_touch;
  lv_obj_t* gravity_root;
  lv_obj_t* cuckoo_root;
  lv_obj_t* cuckoo_hero_card;
  lv_obj_t* gravity_offline_frame;
  lv_obj_t* cuckoo_offline_frame;
  lv_obj_t* cuckoo_card[7];
  lv_obj_t* cuckoo_divider;
  lv_obj_t* cuckoo_fridge_split;
  lv_obj_t* cuckoo_touch;
  lv_obj_t* wid_dashboard_temp_icon;
  lv_obj_t* wid_dashboard_signal_icon;
  lv_obj_t* wid_dashboard_decor[6];
  lv_obj_t* wid_alert_panel;
  lv_obj_t* lbl_alert_type;
  lv_obj_t* lbl_alert_device;
  lv_obj_t* btn_alert_close;
  lv_obj_t* settings_panel;
  lv_obj_t* settings_title;
  lv_obj_t* settings_error;
  lv_obj_t* settings_value[6];
  lv_obj_t* settings_ring_color[RING_COLOR_OPTION_COUNT];
  lv_obj_t* cuckoo_title;
  lv_obj_t* cuckoo_heading;
  lv_obj_t* cuckoo_index;
  lv_obj_t* cuckoo_label[10];
  lv_obj_t* cuckoo_value[10];

  // Data buffers
  char data_name[30];
  char data_device_id[GRAVITY_DEVICE_ID_CAPACITY];
  char data_index[10];
  char data_temp[30];
  char data_temp_compact[16];
  char data_measurement[20];  // Gravity or Pressure
  char data_measurement_precise[16];
  char data_measurement_unit[8];
  int16_t measurement_min;    // Gravity or Pressure
  int16_t measurement_max;    // Gravity or Pressure
  int32_t measurement;        // Gravity or Pressure
  char data_time[30];
  char data_time_ago[20];
  char data_rssi[20];
  char data_type[20];
  char data_source[30];
  char data_status[64];
  char data_history[5][64];
  char data_battery_voltage[10];
  char data_battery_percentage[10];
  int8_t battery_percentage;
  char data_gateway_ip[24];
  char data_wifi_rssi[16];
  char data_progress[8];
  int16_t progress;
  float gravity_sg;
  float temp_c;
  float battery_percent;
  bool battery_alert_active;
  int8_t header_battery_cached;
  int8_t header_battery_alert_cached;
  GatewayOrientation orientation = GATEWAY_DEFAULT_ORIENTATION;
  GatewayAlertEvent alert_event;
  bool alert_event_valid;

  CuckooPiSnapshot cuckoo_snapshot{};
  bool cuckoo_online = false;
  bool measurement_online = true;
  char measurement_online_id[GRAVITY_DEVICE_ID_CAPACITY] = {};
  int cuckoo_layout_mode = -1;
  int16_t cuckoo_layout_width = 0;
  int16_t cuckoo_layout_height = 0;
  GatewayDisplayItemKind display_source = GatewayDisplayItemKind::Measurement;

  GatewaySettingsUiState settings_ui;
  CuckooPiSettingsUiState cuckoo_settings_ui;

  char data_measurement_min[32];  // Gravity or Pressure
  char data_measurement_max[32];

  // Theme state
  bool flg_darkmode;
  bool flg_last_darkmode;

  // Style storage - only for complex objects that need persistent styles
  struct {
    /* layout 1 styles removed */
  } styles;
} gravitymon_state_t;

static gravitymon_state_t g_state{};
static lv_disp_t* g_disp = NULL;
static GatewaySettingsThreadState g_settings_thread_state;
#if defined(ARDUINO_ARCH_ESP32)
static portMUX_TYPE g_cuckoo_frame_mux = portMUX_INITIALIZER_UNLOCKED;
#endif

enum class SettingsAction : intptr_t {
  HomeOg = 1,
  HomeTemperatureChange,
  HomeRingColor,
  HomeOfflineAlarm,
  HomeBack,
  DetailBack,
  Save,
  OgMinus,
  OgPlus,
  TemperatureToggle,
  CompareMinus,
  ComparePlus,
  ThresholdMinus,
  ThresholdPlus,
  OfflineToggle,
  OfflineMinus,
  OfflinePlus,
};

enum class CuckooSettingsAction : intptr_t {
  OfflineToggle = 1,
  OfflineMinus,
  OfflinePlus,
  BeerToggle,
  BeerThresholdMinus,
  BeerThresholdPlus,
  BeerHoldMinus,
  BeerHoldPlus,
  Back,
  Save,
};

static void gravitymon_gateway_rebuild_settings_page();
static void gravitymon_gateway_destroy_alert_ui();
static void gravitymon_gateway_attempt_pending_alert_close();
static GatewayAlarmPersistenceRetryBackoff
    gateway_alarm_persistence_retry_backoff;
static GatewayAlarmPersistenceRetryBackoff
    cuckoo_alarm_persistence_retry_backoff;
static void gravitymon_gateway_update_alert_ui();
static void gravitymon_gateway_open_cuckoo_settings();
static void gravitymon_gateway_rebuild_cuckoo_settings_page();

static void gravitymon_gateway_reset_data(void) {
  snprintf(g_state.data_temp, sizeof(g_state.data_temp), "--");
  snprintf(g_state.data_temp_compact, sizeof(g_state.data_temp_compact), "--");
  snprintf(g_state.data_measurement, sizeof(g_state.data_measurement), "--");
  snprintf(g_state.data_measurement_precise,
           sizeof(g_state.data_measurement_precise), "--");
  g_state.data_measurement_unit[0] = '\0';
  snprintf(g_state.data_battery_voltage,
           sizeof(g_state.data_battery_voltage), "--");
  snprintf(g_state.data_battery_percentage,
           sizeof(g_state.data_battery_percentage), "--");
  snprintf(g_state.data_rssi, sizeof(g_state.data_rssi), "--");
  snprintf(g_state.data_gateway_ip, sizeof(g_state.data_gateway_ip), "IP --");
  snprintf(g_state.data_wifi_rssi, sizeof(g_state.data_wifi_rssi), "RSSI --");
  snprintf(g_state.data_progress, sizeof(g_state.data_progress), "--");
  g_state.battery_percentage = -1;
  g_state.header_battery_cached = -2;
  g_state.battery_alert_active = false;
  g_state.header_battery_alert_cached = -1;
  g_state.progress = -1;
  g_state.gravity_sg = NAN;
  g_state.temp_c = NAN;
  g_state.battery_percent = NAN;
}

static void reset_settings_widget_pointers() {
  g_state.settings_panel = nullptr;
  g_state.settings_title = nullptr;
  g_state.settings_error = nullptr;
  for (lv_obj_t*& value : g_state.settings_value) value = nullptr;
  for (lv_obj_t*& swatch : g_state.settings_ring_color) swatch = nullptr;
}

static void reset_gateway_widget_pointers() {
  g_state.lbl_name = nullptr;
  g_state.lbl_index = nullptr;
  g_state.lbl_temp = nullptr;
  g_state.lbl_measurement = nullptr;
  g_state.lbl_battery = nullptr;
  g_state.lbl_time = nullptr;
  g_state.lbl_status = nullptr;
  g_state.lbl_rssi = nullptr;
  g_state.lbl_type = nullptr;
  g_state.lbl_source = nullptr;
  g_state.lbl_progress = nullptr;
  g_state.lbl_measurement_unit = nullptr;
  g_state.lbl_gateway_ip = nullptr;
  g_state.lbl_wifi_rssi = nullptr;
  g_state.lbl_abv = nullptr;
  g_state.lbl_header_battery = nullptr;
  for (lv_obj_t*& history : g_state.lbl_history) history = nullptr;
  g_state.wid_battery_canvas = nullptr;
  g_state.wid_dashboard_arc = nullptr;
  g_state.wid_dashboard_touch = nullptr;
  g_state.gravity_root = nullptr;
  g_state.cuckoo_root = nullptr;
  g_state.cuckoo_hero_card = nullptr;
  g_state.gravity_offline_frame = nullptr;
  g_state.cuckoo_offline_frame = nullptr;
  for (lv_obj_t*& card : g_state.cuckoo_card) card = nullptr;
  g_state.cuckoo_divider = nullptr;
  g_state.cuckoo_fridge_split = nullptr;
  g_state.cuckoo_layout_mode = -1;
  g_state.cuckoo_layout_width = 0;
  g_state.cuckoo_layout_height = 0;
  g_state.cuckoo_touch = nullptr;
  g_state.cuckoo_title = nullptr;
  g_state.cuckoo_heading = nullptr;
  g_state.cuckoo_index = nullptr;
  g_state.wid_dashboard_temp_icon = nullptr;
  g_state.wid_dashboard_signal_icon = nullptr;
  for (lv_obj_t*& decor : g_state.wid_dashboard_decor) decor = nullptr;
  for (lv_obj_t*& label : g_state.cuckoo_label) label = nullptr;
  for (lv_obj_t*& value : g_state.cuckoo_value) value = nullptr;
  g_state.wid_alert_panel = nullptr;
  g_state.lbl_alert_type = nullptr;
  g_state.lbl_alert_device = nullptr;
  g_state.btn_alert_close = nullptr;
  g_state.alert_event_valid = false;
  reset_settings_widget_pointers();
}

/**
 * Layout manager
 */
typedef struct {
  uint8_t current_layout;
  uint8_t total_layouts;
} gravitymon_layout_mgr_t;

static gravitymon_layout_mgr_t layout_mgr = {
    .current_layout = 0,
    .total_layouts = 0,
};
static constexpr uint8_t kGatewayLayoutCount = GATEWAY_LAYOUT_COUNT;

/**
 * Create a label with specific position, size, alignment, color and font.
 *
 * @param parent Parent LVGL object to attach the label to.
 * @param text Initial text for the label (may be an empty string).
 * @param x X position (pixels) relative to parent.
 * @param y Y position (pixels) relative to parent.
 * @param w Width (pixels).
 * @param h Height (pixels).
 * @param align Text alignment (LV_TEXT_ALIGN_CENTER, _LEFT, _RIGHT).
 * @param color Text color as an `lv_color_t`.
 * @param font Pointer to an LVGL font (or NULL to use default font).
 * @return Pointer to the created `lv_obj_t` label.
 */
static lv_obj_t* create_label(lv_obj_t* parent, const char* text, int32_t x,
                              int32_t y, int32_t w, int32_t h,
                              lv_text_align_t align, lv_color_t color,
                              const lv_font_t* font) {
  lv_obj_t* lbl = lv_label_create(parent);
  lv_label_set_text(lbl, text);
  lv_obj_set_pos(lbl, x, y);
  lv_obj_set_size(lbl, w, h);
  lv_obj_set_style_text_align(lbl, align, LV_PART_MAIN);
  lv_obj_set_style_text_color(lbl, color, LV_PART_MAIN);
  if (font) {
    lv_obj_set_style_text_font(lbl, font, LV_PART_MAIN);
  }
  return lbl;
}

/**
 * Create a button and its centered label.
 *
 * This helper creates a standard LVGL button at the given position/size
 * and places a centered label inside it.
 *
 * @param parent Parent LVGL object.
 * @param label_text Text for the button label.
 * @param x X position.
 * @param y Y position.
 * @param w Width.
 * @param h Height.
 * @param callback Event callback for `LV_EVENT_PRESSED`.
 * @return The created `lv_obj_t*` button.
 */
static lv_obj_t* create_button(lv_obj_t* parent, const char* label_text,
                               int32_t x, int32_t y, int32_t w, int32_t h,
                               lv_event_cb_t callback) {
  lv_obj_t* btn = lv_btn_create(parent);
  lv_obj_set_size(btn, w, h);
  lv_obj_set_pos(btn, x, y);
  lv_obj_add_event_cb(btn, callback, LV_EVENT_PRESSED, NULL);

  lv_obj_t* lbl = lv_label_create(btn);
  lv_label_set_text(lbl, label_text);
  lv_obj_center(lbl);
  return btn;
}

/**
 * Draw a 5-segment battery indicator with dynamic coloring.
 *
 * The returned container holds five child rectangles representing battery
 * segments; filled segments are colored according to the active theme.
 *
 * @param parent Parent LVGL object.
 * @param x X position (top-left of battery outline).
 * @param y Y position (top-left of battery outline).
 * @param percentage Battery percentage (0-100).
 * @return Container `lv_obj_t*` that contains the segment children.
 */
static lv_obj_t* create_battery_indicator(lv_obj_t* parent, int32_t x,
                                          int32_t y, uint8_t percentage) {
  if (percentage >= 200) {
    return NULL;  // Invalid battery level, do not draw anything
  }

  // Create container for battery indicator (5 segments: 9px each + 1px gaps =
  // 55px wide, 20px tall)
  lv_obj_t* container = lv_obj_create(parent);
  lv_obj_set_pos(container, x, y);
  lv_obj_set_size(container, 55, 20);
  lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(container, 2, 0);
  ui_theme_colors_t theme_colors = ui_get_theme_colors(
      g_state.flg_darkmode ? UI_THEME_DARK : UI_THEME_LIGHT);
  lv_obj_set_style_border_color(container, theme_colors.battery_border, 0);
  lv_obj_set_style_pad_all(container, 0, 0);

  // Determine fill color based on battery level
  lv_color_t fill_color;
  if (percentage <= 20) {
    fill_color = theme_colors.battery_low;
  } else if (percentage <= 40) {
    fill_color = theme_colors.battery_mid;
  } else {
    fill_color = theme_colors.battery_good;
  }

  // Calculate how many segments should be filled (each segment = 20%)
  uint8_t filled_segments =
      (percentage + 19) / 20;  // Round up: 1-20% = 1 seg, 21-40% = 2 seg, etc.

  // Create 5 rectangles with 1px gaps (positioned inside the container).
  // Add small inner margins so the container border remains visible.
  for (int i = 0; i < 5; i++) {
    // Leave 2px padding from left/top so container border shows, then 1px gaps
    lv_obj_t* segment = lv_obj_create(container);
    lv_obj_set_pos(segment, 2 + (i * 10),
                   2);  // 9px width + 1px gap, 3px top pad
    lv_obj_set_size(segment, 7,
                    12);  // Leave gap at top and bottom inside border
    lv_obj_set_style_bg_opa(segment, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(segment, 0, 0);
    lv_obj_set_style_border_color(segment, theme_colors.battery_border, 0);
    lv_obj_set_style_pad_all(segment, 0, 0);

    // Fill only segments that should be filled
    if (i < filled_segments) {
      lv_obj_set_style_bg_color(segment, fill_color, 0);
      lv_obj_set_style_bg_opa(segment, LV_OPA_COVER, 0);
    } else {
      // Make empty segments transparent so gaps show through
      lv_obj_set_style_bg_opa(segment, LV_OPA_TRANSP, 0);
    }
  }

  return container;
}

/**
 * Delete any UI objects created by the active layout and clear pointers.
 *
 * This frees LVGL objects referenced in the `g_state` structure and sets
 * those pointers to NULL so a fresh layout can be created.
 */
class GatewayAlertUiTeardownReservation {
 public:
  GatewayAlertUiTeardownReservation()
      : owns_(gatewayAlarmLifecycle.reserveAlertUiTeardown()) {}
  ~GatewayAlertUiTeardownReservation() {
    if (owns_) gatewayAlarmLifecycle.releaseAlertUiTeardown();
  }

  bool owns() const { return owns_; }

 private:
  bool owns_;
};

static bool gravitymon_gateway_cleanup_layout(bool has_reservation = false) {
  if (has_reservation) {
    if (!gatewayAlarmLifecycle.canTearDownAlertUiWithReservation()) {
      return false;
    }
  } else if (!gatewayAlarmLifecycle.canTearDownAlertUi()) {
    return false;
  }
  gatewayRotationReleaseLocks();
  gravitymon_gateway_destroy_alert_ui();
  if (g_state.lbl_name) {
    lv_obj_del(g_state.lbl_name);
    g_state.lbl_name = NULL;
  }
  if (g_state.lbl_index) {
    lv_obj_del(g_state.lbl_index);
    g_state.lbl_index = NULL;
  }
  if (g_state.lbl_temp) {
    lv_obj_del(g_state.lbl_temp);
    g_state.lbl_temp = NULL;
  }
  if (g_state.lbl_measurement) {
    lv_obj_del(g_state.lbl_measurement);
    g_state.lbl_measurement = NULL;
  }
  if (g_state.lbl_battery) {
    lv_obj_del(g_state.lbl_battery);
    g_state.lbl_battery = NULL;
  }
  if (g_state.lbl_time) {
    lv_obj_del(g_state.lbl_time);
    g_state.lbl_time = NULL;
  }
  if (g_state.lbl_status) {
    lv_obj_del(g_state.lbl_status);
    g_state.lbl_status = NULL;
  }
  for (int i = 0; i < 5; i++) {
    if (g_state.lbl_history[i]) {
      lv_obj_del(g_state.lbl_history[i]);
      g_state.lbl_history[i] = NULL;
    }
  }
  if (g_state.lbl_rssi) {
    lv_obj_del(g_state.lbl_rssi);
    g_state.lbl_rssi = NULL;
  }
  if (g_state.lbl_type) {
    lv_obj_del(g_state.lbl_type);
    g_state.lbl_type = NULL;
  }
  if (g_state.lbl_source) {
    lv_obj_del(g_state.lbl_source);
    g_state.lbl_source = NULL;
  }
  if (g_state.lbl_progress) {
    lv_obj_del(g_state.lbl_progress);
    g_state.lbl_progress = NULL;
  }
  if (g_state.lbl_measurement_unit) {
    lv_obj_del(g_state.lbl_measurement_unit);
    g_state.lbl_measurement_unit = NULL;
  }
  if (g_state.lbl_gateway_ip) {
    lv_obj_del(g_state.lbl_gateway_ip);
    g_state.lbl_gateway_ip = NULL;
  }
  if (g_state.lbl_wifi_rssi) {
    lv_obj_del(g_state.lbl_wifi_rssi);
    g_state.lbl_wifi_rssi = NULL;
  }
  if (g_state.lbl_abv) {
    lv_obj_del(g_state.lbl_abv);
    g_state.lbl_abv = NULL;
  }
  if (g_state.lbl_header_battery) {
    lv_obj_del(g_state.lbl_header_battery);
    g_state.lbl_header_battery = NULL;
  }

  /* layout 1 widgets cleaned up as part of removal */
  if (g_state.wid_battery_canvas) {
    lv_obj_del(g_state.wid_battery_canvas);
    g_state.wid_battery_canvas = NULL;
  }
  if (g_state.wid_dashboard_arc) {
    lv_obj_del(g_state.wid_dashboard_arc);
    g_state.wid_dashboard_arc = NULL;
  }
  if (g_state.wid_dashboard_touch) {
    lv_obj_del(g_state.wid_dashboard_touch);
    g_state.wid_dashboard_touch = NULL;
  }
  if (g_state.wid_dashboard_temp_icon) {
    lv_obj_del(g_state.wid_dashboard_temp_icon);
    g_state.wid_dashboard_temp_icon = NULL;
  }
  if (g_state.wid_dashboard_signal_icon) {
    lv_obj_del(g_state.wid_dashboard_signal_icon);
    g_state.wid_dashboard_signal_icon = NULL;
  }
  if (g_state.settings_panel) {
    lv_obj_del(g_state.settings_panel);
    g_state.settings_panel = NULL;
  }
  for (int i = 0; i < 6; i++) {
    if (g_state.wid_dashboard_decor[i]) {
      lv_obj_del(g_state.wid_dashboard_decor[i]);
      g_state.wid_dashboard_decor[i] = NULL;
    }
  }
  if (g_state.gravity_offline_frame) {
    lv_obj_del(g_state.gravity_offline_frame);
    g_state.gravity_offline_frame = nullptr;
  }
  if (g_state.gravity_root) {
    lv_obj_del(g_state.gravity_root);
    g_state.gravity_root = nullptr;
  }
  if (g_state.cuckoo_root) {
    lv_obj_del(g_state.cuckoo_root);
    g_state.cuckoo_root = nullptr;
    g_state.cuckoo_title = nullptr;
    g_state.cuckoo_index = nullptr;
  }
  reset_gateway_widget_pointers();
  return true;
}

/**
 * Common initialization for the Gravitymon Gateway UI.
 *
 * Performs one-time setup such as storing the display reference, applying
 * the screen background color from the active theme, and initializing
 * default data buffers and the layout manager.
 *
 * @param disp Display pointer used by LVGL.
 * @param darkmode True to select dark theme defaults, false for light.
 */
static void gravitymon_gateway_init_common(lv_disp_t* disp, bool darkmode,
                                            uint8_t layout_id) {
  g_disp = disp;
  g_state.flg_darkmode = darkmode;

  // Get active screen
  lv_obj_t* scr = lv_scr_act();
  if (!scr) {
    Log.error(F("UI  : No active screen" CR));
    return;
  }

  // Get theme colors
  ui_theme_colors_t theme_colors =
      ui_get_theme_colors(darkmode ? UI_THEME_DARK : UI_THEME_LIGHT);

  // Apply background
  lv_obj_set_style_bg_color(scr, theme_colors.bg, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);

  // Initialize gravity range (default: 1000-1100)
  g_state.measurement_min = 1000;
  g_state.measurement_max = 1100;

  // Initialize layout manager
  layout_mgr.current_layout = 0;
  layout_mgr.total_layouts = kGatewayLayoutCount;
  g_state.orientation = gatewayInitialOrientationForLayout(layout_id);

  // Initialize last-darkmode tracking so theme is applied on first loop
  g_state.flg_last_darkmode = darkmode;

  const char* orientation_name =
      g_state.orientation == GatewayOrientation::Portrait ? "portrait"
                                                          : "landscape";
  Log.info(F("UI  : Gravitymon Gateway common init complete (%dx%d, %s, %s mode)" CR),
           lv_display_get_horizontal_resolution(disp),
           lv_display_get_vertical_resolution(disp),
           orientation_name, darkmode ? "dark" : "light");
}

/**
 * Setup layout 0 (compact): creates a dense layout with many text elements.
 *
 * This function constructs the labels, history list, and navigation
 * buttons used for the compact view.
 */
static void gravitymon_gateway_setup_layout_0(void) {
  lv_obj_t* scr = lv_scr_act();
  if (!scr) return;

  ui_theme_colors_t theme_colors = ui_get_theme_colors(
      g_state.flg_darkmode ? UI_THEME_DARK : UI_THEME_LIGHT);

  // Device name (5, 5, 250, 36) - left aligned, 20pt font
  g_state.lbl_name = create_label(scr, "", 5, 5, 250, 34, LV_TEXT_ALIGN_CENTER,
                                  theme_colors.text, &lv_font_montserrat_20);
  lv_obj_set_style_border_color(g_state.lbl_name, theme_colors.border, 0);
  lv_obj_set_style_border_width(g_state.lbl_name, 0, 0);
  lv_obj_set_style_radius(g_state.lbl_name, 8, 0);
  lv_obj_set_style_bg_color(g_state.lbl_name, theme_colors.panel_alt_bg,
                            LV_PART_MAIN);
  lv_obj_set_style_bg_opa(g_state.lbl_name, LV_OPA_80, LV_PART_MAIN);
  lv_obj_set_style_pad_top(g_state.lbl_name, 6, LV_PART_MAIN);
  lv_obj_set_style_pad_bottom(g_state.lbl_name, 4, LV_PART_MAIN);

  // Device index (260, 5, 54, 36) - center aligned, 16pt font
  g_state.lbl_index =
      create_label(scr, "", 260, 5, 54, 34, LV_TEXT_ALIGN_CENTER,
                   theme_colors.text, &lv_font_montserrat_16);
  lv_obj_set_style_border_color(g_state.lbl_index, theme_colors.border, 0);
  lv_obj_set_style_border_width(g_state.lbl_index, 0, 0);
  lv_obj_set_style_radius(g_state.lbl_index, 6, 0);
  lv_obj_set_style_bg_color(g_state.lbl_index, theme_colors.muted_bg,
                            LV_PART_MAIN);
  lv_obj_set_style_bg_opa(g_state.lbl_index, LV_OPA_80, LV_PART_MAIN);
  lv_obj_set_style_pad_top(g_state.lbl_index, 6, LV_PART_MAIN);
  lv_obj_set_style_pad_bottom(g_state.lbl_index, 4, LV_PART_MAIN);

  // First row: Gravity (left) and Temperature (right) - larger display
  g_state.lbl_measurement =
      create_label(scr, "", 5, 44, 150, 30, LV_TEXT_ALIGN_CENTER,
                   theme_colors.text, &lv_font_montserrat_16);
  lv_obj_set_style_pad_top(g_state.lbl_measurement, 6, LV_PART_MAIN);
  lv_obj_set_style_bg_color(g_state.lbl_measurement, theme_colors.panel_bg,
                            LV_PART_MAIN);
  lv_obj_set_style_bg_opa(g_state.lbl_measurement, LV_OPA_80, LV_PART_MAIN);
  lv_obj_set_style_border_color(g_state.lbl_measurement, theme_colors.border,
                                0);
  lv_obj_set_style_border_width(g_state.lbl_measurement, 0, 0);
  lv_obj_set_style_radius(g_state.lbl_measurement, 12, 0);

  g_state.lbl_temp =
      create_label(scr, "", 165, 44, 150, 30, LV_TEXT_ALIGN_CENTER,
                   theme_colors.text, &lv_font_montserrat_16);
  lv_obj_set_style_pad_top(g_state.lbl_temp, 6, LV_PART_MAIN);
  lv_obj_set_style_pad_bottom(g_state.lbl_temp, 4, LV_PART_MAIN);
  lv_obj_set_style_bg_color(g_state.lbl_temp, theme_colors.panel_alt_bg,
                            LV_PART_MAIN);
  lv_obj_set_style_bg_opa(g_state.lbl_temp, LV_OPA_80, LV_PART_MAIN);
  lv_obj_set_style_border_color(g_state.lbl_temp, theme_colors.border, 0);
  lv_obj_set_style_border_width(g_state.lbl_temp, 0, 0);
  lv_obj_set_style_radius(g_state.lbl_temp, 10, 0);

  // Second row: Timestamp (left) and Voltage/Battery (right)
  g_state.lbl_time = create_label(scr, "", 5, 79, 150, 30, LV_TEXT_ALIGN_CENTER,
                                  theme_colors.text, &lv_font_montserrat_14);
  lv_obj_set_style_pad_top(g_state.lbl_time, 8, LV_PART_MAIN);
  lv_obj_set_style_pad_bottom(g_state.lbl_time, 4, LV_PART_MAIN);
  lv_obj_set_style_bg_color(g_state.lbl_time, theme_colors.muted_bg,
                            LV_PART_MAIN);
  lv_obj_set_style_bg_opa(g_state.lbl_time, LV_OPA_80, LV_PART_MAIN);
  lv_obj_set_style_border_color(g_state.lbl_time, theme_colors.border, 0);
  lv_obj_set_style_border_width(g_state.lbl_time, 0, 0);
  lv_obj_set_style_radius(g_state.lbl_time, 6, 0);

  g_state.lbl_battery =
      create_label(scr, "", 165, 79, 150, 30, LV_TEXT_ALIGN_CENTER,
                   theme_colors.text, &lv_font_montserrat_16);
  lv_obj_set_style_pad_top(g_state.lbl_battery, 4, LV_PART_MAIN);
  lv_obj_set_style_pad_bottom(g_state.lbl_battery, 2, LV_PART_MAIN);
  lv_obj_set_style_bg_color(g_state.lbl_battery, theme_colors.panel_alt_bg,
                            LV_PART_MAIN);
  lv_obj_set_style_bg_opa(g_state.lbl_battery, LV_OPA_80, LV_PART_MAIN);
  lv_obj_set_style_border_color(g_state.lbl_battery, theme_colors.border, 0);
  lv_obj_set_style_border_width(g_state.lbl_battery, 0, 0);
  lv_obj_set_style_radius(g_state.lbl_battery, 6, 0);

  // Status bar (5, 219, 310, 18) - center aligned, 12pt font
  g_state.lbl_status =
      create_label(scr, "", 5, 219, 310, 18, LV_TEXT_ALIGN_CENTER,
                   theme_colors.text, &lv_font_montserrat_12);
  lv_obj_set_style_bg_color(g_state.lbl_status, theme_colors.status_bg,
                            LV_PART_MAIN);
  lv_obj_set_style_bg_opa(g_state.lbl_status, LV_OPA_60, LV_PART_MAIN);
  lv_obj_set_style_border_color(g_state.lbl_status, theme_colors.border, 0);
  lv_obj_set_style_border_width(g_state.lbl_status, 0, 0);
  lv_obj_set_style_radius(g_state.lbl_status, 0, 0);

  // History items (5, 114+i*21, 310, 18) - left aligned, 12pt font
  for (int i = 0; i < 5; i++) {
    g_state.lbl_history[i] =
        create_label(scr, "", 5, 114 + (i * 21), 310, 18, LV_TEXT_ALIGN_LEFT,
                     theme_colors.text, &lv_font_montserrat_12);
  }
}

/**
 * Setup layout 1
 */
static void gravitymon_gateway_setup_layout_1(void) {
  lv_obj_t* scr = lv_scr_act();
  if (!scr) return;

  ui_theme_colors_t theme_colors = ui_get_theme_colors(
      g_state.flg_darkmode ? UI_THEME_DARK : UI_THEME_LIGHT);

  // Font styles are set directly on labels like chamber controller

  // Header: Device name on left (5, 5, 250, 36) - left aligned, 20pt font
  // Make header span from top of battery (y=5) to bottom of index (y=52) =>
  // height=47
  g_state.lbl_name = create_label(scr, "", 5, 5, 245, 47, LV_TEXT_ALIGN_LEFT,
                                  theme_colors.text, &lv_font_montserrat_20);
  // Center text vertically inside the taller header
  lv_obj_set_style_pad_top(g_state.lbl_name, 12, LV_PART_MAIN);
  lv_obj_set_style_pad_bottom(g_state.lbl_name, 12, LV_PART_MAIN);
  lv_obj_set_style_border_color(g_state.lbl_name, theme_colors.border, 0);
  lv_obj_set_style_border_width(g_state.lbl_name, 0, 0);
  lv_obj_set_style_radius(g_state.lbl_name, 8, 0);
  lv_obj_set_style_bg_color(g_state.lbl_name, theme_colors.panel_alt_bg,
                            LV_PART_MAIN);
  lv_obj_set_style_bg_opa(g_state.lbl_name, LV_OPA_80, LV_PART_MAIN);

  // Battery indicator (top-right, 260, 5) - 55px wide
  g_state.wid_battery_canvas =
      create_battery_indicator(scr, 260, 5, g_state.battery_percentage);
  if (g_state.wid_battery_canvas) {
    lv_obj_set_style_border_color(g_state.wid_battery_canvas,
                                  theme_colors.border, 0);
    lv_obj_set_style_border_width(g_state.wid_battery_canvas, 0, 0);
    lv_obj_set_style_radius(g_state.wid_battery_canvas, 6, 0);
  }

  // Device index/page indicator (295, 5, 25, 36) - center aligned, 12pt font
  g_state.lbl_index =
      create_label(scr, "", 260, 32, 55, 20, LV_TEXT_ALIGN_CENTER,
                   theme_colors.text, &lv_font_montserrat_14);
  lv_obj_set_style_border_color(g_state.lbl_index, theme_colors.border, 0);
  lv_obj_set_style_border_width(g_state.lbl_index, 0, 0);
  lv_obj_set_style_radius(g_state.lbl_index, 6, 0);
  lv_obj_set_style_bg_color(g_state.lbl_index, theme_colors.muted_bg,
                            LV_PART_MAIN);
  lv_obj_set_style_bg_opa(g_state.lbl_index, LV_OPA_80, LV_PART_MAIN);
  lv_obj_set_style_pad_top(g_state.lbl_index, 2, LV_PART_MAIN);
  lv_obj_set_style_pad_bottom(g_state.lbl_index, 2, LV_PART_MAIN);

  // Large gravity display (centered, 60-130px vertically)
  g_state.lbl_measurement =
      create_label(scr, "", 10, 62, 300, 50, LV_TEXT_ALIGN_CENTER,
                   theme_colors.text, &lv_font_montserrat_20);
  lv_obj_set_style_pad_top(g_state.lbl_measurement, 10, LV_PART_MAIN);
  lv_obj_set_style_bg_color(g_state.lbl_measurement, theme_colors.panel_bg,
                            LV_PART_MAIN);
  lv_obj_set_style_bg_opa(g_state.lbl_measurement, LV_OPA_80, LV_PART_MAIN);
  lv_obj_set_style_border_color(g_state.lbl_measurement, theme_colors.border,
                                0);
  lv_obj_set_style_border_width(g_state.lbl_measurement, 0, 0);
  lv_obj_set_style_radius(g_state.lbl_measurement, 12, 0);

  // Temperature row (140-165px, full width centered) - center aligned, 20pt
  // font
  g_state.lbl_temp =
      create_label(scr, "", 40, 126, 240, 29, LV_TEXT_ALIGN_CENTER,
                   theme_colors.text, &lv_font_montserrat_20);
  lv_obj_set_style_pad_top(g_state.lbl_temp, 4, LV_PART_MAIN);
  lv_obj_set_style_bg_color(g_state.lbl_temp, theme_colors.panel_alt_bg,
                            LV_PART_MAIN);
  lv_obj_set_style_bg_opa(g_state.lbl_temp, LV_OPA_80, LV_PART_MAIN);
  lv_obj_set_style_border_color(g_state.lbl_temp, theme_colors.border, 0);
  lv_obj_set_style_border_width(g_state.lbl_temp, 0, 0);
  lv_obj_set_style_radius(g_state.lbl_temp, 10, 0);

  // Updated layout with two rows for the fields
  // Row 1: Time (left) and RSSI (center)
  g_state.lbl_time =
      create_label(scr, "", 5, 165, 150, 22, LV_TEXT_ALIGN_CENTER,
                   theme_colors.text, &lv_font_montserrat_12);
  lv_obj_set_style_pad_top(g_state.lbl_time, 2, LV_PART_MAIN);
  lv_obj_set_style_pad_bottom(g_state.lbl_time, 2, LV_PART_MAIN);
  lv_obj_set_style_bg_color(g_state.lbl_time, theme_colors.muted_bg,
                            LV_PART_MAIN);
  lv_obj_set_style_bg_opa(g_state.lbl_time, LV_OPA_70, LV_PART_MAIN);
  lv_obj_set_style_border_color(g_state.lbl_time, theme_colors.border, 0);
  lv_obj_set_style_border_width(g_state.lbl_time, 0, 0);
  lv_obj_set_style_radius(g_state.lbl_time, 8, 0);

  g_state.lbl_rssi =
      create_label(scr, "", 160, 165, 150, 22, LV_TEXT_ALIGN_CENTER,
                   theme_colors.text, &lv_font_montserrat_12);
  lv_obj_set_style_pad_top(g_state.lbl_rssi, 2, LV_PART_MAIN);
  lv_obj_set_style_pad_bottom(g_state.lbl_rssi, 2, LV_PART_MAIN);
  lv_obj_set_style_bg_color(g_state.lbl_rssi, theme_colors.muted_bg,
                            LV_PART_MAIN);
  lv_obj_set_style_bg_opa(g_state.lbl_rssi, LV_OPA_70, LV_PART_MAIN);
  lv_obj_set_style_border_color(g_state.lbl_rssi, theme_colors.border, 0);
  lv_obj_set_style_border_width(g_state.lbl_rssi, 0, 0);
  lv_obj_set_style_radius(g_state.lbl_rssi, 8, 0);

  // Row 2: API type (left) and Source (right)
  g_state.lbl_type =
      create_label(scr, "", 5, 192, 150, 22, LV_TEXT_ALIGN_CENTER,
                   theme_colors.text, &lv_font_montserrat_12);
  lv_obj_set_style_pad_top(g_state.lbl_type, 2, LV_PART_MAIN);
  lv_obj_set_style_pad_bottom(g_state.lbl_type, 2, LV_PART_MAIN);
  lv_obj_set_style_bg_color(g_state.lbl_type, theme_colors.muted_bg,
                            LV_PART_MAIN);
  lv_obj_set_style_bg_opa(g_state.lbl_type, LV_OPA_70, LV_PART_MAIN);
  lv_obj_set_style_border_color(g_state.lbl_type, theme_colors.border, 0);
  lv_obj_set_style_border_width(g_state.lbl_type, 0, 0);
  lv_obj_set_style_radius(g_state.lbl_type, 8, 0);

  g_state.lbl_source =
      create_label(scr, "", 160, 192, 150, 22, LV_TEXT_ALIGN_CENTER,
                   theme_colors.text, &lv_font_montserrat_12);
  lv_obj_set_style_pad_top(g_state.lbl_source, 2, LV_PART_MAIN);
  lv_obj_set_style_pad_bottom(g_state.lbl_source, 2, LV_PART_MAIN);
  lv_obj_set_style_bg_color(g_state.lbl_source, theme_colors.muted_bg,
                            LV_PART_MAIN);
  lv_obj_set_style_bg_opa(g_state.lbl_source, LV_OPA_70, LV_PART_MAIN);
  lv_obj_set_style_border_color(g_state.lbl_source, theme_colors.border, 0);
  lv_obj_set_style_border_width(g_state.lbl_source, 0, 0);
  lv_obj_set_style_radius(g_state.lbl_source, 8, 0);

  // Measurement and Temperature fields moved up by 4px
  g_state.lbl_measurement =
      create_label(scr, "", 40, 61, 240, 50, LV_TEXT_ALIGN_CENTER,
                   theme_colors.text, &lv_font_montserrat_28);
  lv_obj_set_style_pad_top(g_state.lbl_measurement, 10, LV_PART_MAIN);
  lv_obj_set_style_bg_color(g_state.lbl_measurement, theme_colors.panel_bg,
                            LV_PART_MAIN);
  lv_obj_set_style_bg_opa(g_state.lbl_measurement, LV_OPA_80, LV_PART_MAIN);
  lv_obj_set_style_border_color(g_state.lbl_measurement, theme_colors.border,
                                0);
  lv_obj_set_style_border_width(g_state.lbl_measurement, 0, 0);
  lv_obj_set_style_radius(g_state.lbl_measurement, 12, 0);

  g_state.lbl_temp =
      create_label(scr, "", 40, 126, 240, 29, LV_TEXT_ALIGN_CENTER,
                   theme_colors.text, &lv_font_montserrat_20);
  lv_obj_set_style_pad_top(g_state.lbl_temp, 4, LV_PART_MAIN);
  lv_obj_set_style_bg_color(g_state.lbl_temp, theme_colors.panel_alt_bg,
                            LV_PART_MAIN);
  lv_obj_set_style_bg_opa(g_state.lbl_temp, LV_OPA_80, LV_PART_MAIN);
  lv_obj_set_style_border_color(g_state.lbl_temp, theme_colors.border, 0);
  lv_obj_set_style_border_width(g_state.lbl_temp, 0, 0);
  lv_obj_set_style_radius(g_state.lbl_temp, 10, 0);

  // Status bar (fill bottom of screen) - center aligned, 12pt font
  g_state.lbl_status =
      create_label(scr, "", 0, 219, 320, 21, LV_TEXT_ALIGN_CENTER,
                   theme_colors.text, &lv_font_montserrat_12);
  lv_obj_set_style_bg_color(g_state.lbl_status, theme_colors.status_bg,
                            LV_PART_MAIN);
  lv_obj_set_style_bg_opa(g_state.lbl_status, LV_OPA_60, LV_PART_MAIN);
  lv_obj_set_style_border_color(g_state.lbl_status, theme_colors.border, 0);
  lv_obj_set_style_border_width(g_state.lbl_status, 0, 0);
  lv_obj_set_style_radius(g_state.lbl_status, 0,
                          0);  // No radius for full bottom bar
}

static lv_obj_t* create_dashboard_rect(lv_obj_t* parent, int32_t x, int32_t y,
                                       int32_t w, int32_t h,
                                       lv_color_t color) {
  lv_obj_t* rect = lv_obj_create(parent);
  lv_obj_remove_style_all(rect);
  lv_obj_set_pos(rect, x, y);
  lv_obj_set_size(rect, w, h);
  lv_obj_set_style_bg_color(rect, color, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(rect, LV_OPA_COVER, LV_PART_MAIN);
  return rect;
}

#if defined(GMT154_06)
static lv_obj_t* create_gmt154_offline_frame(lv_obj_t* parent,
                                              int32_t x, int32_t y,
                                              int32_t w, int32_t h) {
  lv_obj_t* frame = lv_obj_create(parent);
  lv_obj_remove_style_all(frame);
  lv_obj_set_pos(frame, x, y);
  lv_obj_set_size(frame, w, h);
  lv_obj_set_style_bg_opa(frame, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_color(frame, lv_color_hex(0xFF5D62), LV_PART_MAIN);
  lv_obj_set_style_border_width(frame, 2, LV_PART_MAIN);
  lv_obj_set_style_radius(frame, 6, LV_PART_MAIN);
  lv_obj_clear_flag(frame, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(frame, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(frame, LV_OBJ_FLAG_HIDDEN);
  return frame;
}
static void show_gmt154_offline_frame(lv_obj_t* frame, bool show) {
  if (!frame) return;
  if (show) lv_obj_clear_flag(frame, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(frame, LV_OBJ_FLAG_HIDDEN);
}
#endif

static void clearDecorativeClickable(lv_obj_t* object) {
  if (object) lv_obj_clear_flag(object, LV_OBJ_FLAG_CLICKABLE);
}

static bool layout_2_is_portrait() {
  return g_state.orientation == GatewayOrientation::Portrait;
}

#if defined(GMT154_06)
constexpr int32_t kGateway35PortraitWidth = 240;
constexpr int32_t kGateway35PortraitHeight = 240;
constexpr int32_t kGateway35LandscapeWidth = 240;
constexpr int32_t kGateway35LandscapeHeight = 240;
#elif defined(GATEWAY_35)
constexpr int32_t kGateway35PortraitWidth = 320;
constexpr int32_t kGateway35PortraitHeight = 480;
constexpr int32_t kGateway35LandscapeWidth = 480;
constexpr int32_t kGateway35LandscapeHeight = 320;
#else
constexpr int32_t kGateway35PortraitWidth = 240;
constexpr int32_t kGateway35PortraitHeight = 320;
constexpr int32_t kGateway35LandscapeWidth = 320;
constexpr int32_t kGateway35LandscapeHeight = 240;
#endif

static int32_t layout_2_screen_width() {
  return layout_2_is_portrait() ? kGateway35PortraitWidth
                                : kGateway35LandscapeWidth;
}

static int32_t layout_2_screen_height() {
  return layout_2_is_portrait() ? kGateway35PortraitHeight
                                : kGateway35LandscapeHeight;
}

static int32_t layout_2_base_width() {
  return layout_2_is_portrait() ? 240 : 320;
}

static int32_t layout_2_base_height() {
  return layout_2_is_portrait() ? 320 : 240;
}

static int32_t layout_2_scale_x(int32_t value) {
  const int32_t base = layout_2_base_width();
  return (value * layout_2_screen_width() + base / 2) / base;
}

static int32_t layout_2_scale_y(int32_t value) {
  const int32_t base = layout_2_base_height();
  return (value * layout_2_screen_height() + base / 2) / base;
}

static const GatewayUiStyle& layout_2_style() {
#if defined(GATEWAY_35)
  static const GatewayUiStyle style = gatewayUiStyle(true);
#else
  static const GatewayUiStyle style = gatewayUiStyle(false);
#endif
  return style;
}

#if defined(GMT154_06)
static const lv_font_t* gmt154_sg_font();
#endif

static const lv_font_t* layout_2_font(uint8_t point_size) {
  switch (point_size) {
    case 12:
      return &lv_font_montserrat_12;
    case 14:
      return &lv_font_montserrat_14;
    case 16:
      return &lv_font_montserrat_16;
    case 20:
      return &lv_font_montserrat_20;
    case 28:
      return &lv_font_montserrat_28;
    case 40:
      return &lv_font_montserrat_40;
#if defined(LV_FONT_MONTSERRAT_48) && LV_FONT_MONTSERRAT_48 > 0
    case 48:
      return &lv_font_montserrat_48;
#endif
#if defined(GATEWAY_35)
    case 80:
      return &lv_font_montserrat_80_digits;
    case 72:
      return &lv_font_montserrat_72_digits;
#endif
#if defined(GMT154_06)
    case 54:
      return gmt154_sg_font();
#endif
    default:
      return &lv_font_montserrat_14;
  }
}

#if defined(GMT154_06)
static const lv_font_t* gmt154_sg_font() {
  return &lv_font_montserrat_54_digits;
}
#endif

static const lv_font_t* layout_2_regular_font(uint8_t point_size) {
#if defined(GATEWAY_35)
  switch (point_size) {
    case 12:
      return &lv_font_montserrat_regular_12;
    case 14:
      return &lv_font_montserrat_regular_14;
    case 16:
      return &lv_font_montserrat_regular_16;
    case 20:
      return &lv_font_montserrat_regular_20;
    default:
      break;
  }
#endif
  return layout_2_font(point_size);
}

static lv_obj_t* create_layout_2_rect(lv_obj_t* parent, int32_t x, int32_t y,
                                      int32_t w, int32_t h, lv_color_t color) {
  return create_dashboard_rect(parent, layout_2_scale_x(x), layout_2_scale_y(y),
                               layout_2_scale_x(w), layout_2_scale_y(h), color);
}

static lv_obj_t* create_layout_2_label(lv_obj_t* parent, const char* text,
                                       int32_t x, int32_t y, int32_t w,
                                       int32_t h, lv_text_align_t align,
                                       lv_color_t color, const lv_font_t* font) {
  return create_label(parent, text, layout_2_scale_x(x), layout_2_scale_y(y),
                      layout_2_scale_x(w), layout_2_scale_y(h), align, color,
                      font);
}

static const char* gateway_alert_type_text(GatewayAlarmType type) {
  switch (type) {
    case GatewayAlarmType::TemperatureChange:
      return "TEMPERATURE CHANGE";
    case GatewayAlarmType::Offline:
      return "OFFLINE";
    case GatewayAlarmType::CuckooOffline:
      return "CUCKOO PI OFFLINE";
    case GatewayAlarmType::CuckooBeerDeviation:
      return "BEER TEMPERATURE";
  }
  return "ALERT";
}

static bool gateway_alert_events_equal(const GatewayAlertEvent& left,
                                       const GatewayAlertEvent& right) {
  return left.type == right.type &&
         strcmp(left.device_id, right.device_id) == 0 &&
         left.condition_token == right.condition_token &&
         left.event_token == right.event_token &&
         left.window_index == right.window_index &&
         left.repeat_index == right.repeat_index;
}

static void gravitymon_gateway_destroy_alert_ui() {
  if (g_state.wid_alert_panel) {
    lv_obj_del(g_state.wid_alert_panel);
  }
  g_state.wid_alert_panel = nullptr;
  g_state.lbl_alert_type = nullptr;
  g_state.lbl_alert_device = nullptr;
  g_state.btn_alert_close = nullptr;
  g_state.alert_event_valid = false;
}

static void gravitymon_gateway_close_alert(lv_event_t* event) {
  (void)event;
  GatewayAlertEvent local{};
  if (!gatewayAlertState.peek(&local)) {
    Log.notice(F("UI  : CLOSE with no alert." CR));
    return;
  }
  Log.notice(F("UI  : CLOSE type=%d dev=%s cond=%lx:%lx tok=%lx:%lx "
               "rep=%d." CR),
             (int)local.type, local.device_id,
             (unsigned long)(uint32_t)(local.condition_token >> 32),
             (unsigned long)(uint32_t)(local.condition_token & 0xffffffffUL),
             (unsigned long)(uint32_t)(local.event_token >> 32),
             (unsigned long)(uint32_t)(local.event_token & 0xffffffffUL),
             (int)local.repeat_index);
  traceAlarmEvent("UI close dev=%s tok=%lx:%lx rep=%d", local.device_id,
                  (unsigned long)(uint32_t)(local.event_token >> 32),
                  (unsigned long)(uint32_t)(local.event_token & 0xffffffffUL),
                  (int)local.repeat_index);
  if (!gatewayAlarmLifecycle.guardClose(local)) {
    Log.notice(F("UI  : CLOSE guard failed." CR));
    return;
  }
  if (!gatewayAlarmLifecycle.confirmClose(local)) {
    gatewayAlarmLifecycle.abortClose(local);
    Log.notice(F("UI  : CLOSE confirm failed." CR));
    return;
  }
  Log.notice(F("UI  : CLOSE armed." CR));
  gravitymon_gateway_attempt_pending_alert_close();
}

static GatewayAlarmLifecycleActionResult gatewayLifecycleResult(
    GatewayAlarmCloseResult result) {
  if (result == GatewayAlarmCloseResult::Retry)
    return GatewayAlarmLifecycleActionResult::Retry;
  if (result == GatewayAlarmCloseResult::NotFound)
    return GatewayAlarmLifecycleActionResult::NotFound;
  return GatewayAlarmLifecycleActionResult::Completed;
}

static GatewayAlarmLifecycleActionResult gatewayLifecycleResult(
    GatewayAudioCancelResult result) {
  if (result == GatewayAudioCancelResult::Retry)
    return GatewayAlarmLifecycleActionResult::Retry;
  if (result == GatewayAudioCancelResult::NotFound)
    return GatewayAlarmLifecycleActionResult::NotFound;
  return GatewayAlarmLifecycleActionResult::Completed;
}

static bool load_gateway_offline_dismissal(
    const char* device_id, GravityDeviceSettings* settings, void* context) {
  (void)context;
  return myConfig.copyDeviceSettings(device_id, settings);
}

static bool save_gateway_offline_dismissal(
    const GravityDeviceSettings& settings, void* context) {
  (void)context;
  return myConfig.saveDeviceSettings(settings);
}

static GatewayAlarmCloseResult validate_gateway_offline_dismissal_runtime(
    const GatewayAlarmEvent& event, void* context) {
  (void)context;
  return gatewayAlarmTracker.validateOfflineDismissalExact(event);
}

static GatewayAlarmCloseResult commit_gateway_offline_dismissal_runtime(
    const GatewayAlarmEvent& event, void* context) {
  (void)context;
  const GatewayAlarmCloseResult tracker_result =
      gatewayAlarmTracker.commitOfflineDismissalExact(event);
  if (tracker_result != GatewayAlarmCloseResult::Closed) {
    return tracker_result;
  }
  GatewayAlertEvent dismissed{};
  if (!gatewayAlertState.dismissCurrentExact(event.event_token, &dismissed)) {
    return GatewayAlarmCloseResult::Retry;
  }
  return GatewayAlarmCloseResult::Closed;
}

static void gravitymon_gateway_attempt_pending_alert_close() {
  GatewayAlarmLifecycleAction action{};
  if (!gatewayAlarmLifecycle.nextCloseAction(&action)) return;
  const GatewayAlertEvent& local = action.event;
  GatewayAlarmLifecycleActionResult result =
      GatewayAlarmLifecycleActionResult::Rejected;
  if (action.type ==
      GatewayAlarmLifecycleActionType::PersistOfflineDismissal) {
    const uint32_t attempt_now_ms = millis();
    if (local.type == GatewayAlarmType::CuckooOffline) {
      if (!cuckoo_alarm_persistence_retry_backoff.canAttempt(attempt_now_ms)) {
        return;
      }
      const GatewayAlarmCloseResult validation =
          gatewayAlarmTracker.validateOfflineDismissalExact(local);
      if (validation == GatewayAlarmCloseResult::Retry) {
        result = GatewayAlarmLifecycleActionResult::Retry;
      } else if (validation == GatewayAlarmCloseResult::NotFound) {
        result = GatewayAlarmLifecycleActionResult::NotFound;
      } else {
        uint32_t status_time_seconds = 0U;
        CuckooPiRuntimeDevice runtime{};
        if (cuckooPiMqttService.copyDeviceRuntime(local.device_id, &runtime) &&
            runtime.has_snapshot && runtime.snapshot.has_status_time) {
          status_time_seconds = runtime.snapshot.status_time_seconds;
        }
        result = myConfig.saveCuckooPiOfflineDismissal(
                     local.device_id, true, status_time_seconds)
                     ? GatewayAlarmLifecycleActionResult::Completed
                     : GatewayAlarmLifecycleActionResult::Retry;
      }
      cuckoo_alarm_persistence_retry_backoff.recordResult(result,
                                                           attempt_now_ms);
      Log.notice(F("UI  : persist Cuckoo offline dismissal result=%d dev=%s."
                   CR),
                 (int)result, local.device_id);
    } else {
    GatewayOfflineDismissalPersistenceOps operations{};
    operations.validateRuntime =
        validate_gateway_offline_dismissal_runtime;
    operations.load = load_gateway_offline_dismissal;
    operations.save = save_gateway_offline_dismissal;
    operations.commitRuntime = commit_gateway_offline_dismissal_runtime;
    if (!attemptGatewayOfflineDismissalWithBackoff(
            local, operations, &gateway_alarm_persistence_retry_backoff,
            attempt_now_ms, &result)) {
      return;
    }
    Log.notice(F("UI  : persist offline dismissal result=%d dev=%s." CR),
               (int)result, local.device_id);
    }
  } else if (action.type ==
             GatewayAlarmLifecycleActionType::CloseTrackerExact) {
    result = gatewayLifecycleResult(
        gatewayAlarmTracker.closeExact(local.event_token));
    Log.notice(F("UI  : close tracker result=%d tok=%lx:%lx." CR),
               (int)result,
               (unsigned long)(uint32_t)(local.event_token >> 32),
               (unsigned long)(uint32_t)(local.event_token & 0xffffffffUL));
    traceAlarmEvent("UI tracker-close result=%d tok=%lx:%lx", (int)result,
                    (unsigned long)(uint32_t)(local.event_token >> 32),
                    (unsigned long)(uint32_t)(local.event_token & 0xffffffffUL));
  } else if (action.type ==
             GatewayAlarmLifecycleActionType::CancelAudioExact) {
    result = gatewayLifecycleResult(gatewayAudio.cancelExactResult(local));
    Log.notice(F("UI  : cancel audio result=%d tok=%lx:%lx pending=%d "
                 "active=%d." CR),
               (int)result,
               (unsigned long)(uint32_t)(local.event_token >> 32),
               (unsigned long)(uint32_t)(local.event_token & 0xffffffffUL),
               (int)gatewayAudio.pendingCount(),
               (int)gatewayAudio.isActive());
    traceAlarmEvent("UI audio-cancel result=%d tok=%lx:%lx pending=%d "
                    "active=%d",
                    (int)result,
                    (unsigned long)(uint32_t)(local.event_token >> 32),
                    (unsigned long)(uint32_t)(local.event_token & 0xffffffffUL),
                    (int)gatewayAudio.pendingCount(),
                    (int)gatewayAudio.isActive());
  } else {
    return;
  }
  if (!gatewayAlarmLifecycle.applyResult(action, result)) {
    Log.notice(F("UI  : close result rejected by lifecycle." CR));
    return;
  }
  if (action.type == GatewayAlarmLifecycleActionType::CancelAudioExact) {
    if (result == GatewayAlarmLifecycleActionResult::Completed ||
        result == GatewayAlarmLifecycleActionResult::NotFound) {
      // Purge any stale entry of the closed window that could have slipped
      // into the alert state between the exact dismissal and this cancel (a
      // PublishAlert retry racing the CLOSE). Scoped to the closed window's
      // event_token so queued alerts of other windows/keys stay intact.
      gatewayAlertState.clearByTokenResult(action.event.event_token);
    }
    if (gatewayAlarmLifecycle.canTearDownAlertUi()) {
      gravitymon_gateway_destroy_alert_ui();
      Log.notice(F("UI  : alert panel closed." CR));
    }
  }
}

static void gravitymon_gateway_create_alert_ui(const GatewayAlertEvent& event) {
  lv_obj_t* scr = lv_scr_act();
  if (!scr) return;

  const GatewayUiStyle& style = layout_2_style();
  const bool portrait = layout_2_is_portrait();
  const int32_t screen_width = layout_2_screen_width();
  const int32_t screen_height = layout_2_screen_height();
  const int32_t panel_x = layout_2_scale_x(8);
  const int32_t requested_panel_y = layout_2_scale_y(42);
  const int32_t panel_y = requested_panel_y < screen_height ? requested_panel_y : 0;
  const int32_t panel_width = screen_width - layout_2_scale_x(16);
  const int32_t panel_height = layout_2_scale_y(portrait ? 142 : 120);
  const int32_t close_y = layout_2_scale_y(portrait ? 94 : 76);
  const int32_t alert_label_width = layout_2_base_width() - 32;
  char title[40] = {};
  snprintf(title, sizeof(title), "%s ALERT", gateway_alert_type_text(event.type));

  g_state.wid_alert_panel = lv_obj_create(scr);
  lv_obj_remove_style_all(g_state.wid_alert_panel);
  lv_obj_set_pos(g_state.wid_alert_panel, panel_x, panel_y);
  lv_obj_set_size(g_state.wid_alert_panel, panel_width, panel_height);
  lv_obj_set_style_bg_color(g_state.wid_alert_panel, lv_color_hex(0x5B1D26),
                            LV_PART_MAIN);
  lv_obj_set_style_bg_opa(g_state.wid_alert_panel, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_color(g_state.wid_alert_panel, lv_color_hex(0xFF5D62),
                                LV_PART_MAIN);
  lv_obj_set_style_border_width(g_state.wid_alert_panel,
                                style.alert_border_width, LV_PART_MAIN);
  lv_obj_set_style_radius(g_state.wid_alert_panel, 8, LV_PART_MAIN);
  lv_obj_clear_flag(g_state.wid_alert_panel, LV_OBJ_FLAG_SCROLLABLE);

  g_state.lbl_alert_type = create_layout_2_label(
      g_state.wid_alert_panel, title, 8, 10, alert_label_width, 24,
      LV_TEXT_ALIGN_CENTER, lv_color_hex(0xFFFFFF),
      layout_2_font(style.alert_title_font));
  g_state.lbl_alert_device = create_layout_2_label(
      g_state.wid_alert_panel, event.device_id, 8, 42, alert_label_width, 24,
      LV_TEXT_ALIGN_CENTER, lv_color_hex(0xFFFFFF),
      layout_2_font(style.alert_device_font));

  g_state.btn_alert_close = lv_btn_create(g_state.wid_alert_panel);
  lv_obj_set_pos(g_state.btn_alert_close, panel_width / 2 - layout_2_scale_x(48),
                 close_y);
  lv_obj_set_size(g_state.btn_alert_close, layout_2_scale_x(96),
                  layout_2_scale_y(32));
  lv_obj_set_style_bg_color(g_state.btn_alert_close, lv_color_hex(0xFF5D62),
                            LV_PART_MAIN);
  lv_obj_set_style_bg_opa(g_state.btn_alert_close, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(g_state.btn_alert_close, 6, LV_PART_MAIN);
  lv_obj_add_event_cb(g_state.btn_alert_close, gravitymon_gateway_close_alert,
                      LV_EVENT_CLICKED, nullptr);
  lv_obj_t* close_label = lv_label_create(g_state.btn_alert_close);
  lv_label_set_text(close_label, "CLOSE");
  lv_obj_set_style_text_color(close_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_set_style_text_font(close_label,
                             layout_2_font(style.alert_button_font),
                             LV_PART_MAIN);
  lv_obj_center(close_label);

  g_state.alert_event = event;
  g_state.alert_event_valid = true;
}

static void gravitymon_gateway_update_alert_ui() {
#if defined(GMT154_06)
  gravitymon_gateway_destroy_alert_ui();
  return;
#endif
  if (!gatewayAlarmLifecycle.canTearDownAlertUi()) return;
  GatewayAlertEvent event{};
  if (!gatewayAlertState.peek(&event)) {
    gravitymon_gateway_destroy_alert_ui();
    return;
  }

  if (g_state.wid_alert_panel && g_state.alert_event_valid &&
      gateway_alert_events_equal(g_state.alert_event, event)) {
    return;
  }

  gravitymon_gateway_destroy_alert_ui();
  gravitymon_gateway_create_alert_ui(event);
}

static const char* layout_2_display_name() {
  if (g_state.data_device_id[0]) {
    return g_state.data_device_id;
  }
  if (g_state.data_name[0] == ' ' && g_state.data_name[1] != '\0') {
    return &g_state.data_name[1];
  }
  if (g_state.data_name[0] != '\0') {
    return g_state.data_name;
  }
  return "--";
}

// Marks the device name as a red badge while the displayed device is still
// offline with its offline alarm dismissed (CLOSE). The badge disappears as
// soon as the device reports data again (markOnline clears the tracker state).
static void gravitymon_gateway_refresh_offline_badge() {
  lv_obj_t* label = g_state.lbl_name;
  if (label == nullptr) return;
  if (gatewayAlarmTracker.isOfflineDismissed(g_state.data_device_id)) {
    lv_obj_set_style_bg_color(label, lv_color_hex(0xFF5D62), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(label, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, lv_color_hex(0xF4F7F7), LV_PART_MAIN);
    lv_obj_set_style_radius(label, 4, LV_PART_MAIN);
  } else {
    lv_obj_set_style_bg_opa(label, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, lv_color_hex(0xF4F7F7), LV_PART_MAIN);
  }
}

static void layout_2_copy_rssi_value(char* out, size_t out_size,
                                     const char* source) {
  if (out_size == 0) {
    return;
  }
  if (!source || source[0] == '\0') {
    snprintf(out, out_size, "--");
    return;
  }

  snprintf(out, out_size, "%s", source);
  char* suffix = strstr(out, " dBm");
  if (suffix) {
    *suffix = '\0';
  }
}

static void style_layout_2_button(lv_obj_t* button, bool primary,
                                  const lv_font_t* font) {
  lv_obj_set_style_bg_color(
      button, primary ? lv_color_hex(0x45D9B2) : lv_color_hex(0x0B1B24),
      LV_PART_MAIN);
  lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_color(
      button, primary ? lv_color_hex(0x45D9B2) : lv_color_hex(0x234153),
      LV_PART_MAIN);
  lv_obj_set_style_border_width(button, 1, LV_PART_MAIN);
  lv_obj_set_style_radius(button, 6, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(button, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(button, 0, LV_PART_MAIN);

  lv_obj_t* label = lv_obj_get_child(button, 0);
  if (label) {
    lv_obj_set_style_text_color(
        label, primary ? lv_color_hex(0x061015) : lv_color_hex(0xF4F7F7),
        LV_PART_MAIN);
    lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  }
}

static lv_obj_t* create_layout_2_centered_label(
    lv_obj_t* parent, const char* text, int32_t x, int32_t y, int32_t width,
    int32_t height, lv_color_t color, const lv_font_t* font);
static lv_obj_t* create_layout_2_centered_label(
    lv_obj_t* parent, const char* text, const GatewayUiRect& rect,
    lv_color_t color, const lv_font_t* font);

static void layout_2_refresh_header_battery(lv_obj_t* parent) {
  const GatewayUiStyle& style = layout_2_style();
  const bool portrait = layout_2_is_portrait();
  const GatewayDashboardHeaderLayout header =
#if defined(GMT154_06)
      gatewayHydrometerPageLayout240().header;
#else
      gatewayDashboardHeaderLayout(
          static_cast<int16_t>(layout_2_screen_width()),
          static_cast<int16_t>(layout_2_screen_height()), portrait);
#endif
  const bool battery_alert = g_state.battery_alert_active;

  if (g_state.wid_battery_canvas &&
      (g_state.header_battery_cached != g_state.battery_percentage ||
       g_state.header_battery_alert_cached !=
           static_cast<int8_t>(battery_alert))) {
    lv_obj_del(g_state.wid_battery_canvas);
    g_state.wid_battery_canvas = NULL;
  }

  if (!g_state.wid_battery_canvas) {
    g_state.wid_battery_canvas = lv_obj_create(parent);
    clearDecorativeClickable(g_state.wid_battery_canvas);
    lv_obj_remove_style_all(g_state.wid_battery_canvas);
    lv_obj_set_pos(g_state.wid_battery_canvas, header.battery_body.x,
                   header.battery_body.y);
    lv_obj_set_size(
        g_state.wid_battery_canvas,
        header.battery_tip.x + header.battery_tip.width -
            header.battery_body.x,
        header.battery_body.height);
    lv_obj_set_style_bg_opa(g_state.wid_battery_canvas, LV_OPA_TRANSP,
                            LV_PART_MAIN);

    lv_obj_t* body = lv_obj_create(g_state.wid_battery_canvas);
    clearDecorativeClickable(body);
    lv_obj_remove_style_all(body);
    lv_obj_set_pos(body, 0, 0);
    lv_obj_set_size(body, header.battery_body.width,
                    header.battery_body.height);
    lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_color(body, lv_color_hex(0xF4F7F7),
                                  LV_PART_MAIN);
    lv_obj_set_style_border_width(body, style.battery_border_width,
                                  LV_PART_MAIN);
    lv_obj_set_style_radius(body, 2, LV_PART_MAIN);

    create_dashboard_rect(
        g_state.wid_battery_canvas, header.battery_tip.x -
                                        header.battery_body.x,
        header.battery_tip.y - header.battery_body.y,
        header.battery_tip.width, header.battery_tip.height,
        lv_color_hex(0xF4F7F7));

    const int32_t fill_inset = layout_2_scale_x(2);
    const int32_t fill_width = gatewayBatteryFillWidth(
        header.battery_body.width, style.battery_border_width, fill_inset,
        g_state.battery_percentage < 0
            ? 0
            : static_cast<int16_t>(lroundf(
                  fminf(100.0f, fmaxf(0.0f, g_state.battery_percentage)))));
    if (fill_width > 0) {
      lv_obj_t* fill = create_dashboard_rect(
          body, fill_inset, fill_inset, fill_width,
          static_cast<int32_t>(
              fminf(static_cast<float>(layout_2_scale_y(10)),
                    static_cast<float>(header.battery_body.height -
                                       (2 * fill_inset)))),
          battery_alert ? lv_color_hex(0xFF5D62) : lv_color_hex(0x45D9B2));
      lv_obj_set_style_radius(fill, 1, LV_PART_MAIN);
    }

    g_state.header_battery_cached = g_state.battery_percentage;
    g_state.header_battery_alert_cached = static_cast<int8_t>(battery_alert);
  }

  if (!g_state.lbl_header_battery) {
    g_state.lbl_header_battery = create_layout_2_centered_label(
        parent, "--", header.battery_percentage, lv_color_hex(0xF4F7F7),
        layout_2_font(style.dashboard_battery_font));
  }

  if (g_state.battery_percentage >= 0) {
    char battery_text[8];
    snprintf(battery_text, sizeof(battery_text), "%d%%",
             g_state.battery_percentage);
    lv_label_set_text(g_state.lbl_header_battery, battery_text);
  } else {
    lv_label_set_text(g_state.lbl_header_battery, "--");
  }
  lv_obj_set_style_text_color(
      g_state.lbl_header_battery,
      battery_alert ? lv_color_hex(0xFF5D62) : lv_color_hex(0xF4F7F7),
      LV_PART_MAIN);
}

static void set_layout_2_gravity_header_visible(bool visible) {
  if (g_state.wid_battery_canvas) {
    if (visible) {
      lv_obj_clear_flag(g_state.wid_battery_canvas, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(g_state.wid_battery_canvas, LV_OBJ_FLAG_HIDDEN);
    }
  }
  if (g_state.lbl_header_battery) {
    if (visible) {
      lv_obj_clear_flag(g_state.lbl_header_battery, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(g_state.lbl_header_battery, LV_OBJ_FLAG_HIDDEN);
    }
  }
}

static bool persist_gateway_settings(const GravityDeviceSettings& settings,
                                     void* context) {
  (void)context;
  return myConfig.saveDeviceSettings(settings);
}

static void close_settings_overlay() {
  g_settings_thread_state.publishOpen(false);
  gatewayRotationSettingsClosed();
  if (g_state.settings_panel) lv_obj_del(g_state.settings_panel);
  reset_settings_widget_pointers();
}

static void gravitymon_gateway_refresh_settings_page();

static bool enter_settings_page(SettingsPage page) {
  GravityDeviceSettings latest{};
  if (!myConfig.copyDeviceSettingsOrDefault(g_state.settings_ui.deviceId(),
                                            &latest))
    return false;
  return g_state.settings_ui.enterPage(page, latest);
}

// Called only by the LVGL task while it owns the UI semaphore.
static void freeze_diag_sample_memory(freeze_diag::MemorySlot slot) {
  lv_mem_monitor_t monitor{};
  lv_mem_monitor(&monitor);
  auto& target=freeze_diag::state.memory[slot];
  target.free.store(monitor.free_size, std::memory_order_relaxed);
  target.largest.store(monitor.free_biggest_size, std::memory_order_relaxed);
  target.sampled.store(millis(), std::memory_order_relaxed);
}
struct FreezeDiagMemoryScope {
  freeze_diag::MemorySlot exit_slot;
  FreezeDiagMemoryScope(freeze_diag::MemorySlot enter_slot, freeze_diag::MemorySlot exit_value):exit_slot(exit_value) { freeze_diag_sample_memory(enter_slot); }
  ~FreezeDiagMemoryScope() { freeze_diag_sample_memory(exit_slot); }
};

static void apply_settings_action(SettingsAction action) {
  freeze_diag::state.lastAction.store(static_cast<uint32_t>(action), std::memory_order_relaxed);
  freeze_diag::Scope diagnostic(freeze_diag::Action);
  FreezeDiagMemoryScope diagnostic_memory(freeze_diag::ActionEnter, freeze_diag::ActionExit);
  switch (action) {
    case SettingsAction::HomeOg:
      enter_settings_page(SettingsPage::Og);
      gravitymon_gateway_rebuild_settings_page();
      return;
    case SettingsAction::HomeTemperatureChange:
      enter_settings_page(SettingsPage::TemperatureChange);
      gravitymon_gateway_rebuild_settings_page();
      return;
    case SettingsAction::HomeRingColor:
      enter_settings_page(SettingsPage::RingColor);
      gravitymon_gateway_rebuild_settings_page();
      return;
    case SettingsAction::HomeOfflineAlarm:
      enter_settings_page(SettingsPage::OfflineAlarm);
      gravitymon_gateway_rebuild_settings_page();
      return;
    case SettingsAction::HomeBack:
      g_state.settings_ui.back();
      close_settings_overlay();
      return;
    case SettingsAction::DetailBack:
      g_state.settings_ui.back();
      gravitymon_gateway_rebuild_settings_page();
      return;
    case SettingsAction::Save:
      if (g_state.settings_ui.save(persist_gateway_settings, nullptr)) {
        g_state.settings_ui.clear();
        close_settings_overlay();
      } else {
        gravitymon_gateway_rebuild_settings_page();
      }
      return;
    case SettingsAction::OgMinus:
      g_state.settings_ui.adjustOg(-1);
      break;
    case SettingsAction::OgPlus:
      g_state.settings_ui.adjustOg(1);
      break;
    case SettingsAction::TemperatureToggle:
      g_state.settings_ui.toggleTemperatureAlarm();
      break;
    case SettingsAction::CompareMinus:
      g_state.settings_ui.adjustTemperatureCompare(-1);
      break;
    case SettingsAction::ComparePlus:
      g_state.settings_ui.adjustTemperatureCompare(1);
      break;
    case SettingsAction::ThresholdMinus:
      g_state.settings_ui.adjustTemperatureThreshold(-1);
      break;
    case SettingsAction::ThresholdPlus:
      g_state.settings_ui.adjustTemperatureThreshold(1);
      break;
    case SettingsAction::OfflineToggle:
      g_state.settings_ui.toggleOfflineAlarm();
      break;
    case SettingsAction::OfflineMinus:
      g_state.settings_ui.adjustOfflineTimeout(-1);
      break;
    case SettingsAction::OfflinePlus:
      g_state.settings_ui.adjustOfflineTimeout(1);
      break;
  }
  gravitymon_gateway_refresh_settings_page();
}

static lv_obj_t* create_settings_centered_label(
    lv_obj_t* parent, const char* text, const GatewaySettingsRect& rect,
    lv_color_t color, const lv_font_t* font) {
  lv_obj_t* region = lv_obj_create(parent);
  lv_obj_remove_style_all(region);
  lv_obj_set_pos(region, rect.x, rect.y);
  lv_obj_set_size(region, rect.width, rect.height);
  lv_obj_clear_flag(region, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(region, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* label = lv_label_create(region);
  lv_label_set_text(label, text);
  lv_obj_set_width(label, rect.width);
  lv_obj_set_height(label, LV_SIZE_CONTENT);
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_set_style_text_color(label, color, LV_PART_MAIN);
  if (font) {
    lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
  }
  lv_obj_center(label);
  return label;
}

static lv_obj_t* create_layout_2_centered_label(
    lv_obj_t* parent, const char* text, int32_t x, int32_t y, int32_t width,
    int32_t height, lv_color_t color, const lv_font_t* font) {
  const GatewaySettingsRect rect{
      static_cast<int16_t>(layout_2_scale_x(x)),
      static_cast<int16_t>(layout_2_scale_y(y)),
      static_cast<int16_t>(layout_2_scale_x(width)),
      static_cast<int16_t>(layout_2_scale_y(height))};
  return create_settings_centered_label(parent, text, rect, color, font);
}

static lv_obj_t* create_layout_2_centered_label(
    lv_obj_t* parent, const char* text, const GatewayUiRect& rect,
    lv_color_t color, const lv_font_t* font) {
  const GatewaySettingsRect settings_rect{rect.x, rect.y, rect.width,
                                           rect.height};
  return create_settings_centered_label(parent, text, settings_rect, color,
                                        font);
}

static lv_obj_t* create_cuckoo_value(lv_obj_t* parent,
                                     const GatewaySettingsRect& rect,
                                     const char* text, const lv_font_t* font) {
  lv_obj_t* value = create_settings_centered_label(
      parent, text, rect, lv_color_hex(0xF4F7F7), font);
  lv_obj_set_style_text_color(value, lv_color_hex(0xF4F7F7), LV_PART_MAIN);
  lv_obj_set_style_text_line_space(value, 0, LV_PART_MAIN);
  return value;
}

static lv_obj_t* create_cuckoo_absolute_value(lv_obj_t* parent,
                                              const char* text,
                                              const lv_font_t* font) {
  lv_obj_t* value = lv_label_create(parent);
  lv_obj_remove_style_all(value);
  lv_label_set_text(value, text ? text : "");
  lv_obj_set_style_text_align(value, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_set_style_text_color(value, lv_color_hex(0xF4F7F7), LV_PART_MAIN);
  if (font) lv_obj_set_style_text_font(value, font, LV_PART_MAIN);
  return value;
}

static CuckooPiTextMetrics measure_cuckoo_text(const char* text,
                                               const lv_font_t* font,
                                               lv_text_flag_t flags) {
  lv_point_t measured{};
  lv_text_get_size(&measured, text ? text : "", font, 0, 0, LV_COORD_MAX,
                   flags);
  return CuckooPiTextMetrics{static_cast<int16_t>(measured.x),
                             static_cast<int16_t>(measured.y),
                             static_cast<int16_t>(font ? font->base_line : 0)};
}

static void set_cuckoo_responsive_text(lv_obj_t* label,
                                       const GatewaySettingsRect& rect,
                                       const char* text,
                                       uint8_t preferred_size,
                                       bool use_regular_font = true) {
  if (!label) return;
  const char* desired_text = text ? text : "";
  // create_settings_centered_label() returns the child label, while its
  // geometry belongs to a wrapper region. Keep that wrapper synchronized
  // even when text/font do not change (for mode and orientation reflows).
  lv_obj_t* region = lv_obj_get_parent(label);
  if (region) {
    lv_obj_set_pos(region, rect.x, rect.y);
    lv_obj_set_size(region, rect.width, rect.height);
  }

  const uint8_t candidate_sizes[] = {28, 20, 16, 14, 12};
  CuckooPiFontMeasurement measurements[sizeof(candidate_sizes) /
                                       sizeof(candidate_sizes[0])] = {};
  for (size_t i = 0; i < sizeof(candidate_sizes) / sizeof(candidate_sizes[0]);
       ++i) {
    measurements[i].point_size = candidate_sizes[i];
    measurements[i].metrics = measure_cuckoo_text(
        text, use_regular_font ? layout_2_regular_font(candidate_sizes[i])
                               : layout_2_font(candidate_sizes[i]),
        LV_TEXT_FLAG_NONE);
  }
  const uint8_t selected_size = selectCuckooPiFontSize(
      measurements, sizeof(measurements) / sizeof(measurements[0]),
      preferred_size, rect.width, rect.height);
  const uint8_t applied_size =
      selected_size ? selected_size
                    : candidate_sizes[sizeof(candidate_sizes) /
                                      sizeof(candidate_sizes[0]) - 1];
  const lv_font_t* selected_font =
      use_regular_font ? layout_2_regular_font(applied_size)
                       : layout_2_font(applied_size);
  CuckooPiTextMetrics selected_metrics = measure_cuckoo_text(
      text, selected_font, LV_TEXT_FLAG_NONE);
  const bool fits = selected_metrics.width <= rect.width &&
                    selected_metrics.height <= rect.height &&
                    selected_size != 0 &&
                    selected_size >= candidate_sizes[sizeof(candidate_sizes) /
                                                            sizeof(candidate_sizes[0]) -
                                                        1];

  const lv_label_long_mode_t desired_long_mode =
      fits ? LV_LABEL_LONG_CLIP : LV_LABEL_LONG_SCROLL_CIRCULAR;
  const char* current_text = lv_label_get_text(label);
  const lv_font_t* current_font =
      lv_obj_get_style_text_font(label, LV_PART_MAIN);
  const bool unchanged = current_text &&
                         strcmp(current_text, desired_text) == 0 &&
                         current_font == selected_font &&
                         lv_label_get_long_mode(label) == desired_long_mode &&
                         lv_obj_get_width(label) == rect.width;
  if (unchanged) {
    lv_obj_center(label);
    return;
  }

  lv_label_set_text(label, desired_text);
  lv_obj_set_style_text_font(label, selected_font, LV_PART_MAIN);
  lv_label_set_long_mode(label, desired_long_mode);
  lv_obj_set_width(label, rect.width);
  lv_obj_set_height(label, LV_SIZE_CONTENT);
  lv_obj_center(label);
}

static GatewaySettingsRect cuckoo_pair_part_rect(
    const GatewaySettingsRect& rect, bool stacked, bool label_part) {
  if (stacked) {
    const int16_t label_height = rect.height / 2;
    if (label_part) {
      return GatewaySettingsRect{rect.x, rect.y, rect.width, label_height};
    }
    return GatewaySettingsRect{rect.x, static_cast<int16_t>(rect.y + label_height),
                               rect.width,
                               static_cast<int16_t>(rect.height - label_height)};
  }
  const int16_t label_width = rect.width / 2;
  if (label_part) {
    return GatewaySettingsRect{rect.x, rect.y, label_width, rect.height};
  }
  return GatewaySettingsRect{static_cast<int16_t>(rect.x + label_width), rect.y,
                             static_cast<int16_t>(rect.width - label_width),
                             rect.height};
}

static void set_cuckoo_colored_pair(
    lv_obj_t* label, lv_obj_t* value, const GatewaySettingsRect& rect,
    const char* label_text, const char* value_text, uint8_t label_size,
    uint8_t value_size, bool stacked) {
  if (!label || !value) return;
  const GatewaySettingsRect label_rect =
      cuckoo_pair_part_rect(rect, stacked, true);
  const GatewaySettingsRect value_rect =
      cuckoo_pair_part_rect(rect, stacked, false);
#if defined(GATEWAY_35)
  const bool use_regular_font = false;
#else
  const bool use_regular_font = true;
#endif
  set_cuckoo_responsive_text(label, label_rect, label_text, label_size, use_regular_font);
  set_cuckoo_responsive_text(value, value_rect, value_text, value_size, use_regular_font);
#if defined(ARDUINO_WAVESHARE_S3_TFT) || defined(GATEWAY_35)
  lv_obj_set_style_text_color(label, lv_color_hex(0x91ABB2), LV_PART_MAIN);
  lv_obj_set_style_text_color(value, lv_color_hex(0xF4F7F7), LV_PART_MAIN);
#else
  lv_obj_set_style_text_color(label, lv_color_hex(0xF6B73C), LV_PART_MAIN);
  lv_obj_set_style_text_color(value, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
#endif
}

static void create_cuckoo_colored_pair(
    lv_obj_t* parent, const GatewaySettingsRect& rect, const char* label_text,
    const char* value_text, uint8_t label_size, uint8_t value_size,
    bool stacked, lv_obj_t*& label, lv_obj_t*& value) {
  const GatewaySettingsRect label_rect =
      cuckoo_pair_part_rect(rect, stacked, true);
  const GatewaySettingsRect value_rect =
      cuckoo_pair_part_rect(rect, stacked, false);
  label = create_cuckoo_value(parent, label_rect, label_text,
                              layout_2_regular_font(label_size));
  value = create_cuckoo_value(parent, value_rect, value_text,
                              layout_2_regular_font(value_size));
  set_cuckoo_colored_pair(label, value, rect, label_text, value_text,
                          label_size, value_size, stacked);
}

static void set_cuckoo_temperature_group(
    lv_obj_t* number_label, lv_obj_t* unit_label,
    const GatewaySettingsRect& region, const char* number_text,
    const char* unit_text, bool has_unit, uint8_t preferred_size) {
  if (!number_label || !unit_label) return;
  const uint8_t candidate_sizes[] = {
#if defined(GMT154_06)
      54, 40};
#else
#if defined(GATEWAY_35)
      80,
#endif
#if defined(ARDUINO_WAVESHARE_S3_TFT)
      48, 40, 28};
#else
      72, 48, 40};
#endif
#endif
  const lv_font_t* unit_font = layout_2_font(20);
  const CuckooPiTextMetrics unit_metrics =
      measure_cuckoo_text(unit_text, unit_font, LV_TEXT_FLAG_NONE);
  const lv_font_t* selected_font = layout_2_font(candidate_sizes[
      sizeof(candidate_sizes) / sizeof(candidate_sizes[0]) - 1]);
  CuckooPiTemperatureGroup selected_group{};
  bool fits = false;
  for (const uint8_t point_size : candidate_sizes) {
    if (point_size > preferred_size) continue;
    const lv_font_t* candidate = layout_2_font(point_size);
    const CuckooPiTextMetrics number_metrics =
        measure_cuckoo_text(number_text, candidate, LV_TEXT_FLAG_NONE);
    const CuckooPiTemperatureGroup group = layoutCuckooPiTemperatureGroup(
        region, number_metrics, unit_metrics, has_unit, 6);
    selected_font = candidate;
    selected_group = group;
    if (group.fits) {
      fits = true;
      break;
    }
  }

  lv_obj_set_style_text_font(number_label, selected_font, LV_PART_MAIN);
  lv_obj_set_style_text_font(unit_label, unit_font, LV_PART_MAIN);
  lv_label_set_text(number_label, number_text ? number_text : "--");
  lv_label_set_text(unit_label, has_unit ? (unit_text ? unit_text : "") : "");
  if (selected_group.number.width <= 0 ||
      (has_unit && selected_group.unit.width <= 0)) {
    lv_obj_set_pos(number_label, region.x, region.y);
    lv_obj_set_size(number_label, region.width, region.height);
    lv_label_set_long_mode(number_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_add_flag(unit_label, LV_OBJ_FLAG_HIDDEN);
    return;
  }
  lv_obj_clear_flag(unit_label, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_pos(number_label, selected_group.number.x,
                 selected_group.number.y);
  lv_obj_set_size(number_label, selected_group.number.width,
                  selected_group.number.height);
  lv_obj_set_pos(unit_label, selected_group.unit.x, selected_group.unit.y);
  lv_obj_set_size(unit_label, selected_group.unit.width,
                  selected_group.unit.height);
  lv_label_set_long_mode(number_label,
                         fits ? LV_LABEL_LONG_CLIP
                              : LV_LABEL_LONG_SCROLL_CIRCULAR);
}

static lv_obj_t* create_cuckoo_card(lv_obj_t* parent,
                                    const GatewaySettingsRect& rect,
                                    lv_color_t color) {
  lv_obj_t* card = create_dashboard_rect(parent, rect.x, rect.y, rect.width,
                                          rect.height, color);
  lv_obj_set_style_radius(card, 4, LV_PART_MAIN);
#if defined(ARDUINO_WAVESHARE_S3_TFT) || defined(GATEWAY_35)
  lv_obj_set_style_border_color(card, lv_color_hex(0x2F8F78), LV_PART_MAIN);
#else
  lv_obj_set_style_border_color(card, lv_color_hex(0x5E4824), LV_PART_MAIN);
#endif
  lv_obj_set_style_border_width(card, 1, LV_PART_MAIN);
  return card;
}

static void set_cuckoo_widget_rect(lv_obj_t* widget,
                                   const GatewaySettingsRect& rect) {
  if (!widget) return;
  if (rect.width <= 0 || rect.height <= 0) {
    lv_obj_add_flag(widget, LV_OBJ_FLAG_HIDDEN);
    return;
  }
  lv_obj_set_pos(widget, rect.x, rect.y);
  lv_obj_set_size(widget, rect.width, rect.height);
  lv_obj_clear_flag(widget, LV_OBJ_FLAG_HIDDEN);
}

static void set_cuckoo_widget_visible(lv_obj_t* widget, bool visible) {
  if (!widget) return;
  if (visible) {
    lv_obj_clear_flag(widget, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(widget, LV_OBJ_FLAG_HIDDEN);
  }
}

static void apply_cuckoo_mode_layout(const CuckooPiScreenLayout& layout,
                                     int mode) {
  if (g_state.cuckoo_layout_mode == mode &&
      g_state.cuckoo_layout_width == layout.screen.width &&
      g_state.cuckoo_layout_height == layout.screen.height) {
    return;
  }
  g_state.cuckoo_layout_mode = mode;
  g_state.cuckoo_layout_width = layout.screen.width;
  g_state.cuckoo_layout_height = layout.screen.height;
  set_cuckoo_widget_rect(g_state.cuckoo_hero_card, layout.temperature_card);
  set_cuckoo_widget_rect(g_state.cuckoo_card[0], layout.mode_card);
  set_cuckoo_widget_rect(g_state.cuckoo_card[1], layout.beer_set);
  set_cuckoo_widget_rect(g_state.cuckoo_card[2], layout.fridge_set);
  set_cuckoo_widget_rect(g_state.cuckoo_card[3], layout.state);
  set_cuckoo_widget_rect(g_state.cuckoo_card[4], layout.runtime);
  set_cuckoo_widget_rect(g_state.cuckoo_card[5], layout.fridge);
  set_cuckoo_widget_rect(g_state.cuckoo_card[6], layout.chiller);
#if defined(ARDUINO_WAVESHARE_S3_TFT) || defined(GATEWAY_35)
  // Keep the existing regions for text positioning, but use their top and
  // inner left edges as dividers instead of drawing rounded boxes.
  const GatewaySettingsRect regions[] = {layout.mode_card, layout.beer_set,
      layout.fridge_set, layout.state, layout.runtime, layout.fridge,
      layout.chiller};
  for (size_t i = 0; i < 7; ++i) {
    if (!g_state.cuckoo_card[i]) continue;
    const bool inner_column = regions[i].x > layout.screen.width / 2 &&
        regions[i].width < layout.screen.width - 16;
    lv_obj_set_style_radius(g_state.cuckoo_card[i], 0, LV_PART_MAIN);
    lv_border_side_t sides = inner_column
        ? (lv_border_side_t)(LV_BORDER_SIDE_TOP | LV_BORDER_SIDE_LEFT)
        : LV_BORDER_SIDE_TOP;
#if defined(ARDUINO_WAVESHARE_S3_TFT)
    if (mode == 1 && layout.screen.width == 320 &&
        layout.screen.height == 240 && i == 2) sides = LV_BORDER_SIDE_LEFT;
    if ((mode == 2 || mode == 3) && layout.screen.width == 320 &&
        layout.screen.height == 240 && i == 1) sides = LV_BORDER_SIDE_LEFT;
#endif
#if defined(GATEWAY_35)
    if (layout.screen.width == 480 && layout.screen.height == 320 &&
        ((mode == 1 && i == 2) || ((mode == 2 || mode == 3) && i == 1)))
      sides = LV_BORDER_SIDE_LEFT;
#endif
    lv_obj_set_style_border_side(g_state.cuckoo_card[i], sides, LV_PART_MAIN);
  }
#if defined(ARDUINO_WAVESHARE_S3_TFT) || defined(GATEWAY_35)
  if (g_state.cuckoo_fridge_split) {
    if (mode == 1) {
      const GatewaySettingsRect& r = layout.screen.height > layout.screen.width
          ? layout.fridge_set : layout.runtime;
      lv_obj_set_pos(g_state.cuckoo_fridge_split, r.x + r.width / 2, r.y + 3);
      lv_obj_set_size(g_state.cuckoo_fridge_split, 1, r.height - 6);
      lv_obj_clear_flag(g_state.cuckoo_fridge_split, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(g_state.cuckoo_fridge_split, LV_OBJ_FLAG_HIDDEN);
    }
  }
#endif
#endif
#if !defined(ARDUINO_WAVESHARE_S3_TFT) && !defined(GATEWAY_35)
  if (g_state.cuckoo_divider) {
    if (layout.fridge.y > 0) {
      lv_obj_set_pos(g_state.cuckoo_divider, 24, layout.fridge.y - 8);
      lv_obj_clear_flag(g_state.cuckoo_divider, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(g_state.cuckoo_divider, LV_OBJ_FLAG_HIDDEN);
    }
  }
#endif
  set_cuckoo_widget_visible(g_state.cuckoo_label[4], layout.show_beer_set);
  set_cuckoo_widget_visible(g_state.cuckoo_value[4], layout.show_beer_set);
  set_cuckoo_widget_visible(g_state.cuckoo_label[5], layout.show_fridge_temp);
  set_cuckoo_widget_visible(g_state.cuckoo_value[5], layout.show_fridge_temp);
  set_cuckoo_widget_visible(g_state.cuckoo_label[9], layout.show_chiller);
  set_cuckoo_widget_visible(g_state.cuckoo_value[9], layout.show_chiller);
}

static void create_cuckoo_dashboard(lv_obj_t* scr) {
  const CuckooPiScreenLayout layout = calculateCuckooPiScreenLayout(
      static_cast<int16_t>(layout_2_screen_width()),
      static_cast<int16_t>(layout_2_screen_height()));
  const bool portrait = layout.screen.height > layout.screen.width;
#if defined(GMT154_06)
  const lv_color_t panel = lv_color_hex(0x000000);
  const lv_color_t screen = lv_color_hex(0x000000);
  const lv_color_t divider = lv_color_hex(0x2F8F78);
#elif defined(ARDUINO_WAVESHARE_S3_TFT)
  const lv_color_t panel = lv_color_hex(0x0B1B24);
  const lv_color_t screen = lv_color_hex(0x000000);
  const lv_color_t divider = lv_color_hex(0x2F8F78);
#elif defined(GATEWAY_35)
  const lv_color_t panel = lv_color_hex(0x0B1B24);
  const lv_color_t screen = lv_color_hex(0x000000);
  const lv_color_t divider = lv_color_hex(0x2F8F78);
#else
  const lv_color_t panel = lv_color_hex(0x1A1915);
  const lv_color_t screen = lv_color_hex(0x151411);
  const lv_color_t divider = lv_color_hex(0x9D722A);
  const lv_color_t mode_accent = lv_color_hex(0xF6B73C);
#endif

  g_state.cuckoo_root = lv_obj_create(scr);
  lv_obj_remove_style_all(g_state.cuckoo_root);
  lv_obj_set_pos(g_state.cuckoo_root, 0, 0);
  lv_obj_set_size(g_state.cuckoo_root, layout.screen.width,
                  layout.screen.height);
  lv_obj_set_style_bg_color(g_state.cuckoo_root, screen, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(g_state.cuckoo_root, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_clear_flag(g_state.cuckoo_root, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t* root = g_state.cuckoo_root;

  lv_obj_t* header = create_cuckoo_card(root, layout.header, panel);
  lv_obj_set_style_border_width(header, 0, LV_PART_MAIN);
  lv_obj_t* temperature =
      create_cuckoo_card(root, layout.temperature_card,
                         screen);
  g_state.cuckoo_hero_card = temperature;
  lv_obj_set_style_border_width(temperature, 0, LV_PART_MAIN);
#if !defined(GMT154_06)
  g_state.cuckoo_card[0] = create_cuckoo_card(root, layout.mode_card, screen);
  g_state.cuckoo_card[1] = create_cuckoo_card(root, layout.beer_set, screen);
  g_state.cuckoo_card[2] = create_cuckoo_card(root, layout.fridge_set, screen);
  g_state.cuckoo_card[3] = create_cuckoo_card(root, layout.state, screen);
  g_state.cuckoo_card[4] = create_cuckoo_card(root, layout.runtime, screen);
  g_state.cuckoo_card[5] = create_cuckoo_card(root, layout.fridge, screen);
  g_state.cuckoo_card[6] = create_cuckoo_card(root, layout.chiller, screen);
#if defined(ARDUINO_WAVESHARE_S3_TFT) || defined(GATEWAY_35)
  g_state.cuckoo_fridge_split = create_dashboard_rect(root, 0, 0, 1, 1, divider);
  lv_obj_add_flag(g_state.cuckoo_fridge_split, LV_OBJ_FLAG_HIDDEN);
#endif
  g_state.cuckoo_layout_mode = 2;
#endif

  create_dashboard_rect(root, layout.header.x, layout.header.y + layout.header.height - 1, layout.header.width,
                        1, divider);
#if defined(GMT154_06)
  create_dashboard_rect(root, 0, 121, layout.screen.width, 1,
                        lv_color_hex(0x244B58));
  create_dashboard_rect(root, 120, 122, 1, 58, lv_color_hex(0x244B58));
  create_dashboard_rect(root, 0, 180, layout.screen.width, 1,
                        lv_color_hex(0x244B58));
#else
#if !defined(GMT154_06) && !defined(ARDUINO_WAVESHARE_S3_TFT) && !defined(GATEWAY_35)
  g_state.cuckoo_divider = create_dashboard_rect(
      root, 24, layout.fridge.y - 8, layout.screen.width - 48, 1, divider);
#endif
#endif

  g_state.cuckoo_title = create_settings_centered_label(
      root, "--", layout.title, lv_color_hex(0xFFFFFF),
      layout_2_font(
#if defined(ARDUINO_WAVESHARE_S3_TFT)
          14));
#else
          portrait ? 16 : 14));
#endif
  lv_obj_set_style_text_align(g_state.cuckoo_title, LV_TEXT_ALIGN_LEFT,
                              LV_PART_MAIN);
  lv_label_set_long_mode(g_state.cuckoo_title, LV_LABEL_LONG_CLIP);
#if !defined(GMT154_06)
#if defined(ARDUINO_WAVESHARE_S3_TFT) || defined(GATEWAY_35)
  g_state.cuckoo_index = create_cuckoo_value(
      root, layout.index, "--", layout_2_font(12));
#else
  g_state.cuckoo_index = create_cuckoo_value(
      root, layout.index, "--", layout_2_font(portrait ? 14 : 12));
#endif
  lv_obj_set_style_text_align(g_state.cuckoo_index, LV_TEXT_ALIGN_CENTER,
                              LV_PART_MAIN);
#endif
#if defined(ARDUINO_WAVESHARE_S3_TFT)
  g_state.cuckoo_value[0] = create_cuckoo_value(
      root, layout.online, "--", layout_2_font(12));
#else
  g_state.cuckoo_value[0] = create_cuckoo_value(
      root, layout.online, "--", layout_2_font(portrait ? 14 : 12));
#endif
  lv_obj_set_style_text_align(g_state.cuckoo_value[0], LV_TEXT_ALIGN_RIGHT,
                              LV_PART_MAIN);
#if defined(GMT154_06)
  lv_obj_set_style_text_color(g_state.cuckoo_value[0], lv_color_hex(0x35D9B2),
                              LV_PART_MAIN);
#elif defined(ARDUINO_WAVESHARE_S3_TFT) || defined(GATEWAY_35)
  lv_obj_set_style_text_color(g_state.cuckoo_value[0], lv_color_hex(0x41DFB8),
                              LV_PART_MAIN);
#endif
#if defined(GMT154_06)
  create_cuckoo_colored_pair(
      root, layout.mode, "MODE", "--", 12, 16, true, g_state.cuckoo_label[1],
      g_state.cuckoo_value[1]);
#else
  create_cuckoo_colored_pair(
      root, layout.mode, "MODE", "--",
#if defined(ARDUINO_WAVESHARE_S3_TFT)
      12, 14, true,
#else
      14, 18, true,
#endif
      g_state.cuckoo_label[1], g_state.cuckoo_value[1]);
#endif
#if defined(GMT154_06)
  create_cuckoo_colored_pair(
      root, layout.state, "STATE", "--", 12, 16, true, g_state.cuckoo_label[2],
      g_state.cuckoo_value[2]);
#else
  create_cuckoo_colored_pair(
      root, layout.state, "STATE", "--",
#if defined(ARDUINO_WAVESHARE_S3_TFT)
      12, 14, true,
#else
      14, 18, true,
#endif
      g_state.cuckoo_label[2], g_state.cuckoo_value[2]);
#endif
#if defined(GMT154_06)
  lv_obj_set_style_text_color(g_state.cuckoo_label[1], lv_color_hex(0x91ABB2),
                              LV_PART_MAIN);
  lv_obj_set_style_text_color(g_state.cuckoo_value[1], lv_color_hex(0xFFFFFF),
                              LV_PART_MAIN);
  lv_obj_set_style_text_color(g_state.cuckoo_label[2], lv_color_hex(0x91ABB2),
                              LV_PART_MAIN);
  lv_obj_set_style_text_color(g_state.cuckoo_value[2], lv_color_hex(0x35D9B2),
                              LV_PART_MAIN);
#endif
  g_state.cuckoo_heading = create_settings_centered_label(
      root, "BEER TEMPERATURE", layout.beer_heading,
#if defined(GMT154_06)
      lv_color_hex(0x91ABB2), layout_2_regular_font(12));
#elif defined(ARDUINO_WAVESHARE_S3_TFT) || defined(GATEWAY_35)
      lv_color_hex(0x91ABB2), layout_2_regular_font(12));
#else
      mode_accent, layout_2_regular_font(12));
#endif
#if defined(GMT154_06)
  g_state.cuckoo_offline_frame = create_gmt154_offline_frame(
      root, layout.beer.x, layout.beer.y, layout.beer.width, layout.beer.height);
  g_state.cuckoo_value[3] = create_cuckoo_absolute_value(
      root, "--", gmt154_sg_font());
#else
  g_state.cuckoo_value[3] = create_cuckoo_absolute_value(
      root, "--", layout_2_font(
#if defined(ARDUINO_WAVESHARE_S3_TFT)
                      portrait ? 40 : 72));
#else
                      portrait ? 80 : 72));
#endif
#endif
#if defined(ARDUINO_WAVESHARE_S3_TFT) || defined(GATEWAY_35)
  lv_obj_set_style_text_color(g_state.cuckoo_value[3], lv_color_hex(0xF4F7F7),
                              LV_PART_MAIN);
#else
  lv_obj_set_style_text_color(g_state.cuckoo_value[3], lv_color_hex(0xFFFFFF),
                              LV_PART_MAIN);
#endif
  g_state.cuckoo_value[8] = create_cuckoo_absolute_value(
      root, "", layout_2_font(20));
#if defined(GMT154_06)
  lv_obj_set_style_text_color(g_state.cuckoo_value[8], lv_color_hex(0x35D9B2),
                              LV_PART_MAIN);
#elif defined(ARDUINO_WAVESHARE_S3_TFT) || defined(GATEWAY_35)
  lv_obj_set_style_text_color(g_state.cuckoo_value[8], lv_color_hex(0x41DFB8),
                              LV_PART_MAIN);
#else
  lv_obj_set_style_text_color(g_state.cuckoo_value[8], lv_color_hex(0xFFFFFF),
                              LV_PART_MAIN);
#endif
#if defined(GMT154_06)
  create_cuckoo_colored_pair(
      root, layout.beer_set, "BEER SET", "--", 12, 16, true,
      g_state.cuckoo_label[4], g_state.cuckoo_value[4]);
#else
  create_cuckoo_colored_pair(
      root, layout.beer_set, "BEER SET", "--",
#if defined(ARDUINO_WAVESHARE_S3_TFT)
      12, 14, true,
#elif defined(GATEWAY_35)
      14, 18, true,
#else
      16, 18, true,
#endif
      g_state.cuckoo_label[4], g_state.cuckoo_value[4]);
#endif
  if (g_state.cuckoo_value[4] && layout.show_beer_set) {
    lv_obj_set_style_text_color(g_state.cuckoo_value[4],
                                lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  }
#if defined(GMT154_06)
  lv_obj_set_style_text_color(g_state.cuckoo_label[4], lv_color_hex(0x91ABB2),
                              LV_PART_MAIN);
#endif
#if !defined(GMT154_06)
  create_cuckoo_colored_pair(
      root, layout.fridge,
#if defined(ARDUINO_WAVESHARE_S3_TFT) || defined(GATEWAY_35)
      "FRIDGE TEMP", "--",
#else
      portrait ? "FRIDGE\nTEMP" : "FRIDGE TEMP", "--",
#endif
#if defined(ARDUINO_WAVESHARE_S3_TFT)
      12, 14, true, g_state.cuckoo_label[5], g_state.cuckoo_value[5]);
#elif defined(GATEWAY_35)
      14, 18, true, g_state.cuckoo_label[5], g_state.cuckoo_value[5]);
#else
      20, 20, true, g_state.cuckoo_label[5], g_state.cuckoo_value[5]);
#endif
#endif
#if defined(GMT154_06)
  create_cuckoo_colored_pair(
      root, layout.fridge_set, "FRIDGE SET", "--", 12, 16, true,
      g_state.cuckoo_label[6], g_state.cuckoo_value[6]);
  lv_obj_set_style_text_color(g_state.cuckoo_label[6], lv_color_hex(0x91ABB2),
                              LV_PART_MAIN);
  lv_obj_set_style_text_color(g_state.cuckoo_value[6], lv_color_hex(0xFFFFFF),
                              LV_PART_MAIN);
#else
  create_cuckoo_colored_pair(
      root, layout.fridge_set, "FRIDGE SET", "--",
#if defined(ARDUINO_WAVESHARE_S3_TFT)
      12, 14, true,
#elif defined(GATEWAY_35)
      14, 18, true,
#else
      16, 18, true,
#endif
      g_state.cuckoo_label[6],
      g_state.cuckoo_value[6]);
#endif
  if (g_state.cuckoo_value[6]) {
    lv_obj_set_style_text_color(g_state.cuckoo_value[6],
                                lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  }
#if !defined(GMT154_06)
  create_cuckoo_colored_pair(
      root, layout.runtime, "RUN TIME", "--",
#if defined(ARDUINO_WAVESHARE_S3_TFT)
      12, 12, false,
#elif defined(GATEWAY_35)
      14, 14, false,
#else
      14, 14, false,
#endif
      g_state.cuckoo_label[7], g_state.cuckoo_value[7]);
#endif
#if !defined(GMT154_06)
  create_cuckoo_colored_pair(
      root, layout.chiller,
#if defined(ARDUINO_WAVESHARE_S3_TFT)
      "CHILLER TEMP", "--",
#elif defined(GATEWAY_35)
      "CHILLER TEMP", "--",
#else
      portrait ? "CHILLER\nTEMP" : "CHILLER TEMP", "--",
#endif
#if defined(ARDUINO_WAVESHARE_S3_TFT)
      12, 14, true, g_state.cuckoo_label[9], g_state.cuckoo_value[9]);
#elif defined(GATEWAY_35)
      14, 18, true, g_state.cuckoo_label[9], g_state.cuckoo_value[9]);
#else
      20, 20, true, g_state.cuckoo_label[9], g_state.cuckoo_value[9]);
#endif
#endif

  set_cuckoo_temperature_group(g_state.cuckoo_value[3], g_state.cuckoo_value[8],
                               layout.beer, "--", "", false,
#if defined(GMT154_06)
                               54);
#else
                               portrait ? 80 : 72);
#endif

  g_state.cuckoo_touch = lv_obj_create(root);
  lv_obj_remove_style_all(g_state.cuckoo_touch);
  lv_obj_set_pos(g_state.cuckoo_touch, layout.touch.x, layout.touch.y);
  lv_obj_set_size(g_state.cuckoo_touch, layout.touch.width,
                  layout.touch.height);
  lv_obj_add_flag(g_state.cuckoo_touch, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(g_state.cuckoo_touch, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(g_state.cuckoo_touch, [](lv_event_t* event) {
    if (lv_event_get_code(event) == LV_EVENT_CLICKED)
      gravitymon_gateway_open_cuckoo_settings();
  }, LV_EVENT_CLICKED, nullptr);
  lv_obj_add_event_cb(g_state.cuckoo_touch, [](lv_event_t* event) {
    if (lv_event_get_code(event) == LV_EVENT_LONG_PRESSED)
      gravitymon_gateway_open_cuckoo_settings();
  }, LV_EVENT_LONG_PRESSED, nullptr);
  lv_obj_add_event_cb(g_state.cuckoo_touch, [](lv_event_t* event) {
    switch (lv_event_get_code(event)) {
      case LV_EVENT_PRESSED:
        gatewayRotationPointerPressed();
        break;
      case LV_EVENT_RELEASED:
        gatewayRotationPointerReleased();
        break;
      case LV_EVENT_PRESS_LOST:
        gatewayRotationPointerLost();
        break;
      default:
        break;
    }
  }, LV_EVENT_PRESSED, nullptr);
  lv_obj_add_event_cb(g_state.cuckoo_touch, [](lv_event_t* event) {
    if (lv_event_get_code(event) == LV_EVENT_RELEASED)
      gatewayRotationPointerReleased();
  }, LV_EVENT_RELEASED, nullptr);
  lv_obj_add_event_cb(g_state.cuckoo_touch, [](lv_event_t* event) {
    if (lv_event_get_code(event) == LV_EVENT_PRESS_LOST)
      gatewayRotationPointerLost();
  }, LV_EVENT_PRESS_LOST, nullptr);
}

static void select_layout_2_source() {
  if (g_state.display_source == GatewayDisplayItemKind::CuckooPi) {
    set_layout_2_gravity_header_visible(false);
    if (!g_state.cuckoo_root) create_cuckoo_dashboard(lv_scr_act());
    if (g_state.gravity_root)
      lv_obj_add_flag(g_state.gravity_root, LV_OBJ_FLAG_HIDDEN);
    if (g_state.cuckoo_root)
      lv_obj_clear_flag(g_state.cuckoo_root, LV_OBJ_FLAG_HIDDEN);
  } else {
    set_layout_2_gravity_header_visible(true);
    if (g_state.gravity_root)
      lv_obj_clear_flag(g_state.gravity_root, LV_OBJ_FLAG_HIDDEN);
    if (g_state.gravity_root)
      layout_2_refresh_header_battery(g_state.gravity_root);
    if (g_state.cuckoo_root)
      lv_obj_add_flag(g_state.cuckoo_root, LV_OBJ_FLAG_HIDDEN);
  }
}

static lv_obj_t* create_settings_button(lv_obj_t* parent, const char* text,
                                        const GatewaySettingsRect& rect,
                                        SettingsAction action, bool primary) {
  lv_obj_t* button = lv_button_create(parent);
  lv_obj_set_pos(button, rect.x, rect.y);
  lv_obj_set_size(button, rect.width, rect.height);
  lv_obj_clear_flag(button, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(
      button,
      [](lv_event_t* event) {
        if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
        SettingsAction action = static_cast<SettingsAction>(
            reinterpret_cast<intptr_t>(lv_event_get_user_data(event)));
        apply_settings_action(action);
      },
      LV_EVENT_CLICKED,
      reinterpret_cast<void*>(static_cast<intptr_t>(action)));
  lv_obj_add_event_cb(
      button,
      [](lv_event_t* event) {
        switch (lv_event_get_code(event)) {
          case LV_EVENT_PRESSED:
            gatewayRotationPointerPressed();
            break;
          case LV_EVENT_RELEASED:
            gatewayRotationPointerReleased();
            break;
          case LV_EVENT_PRESS_LOST:
            gatewayRotationPointerLost();
            break;
          default:
            break;
        }
      },
      LV_EVENT_PRESSED, nullptr);
  lv_obj_add_event_cb(button, [](lv_event_t* event) {
    if (lv_event_get_code(event) == LV_EVENT_RELEASED)
      gatewayRotationPointerReleased();
  }, LV_EVENT_RELEASED, nullptr);
  lv_obj_add_event_cb(button, [](lv_event_t* event) {
    if (lv_event_get_code(event) == LV_EVENT_PRESS_LOST)
      gatewayRotationPointerLost();
  }, LV_EVENT_PRESS_LOST, nullptr);

  lv_obj_t* label = lv_label_create(button);
  lv_label_set_text(label, text);
  lv_obj_set_width(label, rect.width);
  lv_obj_set_height(label, LV_SIZE_CONTENT);
  style_layout_2_button(
      button, primary,
      layout_2_font(layout_2_style().settings_button_font));
  lv_obj_center(label);
  return button;
}

struct CuckooSettingsLayout {
  GatewaySettingsRect screen;
  GatewaySettingsRect header;
  GatewaySettingsRect title;
  GatewaySettingsRect device;
  GatewaySettingsRect offline_toggle;
  GatewaySettingsRect offline_hint;
  GatewaySettingsRect offline_value;
  GatewaySettingsRect offline_minus;
  GatewaySettingsRect offline_plus;
  GatewaySettingsRect beer_toggle;
  GatewaySettingsRect beer_hint;
  GatewaySettingsRect beer_value;
  GatewaySettingsRect beer_minus;
  GatewaySettingsRect beer_plus;
  GatewaySettingsRect hold_hint;
  GatewaySettingsRect hold_value;
  GatewaySettingsRect hold_minus;
  GatewaySettingsRect hold_plus;
  GatewaySettingsRect back;
  GatewaySettingsRect save;
};

static bool cuckoo_settings_compact_28() {
#if defined(ARDUINO_WAVESHARE_S3_TFT) && !defined(GATEWAY_35) && !defined(GMT154_06)
  return layout_2_screen_width() <= 320 && layout_2_screen_height() <= 320;
#else
  return false;
#endif
}

static CuckooSettingsLayout calculate_cuckoo_settings_layout() {
  const int16_t width = static_cast<int16_t>(layout_2_screen_width());
  const int16_t height = static_cast<int16_t>(layout_2_screen_height());
  const bool portrait = height > width;
  CuckooSettingsLayout layout{};
  layout.screen = {0, 0, width, height};
  if (cuckoo_settings_compact_28()) {
    const int16_t header = 30;
    const int16_t row = portrait ? 36 : 31;
    const int16_t y0 = header + 3;
    const int16_t margin = 6;
    const int16_t hint = portrait ? 69 : 92;
    const int16_t button = 32;
    const int16_t minus_x = margin + hint;
    const int16_t plus_x = width - margin - button;
    const int16_t value_x = minus_x + button + 2;
    const int16_t value_w = plus_x - value_x - 2;
    layout.header = {0, 0, width, header};
    layout.title = {4, 1, static_cast<int16_t>(width - 110), 27};
    layout.device = {static_cast<int16_t>(width - 104), 1, 100, 27};
    layout.offline_toggle = {margin, y0, static_cast<int16_t>(width - 2 * margin), 28};
    layout.offline_hint = {margin, static_cast<int16_t>(y0 + row), hint, 28};
    layout.offline_minus = {minus_x, static_cast<int16_t>(y0 + row), button, 28};
    layout.offline_value = {value_x, static_cast<int16_t>(y0 + row), value_w, 28};
    layout.offline_plus = {plus_x, static_cast<int16_t>(y0 + row), button, 28};
    layout.beer_toggle = {margin, static_cast<int16_t>(y0 + 2 * row), static_cast<int16_t>(width - 2 * margin), 28};
    layout.beer_hint = {margin, static_cast<int16_t>(y0 + 3 * row), hint, 28};
    layout.beer_minus = {minus_x, static_cast<int16_t>(y0 + 3 * row), button, 28};
    layout.beer_value = {value_x, static_cast<int16_t>(y0 + 3 * row), value_w, 28};
    layout.beer_plus = {plus_x, static_cast<int16_t>(y0 + 3 * row), button, 28};
    layout.hold_hint = {margin, static_cast<int16_t>(y0 + 4 * row), hint, 28};
    layout.hold_minus = {minus_x, static_cast<int16_t>(y0 + 4 * row), button, 28};
    layout.hold_value = {value_x, static_cast<int16_t>(y0 + 4 * row), value_w, 28};
    layout.hold_plus = {plus_x, static_cast<int16_t>(y0 + 4 * row), button, 28};
    const int16_t footer_y = height - 34;
    const int16_t footer_w = (width - 3 * margin) / 2;
    layout.back = {margin, footer_y, footer_w, 28};
    layout.save = {static_cast<int16_t>(2 * margin + footer_w), footer_y, footer_w, 28};
    return layout;
  }
  if (portrait) {
    layout.header = {0, 0, width, 44};
    layout.title = {8, 3, 184, 36};
    layout.device = {192, 3, 120, 36};
    layout.offline_toggle = {16, 50, 288, 32};
    layout.offline_hint = {16, 88, 150, 24};
    layout.offline_value = {92, 112, 136, 32};
    layout.offline_minus = {16, 112, 64, 32};
    layout.offline_plus = {240, 112, 64, 32};
    layout.beer_toggle = {16, 152, 288, 32};
    layout.beer_hint = {16, 190, 150, 24};
    layout.beer_value = {92, 214, 136, 32};
    layout.beer_minus = {16, 214, 64, 32};
    layout.beer_plus = {240, 214, 64, 32};
    layout.hold_hint = {16, 258, 150, 24};
    layout.hold_value = {92, 282, 136, 32};
    layout.hold_minus = {16, 282, 64, 32};
    layout.hold_plus = {240, 282, 64, 32};
    layout.back = {16, height - 48, 136, 40};
    layout.save = {168, height - 48, 136, 40};
  } else {
    layout.header = {0, 0, width, 40};
    layout.title = {12, 2, 270, 36};
    layout.device = {286, 2, 182, 36};
    layout.offline_toggle = {20, 48, 210, 32};
    layout.offline_hint = {20, 88, 160, 24};
    layout.offline_value = {310, 84, 110, 32};
    layout.offline_minus = {246, 84, 56, 32};
    layout.offline_plus = {426, 84, 44, 32};
    layout.beer_toggle = {20, 132, 210, 32};
    layout.beer_hint = {20, 172, 160, 24};
    layout.beer_value = {310, 168, 110, 32};
    layout.beer_minus = {246, 168, 56, 32};
    layout.beer_plus = {426, 168, 44, 32};
    layout.hold_hint = {20, 220, 160, 24};
    layout.hold_value = {310, 216, 110, 32};
    layout.hold_minus = {246, 216, 56, 32};
    layout.hold_plus = {426, 216, 44, 32};
#if defined(GATEWAY_35)
    // Leave room for the complete maximum timeout (86400 SEC) at 20 px.
    layout.offline_minus.x = 240;
    layout.offline_value = {304, 84, 120, 32};
    layout.beer_minus.x = 240;
    layout.beer_value = {304, 168, 120, 32};
    layout.hold_minus.x = 240;
    layout.hold_value = {304, 216, 120, 32};
#endif
    layout.back = {20, height - 48, 210, 40};
    layout.save = {250, height - 48, 210, 40};
  }
  return layout;
}

static void apply_cuckoo_settings_action(CuckooSettingsAction action);

static lv_obj_t* create_cuckoo_settings_button(
    lv_obj_t* parent, const char* text, const GatewaySettingsRect& rect,
    CuckooSettingsAction action, bool primary = false) {
  lv_obj_t* button = lv_button_create(parent);
  lv_obj_set_pos(button, rect.x, rect.y);
  lv_obj_set_size(button, rect.width, rect.height);
  lv_obj_clear_flag(button, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(
      button,
      [](lv_event_t* event) {
        if (lv_event_get_code(event) == LV_EVENT_CLICKED)
          apply_cuckoo_settings_action(static_cast<CuckooSettingsAction>(
              reinterpret_cast<intptr_t>(lv_event_get_user_data(event))));
      },
      LV_EVENT_CLICKED,
      reinterpret_cast<void*>(static_cast<intptr_t>(action)));
  lv_obj_add_event_cb(button, [](lv_event_t* event) {
    if (lv_event_get_code(event) == LV_EVENT_PRESSED)
      gatewayRotationPointerPressed();
  }, LV_EVENT_PRESSED, nullptr);
  lv_obj_add_event_cb(button, [](lv_event_t* event) {
    if (lv_event_get_code(event) == LV_EVENT_RELEASED)
      gatewayRotationPointerReleased();
  }, LV_EVENT_RELEASED, nullptr);
  lv_obj_add_event_cb(button, [](lv_event_t* event) {
    if (lv_event_get_code(event) == LV_EVENT_PRESS_LOST)
      gatewayRotationPointerLost();
  }, LV_EVENT_PRESS_LOST, nullptr);
  lv_obj_t* label = lv_label_create(button);
  lv_label_set_text(label, text);
  lv_obj_set_width(label, rect.width);
  lv_obj_set_height(label, LV_SIZE_CONTENT);
  style_layout_2_button(button, primary,
                        layout_2_font(cuckoo_settings_compact_28() ? 12 : layout_2_style().settings_button_font));
  lv_obj_center(label);
  return button;
}

static void gravitymon_gateway_handle_long_press(lv_event_t* event) {
  if (lv_event_get_code(event) != LV_EVENT_LONG_PRESSED) {
    return;
  }

  gravitymon_gateway_open_settings();
}

static lv_obj_t* create_layout_2_divider(lv_obj_t* parent, int32_t x, int32_t y,
                                          int32_t w, int32_t h) {
  const int32_t minimum = layout_2_style().divider_min_width;
  if (w < minimum) w = minimum;
  if (h < minimum) h = minimum;
  lv_obj_t* divider =
      create_dashboard_rect(parent, x, y, w, h, lv_color_hex(0x234153));
  clearDecorativeClickable(divider);
  return divider;
}

static lv_obj_t* create_layout_2_scaled_divider(lv_obj_t* parent, int32_t x,
                                                 int32_t y, int32_t w,
                                                 int32_t h) {
  return create_layout_2_divider(parent, layout_2_scale_x(x),
                                  layout_2_scale_y(y), layout_2_scale_x(w),
                                  layout_2_scale_y(h));
}

static lv_obj_t* create_layout_2_icon_container(lv_obj_t* parent, int32_t x,
                                                 int32_t y, int32_t size) {
  lv_obj_t* icon = lv_obj_create(parent);
  lv_obj_remove_style_all(icon);
  lv_obj_set_pos(icon, layout_2_scale_x(x), layout_2_scale_y(y));
  // Icons remain square; only their position follows the vertical scale.
  const int32_t scaled_size = layout_2_scale_x(size);
  lv_obj_set_size(icon, scaled_size, scaled_size);
  clearDecorativeClickable(icon);
  return icon;
}

static lv_obj_t* create_layout_2_thermometer_icon(lv_obj_t* parent, int32_t x,
                                                  int32_t y, int32_t size) {
  const int32_t scaled_size = layout_2_scale_x(size);
  lv_obj_t* icon = create_layout_2_icon_container(parent, x, y, size);
  const lv_color_t color = lv_color_hex(0x45D9B2);
  const int32_t stem_width = scaled_size >= 18 ? 4 : 3;
  const int32_t bulb_size = scaled_size >= 18 ? 10 : 8;

  lv_obj_t* stem = create_dashboard_rect(
      icon, (scaled_size - stem_width) / 2, layout_2_scale_y(1),
      stem_width, scaled_size - bulb_size - layout_2_scale_y(1),
      color);
  lv_obj_set_style_radius(stem, stem_width / 2, LV_PART_MAIN);
  lv_obj_t* bulb = create_dashboard_rect(
      icon, (scaled_size - bulb_size) / 2,
      scaled_size - bulb_size - layout_2_scale_y(1), bulb_size, bulb_size,
      color);
  lv_obj_set_style_radius(bulb, bulb_size / 2, LV_PART_MAIN);
  return icon;
}

static lv_obj_t* create_layout_2_signal_icon(lv_obj_t* parent, int32_t x,
                                              int32_t y, int32_t size) {
  const int32_t scaled_size = layout_2_scale_x(size);
  lv_obj_t* icon = create_layout_2_icon_container(parent, x, y, size);
  const lv_color_t color = lv_color_hex(0xFFC247);
  const int32_t gap = scaled_size >= 18 ? 2 : 1;
  const int32_t bar_width = (scaled_size - (3 * gap)) / 4;
  const int32_t heights[4] = {scaled_size / 4, scaled_size / 2,
                              (scaled_size * 3) / 4, scaled_size};

  for (int i = 0; i < 4; ++i) {
    lv_obj_t* bar = create_dashboard_rect(
        icon, i * (bar_width + gap), scaled_size - heights[i], bar_width,
        heights[i],
        color);
    lv_obj_set_style_radius(bar, 1, LV_PART_MAIN);
  }
  return icon;
}

static uint8_t layout_2_ring_color_index() {
  GravityDeviceSettings settings{};
  return myConfig.copyDeviceSettings(g_state.data_device_id, &settings)
             ? normalize_ring_color_index(settings.ring_color_index)
             : DEFAULT_RING_COLOR_INDEX;
}

static lv_color_t layout_2_ring_color() {
  return lv_color_hex(ring_color_hex(layout_2_ring_color_index()));
}

static void create_layout_2_dashboard_arc(lv_obj_t* scr, int32_t x, int32_t y,
                                          int32_t size) {
  const GatewayUiStyle& style = layout_2_style();
  const int32_t scaled_x = layout_2_scale_x(x);
  const int32_t scaled_y = layout_2_scale_y(y);
  const int32_t scaled_size = layout_2_scale_x(size);

  g_state.wid_dashboard_arc = lv_arc_create(scr);
  lv_obj_set_pos(g_state.wid_dashboard_arc, scaled_x, scaled_y);
  lv_obj_set_size(g_state.wid_dashboard_arc, scaled_size, scaled_size);
  lv_arc_set_range(g_state.wid_dashboard_arc, 0, 100);
  lv_arc_set_bg_angles(g_state.wid_dashboard_arc, 0, 360);
  lv_arc_set_rotation(g_state.wid_dashboard_arc, 270);
  lv_arc_set_value(g_state.wid_dashboard_arc, 0);
  lv_obj_set_style_arc_color(g_state.wid_dashboard_arc, lv_color_hex(0x234153),
                             LV_PART_MAIN);
  lv_obj_set_style_arc_width(g_state.wid_dashboard_arc,
                             style.dashboard_arc_width, LV_PART_MAIN);
  lv_obj_set_style_arc_color(g_state.wid_dashboard_arc, layout_2_ring_color(),
                             LV_PART_INDICATOR);
  lv_obj_set_style_arc_width(g_state.wid_dashboard_arc,
                             style.dashboard_arc_width, LV_PART_INDICATOR);
  lv_obj_remove_style(g_state.wid_dashboard_arc, NULL, LV_PART_KNOB);
  lv_obj_clear_flag(g_state.wid_dashboard_arc, LV_OBJ_FLAG_CLICKABLE);
}

#if defined(GMT154_06)
static void create_gmt154_hydrometer_dashboard(lv_obj_t* scr) {
  const GatewayHydrometerPageLayout layout = gatewayHydrometerPageLayout240();
  const lv_color_t white = lv_color_hex(0xF4F7F7);
  const lv_color_t teal = lv_color_hex(0x41DFB8);

  lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
  g_state.wid_dashboard_decor[4] = create_dashboard_rect(
      scr, 0, 0, 240, layout.header.header_height,
      lv_color_hex(0x000000));
  g_state.wid_dashboard_decor[0] = create_dashboard_rect(
      scr, layout.header_divider.x, layout.header_divider.y,
      layout.header_divider.width, layout.header_divider.height,
       lv_color_hex(0x2F8F78));
  g_state.wid_dashboard_decor[1] = create_dashboard_rect(
      scr, layout.metrics_divider.x, layout.metrics_divider.y,
      layout.metrics_divider.width, layout.metrics_divider.height,
       lv_color_hex(0x2F8F78));

  auto make_label = [](lv_obj_t* parent, const char* text,
                       const GatewayUiRect& rect, lv_color_t color,
                       const lv_font_t* font, lv_text_align_t align) {
    lv_obj_t* label = lv_label_create(parent);
    lv_label_set_text(label, text ? text : "");
    lv_obj_set_pos(label, rect.x,
                   rect.y + (rect.height - font->line_height) / 2);
    lv_obj_set_size(label, rect.width, font->line_height);
    lv_obj_set_style_text_color(label, color, LV_PART_MAIN);
    lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
    lv_obj_set_style_text_align(label, align, LV_PART_MAIN);
    lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(label, LV_OBJ_FLAG_SCROLLABLE);
    return label;
  };

  g_state.lbl_name = create_layout_2_centered_label(
      scr, "", layout.header.name, white, layout_2_font(14));
  lv_obj_set_style_text_align(g_state.lbl_name, LV_TEXT_ALIGN_LEFT,
                              LV_PART_MAIN);
  lv_label_set_long_mode(g_state.lbl_name, LV_LABEL_LONG_CLIP);
  g_state.lbl_source = nullptr;
  g_state.lbl_index = create_layout_2_centered_label(
      scr, "", layout.header.index, teal, layout_2_font(14));

  g_state.gravity_offline_frame = create_gmt154_offline_frame(
      scr, layout.gravity.x, layout.gravity.y, layout.gravity.width, layout.gravity.height);
  g_state.lbl_measurement = create_layout_2_centered_label(
      scr, "--", layout.gravity, white, gmt154_sg_font());
  lv_label_set_long_mode(g_state.lbl_measurement, LV_LABEL_LONG_CLIP);

  g_state.lbl_temp = make_label(scr, "--", layout.temperature, white,
                                layout_2_font(20), LV_TEXT_ALIGN_LEFT);
  g_state.lbl_rssi = make_label(scr, "--", layout.rssi,
                                lv_color_hex(0xFFBD3F), layout_2_font(20),
                                LV_TEXT_ALIGN_LEFT);

  lv_obj_t* ip_panel = lv_obj_create(scr);
  lv_obj_remove_style_all(ip_panel);
  lv_obj_set_pos(ip_panel, layout.ip_panel.x, layout.ip_panel.y);
  lv_obj_set_size(ip_panel, layout.ip_panel.width, layout.ip_panel.height);
   lv_obj_set_style_bg_color(ip_panel, lv_color_hex(0x000000), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(ip_panel, LV_OPA_COVER, LV_PART_MAIN);
   lv_obj_set_style_border_color(ip_panel, lv_color_hex(0x2F8F78), LV_PART_MAIN);
  lv_obj_set_style_border_width(ip_panel, 1, LV_PART_MAIN);
  lv_obj_set_style_radius(ip_panel, 6, LV_PART_MAIN);
  lv_obj_clear_flag(ip_panel, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(ip_panel, LV_OBJ_FLAG_SCROLLABLE);
  g_state.lbl_gateway_ip = create_layout_2_centered_label(
       scr, "IP --", layout.gateway_ip, white, layout_2_font(16));

  g_state.lbl_progress = nullptr;
  g_state.lbl_abv = nullptr;
  g_state.lbl_wifi_rssi = nullptr;
  g_state.wid_dashboard_arc = nullptr;
  g_state.wid_dashboard_touch = nullptr;
  layout_2_refresh_header_battery(scr);
}
#endif

static void create_layout_2_dashboard_landscape(lv_obj_t* scr) {
#if defined(GMT154_06)
  create_gmt154_hydrometer_dashboard(scr);
  return;
#else
  const GatewayUiStyle& style = layout_2_style();
  const GatewayDashboardHeaderLayout header = gatewayDashboardHeaderLayout(
      static_cast<int16_t>(layout_2_screen_width()),
      static_cast<int16_t>(layout_2_screen_height()), false);
  g_state.wid_dashboard_decor[4] =
      create_layout_2_rect(scr, 0, 0, 320, 34, lv_color_hex(0x0B1B24));
  g_state.wid_dashboard_decor[5] =
      create_layout_2_rect(scr, 0, 211, 320, 29, lv_color_hex(0x0B1B24));
  g_state.wid_dashboard_decor[0] =
      create_layout_2_scaled_divider(scr, 0, 34, 320, 1);
  g_state.wid_dashboard_decor[1] =
      create_layout_2_scaled_divider(scr, 12, 181, 296, 1);
  g_state.wid_dashboard_decor[2] =
      create_layout_2_scaled_divider(scr, 107, 181, 1, 30);
  g_state.wid_dashboard_decor[3] =
      create_layout_2_scaled_divider(scr, 214, 181, 1, 30);

  g_state.lbl_name = create_layout_2_centered_label(
      scr, "", header.name, lv_color_hex(0xF4F7F7),
      layout_2_font(style.dashboard_header_primary_font));
  lv_label_set_long_mode(g_state.lbl_name, LV_LABEL_LONG_CLIP);
  g_state.lbl_source = create_layout_2_centered_label(
      scr, "", header.source, lv_color_hex(0x45D9B2),
      layout_2_font(style.dashboard_header_secondary_font));
  g_state.lbl_index = create_layout_2_centered_label(
      scr, "", header.index, lv_color_hex(0xF4F7F7),
      layout_2_font(style.dashboard_header_secondary_font));

#if defined(ARDUINO_WAVESHARE_S3_TFT) && !defined(GATEWAY_35) && !defined(GMT154_06)
  constexpr int32_t dashboard_content_y_offset = 8;
#else
  constexpr int32_t dashboard_content_y_offset = 0;
#endif
  create_layout_2_dashboard_arc(scr, 24, 40 + dashboard_content_y_offset, 120);

  g_state.lbl_progress = create_layout_2_label(
      scr, "--", 40, 84 + dashboard_content_y_offset, 90, 44, LV_TEXT_ALIGN_CENTER,
      lv_color_hex(0x45D9B2), layout_2_font(style.dashboard_progress_font));
  g_state.lbl_measurement = create_layout_2_centered_label(
      scr, "--", 156, 66 + dashboard_content_y_offset, 152, 68,
      lv_color_hex(0xF4F7F7), layout_2_font(style.dashboard_measurement_font));
#if defined(ARDUINO_WAVESHARE_S3_TFT) || defined(GATEWAY_35)
  g_state.lbl_measurement_unit = lv_label_create(lv_obj_get_parent(g_state.lbl_measurement));
  lv_obj_set_style_text_font(g_state.lbl_measurement_unit, layout_2_font(20), LV_PART_MAIN);
  lv_obj_set_style_text_color(g_state.lbl_measurement_unit, lv_color_hex(0x41DFB8), LV_PART_MAIN);
#endif
  lv_label_set_long_mode(g_state.lbl_measurement, LV_LABEL_LONG_CLIP);
  g_state.lbl_temp = create_layout_2_label(
      scr, "--", 0, 188, 107, 18, LV_TEXT_ALIGN_CENTER,
      lv_color_hex(0xF4F7F7), layout_2_font(style.dashboard_metric_font));
  g_state.lbl_abv = create_layout_2_label(
      scr, "--", 107, 188, 107, 18, LV_TEXT_ALIGN_CENTER,
      lv_color_hex(0x45D9B2), layout_2_font(style.dashboard_metric_font));
  g_state.lbl_rssi = create_layout_2_label(
      scr, "--", 214, 188, 106, 18, LV_TEXT_ALIGN_CENTER,
      lv_color_hex(0xFFC247), layout_2_font(style.dashboard_metric_font));
  g_state.lbl_gateway_ip = create_layout_2_label(
      scr, "IP --", 12, 217, 178, 16, LV_TEXT_ALIGN_CENTER,
      lv_color_hex(0xF4F7F7), layout_2_font(style.dashboard_footer_font));
  g_state.lbl_wifi_rssi = create_layout_2_label(
      scr, "RSSI --", 192, 217, 116, 16, LV_TEXT_ALIGN_CENTER,
      lv_color_hex(0xFFC247), layout_2_font(style.dashboard_footer_font));
  g_state.wid_dashboard_touch = lv_obj_create(scr);
  lv_obj_remove_style_all(g_state.wid_dashboard_touch);
  lv_obj_set_pos(g_state.wid_dashboard_touch, layout_2_scale_x(0),
                 layout_2_scale_y(34));
  lv_obj_set_size(g_state.wid_dashboard_touch, layout_2_screen_width(),
                  layout_2_scale_y(211) - layout_2_scale_y(34));
  lv_obj_add_flag(g_state.wid_dashboard_touch, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(g_state.wid_dashboard_touch, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(g_state.wid_dashboard_touch,
                      gravitymon_gateway_handle_long_press,
                      LV_EVENT_LONG_PRESSED, NULL);
  lv_obj_add_event_cb(g_state.wid_dashboard_touch, [](lv_event_t* event) {
    switch (lv_event_get_code(event)) {
      case LV_EVENT_PRESSED:
        gatewayRotationPointerPressed();
        break;
      case LV_EVENT_RELEASED:
        gatewayRotationPointerReleased();
        break;
      case LV_EVENT_PRESS_LOST:
        gatewayRotationPointerLost();
        break;
      default:
        break;
    }
  }, LV_EVENT_PRESSED, NULL);
  lv_obj_add_event_cb(g_state.wid_dashboard_touch, [](lv_event_t* event) {
    if (lv_event_get_code(event) == LV_EVENT_RELEASED)
      gatewayRotationPointerReleased();
  }, LV_EVENT_RELEASED, NULL);
  lv_obj_add_event_cb(g_state.wid_dashboard_touch, [](lv_event_t* event) {
    if (lv_event_get_code(event) == LV_EVENT_PRESS_LOST)
      gatewayRotationPointerLost();
  }, LV_EVENT_PRESS_LOST, NULL);
  layout_2_refresh_header_battery(scr);
#endif
}

static void create_layout_2_dashboard_portrait(lv_obj_t* scr) {
  const GatewayUiStyle& style = layout_2_style();
  const int32_t screen_width = layout_2_screen_width();
  const int32_t screen_height = layout_2_screen_height();
  const GatewayDashboardHeaderLayout header = gatewayDashboardHeaderLayout(
      static_cast<int16_t>(screen_width), static_cast<int16_t>(screen_height),
      true);
  const int32_t bottom_y = layout_2_scale_y(292);
  const int32_t bottom_height = screen_height - bottom_y;
  g_state.wid_dashboard_decor[4] =
      create_layout_2_rect(scr, 0, 0, 240, 34, lv_color_hex(0x0B1B24));
  g_state.wid_dashboard_decor[5] =
      create_dashboard_rect(scr, 0, bottom_y, screen_width, bottom_height,
                            lv_color_hex(0x0B1B24));
  g_state.wid_dashboard_decor[0] =
      create_layout_2_scaled_divider(scr, 0, 34, 240, 1);
#if defined(ARDUINO_WAVESHARE_S3_TFT) || defined(GATEWAY_35)
  // The three metrics use two text rows between the measurement and footer.
  constexpr int32_t metrics_top = 248;
#else
  constexpr int32_t metrics_top = 264;
#endif
  g_state.wid_dashboard_decor[1] =
      create_layout_2_scaled_divider(scr, 0, metrics_top, 240, 1);
  g_state.wid_dashboard_decor[2] =
      create_layout_2_scaled_divider(scr, 80, metrics_top, 1, 292 - metrics_top);
  g_state.wid_dashboard_decor[3] =
      create_layout_2_scaled_divider(scr, 160, metrics_top, 1, 292 - metrics_top);

  g_state.lbl_name = create_layout_2_centered_label(
      scr, "", header.name, lv_color_hex(0xF4F7F7),
      layout_2_font(style.dashboard_header_primary_font));
  lv_label_set_long_mode(g_state.lbl_name, LV_LABEL_LONG_CLIP);
  g_state.lbl_source = create_layout_2_centered_label(
      scr, "", header.source, lv_color_hex(0x45D9B2),
      layout_2_font(style.dashboard_header_secondary_font));
  g_state.lbl_index = create_layout_2_centered_label(
      scr, "", header.index, lv_color_hex(0xF4F7F7),
      layout_2_font(style.dashboard_header_secondary_font));

#if defined(ARDUINO_WAVESHARE_S3_TFT) || defined(GATEWAY_35)
  create_layout_2_dashboard_arc(scr, 54, 44, 132);
  constexpr int32_t progress_y = 87;
  constexpr int32_t measurement_y = 184;
#else
  create_layout_2_dashboard_arc(scr, 48, 48, 144);
  constexpr int32_t progress_y = 96;
  constexpr int32_t measurement_y = 196;
#endif

  g_state.lbl_progress = create_layout_2_label(
      scr, "--", 68, progress_y, 104, 46, LV_TEXT_ALIGN_CENTER,
      lv_color_hex(0x45D9B2), layout_2_font(style.dashboard_progress_font));
  g_state.lbl_measurement = create_layout_2_centered_label(
      scr, "--", 20, measurement_y, 200, 58, lv_color_hex(0xF4F7F7),
      layout_2_font(style.dashboard_measurement_font));
#if defined(ARDUINO_WAVESHARE_S3_TFT) || defined(GATEWAY_35)
  g_state.lbl_measurement_unit = lv_label_create(lv_obj_get_parent(g_state.lbl_measurement));
  lv_obj_set_style_text_font(g_state.lbl_measurement_unit, layout_2_font(20), LV_PART_MAIN);
  lv_obj_set_style_text_color(g_state.lbl_measurement_unit, lv_color_hex(0x41DFB8), LV_PART_MAIN);
#endif
#if defined(ARDUINO_WAVESHARE_S3_TFT) || defined(GATEWAY_35)
  constexpr int32_t metric_value_y = 272;
  constexpr int32_t metric_value_height = 18;
  const lv_color_t metric_label_color = lv_color_hex(0x91ABB2);
  create_layout_2_label(scr, "TEMP", 0, 251, 80, 15, LV_TEXT_ALIGN_CENTER,
                        metric_label_color, layout_2_font(12));
  create_layout_2_label(scr, "ABV", 80, 251, 80, 15, LV_TEXT_ALIGN_CENTER,
                        metric_label_color, layout_2_font(12));
  create_layout_2_label(scr, "RSSI", 160, 251, 80, 15, LV_TEXT_ALIGN_CENTER,
                        metric_label_color, layout_2_font(12));
#else
  constexpr int32_t metric_value_y = 272;
  constexpr int32_t metric_value_height = 18;
#endif
  g_state.lbl_temp = create_layout_2_label(
      scr, "--", 0, metric_value_y, 80, metric_value_height, LV_TEXT_ALIGN_CENTER,
      lv_color_hex(0xF4F7F7), layout_2_font(style.dashboard_metric_font));
  g_state.lbl_abv = create_layout_2_label(
      scr, "--", 80, metric_value_y, 80, metric_value_height, LV_TEXT_ALIGN_CENTER,
      lv_color_hex(0x45D9B2), layout_2_font(style.dashboard_metric_font));
  g_state.lbl_rssi = create_layout_2_label(
      scr, "--", 160, metric_value_y, 80, metric_value_height, LV_TEXT_ALIGN_CENTER,
      lv_color_hex(0xFFC247), layout_2_font(style.dashboard_metric_font));
  g_state.lbl_gateway_ip = create_layout_2_label(
      scr, "IP --", 10, 299, 116, 14, LV_TEXT_ALIGN_CENTER,
      lv_color_hex(0xF4F7F7), layout_2_font(style.dashboard_footer_font));
  g_state.lbl_wifi_rssi = create_layout_2_label(
      scr, "RSSI --", 126, 299, 104, 14, LV_TEXT_ALIGN_CENTER,
      lv_color_hex(0xFFC247), layout_2_font(style.dashboard_footer_font));
  g_state.wid_dashboard_touch = lv_obj_create(scr);
  lv_obj_remove_style_all(g_state.wid_dashboard_touch);
  lv_obj_set_pos(g_state.wid_dashboard_touch, layout_2_scale_x(0),
                 layout_2_scale_y(34));
  lv_obj_set_size(g_state.wid_dashboard_touch, screen_width,
                  bottom_y - layout_2_scale_y(34));
  lv_obj_add_flag(g_state.wid_dashboard_touch, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(g_state.wid_dashboard_touch, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(g_state.wid_dashboard_touch,
                      gravitymon_gateway_handle_long_press,
                      LV_EVENT_LONG_PRESSED, NULL);
  lv_obj_add_event_cb(g_state.wid_dashboard_touch, [](lv_event_t* event) {
    switch (lv_event_get_code(event)) {
      case LV_EVENT_PRESSED:
        gatewayRotationPointerPressed();
        break;
      case LV_EVENT_RELEASED:
        gatewayRotationPointerReleased();
        break;
      case LV_EVENT_PRESS_LOST:
        gatewayRotationPointerLost();
        break;
      default:
        break;
    }
  }, LV_EVENT_PRESSED, NULL);
  lv_obj_add_event_cb(g_state.wid_dashboard_touch, [](lv_event_t* event) {
    if (lv_event_get_code(event) == LV_EVENT_RELEASED)
      gatewayRotationPointerReleased();
  }, LV_EVENT_RELEASED, NULL);
  lv_obj_add_event_cb(g_state.wid_dashboard_touch, [](lv_event_t* event) {
    if (lv_event_get_code(event) == LV_EVENT_PRESS_LOST)
      gatewayRotationPointerLost();
  }, LV_EVENT_PRESS_LOST, NULL);
  layout_2_refresh_header_battery(scr);
}

void gravitymon_gateway_open_settings() {
  if (layout_mgr.current_layout != 2 || gravitymon_gateway_is_settings_open() ||
      !g_state.data_device_id[0]) {
    return;
  }

  gatewayRotationSettingsOpened();
  GravityDeviceSettings settings{};
  if (!myConfig.copyDeviceSettingsOrDefault(g_state.data_device_id,
                                            &settings) ||
      !g_state.settings_ui.openForDevice(g_state.data_device_id, settings)) {
    gatewayRotationSettingsClosed();
    return;
  }
  g_settings_thread_state.publishOpen(true);
  gravitymon_gateway_rebuild_settings_page();
}

static const char* settings_ring_color_name(uint8_t index) {
  static const char* const names[RING_COLOR_OPTION_COUNT] = {
      "YELLOW", "PURPLE", "RED", "GREEN",
      "BLUE", "WHITE", "BLACK", "BLE DEFAULT"};
  return names[normalize_ring_color_index(index)];
}

static const char* settings_page_title(SettingsPage page) {
  switch (page) {
    case SettingsPage::Og:
      return "OG SETTING";
    case SettingsPage::TemperatureChange:
      return "TEMP CHANGE";
    case SettingsPage::RingColor:
      return "RING COLOR";
    case SettingsPage::OfflineAlarm:
      return "OFFLINE ALARM";
    default:
      return "SETTINGS";
  }
}

static lv_obj_t* create_settings_home_cell(
    lv_obj_t* parent, const GatewaySettingsRect& rect, const char* title,
    const char* summary, SettingsAction action) {
  char text[64];
  snprintf(text, sizeof(text), "%s\n%s  >", title, summary);
  lv_obj_t* cell = create_settings_button(parent, text, rect, action, false);
  lv_obj_set_style_bg_color(cell, lv_color_hex(0x0B1B24), LV_PART_MAIN);
  lv_obj_set_style_border_width(cell, 0, LV_PART_MAIN);
  lv_obj_t* label = lv_obj_get_child(cell, 0);
  if (label) {
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  }
  return cell;
}

static void create_settings_shell(const GatewaySettingsLayout& layout,
                                  const char* title) {
  const GatewayUiStyle& style = layout_2_style();
  lv_obj_t* scr = lv_scr_act();
  g_state.settings_panel = create_dashboard_rect(
      scr, layout.screen.x, layout.screen.y, layout.screen.width,
      layout.screen.height, lv_color_hex(0x061015));
  lv_obj_clear_flag(g_state.settings_panel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(g_state.settings_panel, LV_SCROLLBAR_MODE_OFF);
  create_dashboard_rect(g_state.settings_panel, layout.header.x,
                        layout.header.y, layout.header.width,
                        layout.header.height, lv_color_hex(0x0B1B24));
  g_state.settings_title = create_settings_centered_label(
      g_state.settings_panel, title, layout.header_title,
      lv_color_hex(0x45D9B2),
      layout_2_font(style.settings_title_font));
  create_settings_centered_label(
      g_state.settings_panel, g_state.settings_ui.deviceId(),
      layout.header_device, lv_color_hex(0xF4F7F7),
      layout_2_font(style.settings_device_font));
  const GatewaySettingsRect error_rect{
      0, static_cast<int16_t>(layout.header.height - 16), layout.screen.width,
      14};
  g_state.settings_error = create_settings_centered_label(
      g_state.settings_panel,
      g_state.settings_ui.saveError() ? "SAVE FAILED" : "", error_rect,
      lv_color_hex(0xFF5D62), layout_2_font(style.settings_error_font));
  create_layout_2_divider(g_state.settings_panel, 0, layout.header.height - 1,
                          layout.screen.width, 1);
  create_dashboard_rect(g_state.settings_panel, layout.footer.x,
                        layout.footer.y, layout.footer.width,
                        layout.footer.height, lv_color_hex(0x0B1B24));
}

static void create_settings_footer(const GatewaySettingsLayout& layout,
                                   bool home) {
  if (home) {
    create_settings_button(g_state.settings_panel, "BACK", layout.home_back,
                           SettingsAction::HomeBack, false);
    return;
  }
  create_settings_button(g_state.settings_panel, "BACK", layout.detail_back,
                         SettingsAction::DetailBack, false);
  create_settings_button(g_state.settings_panel, "SAVE", layout.detail_save,
                         SettingsAction::Save, true);
}

static void create_settings_step_row(const GatewaySettingsLayout& layout,
                                     int row, const char* label,
                                     SettingsAction minus,
                                     SettingsAction plus) {
  const GatewayUiStyle& style = layout_2_style();
  const GatewaySettingsRect& value = layout.value_fields[row];
  create_gravity_settings_compact_label(
      g_state.settings_panel, label, layout.row_hints[row],
      lv_color_hex(0x7E8F97),
      layout_2_font(style.settings_hint_font));
  create_settings_button(g_state.settings_panel, "-", layout.minus_buttons[row],
                         minus, false);
  create_settings_button(g_state.settings_panel, "+", layout.plus_buttons[row],
                         plus, false);
  g_state.settings_value[row + 1] = create_gravity_settings_compact_label(
      g_state.settings_panel, "--", value, lv_color_hex(0xFFC247),
      layout_2_font(style.settings_value_font));
}

static void create_settings_home(const GatewaySettingsLayout& layout) {
  const GravityDeviceSettings& draft = g_state.settings_ui.draft();
  char og[20];
  char temperature[32];
  char offline[32];
  if (isfinite(draft.og_sg))
    snprintf(og, sizeof(og), "%.3f SG", draft.og_sg);
  else
    snprintf(og, sizeof(og), "NOT SET");
  snprintf(temperature, sizeof(temperature), "%s %u MIN / %.1f C",
           draft.temperature_change_alarm_enabled ? "ON" : "OFF",
           draft.temperature_compare_minutes,
           draft.temperature_change_threshold_c);
  snprintf(offline, sizeof(offline), "%s %lu SEC",
           draft.offline_alarm_enabled ? "ON" : "OFF",
           static_cast<unsigned long>(draft.offline_timeout_seconds));

  create_settings_home_cell(g_state.settings_panel, layout.home_cells[0],
                            "OG SETTING", og, SettingsAction::HomeOg);
  create_settings_home_cell(g_state.settings_panel, layout.home_cells[1],
                            "TEMP CHANGE", temperature,
                            SettingsAction::HomeTemperatureChange);
  create_settings_home_cell(
      g_state.settings_panel, layout.home_cells[2], "RING COLOR",
      settings_ring_color_name(draft.ring_color_index),
      SettingsAction::HomeRingColor);
  create_settings_home_cell(g_state.settings_panel, layout.home_cells[3],
                            "OFFLINE ALARM", offline,
                            SettingsAction::HomeOfflineAlarm);
  create_layout_2_divider(g_state.settings_panel, layout.screen.width / 2,
                          layout.header.height, 1, layout.content.height);
  create_layout_2_divider(g_state.settings_panel, 0,
                          layout.header.height + layout.content.height / 2,
                          layout.screen.width, 1);
  create_settings_footer(layout, true);
}

static void create_settings_og(const GatewaySettingsLayout& layout) {
  create_gravity_settings_compact_label(
      g_state.settings_panel, "1.000-1.200 SG / STEP 0.001", layout.og_hint,
      lv_color_hex(0x7E8F97),
      layout_2_font(layout_2_style().settings_hint_font));
  create_settings_step_row(layout, 0, "OG", SettingsAction::OgMinus,
                           SettingsAction::OgPlus);
  create_settings_footer(layout, false);
}

static void create_settings_temperature(const GatewaySettingsLayout& layout) {
  lv_obj_t* toggle = create_settings_button(
      g_state.settings_panel, "OFF", layout.toggle,
      SettingsAction::TemperatureToggle, true);
  g_state.settings_value[0] = lv_obj_get_child(toggle, 0);
  create_settings_step_row(layout, 0, "COMPARE MIN",
                           SettingsAction::CompareMinus,
                           SettingsAction::ComparePlus);
  create_settings_step_row(layout, 1, "THRESHOLD C",
                           SettingsAction::ThresholdMinus,
                           SettingsAction::ThresholdPlus);
  create_settings_footer(layout, false);
}

static void create_settings_ring_color(const GatewaySettingsLayout& layout) {
  for (uint8_t i = 0; i < RING_COLOR_OPTION_COUNT; ++i) {
    lv_obj_t* swatch = lv_button_create(g_state.settings_panel);
    const GatewaySettingsRect& rect = layout.ring_swatches[i];
    lv_obj_set_pos(swatch, rect.x, rect.y);
    lv_obj_set_size(swatch, rect.width, rect.height);
    lv_obj_set_style_bg_color(swatch, lv_color_hex(ring_color_hex(i)),
                              LV_PART_MAIN);
    lv_obj_set_style_bg_opa(swatch, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(swatch, 6, LV_PART_MAIN);
    lv_obj_add_event_cb(
        swatch,
        [](lv_event_t* event) {
          if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
          const uint8_t index = static_cast<uint8_t>(
              reinterpret_cast<intptr_t>(lv_event_get_user_data(event)));
#if defined(GATEWAY_35) && GATEWAY_TOUCH_DIAGNOSTICS
          lv_area_t rect{};
          lv_obj_get_coords(lv_event_get_target_obj(event), &rect);
          Log.notice(F("TOUCH-HIT color=%d rect=%d:%d:%d:%d" CR),
                     static_cast<int>(index), static_cast<int>(rect.x1),
                     static_cast<int>(rect.y1), static_cast<int>(rect.x2),
                     static_cast<int>(rect.y2));
#endif
          g_state.settings_ui.selectRingColor(index);
          gravitymon_gateway_refresh_settings_page();
        },
         LV_EVENT_CLICKED, reinterpret_cast<void*>(static_cast<intptr_t>(i)));
    lv_obj_add_event_cb(swatch, [](lv_event_t* event) {
      if (lv_event_get_code(event) == LV_EVENT_PRESSED)
        gatewayRotationPointerPressed();
    }, LV_EVENT_PRESSED, nullptr);
    lv_obj_add_event_cb(swatch, [](lv_event_t* event) {
      if (lv_event_get_code(event) == LV_EVENT_RELEASED)
        gatewayRotationPointerReleased();
    }, LV_EVENT_RELEASED, nullptr);
    lv_obj_add_event_cb(swatch, [](lv_event_t* event) {
      if (lv_event_get_code(event) == LV_EVENT_PRESS_LOST)
        gatewayRotationPointerLost();
    }, LV_EVENT_PRESS_LOST, nullptr);
    g_state.settings_ring_color[i] = swatch;
  }
  g_state.settings_value[0] = create_gravity_settings_compact_label(
      g_state.settings_panel, "BLE DEFAULT", layout.ring_summary,
      lv_color_hex(RING_COLOR_DEFAULT_HEX),
      layout_2_font(layout_2_style().settings_summary_font));
  create_settings_footer(layout, false);
}

static void create_settings_offline(const GatewaySettingsLayout& layout) {
  lv_obj_t* toggle = create_settings_button(
      g_state.settings_panel, "OFF", layout.toggle,
      SettingsAction::OfflineToggle, true);
  g_state.settings_value[0] = lv_obj_get_child(toggle, 0);
  create_settings_step_row(layout, 0, "TIMEOUT SEC",
                           SettingsAction::OfflineMinus,
                           SettingsAction::OfflinePlus);
  create_settings_footer(layout, false);
}

static void gravitymon_gateway_refresh_settings_page() {
  if (!g_state.settings_ui.isOpen()) return;
  const GravityDeviceSettings& draft = g_state.settings_ui.draft();
  char value[32];
  if (g_state.settings_error) {
    lv_label_set_text(g_state.settings_error,
                      g_state.settings_ui.saveError() ? "SAVE FAILED" : "");
  }
  switch (g_state.settings_ui.page()) {
    case SettingsPage::Og:
      if (g_state.settings_value[1]) {
        if (isfinite(draft.og_sg))
          snprintf(value, sizeof(value), "%.3f SG", draft.og_sg);
        else
          snprintf(value, sizeof(value), "NOT SET");
        lv_label_set_text(g_state.settings_value[1], value);
      }
      break;
    case SettingsPage::TemperatureChange:
      if (g_state.settings_value[0])
        lv_label_set_text(g_state.settings_value[0],
                          draft.temperature_change_alarm_enabled ? "ON" : "OFF");
      if (g_state.settings_value[1]) {
        snprintf(value, sizeof(value), "%u MIN",
                 draft.temperature_compare_minutes);
        lv_label_set_text(g_state.settings_value[1], value);
      }
      if (g_state.settings_value[2]) {
        snprintf(value, sizeof(value), "%.1f C",
                 draft.temperature_change_threshold_c);
        lv_label_set_text(g_state.settings_value[2], value);
      }
      break;
    case SettingsPage::RingColor:
      if (g_state.settings_value[0])
        lv_label_set_text(g_state.settings_value[0],
                          settings_ring_color_name(draft.ring_color_index));
      for (uint8_t i = 0; i < RING_COLOR_OPTION_COUNT; ++i) {
        if (!g_state.settings_ring_color[i]) continue;
        const bool selected = i == normalize_ring_color_index(
                                        draft.ring_color_index);
        lv_obj_set_style_border_width(g_state.settings_ring_color[i],
                                      selected ? 4 : 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(
            g_state.settings_ring_color[i],
            selected ? lv_color_hex(0xF4F7F7) : lv_color_hex(0x234153),
            LV_PART_MAIN);
      }
      break;
    case SettingsPage::OfflineAlarm:
      if (g_state.settings_value[0])
        lv_label_set_text(g_state.settings_value[0],
                          draft.offline_alarm_enabled ? "ON" : "OFF");
      if (g_state.settings_value[1]) {
        snprintf(value, sizeof(value), "%lu SEC",
                 static_cast<unsigned long>(draft.offline_timeout_seconds));
        lv_label_set_text(g_state.settings_value[1], value);
      }
      break;
    default:
      break;
  }
}

static void gravitymon_gateway_rebuild_settings_page() {
  freeze_diag::Scope diagnostic(freeze_diag::Rebuild);
  FreezeDiagMemoryScope diagnostic_memory(freeze_diag::RebuildEnter, freeze_diag::RebuildExit);
  if (!g_state.settings_ui.isOpen()) {
    close_settings_overlay();
    return;
  }
  if (g_state.settings_panel) lv_obj_del(g_state.settings_panel);
  reset_settings_widget_pointers();
  const int16_t width = static_cast<int16_t>(layout_2_screen_width());
  const int16_t height = static_cast<int16_t>(layout_2_screen_height());
  const GatewaySettingsLayout layout =
      calculateGatewaySettingsLayout(width, height);
  create_settings_shell(layout,
                        settings_page_title(g_state.settings_ui.page()));
  switch (g_state.settings_ui.page()) {
    case SettingsPage::Home:
      create_settings_home(layout);
      break;
    case SettingsPage::Og:
      create_settings_og(layout);
      break;
    case SettingsPage::TemperatureChange:
      create_settings_temperature(layout);
      break;
    case SettingsPage::RingColor:
      create_settings_ring_color(layout);
      break;
    case SettingsPage::OfflineAlarm:
      create_settings_offline(layout);
      break;
  }
  gravitymon_gateway_refresh_settings_page();
}

/**
 * Setup layout 2: fixed dark dashboard for the gateway display.
 */
static void gravitymon_gateway_setup_layout_2(void) {
  lv_obj_t* scr = lv_scr_act();
  if (!scr) return;

#if defined(ARDUINO_WAVESHARE_S3_TFT) || defined(GATEWAY_35)
  const lv_color_t kLayout2Background = lv_color_hex(0x000000);
#else
  const lv_color_t kLayout2Background = lv_color_hex(0x080B0E);
#endif
  lv_obj_set_style_bg_color(scr, kLayout2Background, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);

  g_state.gravity_root = lv_obj_create(scr);
  lv_obj_remove_style_all(g_state.gravity_root);
  lv_obj_set_pos(g_state.gravity_root, 0, 0);
  lv_obj_set_size(g_state.gravity_root, layout_2_screen_width(),
                  layout_2_screen_height());
  lv_obj_set_style_bg_color(g_state.gravity_root, kLayout2Background,
                            LV_PART_MAIN);
  lv_obj_set_style_bg_opa(g_state.gravity_root, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_clear_flag(g_state.gravity_root, LV_OBJ_FLAG_SCROLLABLE);
  if (layout_2_is_portrait()) {
    create_layout_2_dashboard_portrait(g_state.gravity_root);
  } else {
    create_layout_2_dashboard_landscape(g_state.gravity_root);
  }
  if (g_state.display_source == GatewayDisplayItemKind::CuckooPi) {
    create_cuckoo_dashboard(scr);
  }
  select_layout_2_source();
}

/**
 * Fetch the current active layout
 *
 * @return layout_id Layout index
 */
uint8_t gravitymon_gateway_get_layout() { return layout_mgr.current_layout; }

static bool gravity_settings_state_open() {
  return g_state.settings_ui.isOpen();
}

bool gravitymon_gateway_is_settings_open() {
  return g_settings_thread_state.isOpen() &&
         (gravity_settings_state_open() ||
          g_state.cuckoo_settings_ui.isOpen());
}

static void gravitymon_gateway_open_cuckoo_settings() {
  if (layout_mgr.current_layout != 2 || gravitymon_gateway_is_settings_open() ||
      !g_state.data_device_id[0]) {
    return;
  }
  CuckooPiConfig config{};
  if (!myConfig.copyCuckooPiConfig(&config) || !config.enabled) return;
  CuckooPiDeviceConfig settings{};
  bool found = false;
  for (uint8_t index = 0; index < config.device_count &&
                            index < kMaxCuckooPiDevices;
       ++index) {
    if (strcmp(config.devices[index].id, g_state.data_device_id) == 0) {
      settings = config.devices[index];
      found = true;
      break;
    }
  }
  if (!found || !g_state.cuckoo_settings_ui.openForDevice(
                    g_state.data_device_id, settings))
    return;
  gatewayRotationSettingsOpened();
  g_settings_thread_state.publishOpen(true);
  gravitymon_gateway_rebuild_cuckoo_settings_page();
}

/**
 * Switch to the specified layout ID.
 *
 * Cleans up the current layout, resets state, and constructs the layout
 * selected by the active board's layout normalization policy.
 *
 * @param layout_id Layout index to activate.
 */
bool gravitymon_gateway_set_layout(uint8_t layout_id) {
  layout_id = gatewayNormalizeLayoutId(layout_id);

  const GatewayLayoutTransitionPlan plan = gatewayPlanLayoutTransition(
      layout_mgr.current_layout, layout_id, true);
  if (plan.result == GatewayLayoutTransitionResult::Noop) {
    Log.verbose(F("UI  : Layout %d already active" CR), layout_id);
    return true;
  }
  GatewayAlertUiTeardownReservation reservation;
  if (!reservation.owns() ||
      plan.result == GatewayLayoutTransitionResult::Rejected) {
    return false;
  }

  // Cleanup current layout objects
  const bool cleanup_succeeded =
      gravitymon_gateway_cleanup_layout(reservation.owns());
  if (gatewayCommitLayoutTransition(plan, cleanup_succeeded) !=
      GatewayLayoutTransitionResult::Accepted) {
    return false;
  }

  g_state.settings_ui.clear();
  g_state.cuckoo_settings_ui.clear();
  close_settings_overlay();
  gatewayRotationReleaseLocks();

  // Update layout ID
  layout_mgr.current_layout = plan.target_layout;
  g_state.orientation = plan.target_orientation;

  if (layout_id != 2) {
    lv_obj_t* scr = lv_scr_act();
    if (scr) {
      ui_theme_colors_t theme_colors = ui_get_theme_colors(
          g_state.flg_darkmode ? UI_THEME_DARK : UI_THEME_LIGHT);
      lv_obj_set_style_bg_color(scr, theme_colors.bg, LV_PART_MAIN);
      lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
    }
  }

  // Setup new layout
  if (plan.target_layout == 0) {
    gravitymon_gateway_setup_layout_0();
  } else if (plan.target_layout == 1) {
    gravitymon_gateway_setup_layout_1();
  } else if (plan.target_layout == 2) {
    gravitymon_gateway_setup_layout_2();
  }

  Log.verbose(F("UI  : Layout switched to %d" CR), plan.target_layout);
  return true;
}

static void create_cuckoo_settings_shell(const CuckooSettingsLayout& layout) {
  const GatewayUiStyle& style = layout_2_style();
  lv_obj_t* scr = lv_scr_act();
  g_state.settings_panel = create_dashboard_rect(
      scr, layout.screen.x, layout.screen.y, layout.screen.width,
      layout.screen.height, lv_color_hex(0x061015));
  lv_obj_clear_flag(g_state.settings_panel, LV_OBJ_FLAG_SCROLLABLE);
  create_dashboard_rect(g_state.settings_panel, layout.header.x,
                        layout.header.y, layout.header.width,
                        layout.header.height, lv_color_hex(0x0B1B24));
  g_state.settings_title = create_settings_centered_label(
      g_state.settings_panel, "CUCKOO SETTINGS", layout.title,
      lv_color_hex(0x45D9B2), layout_2_font(cuckoo_settings_compact_28() ? 12 : style.settings_title_font));
  lv_obj_t* device_label = create_settings_centered_label(
      g_state.settings_panel, g_state.cuckoo_settings_ui.deviceId(),
      layout.device, lv_color_hex(0xF4F7F7),
      layout_2_font(cuckoo_settings_compact_28() ? 12 : style.settings_device_font));
  if (cuckoo_settings_compact_28()) {
    lv_label_set_long_mode(device_label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_height(device_label, layout.device.height);
    lv_obj_center(device_label);
  }
  g_state.settings_error = create_settings_centered_label(
      g_state.settings_panel,
      g_state.cuckoo_settings_ui.saveError() ? "SAVE FAILED" : "",
      cuckoo_settings_compact_28() ? layout.title : GatewaySettingsRect{0, static_cast<int16_t>(layout.header.height - 14), layout.screen.width,
       12},
      lv_color_hex(0xFF5D62), layout_2_font(style.settings_error_font));
  create_layout_2_divider(g_state.settings_panel, 0,
                          layout.header.height - 1, layout.screen.width, 1);
}

static int cuckoo_settings_value_font() {
#if defined(GATEWAY_35)
  // The large-display 28 px font wraps even the default timeout. 20 px
  // fits the full supported seconds range in both 3.5 orientations.
  return 20;
#else
  return cuckoo_settings_compact_28() ? 12 : layout_2_style().settings_value_font;
#endif
}

static void create_cuckoo_settings_page() {
  const CuckooSettingsLayout layout = calculate_cuckoo_settings_layout();
  create_cuckoo_settings_shell(layout);
  lv_obj_t* offline = create_cuckoo_settings_button(
      g_state.settings_panel, "OFFLINE ALARM", layout.offline_toggle,
      CuckooSettingsAction::OfflineToggle);
  g_state.settings_value[0] = lv_obj_get_child(offline, 0);
  create_settings_centered_label(
      g_state.settings_panel, cuckoo_settings_compact_28() ? "TIMEOUT" : "TIMEOUT SEC", layout.offline_hint,
      lv_color_hex(0x7E8F97), layout_2_font(cuckoo_settings_compact_28() ? 12 : layout_2_style().settings_hint_font));
  create_cuckoo_settings_button(g_state.settings_panel, "-",
                                layout.offline_minus,
                                CuckooSettingsAction::OfflineMinus);
  g_state.settings_value[1] = create_settings_centered_label(
      g_state.settings_panel, "--", layout.offline_value,
      lv_color_hex(0xFFC247), layout_2_font(cuckoo_settings_value_font()));
  create_cuckoo_settings_button(g_state.settings_panel, "+",
                                layout.offline_plus,
                                CuckooSettingsAction::OfflinePlus);

  lv_obj_t* beer = create_cuckoo_settings_button(
      g_state.settings_panel, "BEER ALARM", layout.beer_toggle,
      CuckooSettingsAction::BeerToggle);
  g_state.settings_value[2] = lv_obj_get_child(beer, 0);
  create_settings_centered_label(
      g_state.settings_panel, cuckoo_settings_compact_28() ? "DEV. C" : "DEVIATION C", layout.beer_hint,
      lv_color_hex(0x7E8F97), layout_2_font(cuckoo_settings_compact_28() ? 12 : layout_2_style().settings_hint_font));
  create_cuckoo_settings_button(g_state.settings_panel, "-", layout.beer_minus,
                                CuckooSettingsAction::BeerThresholdMinus);
  g_state.settings_value[3] = create_settings_centered_label(
      g_state.settings_panel, "--", layout.beer_value,
      lv_color_hex(0xFFC247), layout_2_font(cuckoo_settings_value_font()));
  create_cuckoo_settings_button(g_state.settings_panel, "+", layout.beer_plus,
                                CuckooSettingsAction::BeerThresholdPlus);

  create_settings_centered_label(
      g_state.settings_panel, "HOLD SEC", layout.hold_hint,
      lv_color_hex(0x7E8F97), layout_2_font(cuckoo_settings_compact_28() ? 12 : layout_2_style().settings_hint_font));
  create_cuckoo_settings_button(g_state.settings_panel, "-", layout.hold_minus,
                                CuckooSettingsAction::BeerHoldMinus);
  g_state.settings_value[4] = create_settings_centered_label(
      g_state.settings_panel, "--", layout.hold_value,
      lv_color_hex(0xFFC247), layout_2_font(cuckoo_settings_value_font()));
  create_cuckoo_settings_button(g_state.settings_panel, "+", layout.hold_plus,
                                CuckooSettingsAction::BeerHoldPlus);
  create_cuckoo_settings_button(g_state.settings_panel, "BACK", layout.back,
                                CuckooSettingsAction::Back);
  create_cuckoo_settings_button(g_state.settings_panel, "SAVE", layout.save,
                                CuckooSettingsAction::Save, true);
}

static bool persist_cuckoo_settings(
    const CuckooPiDeviceConfig& settings, void* context) {
  (void)context;
  return myConfig.saveCuckooPiDeviceSettings(settings);
}

// Avoid invalidating unchanged labels; text comparison occurs under the UI lock.
static void cuckoo_set_text_if_changed(lv_obj_t* label, const char* text) {
  const char* current = lv_label_get_text(label);
  if (!current || strcmp(current, text) != 0) lv_label_set_text(label, text);
}

static void refresh_cuckoo_settings_page() {
  if (!g_state.cuckoo_settings_ui.isOpen()) return;
  const CuckooPiDeviceConfig& draft = g_state.cuckoo_settings_ui.draft();
  if (cuckoo_settings_compact_28() && g_state.settings_title) {
    if (g_state.cuckoo_settings_ui.saveError())
      lv_obj_add_flag(g_state.settings_title, LV_OBJ_FLAG_HIDDEN);
    else
      lv_obj_clear_flag(g_state.settings_title, LV_OBJ_FLAG_HIDDEN);
  }
  if (g_state.settings_error) {
    cuckoo_set_text_if_changed(g_state.settings_error,
                      g_state.cuckoo_settings_ui.saveError() ? "SAVE FAILED"
                                                             : "");
  }
  if (g_state.settings_value[0]) {
    cuckoo_set_text_if_changed(g_state.settings_value[0],
                      draft.offline_alarm_enabled ? "OFFLINE ALARM: ON"
                                                   : "OFFLINE ALARM: OFF");
  }
  if (g_state.settings_value[1]) {
    char value[24];
    snprintf(value, sizeof(value), "%lu SEC",
             static_cast<unsigned long>(draft.offline_timeout_seconds));
    cuckoo_set_text_if_changed(g_state.settings_value[1], value);
  }
  if (g_state.settings_value[2]) {
    cuckoo_set_text_if_changed(g_state.settings_value[2],
                      draft.beer_deviation_alarm_enabled ? "BEER ALARM: ON"
                                                         : "BEER ALARM: OFF");
  }
  if (g_state.settings_value[3]) {
    char value[24];
    snprintf(value, sizeof(value), "%.1f C",
             draft.beer_deviation_threshold_c);
    cuckoo_set_text_if_changed(g_state.settings_value[3], value);
  }
  if (g_state.settings_value[4]) {
    char value[24];
    snprintf(value, sizeof(value), "%lu SEC",
             static_cast<unsigned long>(draft.beer_deviation_hold_seconds));
    cuckoo_set_text_if_changed(g_state.settings_value[4], value);
  }
}

static void apply_cuckoo_settings_action(CuckooSettingsAction action) {
  freeze_diag::state.cuckooLastAction.store(static_cast<uint32_t>(action), std::memory_order_relaxed);
  freeze_diag::Scope diagnostic(freeze_diag::CuckooAction);
  switch (action) {
    case CuckooSettingsAction::OfflineToggle:
      g_state.cuckoo_settings_ui.toggleOfflineAlarm();
      break;
    case CuckooSettingsAction::OfflineMinus:
      g_state.cuckoo_settings_ui.adjustOfflineTimeout(-1);
      break;
    case CuckooSettingsAction::OfflinePlus:
      g_state.cuckoo_settings_ui.adjustOfflineTimeout(1);
      break;
    case CuckooSettingsAction::BeerToggle:
      g_state.cuckoo_settings_ui.toggleBeerDeviationAlarm();
      break;
    case CuckooSettingsAction::BeerThresholdMinus:
      g_state.cuckoo_settings_ui.adjustBeerDeviationThreshold(-1);
      break;
    case CuckooSettingsAction::BeerThresholdPlus:
      g_state.cuckoo_settings_ui.adjustBeerDeviationThreshold(1);
      break;
    case CuckooSettingsAction::BeerHoldMinus:
      g_state.cuckoo_settings_ui.adjustBeerDeviationHold(-1);
      break;
    case CuckooSettingsAction::BeerHoldPlus:
      g_state.cuckoo_settings_ui.adjustBeerDeviationHold(1);
      break;
    case CuckooSettingsAction::Back:
      g_state.cuckoo_settings_ui.back();
      close_settings_overlay();
      return;
    case CuckooSettingsAction::Save:
      if (g_state.cuckoo_settings_ui.save(persist_cuckoo_settings, nullptr)) {
        close_settings_overlay();
      } else {
        gravitymon_gateway_rebuild_cuckoo_settings_page();
      }
      return;
  }
  refresh_cuckoo_settings_page();
}

static void gravitymon_gateway_rebuild_cuckoo_settings_page() {
  freeze_diag::Scope diagnostic(freeze_diag::CuckooRebuild);
  if (!g_state.cuckoo_settings_ui.isOpen()) {
    close_settings_overlay();
    return;
  }
  if (g_state.settings_panel) lv_obj_del(g_state.settings_panel);
  reset_settings_widget_pointers();
  create_cuckoo_settings_page();
  refresh_cuckoo_settings_page();
}

/**
 * Initialize the Gravitymon Gateway UI subsystem.
 *
 * Performs common initialization and then creates the requested layout.
 * This is the entry point used by the application to bring up the UI.
 *
 * @param disp LVGL display pointer.
 * @param darkmode Select dark theme when true.
 * @param layout_id Initial layout index to use.
 */
void gravitymon_gateway_init(lv_disp_t* disp, bool darkmode,
                             uint8_t layout_id) {
  // Apply board-specific layout policy before any init path consumes the ID.
  layout_id = gatewayNormalizeLayoutId(layout_id);
  gatewayRotationReleaseLocks();
  close_settings_overlay();
  if (!disp) {
    Log.error(F("UI  : gravitymon_gateway_init: NULL display" CR));
    return;
  }

  if (g_disp != nullptr) gravitymon_gateway_cleanup_layout();
  g_state = gravitymon_state_t{};
  reset_gateway_widget_pointers();
  gravitymon_gateway_reset_data();

  // Common initialization (one-time setup)
  gravitymon_gateway_init_common(disp, darkmode, layout_id);
  layout_mgr.current_layout = layout_id;

  // Setup requested layout
  if (layout_id == 0) {
    gravitymon_gateway_setup_layout_0();
  } else if (layout_id == 1) {
    gravitymon_gateway_setup_layout_1();
  } else if (layout_id == 2) {
    gravitymon_gateway_setup_layout_2();
  }

  const char* orientation_name =
      g_state.orientation == GatewayOrientation::Portrait ? "portrait"
                                                          : "landscape";
  Log.info(F("UI  : Gravitymon Gateway UI initialized (%dx%d %s, %s mode) - "
             "layout %d active" CR),
           lv_display_get_horizontal_resolution(disp),
           lv_display_get_vertical_resolution(disp), orientation_name,
           darkmode ? "dark" : "light", layout_id);
}

/**
 * Update the cached device name shown in the UI.
 *
 * The new value is copied into `g_state.data_name` and will be applied
 * to visible labels on the next UI loop iteration.
 *
 * @param name Null-terminated string with the device name.
 */
void gravitymon_gateway_set_name(const char* name) {
  if (!name) return;
  snprintf(g_state.data_name, sizeof(g_state.data_name), " %s", name);
}

void gravitymon_gateway_set_measurement_online(const char* device_id, bool online) {
#if defined(ARDUINO_ARCH_ESP32)
  portENTER_CRITICAL(&g_cuckoo_frame_mux);
#endif
  snprintf(g_state.measurement_online_id, sizeof(g_state.measurement_online_id),
           "%s", device_id ? device_id : "");
  g_state.measurement_online = online;
#if defined(ARDUINO_ARCH_ESP32)
  portEXIT_CRITICAL(&g_cuckoo_frame_mux);
#endif
}

void gravitymon_gateway_set_device_id(const char* device_id) {
  g_state.display_source = GatewayDisplayItemKind::Measurement;
  if (gateway_settings_copy_display_device_id(device_id,
                                              g_state.data_device_id))
    return;
  g_settings_thread_state.requestClose();
}

/**
 * Set the device index text (e.g. "1/3").
 *
 * The value is formatted into `g_state.data_index` and will be applied
 * during the next UI update.
 *
 * @param current Current device number.
 * @param total Total number of devices.
 */
void gravitymon_gateway_set_index(uint8_t current, uint8_t total) {
  snprintf(g_state.data_index, sizeof(g_state.data_index), "%d/%d", current,
           total);
}

/**
 * Configure the measurement gravity min and max range.
 *
 * Updates both the stored min/max values and the gauge range if the
 * gauge object is present.
 *
 * @param min Minimum value.
 * @param max Maximum value.
 * @param unit 'G' for SG gravity formatting, otherwise pressure formatting.
 */
void gravitymon_gateway_set_gravity_range(float min, float max, char unit) {
  /* layout 1 gauge removed; only update stored numeric range and strings */

  if (unit == 'G') {
    g_state.measurement_min = min * 1000.0f;
    g_state.measurement_max = max * 1000.0f;
    snprintf(g_state.data_measurement_min, sizeof(g_state.data_measurement_min),
             "%.2f", min);
    snprintf(g_state.data_measurement_max, sizeof(g_state.data_measurement_max),
             "%.2f", max);
  } else {
    g_state.measurement_min = min * 10.0f;
    g_state.measurement_max = max * 10.0f;
    snprintf(g_state.data_measurement_min, sizeof(g_state.data_measurement_min),
             "%.1f", min);
    snprintf(g_state.data_measurement_max, sizeof(g_state.data_measurement_max),
             "%.1f", max);
  }
}

/**
 * Update the current measurement gravity string.
 *
 * Formats the numeric value into `g_state.data_measurement` according to
 * the provided unit and updates the internal numeric representation used
 * by the gauge.
 *
 * @param gravity Numeric measurement value.
 * @param unit 'G' for SG gravity formatting, otherwise pressure.
 */
void gravitymon_gateway_set_gravity(float gravity, char unit) {
  if (isnan(gravity)) {
    snprintf(g_state.data_measurement, sizeof(g_state.data_measurement), "--");
    snprintf(g_state.data_measurement_precise,
             sizeof(g_state.data_measurement_precise), "--");
    g_state.data_measurement_unit[0] = '\0';
    g_state.measurement = 0;
    g_state.gravity_sg = NAN;
  } else if (unit == 'G') {
    snprintf(g_state.data_measurement, sizeof(g_state.data_measurement),
             "%.2f SG", gravity);
    snprintf(g_state.data_measurement_precise,
             sizeof(g_state.data_measurement_precise), "%.3f", gravity);
    snprintf(g_state.data_measurement_unit,
             sizeof(g_state.data_measurement_unit), "SG");
    g_state.measurement = gravity * 100.0f;
    g_state.gravity_sg = gravity;
  } else if (unit == 'P') {
    snprintf(g_state.data_measurement, sizeof(g_state.data_measurement),
#if defined(GMT154_06)
             "%.2f P", gravity);
#else
             "%.3f P", gravity);
#endif
    snprintf(g_state.data_measurement_precise,
             sizeof(g_state.data_measurement_precise),
#if defined(GMT154_06)
             "%.2f", gravity);
#else
             "%.3f", gravity);
#endif
    snprintf(g_state.data_measurement_unit,
             sizeof(g_state.data_measurement_unit), "P");
    g_state.measurement = gravity * 10.0f;
    g_state.gravity_sg = gravityPlatoToSg(gravity);
  } else {
    snprintf(g_state.data_measurement, sizeof(g_state.data_measurement), "");
    snprintf(g_state.data_measurement_precise,
             sizeof(g_state.data_measurement_precise), "--");
    g_state.data_measurement_unit[0] = '\0';
    g_state.measurement = 0;
    g_state.gravity_sg = NAN;
  }
}

/**
 * Configure the measurement pressure min and max range.
 *
 * Updates both the stored min/max values and the gauge range if the
 * gauge object is present.
 *
 * @param min Minimum value.
 * @param max Maximum value.
 * @param unit 'P' for PSI, 'B' for Bar amd 'k' for KPa
 */
void gravitymon_gateway_set_pressure_range(float min, float max,
                                           const char* unit) {
  /* layout 1 gauge removed; only update stored numeric range and strings */

  if (strcmp(unit, "PSI") == 0) {
    g_state.measurement_min = min * 100.0f;
    g_state.measurement_max = max * 100.0f;
    snprintf(g_state.data_measurement_min, sizeof(g_state.data_measurement_min),
             "%.2f", min);
    snprintf(g_state.data_measurement_max, sizeof(g_state.data_measurement_max),
             "%.2f", max);
  } else if (strcmp(unit, "Bar") == 0) {
    g_state.measurement_min = min * 10.0f;
    g_state.measurement_max = max * 10.0f;
    snprintf(g_state.data_measurement_min, sizeof(g_state.data_measurement_min),
             "%.1f", min);
    snprintf(g_state.data_measurement_max, sizeof(g_state.data_measurement_max),
             "%.1f", max);
  } else if (strcmp(unit, "kPa") == 0) {
    g_state.measurement_min = min;
    g_state.measurement_max = max;
    snprintf(g_state.data_measurement_min, sizeof(g_state.data_measurement_min),
             "%.0f", min);
    snprintf(g_state.data_measurement_max, sizeof(g_state.data_measurement_max),
             "%.0f", max);
  } else {
    g_state.measurement_min = min;
    g_state.measurement_max = max;
    snprintf(g_state.data_measurement_min, sizeof(g_state.data_measurement_min),
             "");
    snprintf(g_state.data_measurement_max, sizeof(g_state.data_measurement_max),
             "");
  }
}

/**
 * Update the current measurement pressure string.
 *
 * Formats the numeric value into `g_state.data_measurement` according to
 * the provided unit and updates the internal numeric representation used
 * by the gauge.
 *
 * @param pressure Numeric measurement value.
 * @param unit 'G' for SG gravity formatting, otherwise pressure.
 */
void gravitymon_gateway_set_pressure(float pressure, float pressure2,
                                     const char* unit) {
  snprintf(g_state.data_measurement_precise,
           sizeof(g_state.data_measurement_precise), "--");
  g_state.data_measurement_unit[0] = '\0';

  if (isnan(pressure) && isnan(pressure2)) {
    snprintf(g_state.data_measurement, sizeof(g_state.data_measurement), "--");
    g_state.measurement = 0;
  } else if (isnan(pressure2)) {
    if (strcmp(unit, "PSI") == 0) {
      g_state.measurement = pressure * 100.0f;
      snprintf(g_state.data_measurement, sizeof(g_state.data_measurement),
               "%.2f PSI", pressure);
      snprintf(g_state.data_measurement_precise,
               sizeof(g_state.data_measurement_precise), "%.2f", pressure);
      snprintf(g_state.data_measurement_unit,
               sizeof(g_state.data_measurement_unit), "PSI");
    } else if (strcmp(unit, "Bar") == 0) {
      g_state.measurement = pressure * 10.0f;
      snprintf(g_state.data_measurement, sizeof(g_state.data_measurement),
               "%.1f Bar", pressure);
      snprintf(g_state.data_measurement_precise,
               sizeof(g_state.data_measurement_precise), "%.1f", pressure);
      snprintf(g_state.data_measurement_unit,
               sizeof(g_state.data_measurement_unit), "Bar");
    } else if (strcmp(unit, "kPa") == 0) {
      g_state.measurement = pressure;
      snprintf(g_state.data_measurement, sizeof(g_state.data_measurement),
               "%.0f kPa", pressure);
      snprintf(g_state.data_measurement_precise,
               sizeof(g_state.data_measurement_precise), "%.0f", pressure);
      snprintf(g_state.data_measurement_unit,
               sizeof(g_state.data_measurement_unit), "kPa");
    }
  } else {
    if (strcmp(unit, "PSI") == 0) {
      g_state.measurement = pressure * 100.0f;
      snprintf(g_state.data_measurement, sizeof(g_state.data_measurement),
               "%.2f / %.2f PSI", pressure, pressure2);
    } else if (strcmp(unit, "Bar") == 0) {
      g_state.measurement = pressure * 10.0f;
      snprintf(g_state.data_measurement, sizeof(g_state.data_measurement),
               "%.1f / %.1f Bar", pressure, pressure2);
    } else if (strcmp(unit, "kPa") == 0) {
      g_state.measurement = pressure;
      snprintf(g_state.data_measurement, sizeof(g_state.data_measurement),
               "%.0f / %.0f kPa", pressure, pressure2);
    }

    if (strcmp(unit, "PSI") == 0) {
      snprintf(g_state.data_measurement_precise,
               sizeof(g_state.data_measurement_precise), "%.2f", pressure);
      snprintf(g_state.data_measurement_unit,
               sizeof(g_state.data_measurement_unit), "PSI");
    } else if (strcmp(unit, "Bar") == 0) {
      snprintf(g_state.data_measurement_precise,
               sizeof(g_state.data_measurement_precise), "%.1f", pressure);
      snprintf(g_state.data_measurement_unit,
               sizeof(g_state.data_measurement_unit), "Bar");
    } else if (strcmp(unit, "kPa") == 0) {
      snprintf(g_state.data_measurement_precise,
               sizeof(g_state.data_measurement_precise), "%.0f", pressure);
      snprintf(g_state.data_measurement_unit,
               sizeof(g_state.data_measurement_unit), "kPa");
    }
  }
}

/**
 * Update the cached battery percentage string and numeric value.
 *
 * @param percent Battery percentage (0..100).
 */
void gravitymon_gateway_set_battery_percentage(float percent) {
  if (isnan(percent)) {
    snprintf(g_state.data_battery_percentage,
             sizeof(g_state.data_battery_percentage), "--");
    g_state.battery_percentage = (uint8_t)255;
    g_state.battery_percent = NAN;
  } else {
    snprintf(g_state.data_battery_percentage,
             sizeof(g_state.data_battery_percentage), "%.0f%%", percent);
    g_state.battery_percentage = (uint8_t)percent;
    g_state.battery_percent = percent;
  }
}

/**
 * Update the cached battery voltage string (e.g. "3.70V").
 *
 * @param voltage Battery voltage in volts.
 */
void gravitymon_gateway_set_battery_voltage(float voltage) {
  if (isnan(voltage)) {
    snprintf(g_state.data_battery_voltage, sizeof(g_state.data_battery_voltage),
             "--");
  } else {
    snprintf(g_state.data_battery_voltage, sizeof(g_state.data_battery_voltage),
             "%.2fV", voltage);
  }
}

/**
 * Update the connection/device type string.
 *
 * Stores the provided `type` into `g_state.data_type` so it will be
 * reflected on-screen during the next UI loop iteration.
 *
 * @param type Null-terminated type string (e.g. "WiFi", "BLE").
 */
void gravitymon_gateway_set_type(const char* type) {
  snprintf(g_state.data_type, sizeof(g_state.data_type), "%s", type);
}

/**
 * Update the device/source identifier string.
 *
 * Stores the provided `source` into `g_state.data_source` so it will be
 * reflected on-screen during the next UI loop iteration.
 *
 * @param source Null-terminated source string (e.g. MAC address, location).
 */
void gravitymon_gateway_set_source(const char* source) {
  snprintf(g_state.data_source, sizeof(g_state.data_source), "%s", source);
}

/**
 * Set the temperature display string.
 *
 * Formats temperature according to the unit and stores it in the
 * `g_state.data_temp` buffer. If `temp` is NaN the display shows "--".
 *
 * @param temp Temperature value.
 * @param temp2 Secondary temperature value
 * @param unit 'C' for Celsius, otherwise Fahrenheit.
 */
void gravitymon_gateway_set_temp(float temp, float temp2, char unit) {
  if (isnan(temp) && isnan(temp2)) {
    snprintf(g_state.data_temp, sizeof(g_state.data_temp), "--");
    snprintf(g_state.data_temp_compact, sizeof(g_state.data_temp_compact),
             "--");
    g_state.temp_c = NAN;
  } else if (isnan(temp2)) {
    g_state.temp_c = unit == 'F' ? fahrenheitToCelsius(temp) : temp;
    if (unit == 'C') {
      snprintf(g_state.data_temp, sizeof(g_state.data_temp), "%.2f°C", temp);
      snprintf(g_state.data_temp_compact,
               sizeof(g_state.data_temp_compact), "%.1f°C", temp);
    } else {
      snprintf(g_state.data_temp, sizeof(g_state.data_temp), "%.1f°F", temp);
      snprintf(g_state.data_temp_compact,
               sizeof(g_state.data_temp_compact), "%.1f°F", temp);
    }
  } else {
    g_state.temp_c = unit == 'F' ? fahrenheitToCelsius(temp) : temp;
    if (unit == 'C') {
      snprintf(g_state.data_temp, sizeof(g_state.data_temp), "%.2f / %.2f°C",
               temp, temp2);
      snprintf(g_state.data_temp_compact,
               sizeof(g_state.data_temp_compact), "%.1f/%.1f°C", temp,
               temp2);
    } else {
      snprintf(g_state.data_temp, sizeof(g_state.data_temp), "%.1f / %.1f°F",
               temp, temp2);
      snprintf(g_state.data_temp_compact,
               sizeof(g_state.data_temp_compact), "%.1f/%.1f°F", temp,
               temp2);
    }
  }
}

/**
 * Update the RSSI text shown in layout 2.
 *
 * @param rssi_dbm RSSI in dBm.
 */
void gravitymon_gateway_set_rssi(int8_t rssi_dbm) {
  if (rssi_dbm == 0) {
    snprintf(g_state.data_rssi, sizeof(g_state.data_rssi), "--");
  } else {
    snprintf(g_state.data_rssi, sizeof(g_state.data_rssi), "%d dBm", rssi_dbm);
  }
}

void gravitymon_gateway_set_network(const char* ip, int wifi_rssi) {
  if (!ip || ip[0] == '\0' || strcmp(ip, "0.0.0.0") == 0) {
    snprintf(g_state.data_gateway_ip, sizeof(g_state.data_gateway_ip),
             "IP --");
  } else {
    snprintf(g_state.data_gateway_ip, sizeof(g_state.data_gateway_ip),
             "IP %s", ip);
  }

  if (wifi_rssi == 0) {
    snprintf(g_state.data_wifi_rssi, sizeof(g_state.data_wifi_rssi),
             "RSSI --");
  } else {
    snprintf(g_state.data_wifi_rssi, sizeof(g_state.data_wifi_rssi),
             "RSSI %d", wifi_rssi);
  }
}

void gravitymon_gateway_set_cuckoo_pi(const CuckooPiSnapshot& snapshot,
                                      bool online) {
#if defined(ARDUINO_ARCH_ESP32)
  portENTER_CRITICAL(&g_cuckoo_frame_mux);
#endif
  g_state.cuckoo_snapshot = snapshot;
  g_state.cuckoo_online = online;
  g_state.display_source = GatewayDisplayItemKind::CuckooPi;
  if (normalizeCuckooPiId(snapshot.device_id, g_state.data_device_id)) {
    snprintf(g_state.data_name, sizeof(g_state.data_name), "Cuckoo Pi");
  } else {
    g_state.data_device_id[0] = '\0';
  }
#if defined(ARDUINO_ARCH_ESP32)
  portEXIT_CRITICAL(&g_cuckoo_frame_mux);
#endif
}

void gravitymon_gateway_set_progress(float progress) {
  if (isnan(progress) || progress < 0.0f || progress > 100.0f) {
    g_state.progress = -1;
    snprintf(g_state.data_progress, sizeof(g_state.data_progress), "--");
    return;
  }

  g_state.progress = (int16_t)lroundf(progress);
  snprintf(g_state.data_progress, sizeof(g_state.data_progress), "%d%%",
           g_state.progress);
}

static void reset_all_gateway_indev(void* context) {
  (void)context;
  lv_indev_reset(nullptr, nullptr);
}

static bool cleanup_gateway_for_rotation(void* context) {
  (void)context;
  return gravitymon_gateway_cleanup_layout(true);
}

static void rebuild_gateway_after_rotation(void* context) {
  (void)context;
  gravitymon_gateway_setup_layout_2();
}

bool gravitymon_gateway_set_orientation(GatewayOrientation orientation) {
  if (g_state.orientation == orientation) return true;
  if (layout_mgr.current_layout != 2) {
    g_state.orientation = orientation;
    return true;
  }
  if (gravitymon_gateway_is_settings_open()) {
    gatewayRotationGuard().requestOrientation(orientation);
    return false;
  }
  GatewayAlertUiTeardownReservation reservation;
  if (!reservation.owns()) {
    gatewayRotationGuard().requestOrientation(orientation);
    return false;
  }
  const GatewayOrientation old_orientation = g_state.orientation;
  g_state.orientation = orientation;

  // Rebuild without reloading configuration. Rotation state is value-only;
  // teardown callbacks do not retain a stack address between calls.
  const GatewayRotationTeardown teardown = {
      reset_all_gateway_indev, cleanup_gateway_for_rotation,
      rebuild_gateway_after_rotation, nullptr};
  if (!runGatewayRotationTeardown(teardown)) {
    g_state.orientation = old_orientation;
    gatewayRotationGuard().requestOrientation(orientation);
    return false;
  }
  return true;
}

/**
 * Set device timestamp and compute elapsed time string.
 *
 * Parses an incoming timestamp of the form "YYYY-MM-DD HH:MM:SS" and
 * stores a human-friendly "time ago" string into `g_state.data_time_ago`.
 * If parsing fails the field is set to "--".
 *
 * @param time_str Null-terminated timestamp string.
 */
void gravitymon_gateway_set_time(const char* time_str) {
  if (!time_str) return;

  // Store raw timestamp
  snprintf(g_state.data_time, sizeof(g_state.data_time), "%s", time_str);

  // Parse incoming time string: "YYYY-MM-DD HH:MM:SS"
  struct tm parsed_time = {};
  if (sscanf(time_str, "%d-%d-%d %d:%d:%d", &parsed_time.tm_year,
             &parsed_time.tm_mon, &parsed_time.tm_mday, &parsed_time.tm_hour,
             &parsed_time.tm_min, &parsed_time.tm_sec) != 6) {
    snprintf(g_state.data_time_ago, sizeof(g_state.data_time_ago), "--");
    return;
  }

  // Adjust parsed values to struct tm format (year since 1900, month 0-11)
  parsed_time.tm_year -= 1900;
  parsed_time.tm_mon -= 1;
  parsed_time.tm_isdst = -1;  // Let mktime determine DST

  // Convert parsed time to time_t
  time_t parsed_timestamp = mktime(&parsed_time);
  if (parsed_timestamp == -1) {
    snprintf(g_state.data_time_ago, sizeof(g_state.data_time_ago), "--");
    return;
  }

  // Get current time
  time_t now = std::time(NULL);

  // Calculate elapsed time in seconds
  int64_t elapsed_seconds = difftime(now, parsed_timestamp);
  if (elapsed_seconds < 0) {
    snprintf(g_state.data_time_ago, sizeof(g_state.data_time_ago), "--");
    return;
  }

  // Format as "Xh Xm Xs" or shorter variants
  int64_t hours = elapsed_seconds / 3600;
  int64_t minutes = (elapsed_seconds % 3600) / 60;
  int64_t seconds = elapsed_seconds % 60;

  if (hours > 0) {
    snprintf(g_state.data_time_ago, sizeof(g_state.data_time_ago),
             "%ldh %ldm %lds", hours, minutes, seconds);
  } else if (minutes > 0) {
    snprintf(g_state.data_time_ago, sizeof(g_state.data_time_ago), "%ldm %lds",
             minutes, seconds);
  } else {
    snprintf(g_state.data_time_ago, sizeof(g_state.data_time_ago), "%lds",
             seconds);
  }
}

/**
 * Store a historical measurement value for the given index (0..4).
 *
 * The string is formatted and placed into the `g_state.data_history`
 * buffer for later display.
 *
 * @param index History index (0-4).
 * @param value Numeric value to store.
 */
void gravitymon_gateway_set_history(uint8_t index, const char* value) {
  if (index >= 5 || !value) return;
  snprintf(g_state.data_history[index], sizeof(g_state.data_history[index]),
           "%s", value);
}

/**
 * Set the status bar message text.
 *
 * @param status Null-terminated status message string.
 */
void gravitymon_gateway_set_status(const char* status) {
  if (!status) return;
  snprintf(g_state.data_status, sizeof(g_state.data_status), "%s", status);
}

/**
 * Toggle the internal dark/light theme flag used when drawing UI elements.
 *
 * The actual UI update to apply the theme occurs during the next loop
 * (or when layouts are rebuilt).
 *
 * @param darkmode True to enable dark theme, false for light.
 */
void gravitymon_gateway_set_theme(bool darkmode) {
  g_state.flg_darkmode = darkmode;
}

/**
 * Layout 0 loop handler: push cached strings into visible labels.
 *
 * This function is intended to be called from the LVGL main/background
 * thread to update on-screen content from the `g_state` buffers.
 */
static void gravitymon_gateway_loop_layout_0(void) {
  // Update all text labels
  if (g_state.lbl_name) {
    lv_label_set_text(g_state.lbl_name, g_state.data_name);
    gravitymon_gateway_refresh_offline_badge();
  }
  if (g_state.lbl_index) {
    lv_label_set_text(g_state.lbl_index, g_state.data_index);
  }
  if (g_state.lbl_temp) {
    lv_label_set_text(g_state.lbl_temp, g_state.data_temp);
  }
  if (g_state.lbl_measurement) {
    lv_label_set_text(g_state.lbl_measurement, g_state.data_measurement);
  }
  if (g_state.lbl_battery) {
    lv_label_set_text(g_state.lbl_battery,
                      strcmp(g_state.data_battery_voltage, "--")
                          ? g_state.data_battery_voltage
                          : g_state.data_battery_percentage);
  }
  if (g_state.lbl_time) {
    lv_label_set_text(g_state.lbl_time, g_state.data_time);
  }
  if (g_state.lbl_status) {
    lv_label_set_text(g_state.lbl_status, g_state.data_status);
  }

  // Update history items
  for (int i = 0; i < 5; i++) {
    if (g_state.lbl_history[i]) {
      lv_label_set_text(g_state.lbl_history[i], g_state.data_history[i]);
    }
  }
}

/**
 * Layout 1 loop handler
 */
static void gravitymon_gateway_loop_layout_1(void) {
  if (g_state.lbl_name) {
    lv_label_set_text(g_state.lbl_name, g_state.data_name);
    gravitymon_gateway_refresh_offline_badge();
  }
  if (g_state.lbl_index) {
    lv_label_set_text(g_state.lbl_index, g_state.data_index);
  }
  if (g_state.wid_battery_canvas) {
    lv_obj_del(g_state.wid_battery_canvas);
  }
  // Create battery canbas
  g_state.wid_battery_canvas = create_battery_indicator(
      lv_scr_act(), 260, 5, g_state.battery_percentage);

  if (g_state.lbl_measurement) {
    lv_label_set_text(g_state.lbl_measurement, g_state.data_measurement);
  }
  if (g_state.lbl_temp) {
    lv_label_set_text(g_state.lbl_temp, g_state.data_temp);
  }
  if (g_state.lbl_time) {
    lv_label_set_text(g_state.lbl_time, g_state.data_time_ago);
  }
  if (g_state.lbl_rssi) {
    lv_label_set_text(g_state.lbl_rssi, g_state.data_rssi);
  }
  if (g_state.lbl_type) {
    lv_label_set_text(g_state.lbl_type, g_state.data_type);
  }
  if (g_state.lbl_source) {
    lv_label_set_text(g_state.lbl_source, g_state.data_source);
  }
  if (g_state.lbl_status) {
    lv_label_set_text(g_state.lbl_status, g_state.data_status);
  }
}

static const char* gravitymon_gateway_compact_source(void) {
  if (strstr(g_state.data_source, "BLE") != NULL) {
    return "BLE";
  }
  if (strstr(g_state.data_source, "HTTP") != NULL ||
      strstr(g_state.data_type, "HTTP") != NULL ||
      strstr(g_state.data_type, "WiFi") != NULL) {
    return "HTTP";
  }
  return "--";
}

/** Layout 2 loop handler: fixed dark gateway dashboard. */
static void gravitymon_gateway_loop_layout_2(void) {
  float attenuation = NAN;
  float abv = NAN;
  bool battery_alert = false;
  char rssi_value[20];
  GravityDeviceSettings settings{};
  if (myConfig.copyDeviceSettings(g_state.data_device_id, &settings)) {
    if (calculateFermentation(settings.og_sg, g_state.gravity_sg,
                              &attenuation, &abv)) {
      gravitymon_gateway_set_progress(attenuation);
    } else {
      gravitymon_gateway_set_progress(NAN);
    }
    battery_alert = isBatteryPercentageLow(g_state.battery_percent,
                                           settings.low_battery_percent);
  } else {
    gravitymon_gateway_set_progress(NAN);
  }
  g_state.battery_alert_active = battery_alert;
#if defined(GMT154_06)
  bool measurement_online = true;
  char measurement_online_id[GRAVITY_DEVICE_ID_CAPACITY] = {};
#if defined(ARDUINO_ARCH_ESP32)
  portENTER_CRITICAL(&g_cuckoo_frame_mux);
#endif
  measurement_online = g_state.measurement_online;
  snprintf(measurement_online_id, sizeof(measurement_online_id), "%s",
           g_state.measurement_online_id);
#if defined(ARDUINO_ARCH_ESP32)
  portEXIT_CRITICAL(&g_cuckoo_frame_mux);
#endif
  show_gmt154_offline_frame(g_state.gravity_offline_frame,
      gmt154MatchingMeasurementOffline(g_state.data_device_id,
                                        measurement_online_id, measurement_online));
#endif

  if (g_state.lbl_name) {
    lv_label_set_text(g_state.lbl_name, layout_2_display_name());
    gravitymon_gateway_refresh_offline_badge();
  }
  if (g_state.lbl_source) {
    lv_label_set_text(g_state.lbl_source,
                      gravitymon_gateway_compact_source());
  }
  if (g_state.lbl_index) {
    lv_label_set_text(g_state.lbl_index,
                      g_state.data_index[0] ? g_state.data_index : "--");
  }
  if (g_state.lbl_progress) {
    lv_label_set_text(g_state.lbl_progress, g_state.data_progress);
  }
  if (g_state.lbl_abv) {
    char abv_text[16];
    if (isfinite(abv)) {
      snprintf(abv_text, sizeof(abv_text), "%.1f%%", abv);
    } else {
      snprintf(abv_text, sizeof(abv_text), "--");
    }
    lv_label_set_text(g_state.lbl_abv, abv_text);
  }
  if (g_state.wid_dashboard_arc) {
    lv_arc_set_value(g_state.wid_dashboard_arc,
                     g_state.progress >= 0 ? g_state.progress : 0);
    lv_obj_set_style_arc_color(g_state.wid_dashboard_arc, layout_2_ring_color(),
                               LV_PART_INDICATOR);
  }
#if defined(ARDUINO_WAVESHARE_S3_TFT) || defined(GATEWAY_35)
  const bool show_plato_unit = strcmp(g_state.data_measurement_unit, "P") == 0;
#endif
  if (g_state.lbl_measurement) {
#if defined(ARDUINO_WAVESHARE_S3_TFT) || defined(GATEWAY_35)
    if (show_plato_unit && g_state.lbl_measurement_unit) {
      lv_obj_t* region = lv_obj_get_parent(g_state.lbl_measurement);
      const GatewaySettingsRect bounds{0, 0,
          static_cast<int16_t>(lv_obj_get_width(region)),
          static_cast<int16_t>(lv_obj_get_height(region))};
      // The number was created with lv_obj_center(), and the unit was aligned
      // on the right. Clear those alignment styles before applying positions
      // calculated in the shared parent coordinate space.
      lv_obj_set_style_align(g_state.lbl_measurement, LV_ALIGN_TOP_LEFT, LV_PART_MAIN);
      lv_obj_set_style_align(g_state.lbl_measurement_unit, LV_ALIGN_TOP_LEFT, LV_PART_MAIN);
      set_cuckoo_temperature_group(g_state.lbl_measurement,
          g_state.lbl_measurement_unit, bounds,
          g_state.data_measurement_precise, "P", true,
#if defined(GATEWAY_35)
          72);
#else
          40);
#endif
    } else {
      lv_obj_set_style_text_font(g_state.lbl_measurement,
          layout_2_font(layout_2_style().dashboard_measurement_font), LV_PART_MAIN);
      lv_obj_set_width(g_state.lbl_measurement, LV_SIZE_CONTENT);
      lv_obj_set_height(g_state.lbl_measurement, LV_SIZE_CONTENT);
      lv_label_set_long_mode(g_state.lbl_measurement, LV_LABEL_LONG_CLIP);
      lv_obj_center(g_state.lbl_measurement);
    }
#endif
    lv_label_set_text(g_state.lbl_measurement,
                      g_state.data_measurement_precise);
  }
  if (g_state.lbl_measurement_unit) {
#if defined(ARDUINO_WAVESHARE_S3_TFT) || defined(GATEWAY_35)
    lv_label_set_text(g_state.lbl_measurement_unit, show_plato_unit ? "P" : "");
    if (!show_plato_unit) lv_obj_add_flag(g_state.lbl_measurement_unit, LV_OBJ_FLAG_HIDDEN);
#else
    lv_label_set_text(g_state.lbl_measurement_unit,
                      g_state.data_measurement_unit);
#endif
  }
  if (g_state.lbl_temp) {
    lv_label_set_text(g_state.lbl_temp, g_state.data_temp_compact);
    lv_obj_set_style_text_color(g_state.lbl_temp, lv_color_hex(0xF4F7F7),
                                LV_PART_MAIN);
  }
#if defined(GMT154_06)
  snprintf(rssi_value, sizeof(rssi_value), "%s", g_state.data_rssi);
#else
  layout_2_copy_rssi_value(rssi_value, sizeof(rssi_value), g_state.data_rssi);
#endif
  if (g_state.lbl_rssi) {
    lv_label_set_text(g_state.lbl_rssi, rssi_value);
  }
  if (g_state.lbl_gateway_ip) {
    lv_label_set_text(g_state.lbl_gateway_ip, g_state.data_gateway_ip);
  }
  if (g_state.lbl_wifi_rssi) {
    lv_label_set_text(g_state.lbl_wifi_rssi, g_state.data_wifi_rssi);
  }
  if (layout_mgr.current_layout == 2) {
    layout_2_refresh_header_battery(lv_scr_act());
  }
}

static void gravitymon_gateway_loop_cuckoo(void) {
  CuckooPiSnapshot snapshot{};
  bool cuckoo_online = false;
  char device_id[GRAVITY_DEVICE_ID_CAPACITY] = {};
#if defined(ARDUINO_ARCH_ESP32)
  portENTER_CRITICAL(&g_cuckoo_frame_mux);
#endif
  snapshot = g_state.cuckoo_snapshot;
  cuckoo_online = g_state.cuckoo_online;
  snprintf(device_id, sizeof(device_id), "%s", g_state.data_device_id);
#if defined(ARDUINO_ARCH_ESP32)
  portEXIT_CRITICAL(&g_cuckoo_frame_mux);
#endif
#if defined(GMT154_06)
  show_gmt154_offline_frame(g_state.cuckoo_offline_frame,
      gmt154ShowOfflineFrame(device_id[0] != '\0', cuckoo_online));
#endif
  char value[64];
  const bool portrait = layout_2_screen_height() > layout_2_screen_width();
  const CuckooPiScreenLayout layout = calculateCuckooPiScreenLayout(
      static_cast<int16_t>(layout_2_screen_width()),
      static_cast<int16_t>(layout_2_screen_height()),
      snapshot.has_mode ? snapshot.mode_raw : 2);
#if !defined(GMT154_06)
  apply_cuckoo_mode_layout(layout, snapshot.has_mode ? snapshot.mode_raw : 2);
#endif
#if defined(GMT154_06)
  const lv_color_t online_color = cuckoo_online
                                      ? lv_color_hex(0x35D9B2)
                                      : lv_color_hex(0xFF5D62);
#elif defined(ARDUINO_WAVESHARE_S3_TFT)
  const lv_color_t online_color = cuckoo_online
                                      ? lv_color_hex(0xA8F0BE)
                                      : lv_color_hex(0xFFD7D7);
#else
  const lv_color_t online_color = cuckoo_online
                                      ? lv_color_hex(0x45D9B2)
                                      : lv_color_hex(0xFF5D62);
#endif

#if defined(GMT154_06)
  const bool offline_alarm_active = gatewayAlarmTracker.isConditionActive(
      GatewayAlarmType::CuckooOffline, device_id);
  const bool beer_deviation_alarm_active = gatewayAlarmTracker.isConditionActive(
      GatewayAlarmType::CuckooBeerDeviation, device_id);
  const bool device_alarm_active =
      offline_alarm_active || beer_deviation_alarm_active;
  const lv_color_t device_id_color =
      device_alarm_active ? lv_color_hex(0xFF5D62) : lv_color_hex(0xFFFFFF);
#else
  const lv_color_t device_id_color = lv_color_hex(0xFFFFFF);
#endif

  if (g_state.cuckoo_title) {
    char title[32];
    snprintf(title, sizeof(title), "%s", device_id[0] ? device_id : "--");
    lv_label_set_text(g_state.cuckoo_title, title);
    lv_obj_set_style_text_color(g_state.cuckoo_title, device_id_color,
                                LV_PART_MAIN);
  }
#if !defined(GMT154_06)
  if (g_state.cuckoo_index) {
    lv_label_set_text(g_state.cuckoo_index,
                      g_state.data_index[0] ? g_state.data_index : "--");
    lv_obj_set_style_text_color(g_state.cuckoo_index,
#if defined(ARDUINO_WAVESHARE_S3_TFT) || defined(GATEWAY_35)
                                lv_color_hex(0xDCEFE2), LV_PART_MAIN);
#else
                                lv_color_hex(0xFFFFFF), LV_PART_MAIN);
#endif
  }
#endif
  if (g_state.cuckoo_value[0]) {
    lv_label_set_text(g_state.cuckoo_value[0],
                      cuckoo_online ? "ONLINE" : "OFFLINE");
    lv_obj_set_style_text_color(g_state.cuckoo_value[0], online_color,
                                LV_PART_MAIN);
  }

  snprintf(value, sizeof(value), "%s",
           snapshot.has_mode ? cuckooPiModeLabel(snapshot.mode_raw) : "--");
#if defined(GMT154_06)
  if (g_state.cuckoo_label[1] && g_state.cuckoo_value[1]) {
    set_cuckoo_colored_pair(g_state.cuckoo_label[1], g_state.cuckoo_value[1],
                            layout.mode, "MODE", value, 12, 16, true);
    lv_obj_set_style_text_color(g_state.cuckoo_label[1],
                                lv_color_hex(0x91ABB2), LV_PART_MAIN);
    lv_obj_set_style_text_color(g_state.cuckoo_value[1],
                                lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  }
#else
  if (g_state.cuckoo_label[1] && g_state.cuckoo_value[1]) {
    set_cuckoo_colored_pair(
        g_state.cuckoo_label[1], g_state.cuckoo_value[1], layout.mode,
        "MODE", value,
#if defined(ARDUINO_WAVESHARE_S3_TFT)
        12, 14, true);
#else
        14, 18, true);
#endif
  }
#endif
  snprintf(value, sizeof(value), "%s",
           snapshot.has_state ? cuckooPiStateLabel(snapshot.state_raw) : "--");
  if (g_state.cuckoo_value[2]) {
#if defined(GMT154_06)
    set_cuckoo_colored_pair(g_state.cuckoo_label[2], g_state.cuckoo_value[2],
                            layout.state, "STATE", value, 12, 16, true);
    lv_obj_set_style_text_color(g_state.cuckoo_label[2],
                                lv_color_hex(0x91ABB2), LV_PART_MAIN);
    lv_obj_set_style_text_color(g_state.cuckoo_value[2],
                                lv_color_hex(0x35D9B2), LV_PART_MAIN);
#else
    set_cuckoo_colored_pair(
        g_state.cuckoo_label[2], g_state.cuckoo_value[2], layout.state,
#if defined(ARDUINO_WAVESHARE_S3_TFT)
        "STATE", value, 12, 14, true);
#else
        "STATE", value, 14, 18, true);
#endif
#if defined(ARDUINO_WAVESHARE_S3_TFT) || defined(GATEWAY_35)
    lv_obj_set_style_text_color(g_state.cuckoo_value[2],
                                lv_color_hex(0x41DFB8), LV_PART_MAIN);
#endif
#endif
  }

  const char* unit = myConfig.isTempFormatF() ? "F" : "C";
  const bool beer_set_selected = snapshot.has_mode &&
#if defined(GMT154_06)
      snapshot.mode_raw == 2;
#else
      (snapshot.mode_raw == 2 || snapshot.mode_raw == 3);
#endif
  const bool fridge_set_selected = snapshot.has_mode && snapshot.mode_raw == 1;
#if defined(GMT154_06)
  const auto temperature_presentation =
      gmt154_cuckoo_temperature_policy::selectTemperaturePresentation(
          true, snapshot.has_mode, snapshot.mode_raw, snapshot.has_fridge_temp,
          snapshot.fridge_temp_c, snapshot.has_beer_temp, snapshot.beer_temp_c,
          myConfig.isTempFormatF());
  if (g_state.cuckoo_heading) {
    lv_label_set_text(g_state.cuckoo_heading,
                      temperature_presentation.heading);
  }
  if (temperature_presentation.has_temperature) {
    snprintf(value, sizeof(value), "%.2f",
             static_cast<double>(
                 temperature_presentation.display_temperature));
  } else {
    snprintf(value, sizeof(value), "--");
  }
  set_cuckoo_temperature_group(
      g_state.cuckoo_value[3], g_state.cuckoo_value[8], layout.beer, value,
      unit, temperature_presentation.unit_visible, 54);
#else
  const bool fridge_constant_mode = snapshot.has_mode && snapshot.mode_raw == 1;
  const bool has_hero_temp = fridge_constant_mode ? snapshot.has_fridge_temp
                                                  : snapshot.has_beer_temp;
  const float hero_temp_c = fridge_constant_mode ? snapshot.fridge_temp_c
                                                 : snapshot.beer_temp_c;
  if (has_hero_temp) {
    const float temp = myConfig.isTempFormatF() ? convertCtoF(hero_temp_c)
                                               : hero_temp_c;
    snprintf(value, sizeof(value), "%.2f", temp);
  } else {
    snprintf(value, sizeof(value), "--");
  }
  if (g_state.cuckoo_heading) {
    lv_label_set_text(g_state.cuckoo_heading,
                      fridge_constant_mode ? "FRIDGE TEMPERATURE"
                                           : "BEER TEMPERATURE");
  }
  set_cuckoo_temperature_group(g_state.cuckoo_value[3], g_state.cuckoo_value[8],
                               layout.beer, value, unit,
                               has_hero_temp,
#if defined(ARDUINO_WAVESHARE_S3_TFT)
                               portrait ? 48 : 40);
#elif defined(GATEWAY_35)
                               portrait ? 80 : 72);
#else
                               portrait ? 80 : 72);
#endif
#endif
  if (g_state.cuckoo_value[3]) {
    lv_obj_set_style_text_color(
        g_state.cuckoo_value[3],
#if defined(ARDUINO_WAVESHARE_S3_TFT) || defined(GATEWAY_35)
        lv_color_hex(0xF4F7F7),
#else
        lv_color_hex(0xFFFFFF),
#endif
        LV_PART_MAIN);
  }
  if (g_state.cuckoo_value[8]) {
    lv_obj_set_style_text_color(
        g_state.cuckoo_value[8],
#if defined(GMT154_06)
        lv_color_hex(0x35D9B2),
#elif defined(ARDUINO_WAVESHARE_S3_TFT) || defined(GATEWAY_35)
        lv_color_hex(0x41DFB8),
#else
        lv_color_hex(0xFFFFFF),
#endif
        LV_PART_MAIN);
  }
  if (snapshot.has_beer_set) {
    const float temp = myConfig.isTempFormatF()
                           ? convertCtoF(snapshot.beer_set_c)
                           : snapshot.beer_set_c;
    snprintf(value, sizeof(value), "%.2f %s", static_cast<double>(temp), unit);
  } else {
    snprintf(value, sizeof(value), "--");
  }
  if (g_state.cuckoo_value[4]) {
#if defined(GMT154_06)
    const char* left_value = temperature_presentation.left_value
                                 ? temperature_presentation.left_value
                                 : value;
    const uint8_t left_value_size =
        temperature_presentation.fridge_constant_mode ? 12 : 16;
    set_cuckoo_colored_pair(
        g_state.cuckoo_label[4], g_state.cuckoo_value[4], layout.beer_set,
        temperature_presentation.left_label, left_value, 12, left_value_size,
        true);
    lv_obj_set_style_text_color(g_state.cuckoo_label[4],
                                lv_color_hex(0x91ABB2), LV_PART_MAIN);
#else
    set_cuckoo_colored_pair(g_state.cuckoo_label[4], g_state.cuckoo_value[4],
                            layout.beer_set, "BEER SET", value,
#if defined(ARDUINO_WAVESHARE_S3_TFT)
                            12, 14, true);
#elif defined(GATEWAY_35)
                            14, 18, true);
#else
                            16, 18, true);
#endif
#endif
    lv_obj_set_style_text_color(
        g_state.cuckoo_value[4],
        lv_color_hex(beer_set_selected ? 0xFFBF47 : 0xFFFFFF), LV_PART_MAIN);
  }
#if !defined(GMT154_06)
  const float fridge_temp = myConfig.isTempFormatF()
                                ? convertCtoF(snapshot.fridge_temp_c)
                                : snapshot.fridge_temp_c;
  if (snapshot.has_fridge_temp) {
    snprintf(value, sizeof(value), "%.2f %s", static_cast<double>(fridge_temp),
             unit);
  } else {
    snprintf(value, sizeof(value), "--");
  }
  if (g_state.cuckoo_value[5] && layout.show_fridge_temp) {
    set_cuckoo_colored_pair(
        g_state.cuckoo_label[5], g_state.cuckoo_value[5], layout.fridge,
#if defined(ARDUINO_WAVESHARE_S3_TFT)
        "FRIDGE TEMP", value,
#elif defined(GATEWAY_35)
        "FRIDGE TEMP", value,
#else
        portrait ? "FRIDGE\nTEMP" : "FRIDGE TEMP", value,
#endif
#if defined(ARDUINO_WAVESHARE_S3_TFT)
        12, 14, true);
#elif defined(GATEWAY_35)
        14, 18, true);
#else
        20, 20, true);
#endif
  }
#endif
  const float fridge_set = myConfig.isTempFormatF()
                               ? convertCtoF(snapshot.fridge_set_c)
                               : snapshot.fridge_set_c;
  if (snapshot.has_fridge_set) {
    snprintf(value, sizeof(value), "%.2f %s", static_cast<double>(fridge_set),
             unit);
  } else {
    snprintf(value, sizeof(value), "--");
  }
  if (g_state.cuckoo_value[6]) {
#if defined(GMT154_06)
    set_cuckoo_colored_pair(g_state.cuckoo_label[6], g_state.cuckoo_value[6],
                            layout.fridge_set, "FRIDGE SET", value, 12, 16,
                            true);
    lv_obj_set_style_text_color(g_state.cuckoo_label[6],
                                lv_color_hex(0x91ABB2), LV_PART_MAIN);
#else
    set_cuckoo_colored_pair(
        g_state.cuckoo_label[6], g_state.cuckoo_value[6], layout.fridge_set,
        "FRIDGE SET", value,
#if defined(ARDUINO_WAVESHARE_S3_TFT)
        12, fridge_constant_mode ? (portrait ? 20 : 28) : 14,
        !(portrait && fridge_constant_mode));
#elif defined(GATEWAY_35)
        14, fridge_constant_mode ? (portrait ? 20 : 28) : 18,
        !(portrait && fridge_constant_mode));
#else
        16, 18, true);
#endif
#endif
    lv_obj_set_style_text_color(
        g_state.cuckoo_value[6],
        lv_color_hex(fridge_set_selected ? 0xFFBF47 : 0xFFFFFF), LV_PART_MAIN);
  }
#if !defined(GMT154_06)
  if (snapshot.has_status_time) {
    formatCuckooPiRuntime(snapshot.status_time_seconds, value, sizeof(value));
  } else {
    snprintf(value, sizeof(value), "--");
  }
  if (g_state.cuckoo_value[7]) {
    set_cuckoo_colored_pair(g_state.cuckoo_label[7], g_state.cuckoo_value[7],
                            layout.runtime, "RUN TIME", value,
#if defined(ARDUINO_WAVESHARE_S3_TFT)
                            12, 12, false);
#elif defined(GATEWAY_35)
                            14, 14, false);
#else
                            14, 14, false);
#endif
  }
#endif
#if !defined(GMT154_06)
  const float room_temp = myConfig.isTempFormatF()
                               ? convertCtoF(snapshot.room_temp_c)
                               : snapshot.room_temp_c;
  if (snapshot.has_room_temp) {
    snprintf(value, sizeof(value), "%.2f %s", static_cast<double>(room_temp),
             unit);
  } else {
    snprintf(value, sizeof(value), "--");
  }
  if (g_state.cuckoo_value[9] && layout.show_chiller) {
    if (snapshot.has_mode && snapshot.mode_raw == 3) {
      if (snapshot.has_profile_step && snapshot.has_profile_steps) {
        snprintf(value, sizeof(value), "%d / %d", snapshot.profile_step + 1,
                 snapshot.profile_steps);
      } else {
        snprintf(value, sizeof(value), "--");
      }
      set_cuckoo_colored_pair(
          g_state.cuckoo_label[9], g_state.cuckoo_value[9], layout.chiller,
          "CURRENT STEP", value,
#if defined(ARDUINO_WAVESHARE_S3_TFT)
          12, 14, true);
#elif defined(GATEWAY_35)
          14, 18, true);
#else
          20, 20, true);
#endif
    } else {
    set_cuckoo_colored_pair(
        g_state.cuckoo_label[9], g_state.cuckoo_value[9], layout.chiller,
#if defined(ARDUINO_WAVESHARE_S3_TFT)
        "CHILLER TEMP", value,
#elif defined(GATEWAY_35)
        "CHILLER TEMP", value,
#else
        portrait ? "CHILLER\nTEMP" : "CHILLER TEMP", value,
#endif
#if defined(ARDUINO_WAVESHARE_S3_TFT)
        12, 14, true);
#elif defined(GATEWAY_35)
        14, 18, true);
#else
        20, 20, true);
#endif
    }
  }
#endif
}

/**
 * Gravitymon gateway UI main loop handler.
 *
 * Dispatches to the active layout's update function and applies theme
 * updates when required. This should be called periodically from the
 * LVGL task context.
 */
void gravitymon_gateway_loop(void) {
  if (g_settings_thread_state.consumeCloseRequest()) {
    g_state.settings_ui.clear();
    g_state.cuckoo_settings_ui.clear();
    close_settings_overlay();
  }

  // The fullscreen settings overlay hides the dashboard. Keep alerts and
  // close requests active, but defer background visual updates until it closes.
  const bool pause_cuckoo_visual =
#if defined(GATEWAY_35) || (defined(ARDUINO_WAVESHARE_S3_TFT) && !defined(GMT154_06))
      g_state.cuckoo_settings_ui.isOpen();
#else
      false;
#endif
  if (pause_cuckoo_visual) {
    gravitymon_gateway_attempt_pending_alert_close();
    gravitymon_gateway_update_alert_ui();
    return;
  }

  if (layout_mgr.current_layout == 0) {
    gravitymon_gateway_loop_layout_0();
  } else if (layout_mgr.current_layout == 1) {
    gravitymon_gateway_loop_layout_1();
  } else if (layout_mgr.current_layout == 2) {
    const bool cuckoo_screen =
        g_state.display_source == GatewayDisplayItemKind::CuckooPi;
    if (cuckoo_screen && !g_state.cuckoo_root &&
        !gravitymon_gateway_is_settings_open()) {
      create_cuckoo_dashboard(lv_scr_act());
    }
    select_layout_2_source();
    if (cuckoo_screen) {
      gravitymon_gateway_loop_cuckoo();
    } else {
      gravitymon_gateway_loop_layout_2();
    }
  }

  // Apply theme only when it changed since last application
  lv_obj_t* scr = lv_scr_act();
  if (layout_mgr.current_layout == 2) {
    g_state.flg_last_darkmode = g_state.flg_darkmode;
  } else if (scr && g_state.flg_darkmode != g_state.flg_last_darkmode) {
    ui_theme_colors_t theme_colors = ui_get_theme_colors(
        g_state.flg_darkmode ? UI_THEME_DARK : UI_THEME_LIGHT);
    ui_apply_theme_to_screen(scr, &theme_colors);
    g_state.flg_last_darkmode = g_state.flg_darkmode;
  }

  gravitymon_gateway_attempt_pending_alert_close();
  gravitymon_gateway_update_alert_ui();
}

/**
 * Cleanup and reset all UI state.
 *
 * Clears `g_state` so subsequent initialization starts from a clean state.
 */
void gravitymon_gateway_cleanup(void) {
  if (!gatewayAlarmLifecycle.canTearDownAlertUi()) return;
  gatewayRotationSettingsClosed();
  gatewayRotationReleaseLocks();
  gravitymon_gateway_cleanup_layout();
  g_state = gravitymon_state_t{};
  reset_gateway_widget_pointers();
  gravitymon_gateway_reset_data();
  g_disp = nullptr;
}

#endif  // ENABLE_LVGL

// EOF
