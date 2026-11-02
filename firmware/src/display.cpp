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
#if defined(GATEWAY)

#include <cstdio>
#include <cstdlib>
#include "settings_freeze_diag_events.hpp"
#if (defined(ARDUINO_WAVESHARE_S3_TFT) && !defined(GATEWAY_35) && !defined(GMT154_06)) || defined(GATEWAY_35) || (defined(GMT154_06) && !defined(GMT154_06_C3))
#include "psram_lvgl_pool.hpp"
#include <esp_heap_caps.h>
#endif
#include <display.hpp>

#if defined(ENABLE_TFT)

#if defined(GMT154_06)
#include <gmt154_display_adapter.hpp>
#include <gmt154_backlight.hpp>
#if defined(GMT154_06_C3)
#include <gmt154_lvgl_budget_probe.hpp>
#endif
#endif

#if defined(GATEWAY_35)
#include <gateway35_display.hpp>
#include <gateway35_touch.hpp>
#else
#if defined(GMT154_06)
// GMT154-06 has no touch controller; do not include CST328/CST3530.
#elif defined(WAVESHARE_CST3530)
#include <Touch_CST3530.h>
#else
#include <Touch_CST328.h>
#endif
#endif

#include <fonts.hpp>
#include <gateway_touch.hpp>
#include <gateway_rotation_teardown.hpp>
#include <log.hpp>
#include <looptimer.hpp>
#include <ui_gravitymon_gateway.hpp>
#include <ui_helpers.hpp>

#if (defined(ARDUINO_WAVESHARE_S3_TFT) && !defined(GATEWAY_35) && !defined(GMT154_06)) || defined(GATEWAY_35) || (defined(GMT154_06) && !defined(GMT154_06_C3))
namespace {
struct LvglPsramBackend {
  uint32_t internalFree() const { return heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT); }
  uint32_t psramTotal() const { return heap_caps_get_total_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); }
  uint32_t psramFree() const { return heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); }
  uint32_t psramLargest() const { return heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); }
  psram_lvgl::Monitor monitor() const { lv_mem_monitor_t m{}; lv_mem_monitor(&m); return {static_cast<uint32_t>(m.total_size),static_cast<uint32_t>(m.free_size),static_cast<uint32_t>(m.free_biggest_size)}; }
  void* allocate(size_t n) const { return heap_caps_aligned_alloc(8, n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); }
  void release(void* p) const { heap_caps_free(p); }
  void* add(void* p, size_t n) const { return lv_mem_add_pool(p,n); }
};
// Never release a registered pool while LVGL is running.
psram_lvgl::Context lvglPsramContext;
// Probe before the pre-existing PSRAM display-buffer allocation so HTTP can
// distinguish unavailable PSRAM even if that allocation causes UI setup to exit.
void probeLvglPsramPool() {
  LvglPsramBackend backend;
  const auto m=backend.monitor();
  auto& p=freeze_diag::state.pool;
  p[1].store(psram_lvgl::kExtraBytes,std::memory_order_relaxed);
  p[3].store(backend.internalFree(),std::memory_order_relaxed);
  p[5].store(backend.psramTotal(),std::memory_order_relaxed);
  p[6].store(backend.psramFree(),std::memory_order_relaxed);
  p[7].store(backend.psramLargest(),std::memory_order_relaxed);
  p[10].store(m.total,std::memory_order_relaxed);
  p[11].store(m.free,std::memory_order_relaxed);
  p[12].store(m.largest,std::memory_order_relaxed);
  p[0].store(psram_lvgl::NotAttempted,std::memory_order_release);
}
void initializeLvglPsramPool() {
  LvglPsramBackend backend;
  psram_lvgl::initialize(lvglPsramContext, backend);
  const auto& s=lvglPsramContext.snapshot;
  const uint32_t values[freeze_diag::PoolFieldCount] = {
    s.status,s.requested,s.status==psram_lvgl::Registered?s.requested:0U,
    s.internalBefore,s.internalAfter,s.psramTotal,s.psramBefore,s.psramLargestBefore,
    s.psramAfter,s.psramLargestAfter,s.lvBefore.total,s.lvBefore.free,s.lvBefore.largest,
    s.lvAfter.total,s.lvAfter.free,s.lvAfter.largest};
  for(unsigned i=1;i<freeze_diag::PoolFieldCount;++i)
    freeze_diag::state.pool[i].store(values[i],std::memory_order_relaxed);
  freeze_diag::state.pool[0].store(values[0],std::memory_order_release);
}
}
#endif

TaskHandle_t lvglTaskHandler;
#if !defined(GATEWAY_35)
static bool touchAvailable = false;
#endif

constexpr auto TTF_CALIBRATION_FILENAME = "/tft.dat";
constexpr uint32_t kOrientationPollIntervalMs = 200;
constexpr uint32_t kDashboardSettingsLongPressMs = 1200;


#if defined(GMT154_06)
struct Gmt154BacklightIo {
  void pin_mode(int pin) { pinMode(pin, OUTPUT); }
  void digital_write(int pin, bool high) {
    digitalWrite(pin, high ? HIGH : LOW);
  }
};

Gmt154BacklightIo gmt154BacklightIo;
gmt154_backlight::Controller<Gmt154BacklightIo> gmt154Backlight(
    gmt154BacklightIo, GMT154_06_BACKLIGHT_PIN);

struct Gmt154FirstFrameDiagnostics {
  uint32_t start_ms = 0;
  bool task_entry_logged = false;
  bool semaphore_logged = false;
  bool handler_begin_logged = false;
  bool handler_end_logged = false;
  bool flush_start_logged = false;
  bool flush_finish_logged = false;
  bool last_chunk_returned_logged = false;
};

Gmt154FirstFrameDiagnostics gmt154FirstFrameDiagnostics;

uint64_t gmt154FirstFrameElapsedMs(uint32_t now_ms) {
  // espframework's %u consumes uint64_t (it is not printf's %lu).
  return static_cast<uint64_t>(now_ms - gmt154FirstFrameDiagnostics.start_ms);
}

void gmt154DisplayDiagnosticEventHandler(lv_event_t *event) {
  if (event == nullptr) return;

  const lv_event_code_t code = lv_event_get_code(event);
  if (code == LV_EVENT_FLUSH_START &&
      !gmt154FirstFrameDiagnostics.flush_start_logged) {
    gmt154FirstFrameDiagnostics.flush_start_logged = true;
    const uint32_t now_ms = millis();
    Log.notice(F("DIAG: GMT154 first flush start dt=%u ms." CR),
               gmt154FirstFrameElapsedMs(now_ms));
  }

  if (code != LV_EVENT_FLUSH_FINISH) return;

  if (!gmt154FirstFrameDiagnostics.flush_finish_logged) {
    gmt154FirstFrameDiagnostics.flush_finish_logged = true;
    const uint32_t now_ms = millis();
    Log.notice(F("DIAG: GMT154 first flush finish dt=%u ms." CR),
               gmt154FirstFrameElapsedMs(now_ms));
  }

  lv_display_t *display = static_cast<lv_display_t *>(
      lv_event_get_current_target(event));
  if (display != nullptr &&
      lv_display_flush_is_last(display) &&
      !gmt154FirstFrameDiagnostics.last_chunk_returned_logged) {
    gmt154FirstFrameDiagnostics.last_chunk_returned_logged = true;
    const uint32_t now_ms = millis();
    Log.notice(
        F("DIAG: GMT154 last-chunk callback returned dt=%u ms." CR),
        gmt154FirstFrameElapsedMs(now_ms));
  }
}
#endif

#ifndef GATEWAY_ORIENTATION_DIAGNOSTICS
#define GATEWAY_ORIENTATION_DIAGNOSTICS 0
#endif

#ifndef GATEWAY_TOUCH_DIAGNOSTICS
#define GATEWAY_TOUCH_DIAGNOSTICS 0
#endif

Display::Display() {
#if defined(GATEWAY_35)
  _gateway35 = new gateway35::Gateway35DisplayAdapter();
#else
  _tft = new TFT_eSPI();
#endif
}

bool Display::applyDisplayRotation(GatewayOrientation orientation) {
  const GatewayDisplayRotation rotation = gatewayDisplayRotation(orientation);
#if defined(GATEWAY_35)
  if (_gateway35 == nullptr ||
      !_gateway35->setRotation(rotation.tft_rotation)) {
    return false;
  }
#else
  if (_tft == nullptr) return false;
  _tft->setRotation(rotation.tft_rotation);
#endif
  if (_display != nullptr) {
    lv_display_set_rotation(
        _display, rotation.lvgl_rotation_degrees == 90
                      ? LV_DISPLAY_ROTATION_90
                      : LV_DISPLAY_ROTATION_0);
  }
  return true;
}

void Display::setup() {
  Log.notice(F("DIAG: DISP stage=setup.begin." CR));
#if defined(GATEWAY_35)
  Log.notice(F("DIAG: DISP stage=panel.init.begin." CR));
  if (_gateway35 == nullptr || !_gateway35->begin()) {
    Log.error(F("DISP: Unable to initialize Waveshare 3.5 panel." CR));
    return;
  }
  Log.notice(F("DIAG: DISP stage=panel.init.done." CR));
  Log.notice(F("DIAG: DISP stage=clear.begin." CR));
  clear(0);
  Log.notice(F("DIAG: DISP stage=clear.done." CR));
  setFont(FontSize::FONT_9);
  _orientationSensorReady = _orientationSensor.beginOnInitializedBus(Wire);
  if (!_orientationSensorReady && !_orientationInitLogged) {
    Log.warning(F("DISP: Unable to initialize QMI8658 orientation sensor." CR));
    _orientationInitLogged = true;
  }
#else
  if (!_tft) {
    Log.notice(F("DIAG: DISP stage=setup.no_panel." CR));
    return;
  }

#if defined(GMT154_06)
  gmt154Backlight.initialize_off();
#endif
  Log.notice(F("DIAG: DISP stage=panel.init.begin." CR));
  _tft->init();
  Log.notice(F("DIAG: DISP stage=panel.init.done." CR));
  _tft->setSwapBytes(true);
  const GatewayDisplayRotation initial_rotation =
      gatewayDisplayRotation(_orientation);
  Log.notice(F("DIAG: DISP stage=rotation.begin." CR));
  _tft->setRotation(initial_rotation.tft_rotation);
  Log.notice(F("DIAG: DISP stage=rotation.done." CR));
  Log.notice(F("DIAG: DISP stage=clear.begin." CR));
  clear(TFT_BLACK);
  Log.notice(F("DIAG: DISP stage=clear.done." CR));
#if defined(GMT154_06)
  gmt154Backlight.turn_on();
#endif
  setFont(FontSize::FONT_9);

  Log.notice(F("DIAG: DISP stage=touch.init.begin." CR));
#if defined(WAVESHARE_CST3530)
  touchAvailable = TOUCH2_Init();
  if (!touchAvailable) {
    Log.error(F("DISP: Unable to initialize CST3530 touch controller." CR));
  }
  _orientationSensorReady = _orientationSensor.begin();
  if (!_orientationSensorReady && !_orientationInitLogged) {
    Log.warning(F("DISP: Unable to initialize QMI8658 orientation sensor." CR));
    _orientationInitLogged = true;
  }
#elif defined(GMT154_06)
  Log.notice(F("DIAG: DISP stage=touch.init.skipped." CR));
  Log.notice(F("DISP: GMT154 touch disabled; no touch controller initialized." CR));
#elif TOUCH_CS == -1  // Using CST328 touch on waveshare
  if (!Touch_Init()) {
    Log.error(F("DISP: Unable to initialize CST328 touch controller." CR));
  }
#endif
  Log.notice(F("DIAG: DISP stage=touch.init.done." CR));
#endif
  Log.notice(F("DIAG: DISP stage=setup.done." CR));
}

void Display::setFont(FontSize f) {
#if defined(GATEWAY_35)
  if (_gateway35 == nullptr) return;
  switch (f) {
    case FontSize::FONT_24:
      _gateway35->setFontPixels(3);
      break;
    case FontSize::FONT_18:
      _gateway35->setFontPixels(2);
      break;
    case FontSize::FONT_12:
    case FontSize::FONT_9:
    default:
      _gateway35->setFontPixels(1);
      break;
  }
#else
  if (!_tft) return;

  switch (f) {
    default:
    case FontSize::FONT_9:
      _tft->setFreeFont(FF17);
      break;
    case FontSize::FONT_12:
      _tft->setFreeFont(FF18);
      break;
    case FontSize::FONT_18:
      _tft->setFreeFont(FF19);
      break;
    case FontSize::FONT_24:
      _tft->setFreeFont(FF20);
      break;
  }
#endif
}

void Display::printLine(int l, const String &text) {
#if defined(GATEWAY_35)
  if (_gateway35 != nullptr) {
    _gateway35->printLine(static_cast<uint8_t>(l), text.c_str(), false);
  }
#else
  if (!_tft) return;

  uint16_t h = _tft->fontHeight();
  _tft->fillRect(0, l * h, _tft->width(), h, TFT_BLACK);
  _tft->drawString(text.c_str(), 0, l * h, GFXFF);
#endif
}

void Display::printLineCentered(int l, const String &text) {
#if defined(GATEWAY_35)
  if (_gateway35 != nullptr) {
    _gateway35->printLine(static_cast<uint8_t>(l), text.c_str(), true);
  }
#else
  if (!_tft) return;

  uint16_t h = _tft->fontHeight();
  uint16_t w = _tft->textWidth(text);
  _tft->fillRect(0, l * h, _tft->width(), h, TFT_BLACK);
  _tft->drawString(text.c_str(), (_tft->width() - w) / 2, l * h, GFXFF);
#endif
}

void Display::clear(uint32_t color) {
#if defined(GATEWAY_35)
  _backgroundColor = color;
  if (_gateway35 != nullptr) {
    _gateway35->clear(static_cast<uint16_t>(color));
  }
#else
  if (!_tft) return;

  _backgroundColor = color;
  _tft->fillScreen(_backgroundColor);
  delay(1);
#endif
}

void Display::createUI(uint8_t layoutId) {
#if defined(GMT154_06)
  gmt154FirstFrameDiagnostics = Gmt154FirstFrameDiagnostics{};
  gmt154FirstFrameDiagnostics.start_ms = millis();
#endif
  layoutId = gatewayNormalizeLayoutId(layoutId);
#if defined(GATEWAY_35)
  if (_gateway35 == nullptr) return;
  _layoutId = layoutId;
  _orientation = gatewayInitialOrientationForLayout(_layoutId);
  _orientationDebouncer = GatewayOrientationDebouncer(_orientation);
  _uiSemaphore = xSemaphoreCreateMutex();
  if (_uiSemaphore == nullptr) {
    Log.error(F("DISP: Failed to create UI semaphore." CR));
    return;
  }
  lv_init();
  lv_log_register_print_cb(log_print);
  probeLvglPsramPool();
  _display = _gateway35->createLvglDisplay([](size_t bytes) {
    freeze_diag::state.drawBufferBytes.store(bytes, std::memory_order_relaxed);
    freeze_diag::state.drawSource.store(1, std::memory_order_relaxed);
    initializeLvglPsramPool();
  });
  if (_display == nullptr) {
    Log.error(F("DISP: Failed to create Waveshare 3.5 LVGL display." CR));
    return;
  }
  lv_display_add_event_cb(_display, [](lv_event_t* e) { freeze_diag::recordDisplayEvent(lv_event_get_code(e)); }, LV_EVENT_ALL, nullptr);
  if (!applyDisplayRotation(_orientation)) {
    Log.error(F("DISP: Failed to apply Waveshare 3.5 display rotation." CR));
    return;
  }
  lv_indev_t* indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, touchScreenHandler);
  lv_indev_set_long_press_time(indev, kDashboardSettingsLongPressMs);
  lv_obj_t* scr = lv_scr_act();
  lv_obj_add_event_cb(scr, gestureScreenHandler, LV_EVENT_GESTURE, NULL);
  gravitymon_gateway_init(_display, false, _layoutId);
  gravitymon_gateway_set_orientation(_orientation);
  xTaskCreatePinnedToCore(lvgl_loop_handler, "LVGL_Handler", 10000, NULL, 0,
                          &lvglTaskHandler, 0);
  return;
#else
  if (!_tft) return;

  _orientation = gatewayInitialOrientationForLayout(layoutId);
#if defined(WAVESHARE_CST3530)
  _orientationDebouncer = GatewayOrientationDebouncer(_orientation);
#endif
  const GatewayDisplayRotation initial_rotation =
      gatewayDisplayRotation(_orientation);
  _tft->setRotation(initial_rotation.tft_rotation);

  // Create UI semaphore for thread-safe access
  _uiSemaphore = xSemaphoreCreateMutex();
  if (!_uiSemaphore) {
    Log.error(F("DISP: Failed to create UI semaphore." CR));
    return;
  }

  Log.notice(F("DISP: Using LVL v%d.%d.%d." CR), lv_version_major(),
             lv_version_minor(), lv_version_patch());

  lv_init();
#if (defined(ARDUINO_WAVESHARE_S3_TFT) && !defined(GATEWAY_35) && !defined(GMT154_06)) || (defined(GMT154_06) && !defined(GMT154_06_C3))
  probeLvglPsramPool();
#endif
  lv_log_register_print_cb(log_print);

#if (defined(ARDUINO_WAVESHARE_S3_TFT) && !defined(GATEWAY_35) && !defined(GMT154_06)) || (defined(GMT154_06) && !defined(GMT154_06_C3))
#define DRAW_BUF_SIZE (TFT_WIDTH * TFT_HEIGHT / 5 * (LV_COLOR_DEPTH / 8))
#else
#define DRAW_BUF_SIZE (TFT_WIDTH * TFT_HEIGHT / 10 * (LV_COLOR_DEPTH / 8))
#endif

#if defined(GMT154_06)
  void *draw_buf = nullptr;
#else
  size_t draw_buf_bytes = DRAW_BUF_SIZE;
#if defined(ARDUINO_WAVESHARE_S3_TFT)
  auto allocate_draw = [](size_t bytes) -> void* {
    const size_t caps=MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT;
    if(!psram_budget::allows(heap_caps_get_total_size(caps),heap_caps_get_free_size(caps),heap_caps_get_largest_free_block(caps),bytes)) return nullptr;
    return ps_malloc(bytes);
  };
  void *draw_buf = allocate_draw(draw_buf_bytes);
  if (!draw_buf) { draw_buf_bytes /= 2; draw_buf = allocate_draw(draw_buf_bytes); }
#else
  void* draw_buf = ps_malloc(draw_buf_bytes);
#endif
#endif

#if defined(GMT154_06) && !defined(GMT154_06_C3)
  const uint32_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  const uint32_t psram_largest =
      heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
  const uint32_t internal_free =
      heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  const uint32_t internal_largest = heap_caps_get_largest_free_block(
      MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  Log.notice(F("DIAG: DISP heap psram_free=%u psram_largest=%u "
               "internal_free=%u internal_largest=%u" CR),
             static_cast<uint64_t>(psram_free),
             static_cast<uint64_t>(psram_largest),
             static_cast<uint64_t>(internal_free),
             static_cast<uint64_t>(internal_largest));
#elif defined(GMT154_06_C3)
  const uint32_t internal_free =
      heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  const uint32_t internal_largest = heap_caps_get_largest_free_block(
      MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  Log.notice(F("DIAG: DISP heap internal_free=%u internal_largest=%u" CR),
             static_cast<uint64_t>(internal_free),
             static_cast<uint64_t>(internal_largest));
#endif

#if defined(GMT154_06)
  const gmt154_display::BufferAllocation allocation =
      gmt154_display::allocateDrawBuffer(DRAW_BUF_SIZE);
  draw_buf = allocation.buffer;

  if (!draw_buf) {
    Log.error(F("DISP: Failed to allocate GMT154 display buffer, size=%d" CR),
              DRAW_BUF_SIZE);
    return;
  }
  Log.notice(F("DIAG: DISP LVGL buffer source=%s size=%d" CR),
             gmt154_display::bufferSourceName(allocation.source),
             allocation.bytes);
#else
  freeze_diag::state.drawBufferBytes.store(draw_buf ? draw_buf_bytes : 0,
                                           std::memory_order_release);
  freeze_diag::state.drawSource.store(draw_buf ? 1 : 0, std::memory_order_release);
  if (!draw_buf) {
    Log.error(
        F("DISP: Failed to allocate ps ram for display buffer, size=%d" CR),
        DRAW_BUF_SIZE);
    return;
  }
#endif

#if (defined(ARDUINO_WAVESHARE_S3_TFT) && !defined(GATEWAY_35) && !defined(GMT154_06)) || (defined(GMT154_06) && !defined(GMT154_06_C3))
#if defined(GMT154_06)
  freeze_diag::state.drawBufferBytes.store(allocation.bytes, std::memory_order_release);
  freeze_diag::state.drawSource.store(allocation.source == gmt154_display::BufferSource::Psram ? 1 : 2, std::memory_order_release);
#endif
  // Preserve the existing display-buffer allocation first. Register the
  // optional PSRAM pool before any LVGL display/input objects or UI task.
  initializeLvglPsramPool();
#endif

#if defined(GMT154_06)
  _display = lv_display_create(TFT_WIDTH, TFT_HEIGHT);
#else
  _display = lv_tft_espi_create(TFT_WIDTH, TFT_HEIGHT, draw_buf, draw_buf_bytes);
#endif
  if (_display == nullptr) {
    Log.error(F("DIAG: DISP LVGL display object creation failed." CR));
    free(draw_buf);
    return;
  }
  Log.notice(F("DIAG: DISP LVGL display object creation success." CR));

#if defined(GMT154_06)
  lv_display_set_color_format(_display, LV_COLOR_FORMAT_RGB565);
  gmt154_display::registerDrawBuffer(_display, allocation);
  lv_display_set_user_data(_display, _tft);
  lv_display_set_flush_cb(_display, gmt154_display::flush);
  lv_display_add_event_cb(_display, gmt154DisplayDiagnosticEventHandler,
                          LV_EVENT_FLUSH_START, nullptr);
  lv_display_add_event_cb(_display, gmt154DisplayDiagnosticEventHandler,
                          LV_EVENT_FLUSH_FINISH, nullptr);
#endif

  lv_display_add_event_cb(_display, [](lv_event_t* e) { freeze_diag::recordDisplayEvent(lv_event_get_code(e)); }, LV_EVENT_ALL, nullptr);

  lv_display_set_rotation(
      _display,
      initial_rotation.lvgl_rotation_degrees == 90 ? LV_DISPLAY_ROTATION_90
                                                   : LV_DISPLAY_ROTATION_0);

  // A failed touch probe must not feed invalid coordinates to LVGL.
#if !defined(GMT154_06)
#if defined(WAVESHARE_CST3530)
  if (touchAvailable) {
#endif
    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, touchScreenHandler);
    lv_indev_set_long_press_time(indev, kDashboardSettingsLongPressMs);
#if defined(WAVESHARE_CST3530)
  }
#endif
#endif

  // Register gesture event handler for the main screen
  lv_obj_t *scr = lv_scr_act();
  lv_obj_add_event_cb(scr, gestureScreenHandler, LV_EVENT_GESTURE, NULL);

  // Initialize the gravitymon gateway UI with selected layout
  gravitymon_gateway_init(_display, false, layoutId);
  gravitymon_gateway_set_orientation(_orientation);

#if defined(GMT154_06)
  const uint32_t internal_free_before_task =
      heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  const uint32_t internal_largest_before_task =
      heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  const BaseType_t task_result =
      xTaskCreatePinnedToCore(lvgl_loop_handler,  // Function to implement the task
                              "LVGL_Handler",     // Name of the task
                              10000,              // ESP-IDF stack size in bytes
                              NULL,               // Task input parameter
                              0,                  // Priority of the task
                              &lvglTaskHandler,   // Task handle.
                              0);                 // Core where the task should run
  const uint32_t internal_free_after_task =
      heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  const uint32_t internal_largest_after_task =
      heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  const uint32_t now_ms = millis();
  Log.notice(F("DIAG: GMT154 task result=%d internal_free_before=%u "
               "internal_largest_before=%u internal_free_after=%u "
               "internal_largest_after=%u dt=%u ms." CR),
             static_cast<int>(task_result),
             static_cast<uint64_t>(internal_free_before_task),
             static_cast<uint64_t>(internal_largest_before_task),
             static_cast<uint64_t>(internal_free_after_task),
             static_cast<uint64_t>(internal_largest_after_task),
             gmt154FirstFrameElapsedMs(now_ms));
#else
  xTaskCreatePinnedToCore(lvgl_loop_handler,  // Function to implement the task
                          "LVGL_Handler",     // Name of the task
                          10000,              // ESP-IDF stack size in bytes
                          NULL,               // Task input parameter
                          0,                  // Priority of the task
                          &lvglTaskHandler,   // Task handle.
                          0);                 // Core where the task should run
#endif
#endif
}

void Display::updateEmpty() {
  gravitymon_gateway_set_name("");
  gravitymon_gateway_set_device_id("");
  gravitymon_gateway_set_index(0, 0);
  gravitymon_gateway_set_type("");
  gravitymon_gateway_set_source("");
  gravitymon_gateway_set_time("");
  gravitymon_gateway_set_gravity(NAN, ' ');
  gravitymon_gateway_set_temp(NAN, NAN, ' ');
  gravitymon_gateway_set_battery_voltage(NAN);
  gravitymon_gateway_set_battery_percentage(NAN);
  gravitymon_gateway_set_rssi(0);
}

void Display::pollOrientation() {
#if defined(ENABLE_TFT) && \
    (defined(GATEWAY_35) || defined(WAVESHARE_CST3530))
  if (!gatewayAutoRotationEnabledForLayout(gravitymon_gateway_get_layout())) {
    return;
  }
#if defined(GATEWAY_35)
  const bool orientation_hardware_ready =
      _gateway35 != nullptr && _display != nullptr && _orientationSensorReady;
#else
  const bool orientation_hardware_ready =
      _tft != nullptr && _display != nullptr && _orientationSensorReady;
#endif
  if (!orientation_hardware_ready) {
    return;
  }

  GatewayRotationGuard& rotation_guard = gatewayRotationGuard();
  if (rotation_guard.consumeFreshSampleRequest()) {
    _orientationDebouncer.resetForFreshSample();
    return;
  }
  if (rotation_guard.settingsOpen()) {
    _orientationDebouncer.resetForFreshSample();
    return;
  }

  GatewayOrientation accepted = _orientation;
  bool orientation_ready =
      rotation_guard.takePendingOrientation(&accepted);

  if (!orientation_ready) {
    const uint32_t now_ms = millis();
    if (now_ms - _lastOrientationPollMs < kOrientationPollIntervalMs) {
      return;
    }
    _lastOrientationPollMs = now_ms;

    GatewayAccelerationSample sample{};
    if (!_orientationSensor.readAcceleration(&sample)) {
      if (!_orientationReadLogged) {
        Log.warning(F("DISP: Unable to read QMI8658 acceleration sample." CR));
        _orientationReadLogged = true;
      }
      return;
    }
#if defined(GATEWAY_35)
    sample = gateway35OrientationSample(sample);
#endif

#if GATEWAY_ORIENTATION_DIAGNOSTICS
    static uint32_t last_orientation_log_ms = 0;
    if (now_ms - last_orientation_log_ms >= 1000) {
      Log.info(F("DISP: QMI raw=%d,%d,%d" CR),
               static_cast<int>(sample.x_g * 100.0f),
               static_cast<int>(sample.y_g * 100.0f),
               static_cast<int>(sample.z_g * 100.0f));
      last_orientation_log_ms = now_ms;
    }
#endif

    if (!_orientationDebouncer.update(sample, now_ms, &accepted) ||
        accepted == _orientation) {
      return;
    }

    rotation_guard.requestOrientation(accepted);
    orientation_ready =
        rotation_guard.takePendingOrientation(&accepted);
    if (!orientation_ready) return;
  }

  if (_uiSemaphore == nullptr ||
      !xSemaphoreTake(_uiSemaphore, pdMS_TO_TICKS(100))) {
    rotation_guard.requestOrientation(accepted);
    return;
  }

  const GatewayOrientation old_orientation = _orientation;
  if (!applyDisplayRotation(accepted)) {
    rotation_guard.requestOrientation(accepted);
    xSemaphoreGive(_uiSemaphore);
    return;
  }

  if (!gravitymon_gateway_set_orientation(accepted)) {
    (void)applyDisplayRotation(old_orientation);
    _orientation = old_orientation;
    _orientationDebouncer = GatewayOrientationDebouncer(_orientation);
    rotation_guard.requestOrientation(accepted);
    xSemaphoreGive(_uiSemaphore);
    return;
  }

  _orientation = accepted;
  xSemaphoreGive(_uiSemaphore);
#endif
}

void Display::updateGravity(const char *deviceId, const char *name, uint8_t index, uint8_t maxIndex,
                            const char *type, const char *source,
                            const char *timestamp, float gravity,
                            char gravityUnit, float temp, char tempUnit,
                            float batteryVoltage, float batteryPercentage,
                            int rrsi, bool online) {
  gravitymon_gateway_set_name(name);
  gravitymon_gateway_set_device_id(deviceId);
  gravitymon_gateway_set_measurement_online(deviceId, online);
  gravitymon_gateway_set_index(index, maxIndex);
  gravitymon_gateway_set_type(type);
  gravitymon_gateway_set_source(source);
  gravitymon_gateway_set_time(timestamp);
  gravitymon_gateway_set_gravity(gravity, gravityUnit);
  gravitymon_gateway_set_temp(temp, NAN, tempUnit);
  gravitymon_gateway_set_battery_voltage(batteryVoltage);
  gravitymon_gateway_set_battery_percentage(batteryPercentage);
  gravitymon_gateway_set_rssi(rrsi);
}

void Display::updatePressure(const char *deviceId, const char *name, uint8_t index, uint8_t maxIndex,
                             const char *type, const char *source,
                             const char *timestamp, float pressure,
                             float pressure2, const char *pressureUnit,
                             float temp, char tempUnit, float batteryVoltage,
                             float batteryPercentage, int rrsi) {
  gravitymon_gateway_set_name(name);
  gravitymon_gateway_set_device_id(deviceId);
  gravitymon_gateway_set_index(index, maxIndex);
  gravitymon_gateway_set_type(type);
  gravitymon_gateway_set_source(source);
  gravitymon_gateway_set_time(timestamp);
  gravitymon_gateway_set_pressure(pressure, pressure2, pressureUnit);
  gravitymon_gateway_set_temp(temp, NAN, tempUnit);
  gravitymon_gateway_set_battery_voltage(batteryVoltage);
  gravitymon_gateway_set_battery_percentage(batteryPercentage);
  gravitymon_gateway_set_rssi(rrsi);
}

void Display::updateTemperature(const char *deviceId, const char *name, uint8_t index,
                                uint8_t maxIndex, const char *type,
                                const char *source, const char *timestamp,
                                float temp, float temp2, char tempUnit,
                                int rrsi) {
  gravitymon_gateway_set_name(name);
  gravitymon_gateway_set_device_id(deviceId);
  gravitymon_gateway_set_index(index, maxIndex);
  gravitymon_gateway_set_type(type);
  gravitymon_gateway_set_source(source);
  gravitymon_gateway_set_time(timestamp);
  gravitymon_gateway_set_gravity(NAN, ' ');
  gravitymon_gateway_set_temp(temp, temp2, tempUnit);
  gravitymon_gateway_set_battery_voltage(NAN);
  gravitymon_gateway_set_battery_percentage(NAN);
  gravitymon_gateway_set_rssi(rrsi);
}

void Display::updateCuckooPi(const CuckooPiSnapshot& snapshot, uint8_t index,
                             uint8_t maxIndex, bool online) {
  gravitymon_gateway_set_index(index, maxIndex);
  gravitymon_gateway_set_cuckoo_pi(snapshot, online);
}

void Display::updateHistory(const char *history, int idx) {
  gravitymon_gateway_set_history(idx, history);
}

void Display::updateStatus(const char *status) {
  gravitymon_gateway_set_status(status);
}

void Display::updateNetworkStatus(const char *ip, int wifiRssi) {
  gravitymon_gateway_set_network(ip, wifiRssi);
}

void Display::updateDarkmode(bool darkmode) {
  gravitymon_gateway_set_theme(darkmode);
}

void Display::setLayout(uint8_t layoutId) {
#if defined(GATEWAY_35)
  layoutId = gatewayNormalizeLayoutId(layoutId);
  const uint8_t old_layout = gravitymon_gateway_get_layout();
  const GatewayLayoutTransitionPlan plan =
      gatewayPlanLayoutTransition(old_layout, layoutId, true);
  if (plan.result == GatewayLayoutTransitionResult::Noop) return;
  if (_uiSemaphore && xSemaphoreTake(_uiSemaphore, pdMS_TO_TICKS(100))) {
    const GatewayOrientation old_orientation = _orientation;
    const uint8_t old_layout_id = _layoutId;
    _orientation = plan.target_orientation;
    _layoutId = layoutId;
    _orientationDebouncer = GatewayOrientationDebouncer(_orientation);
    if (!applyDisplayRotation(_orientation) ||
        !gravitymon_gateway_set_layout(layoutId)) {
      _orientation = old_orientation;
      _layoutId = old_layout_id;
      _orientationDebouncer = GatewayOrientationDebouncer(_orientation);
      (void)applyDisplayRotation(old_orientation);
    }
    xSemaphoreGive(_uiSemaphore);
  } else {
    Log.warning(
        F("DISP: Failed to acquire UI semaphore for layout change." CR));
  }
  return;
#else
  layoutId = gatewayNormalizeLayoutId(layoutId);
  const uint8_t old_layout = gravitymon_gateway_get_layout();
  const GatewayLayoutTransitionPlan plan =
      gatewayPlanLayoutTransition(old_layout, layoutId, true);
  if (plan.result == GatewayLayoutTransitionResult::Noop) {
    return;
  }

  if (_uiSemaphore && xSemaphoreTake(_uiSemaphore, pdMS_TO_TICKS(100))) {
    const GatewayOrientation old_orientation = _orientation;
    const GatewayDisplayRotation old_rotation =
        gatewayDisplayRotation(old_orientation);

    _orientation = plan.target_orientation;
    const GatewayDisplayRotation rotation =
        gatewayDisplayRotation(_orientation);
    _tft->setRotation(rotation.tft_rotation);
    if (_display) {
      lv_display_set_rotation(
          _display,
          rotation.lvgl_rotation_degrees == 90 ? LV_DISPLAY_ROTATION_90
                                               : LV_DISPLAY_ROTATION_0);
    }
#if defined(WAVESHARE_CST3530)
    _orientationDebouncer = GatewayOrientationDebouncer(_orientation);
#endif

    if (!gravitymon_gateway_set_layout(layoutId)) {
      _orientation = old_orientation;
      _tft->setRotation(old_rotation.tft_rotation);
      if (_display) {
        lv_display_set_rotation(
            _display,
            old_rotation.lvgl_rotation_degrees == 90
                ? LV_DISPLAY_ROTATION_90
                : LV_DISPLAY_ROTATION_0);
      }
#if defined(WAVESHARE_CST3530)
      _orientationDebouncer = GatewayOrientationDebouncer(_orientation);
#endif
    }
    xSemaphoreGive(_uiSemaphore);
  } else {
    Log.warning(
        F("DISP: Failed to acquire UI semaphore for layout change." CR));
  }
#endif
}

bool Display::isSettingsOpen() const {
  return gravitymon_gateway_is_settings_open();
}

void Display::calibrateTouch() {
#if defined(GATEWAY_35)
  return;
#else
#if defined(ENABLE_LVGL) && \
    TOUCH_CS != -1  // Only needed when using TFT_eSPI touch handler
  if (!_tft) return;

  uint16_t x, y, pressed, i = 0;

  myDisplay.printLineCentered(4, "Press screen to calibrate");

  do {
    delay(300);
    pressed = _tft->getTouch(&x, &y, 600);
    Log.info(F("DISP: Screen touched %d." CR), pressed);
  } while (!pressed && ++i < 10);

  if (pressed) {
    clear(TFT_GREEN);
    myDisplay.printLineCentered(4, "Touch detected");
    Log.info(F("DISP: Touch screen pressed, force calibration." CR));
    delay(3000);
  }

  File file = LittleFS.open(TTF_CALIBRATION_FILENAME, "r");

  if (file) {
    Log.info(F("DISP: Loading touch calibration data from file." CR));
    file.read(reinterpret_cast<uint8_t *>(&this->_touchCalibrationlData),
              sizeof(_touchCalibrationlData));
    file.close();
  } else {
    _touchCalibrationlData[0] = 0;
  }

  if (pressed || (_touchCalibrationlData[0] == 0)) {
    Log.info(F("DISP: Running calibration sequence." CR));

    clear(TFT_BLACK);
    myDisplay.printLineCentered(4, "Calibration started");
    _tft->calibrateTouch(_touchCalibrationlData, TFT_GREEN, TFT_BLACK, 15);

    file = LittleFS.open(TTF_CALIBRATION_FILENAME, "w");

    if (file) {
      file.write(reinterpret_cast<uint8_t *>(&this->_touchCalibrationlData),
                 sizeof(_touchCalibrationlData));
      file.close();
    } else {
      Log.warning(F("DISP: Failed to write calibration data to file." CR));
    }

    myDisplay.printLineCentered(4, "Touch calibration completed");
    delay(3000);
  }

  myDisplay.printLineCentered(4, "");
#endif
#endif
}

bool Display::getTouch(uint16_t *x, uint16_t *y) {
#if defined(ENABLE_TFT)

#if defined(GMT154_06)
  (void)x;
  (void)y;
  return false;
#elif defined(GATEWAY_35)
  if (_gateway35 == nullptr || x == nullptr || y == nullptr) return false;
  uint16_t raw_x = 0;
  uint16_t raw_y = 0;
  if (!_gateway35->getTouch(&raw_x, &raw_y)) return false;
  const GatewayTouchPoint point =
      gateway35NativeTouchPoint(raw_x, raw_y, _orientation);
  _lastTouchRawX = raw_x;
  _lastTouchRawY = raw_y;
  _lastTouchTransformedX = point.x;
  _lastTouchTransformedY = point.y;
  *x = point.x;
  *y = point.y;
  return true;
#elif defined(WAVESHARE_CST3530)
  if (!touchAvailable) return false;

  uint16_t xt[CST3530_LCD_TOUCH_MAX_POINTS] = {0};
  uint16_t yt[CST3530_LCD_TOUCH_MAX_POINTS] = {0};
  uint16_t strength[CST3530_LCD_TOUCH_MAX_POINTS] = {0};
  uint8_t cnt = 0;

  if (Touch2_Read_Data() &&
      Touch2_Get_XY(xt, yt, strength, &cnt,
                    uint8_t(CST3530_LCD_TOUCH_MAX_POINTS)) &&
      cnt > 0) {
    const GatewayTouchPoint point =
        gatewayTouchTransform(xt[0], yt[0], _orientation);
    _lastTouchRawX = xt[0];
    _lastTouchRawY = yt[0];
    _lastTouchTransformedX = point.x;
    _lastTouchTransformedY = point.y;
    *x = point.x;
    *y = point.y;
    return true;
  }
#elif TOUCH_CS == -1  // Using CST328 touch on waveshare
  uint16_t xt[CST328_LCD_TOUCH_MAX_POINTS] = {0};
  uint16_t yt[CST328_LCD_TOUCH_MAX_POINTS] = {0};
  uint16_t strength[CST328_LCD_TOUCH_MAX_POINTS] = {0};
  uint8_t cnt = 0;

  Touch_Read_Data();
  uint8_t b =
      Touch_Get_XY(xt, yt, strength, &cnt, uint8_t CST328_LCD_TOUCH_MAX_POINTS);

  if (b && cnt > 0) {
    const GatewayTouchPoint point =
        gatewayTouchTransform(xt[0], yt[0], _orientation);
    _lastTouchRawX = xt[0];
    _lastTouchRawY = yt[0];
    _lastTouchTransformedX = point.x;
    _lastTouchTransformedY = point.y;
    *x = point.x;
    *y = point.y;
    return true;
  }
#else
  uint16_t xt, yt;
  uint8_t b = _tft->getTouch(&xt, &yt);

  if (b) {
    if (xt < 0) xt = 0;
    if (yt < 0) yt = 0;

    // if (_rotation == Rotation::ROTATION_90) {
    *x = yt;
    *y = TFT_HEIGHT - xt;
    // } else {  // Rotation::ROTATION_270
    //   *x = yt;
    //   *y = TFT_HEIGHT - xt;
    // }
    return true;
  }
#endif
  return false;
#else
  return false;
#endif
}

// LVGL Wrappers and Handlers
// **************************************************************************************************

void touchScreenHandler(lv_indev_t *indev, lv_indev_data_t *data) {
  freeze_diag::Scope diagnostic(freeze_diag::Touch);
  (void)indev;
  uint16_t x = 0, y = 0;
#if defined(GATEWAY_35) && GATEWAY_TOUCH_DIAGNOSTICS
  static bool diagnostic_touch_down = false;
#endif

  if (myDisplay.getTouch(&x, &y)) {
    gatewayRotationPointerPressed();
    data->state = LV_INDEV_STATE_PRESSED;
    data->point.x = x;
    data->point.y = y;

#if defined(GATEWAY_35) && GATEWAY_TOUCH_DIAGNOSTICS
    if (!diagnostic_touch_down) {
      const GatewayTouchPoint final_point = gatewayLvglPointerDiagnosticPoint(
          GatewayTouchPoint{x, y}, myDisplay.orientation(), 320, 480);
      const GatewayDisplayRotation rotation =
          gatewayDisplayRotation(myDisplay.orientation());
      Log.notice(F("TOUCH-DIAG raw=%d:%d project=%d:%d lvgl=%d:%d "
                   "orientation=%d rotation=%d" CR),
                 static_cast<int>(myDisplay.lastTouchRawX()),
                 static_cast<int>(myDisplay.lastTouchRawY()),
                 static_cast<int>(x), static_cast<int>(y),
                 static_cast<int>(final_point.x),
                 static_cast<int>(final_point.y),
                 static_cast<int>(myDisplay.orientation()),
                 static_cast<int>(rotation.lvgl_rotation_degrees));
      diagnostic_touch_down = true;
    }
#endif

#if GATEWAY_ORIENTATION_DIAGNOSTICS
    const GatewayDisplayRotation rotation =
        gatewayDisplayRotation(myDisplay.orientation());
    Log.verbose(F("TOUCH raw=%u:%u transformed=%u:%u orientation=%u "
                  "lvgl_rotation=%u pressed=1 released=0" CR),
                myDisplay.lastTouchRawX(), myDisplay.lastTouchRawY(), x, y,
                static_cast<unsigned>(myDisplay.orientation()),
                static_cast<unsigned>(rotation.lvgl_rotation_degrees));
#endif
  } else {
    gatewayRotationPointerReleased();
    data->state = LV_INDEV_STATE_RELEASED;
#if defined(GATEWAY_35) && GATEWAY_TOUCH_DIAGNOSTICS
    diagnostic_touch_down = false;
#endif
#if GATEWAY_ORIENTATION_DIAGNOSTICS
    const GatewayDisplayRotation rotation =
        gatewayDisplayRotation(myDisplay.orientation());
    Log.verbose(F("TOUCH raw=%u:%u transformed=%u:%u orientation=%u "
                  "lvgl_rotation=%u pressed=0 released=1" CR),
                myDisplay.lastTouchRawX(), myDisplay.lastTouchRawY(),
                myDisplay.lastTouchTransformedX(),
                myDisplay.lastTouchTransformedY(),
                static_cast<unsigned>(myDisplay.orientation()),
                static_cast<unsigned>(rotation.lvgl_rotation_degrees));
#endif
  }
}

void btnLeftEventHandler(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    gestureLeft();
  }
}

void btnRightEventHandler(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    gestureRight();
  }
}

void gestureScreenHandler(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_GESTURE) {
    // Layout 2 is a fixed full-screen dashboard; keep long-press settings but
    // prevent horizontal swipes from changing pages.
    if (gravitymon_gateway_get_layout() == 2) {
      return;
    }
    lv_dir_t gesture = lv_indev_get_gesture_dir(lv_indev_get_act());
    switch (gesture) {
      case LV_DIR_LEFT:
        Log.info(F("DISP: Gesture LEFT." CR));
        gestureLeft();
        break;
      case LV_DIR_RIGHT:
        Log.info(F("DISP: Gesture RIGHT." CR));
        gestureRight();
        break;
      case LV_DIR_TOP:
        Log.info(F("DISP: Gesture UP." CR));
        break;
      case LV_DIR_BOTTOM:
        Log.info(F("DISP: Gesture DOWN." CR));
        break;
      default:
        break;
    }
  }
}

void log_print(lv_log_level_t level, const char *buf) {
  LV_UNUSED(level);
  Log.notice(F("LVGL: %s." CR), buf);
}

void updateLabel(lv_obj_t *obj, const char *label) {
  lv_label_set_text(obj, label);
}

#if defined(GMT154_06_C3)
// Called only after the LVGL task has released its UI mutex.
void gmt154LogLvglBudget(const gmt154::LvglBudgetSnapshot& snapshot) {
  if (!snapshot.valid) return;
  char line[gmt154::lvglBudgetLineCapacity];
  gmt154::formatLvglBudget(line, sizeof(line), snapshot);
  Log.notice(F("%s" CR), line);
}
#endif

void lvgl_loop_handler(void *parameter) {
  (void)parameter;
#if defined(GMT154_06)
  if (!gmt154FirstFrameDiagnostics.task_entry_logged) {
    gmt154FirstFrameDiagnostics.task_entry_logged = true;
    const uint32_t now_ms = millis();
    Log.notice(F("DIAG: GMT154 first LVGL task entry dt=%u ms." CR),
               gmt154FirstFrameElapsedMs(now_ms));
  }
#endif
  LoopTimer taskLoop(500);
#if defined(GMT154_06_C3)
  gmt154::LvglBudgetSampler lvglBudgetSampler;
#endif

  #if defined(GATEWAY_35) || (defined(ARDUINO_WAVESHARE_S3_TFT) && !defined(GMT154_06)) || (defined(GMT154_06) && !defined(GMT154_06_C3))
  uint32_t previous_tick_ms = millis();
  #endif
  for (;;) {
#if defined(GMT154_06_C3)
    gmt154::LvglBudgetSnapshot lvglBudgetSnapshot;
#endif
#if defined(GATEWAY_35) || (defined(ARDUINO_WAVESHARE_S3_TFT) && !defined(GMT154_06)) || (defined(GMT154_06) && !defined(GMT154_06_C3))
    const uint32_t current_tick_ms = millis();
    lv_tick_inc(current_tick_ms - previous_tick_ms);
    previous_tick_ms = current_tick_ms;
#else
    lv_tick_inc(10);
#endif
    SemaphoreHandle_t semaphore = myDisplay.getUISemaphore();
#if defined(GMT154_06)
    const bool semaphore_acquired =
        semaphore != nullptr && xSemaphoreTake(semaphore, pdMS_TO_TICKS(100));
    if (semaphore != nullptr && !gmt154FirstFrameDiagnostics.semaphore_logged) {
      gmt154FirstFrameDiagnostics.semaphore_logged = true;
      const uint32_t now_ms = millis();
      Log.notice(F("DIAG: GMT154 first semaphore %s dt=%u ms." CR),
                 semaphore_acquired ? "acquired" : "timeout",
                 gmt154FirstFrameElapsedMs(now_ms));
    }
    if (semaphore_acquired)
#else
    if (semaphore != nullptr && xSemaphoreTake(semaphore, pdMS_TO_TICKS(100)))
#endif
    {
      if (taskLoop.hasExpired()) {
#if (defined(ARDUINO_WAVESHARE_S3_TFT) && !defined(GMT154_06_C3)) || (defined(GMT154_06) && !defined(GMT154_06_C3)) || defined(GATEWAY_35)
        const size_t caps=MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT;
        lv_mem_monitor_t monitor{}; lv_mem_monitor(&monitor);
        freeze_diag::state.runtime[0].store(heap_caps_get_free_size(caps),std::memory_order_relaxed);
        freeze_diag::state.runtime[1].store(heap_caps_get_largest_free_block(caps),std::memory_order_relaxed);
        freeze_diag::state.runtime[2].store(monitor.free_size,std::memory_order_relaxed);
        freeze_diag::state.runtime[3].store(monitor.free_biggest_size,std::memory_order_relaxed);
        freeze_diag::state.runtime[4].store(millis(),std::memory_order_release);
#endif
        taskLoop.reset();
        { freeze_diag::Scope diagnostic(freeze_diag::Gateway); gravitymon_gateway_loop(); }
      }

      // LVGL reads and dispatches pointer input while rotation is excluded.
      // touchScreenHandler sets the guard before this call returns.
#if defined(GMT154_06)
      if (!gmt154FirstFrameDiagnostics.handler_begin_logged) {
        gmt154FirstFrameDiagnostics.handler_begin_logged = true;
        const uint32_t now_ms = millis();
      Log.notice(F("DIAG: GMT154 first LVGL handler begin dt=%u ms." CR),
                   gmt154FirstFrameElapsedMs(now_ms));
      }
#endif
      { freeze_diag::Scope diagnostic(freeze_diag::Handler); lv_task_handler(); }
#if defined(GMT154_06)
      if (!gmt154FirstFrameDiagnostics.handler_end_logged) {
        gmt154FirstFrameDiagnostics.handler_end_logged = true;
        const uint32_t now_ms = millis();
      Log.notice(F("DIAG: GMT154 first LVGL handler end dt=%u ms." CR),
                   gmt154FirstFrameElapsedMs(now_ms));
      }
#endif
#if defined(GMT154_06_C3)
      gmt154::captureLvglBudget(lvglBudgetSampler, true, millis(), lvglBudgetSnapshot);
#endif
      xSemaphoreGive(semaphore);
#if defined(GMT154_06_C3)
      gmt154LogLvglBudget(lvglBudgetSnapshot);
#endif

      // Rotation teardown may acquire the semaphore again only after LVGL has
      // completed input dispatch and released the current UI critical section.
      myDisplay.pollOrientation();
    }
    delay(10);
  }
}

#else  // ENABLE_TFT

Display::Display() {}

void Display::setup() {}

void Display::createUI(uint8_t layoutId) {}

void Display::calibrateTouch() {}

void Display::setFont(FontSize f) {}

void Display::printLine(int l, const String &text) {}

void Display::printLineCentered(int l, const String &text) {}

void Display::clear(uint32_t color) {}

void Display::updateEmpty() {}
void Display::updateGravity(const char *deviceId, const char *name, uint8_t index, uint8_t maxIndex,
                            const char *type, const char *source,
                            const char *timestamp, float gravity, char unit,
                            float temp, char tempUnit, float batteryVoltage,
                            float batteryPercentage, int rssi, bool online) {}

void Display::updatePressure(const char *deviceId, const char *name, uint8_t index, uint8_t maxIndex,
                             const char *type, const char *source,
                             const char *timestamp, float pressure,
                             float pressure2, const char *pressureUnit,
                             float temp, char tempUnit, float batteryVoltage,
                             float batteryPercentage, int rssi) {}
void Display::updateTemperature(const char *deviceId, const char *name, uint8_t index,
                                uint8_t maxIndex, const char *type,
                                const char *source, const char *timestamp,
                                float temp, float temp2, char tempUnit,
                                int rssi) {}
void Display::updateCuckooPi(const CuckooPiSnapshot&, uint8_t, uint8_t, bool) {}
void Display::updateHistory(const char *history, int idx) {}

void Display::updateStatus(const char *status) {}

void Display::updateNetworkStatus(const char *ip, int wifiRssi) {}

void Display::updateDarkmode(bool darkmode) {}

void Display::setLayout(uint8_t layoutId) {}

bool Display::isSettingsOpen() const { return false; }

void Display::pollOrientation() {}

#endif  // ENABLE_TFT

#endif  // GATEWAY

// EOF
