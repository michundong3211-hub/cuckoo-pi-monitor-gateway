#if defined(GATEWAY_35)

#include "gateway35_display.hpp"

#include <Arduino_GFX_Library.h>
#include <Wire.h>

#include "gateway35_board_io.hpp"
#include "gateway35_touch.hpp"
#include "waveshare_35_profile.hpp"

namespace gateway35 {
namespace {
constexpr uint16_t kBlack = 0x0000;
constexpr uint16_t kWhite = 0xFFFF;
}

Gateway35DisplayAdapter::Gateway35DisplayAdapter() = default;

Gateway35DisplayAdapter::~Gateway35DisplayAdapter() {
  if (_display != nullptr) lv_display_delete(_display);
  if (_drawBuffer != nullptr) free(_drawBuffer);
  delete _gfx;
  delete _bus;
}

bool Gateway35DisplayAdapter::begin() {
  if (_ready) return true;
  StartupHooks hooks = {this, &Gateway35DisplayAdapter::startupProbeTouch,
                        &Gateway35DisplayAdapter::startupBeginDisplay,
                        &Gateway35DisplayAdapter::startupFillBlack,
                        &Gateway35DisplayAdapter::startupSetBacklight,
                        &Gateway35DisplayAdapter::startupDelay};
  _ready = initializePanelHardware(Wire, hooks);
  return _ready;
}

lv_display_t* Gateway35DisplayAdapter::createLvglDisplay() {
  if (!_ready) return nullptr;
  if (_display != nullptr) return _display;

  _drawBufferBytes = kDisplayWidth * kDisplayHeight / 10 * 2;
  _drawBuffer = static_cast<uint8_t*>(ps_malloc(_drawBufferBytes));
  if (_drawBuffer == nullptr) {
    _drawBufferBytes = 0;
    return nullptr;
  }
  _display = lv_display_create(kDisplayWidth, kDisplayHeight);
  if (_display == nullptr) {
    free(_drawBuffer);
    _drawBuffer = nullptr;
    _drawBufferBytes = 0;
    return nullptr;
  }
  lv_display_set_color_format(_display, LV_COLOR_FORMAT_RGB565);
  lv_display_set_buffers(_display, _drawBuffer, nullptr, _drawBufferBytes,
                         LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_user_data(_display, this);
  lv_display_set_flush_cb(_display, &Gateway35DisplayAdapter::flush);
  return _display;
}

bool Gateway35DisplayAdapter::setRotation(uint8_t rotation) {
  if (!_ready || _gfx == nullptr) return false;
  _gfx->setRotation(rotation);
  return true;
}

void Gateway35DisplayAdapter::clear(uint16_t rgb565) {
  if (_gfx != nullptr) _gfx->fillScreen(rgb565);
}

void Gateway35DisplayAdapter::setFontPixels(uint8_t pixels) {
  if (_gfx != nullptr) _gfx->setTextSize(pixels);
}

void Gateway35DisplayAdapter::printLine(uint8_t line, const char* text,
                                         bool centered) {
  if (_gfx == nullptr || text == nullptr) return;
  constexpr uint16_t kLineHeight = 24;
  const int16_t y = static_cast<int16_t>(line * kLineHeight);
  int16_t x = 0;
  if (centered) {
    int16_t bounds_x = 0;
    int16_t bounds_y = 0;
    uint16_t text_width = 0;
    uint16_t text_height = 0;
    _gfx->getTextBounds(text, 0, y, &bounds_x, &bounds_y, &text_width,
                        &text_height);
    x = static_cast<int16_t>((kDisplayWidth - text_width) / 2);
  }
  _gfx->fillRect(0, y, kDisplayWidth, kLineHeight, kBlack);
  _gfx->setTextColor(kWhite);
  _gfx->setCursor(x, y);
  _gfx->print(text);
}

bool Gateway35DisplayAdapter::getTouch(uint16_t* x, uint16_t* y) {
  if (!_ready || x == nullptr || y == nullptr) return false;
  TouchPoint point{};
  if (readTouch(Wire, &point) != TouchResult::kValid) return false;
  *x = point.x;
  *y = point.y;
  return true;
}

void Gateway35DisplayAdapter::flush(lv_display_t* display,
                                    const lv_area_t* area, uint8_t* px_map) {
  if (display == nullptr) return;
  Gateway35DisplayAdapter* adapter =
      static_cast<Gateway35DisplayAdapter*>(lv_display_get_user_data(display));
  if (adapter == nullptr || adapter->_gfx == nullptr || area == nullptr ||
      px_map == nullptr) {
    finishFlush(display);
    return;
  }
  const int32_t width = area->x2 - area->x1 + 1;
  const int32_t height = area->y2 - area->y1 + 1;
  if (width <= 0 || height <= 0) {
    finishFlush(display);
    return;
  }
  adapter->_gfx->draw16bitRGBBitmap(
      static_cast<int16_t>(area->x1), static_cast<int16_t>(area->y1),
      reinterpret_cast<uint16_t*>(px_map), static_cast<int16_t>(width),
      static_cast<int16_t>(height));
  finishFlush(display);
}

bool Gateway35DisplayAdapter::startupProbeTouch(void*, TwoWire& wire) {
  return probeTouch(wire);
}

bool Gateway35DisplayAdapter::startupBeginDisplay(void* context) {
  return static_cast<Gateway35DisplayAdapter*>(context)->beginDisplay();
}

void Gateway35DisplayAdapter::startupFillBlack(void* context) {
  static_cast<Gateway35DisplayAdapter*>(context)->fillBlack();
}

void Gateway35DisplayAdapter::startupSetBacklight(void* context, bool enabled) {
  static_cast<Gateway35DisplayAdapter*>(context)->setBacklight(enabled);
}

void Gateway35DisplayAdapter::startupDelay(void*, uint32_t milliseconds) {
  delay(milliseconds);
}

void Gateway35DisplayAdapter::finishFlush(lv_display_t* display) {
  lv_display_flush_ready(display);
}

bool Gateway35DisplayAdapter::beginDisplay() {
  if (_gfx == nullptr) {
    _bus = new Arduino_ESP32SPI(kLcdDcPin, kLcdCsPin, kSpiSclkPin,
                                kSpiMosiPin, kSpiMisoPin);
    _gfx = new Arduino_ST7796(_bus, kLcdResetPin, 0, true, kDisplayWidth,
                              kDisplayHeight);
  }
  return _gfx != nullptr && _gfx->begin();
}

void Gateway35DisplayAdapter::fillBlack() { clear(kBlack); }

void Gateway35DisplayAdapter::setBacklight(bool enabled) {
  pinMode(kLcdBacklightPin, OUTPUT);
  digitalWrite(kLcdBacklightPin, enabled ? HIGH : LOW);
}

}  // namespace gateway35

#endif  // GATEWAY_35
