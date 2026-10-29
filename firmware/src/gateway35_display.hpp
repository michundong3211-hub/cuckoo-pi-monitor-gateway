#pragma once

#include <stddef.h>
#include <stdint.h>

#include <lvgl.h>

class Arduino_DataBus;
class Arduino_GFX;
class TwoWire;

namespace gateway35 {

class Gateway35DisplayAdapter {
 public:
  Gateway35DisplayAdapter();
  ~Gateway35DisplayAdapter();

  bool begin();
  lv_display_t* createLvglDisplay();
  bool setRotation(uint8_t rotation);
  void clear(uint16_t rgb565);
  void setFontPixels(uint8_t pixels);
  void printLine(uint8_t line, const char* text, bool centered);
  bool getTouch(uint16_t* x, uint16_t* y);
  static void flush(lv_display_t* display, const lv_area_t* area,
                    uint8_t* px_map);

 private:
  static bool startupProbeTouch(void* context, TwoWire& wire);
  static bool startupBeginDisplay(void* context);
  static void startupFillBlack(void* context);
  static void startupSetBacklight(void* context, bool enabled);
  static void startupDelay(void* context, uint32_t milliseconds);
  static void finishFlush(lv_display_t* display);

  bool beginDisplay();
  void fillBlack();
  void setBacklight(bool enabled);

  Arduino_DataBus* _bus = nullptr;
  Arduino_GFX* _gfx = nullptr;
  uint8_t* _drawBuffer = nullptr;
  size_t _drawBufferBytes = 0;
  lv_display_t* _display = nullptr;
  bool _ready = false;
};

}  // namespace gateway35
