/*
MIT License

Copyright (c) 2024 Magnus

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
#ifndef SRC_DISPLAY_HPP_
#define SRC_DISPLAY_HPP_

#if defined(GATEWAY)

#include <SPI.h>

#include <config_gateway.hpp>
#include <cuckoo_pi_model.hpp>
#include <main.hpp>

#if defined(ENABLE_TFT)
#include <gateway_orientation.hpp>
#if defined(GATEWAY_35)
#include <gateway35_display.hpp>
#include <qmi8658/Qmi8658.hpp>
#else
#if defined(WAVESHARE_CST3530)
#include <qmi8658/Qmi8658.hpp>
#endif
#include <TFT_eSPI.h>
#endif
#include <freertos/semphr.h>
#include <lvgl.h>

#include <ui_helpers.hpp>
#endif

enum FontSize { FONT_9 = 9, FONT_12 = 12, FONT_18 = 18, FONT_24 = 24 };

class Display {
 private:
#if defined(ENABLE_TFT)
#if defined(GATEWAY_35)
  gateway35::Gateway35DisplayAdapter* _gateway35 = nullptr;
#else
  TFT_eSPI* _tft = nullptr;
#endif
#if defined(GATEWAY_35) || defined(WAVESHARE_CST3530)
  Qmi8658 _orientationSensor;
  GatewayOrientationDebouncer _orientationDebouncer{GATEWAY_DEFAULT_ORIENTATION};
  uint32_t _lastOrientationPollMs = 0;
  bool _orientationSensorReady = false;
  bool _orientationInitLogged = false;
  bool _orientationReadLogged = false;
#endif
  lv_display_t* _display = nullptr;
  uint32_t _backgroundColor = 0;
  uint16_t _touchCalibrationlData[5] = {0, 0, 0, 0, 0};
  SemaphoreHandle_t _uiSemaphore = nullptr;
  GatewayOrientation _orientation = GATEWAY_DEFAULT_ORIENTATION;
  uint8_t _layoutId = 0;
  uint16_t _lastTouchRawX = 0;
  uint16_t _lastTouchRawY = 0;
  uint16_t _lastTouchTransformedX = 0;
  uint16_t _lastTouchTransformedY = 0;
  bool applyDisplayRotation(GatewayOrientation orientation);
#endif
  FontSize _fontSize = FontSize::FONT_9;
  // Rotation _rotation = ROTATION_90;

 public:
  Display();
  void setup();
  void createUI();
  void calibrateTouch();
  void pollOrientation();

  SPIClass& getSPI() {
#if defined(GATEWAY_35)
    return SPI;
#elif defined(ENABLE_TFT)
    return _tft->getSPIinstance();
#else
    return SPI;
#endif
  }

  void clear(uint32_t color);
  void setFont(FontSize f);
  void printLine(int l, const String& text);
  void printLineCentered(int l, const String& text);
  // Rotation getRotation() { return _rotation; }
  // void setRotation(Rotation rotation);

  void createUI(uint8_t layoutId = 0);

  // Composite setters for device data
  void updateEmpty();
  void updateGravity(const char* deviceId, const char* name, uint8_t index, uint8_t maxIndex,
                     const char* type, const char* source,
                     const char* timestamp, float gravity, char gravityUnit,
                     float temp, char tempUnit, float batteryVoltage,
                     float batteryPercentage, int rssi, bool online);
  void updatePressure(const char* deviceId, const char* name, uint8_t index, uint8_t maxIndex,
                      const char* type, const char* source,
                      const char* timestamp, float pressure, float pressure2,
                      const char* pressureUnit, float temp, char tempUnit,
                      float batteryVoltage, float batteryPercentage, int rssi);
  void updateTemperature(const char* deviceId, const char* name, uint8_t index, uint8_t maxIndex,
                         const char* type, const char* source,
                         const char* timestamp, float temp, float temp2,
                         char tempUnit, int rssi);
  void updateCuckooPi(const CuckooPiSnapshot& snapshot, uint8_t index,
                      uint8_t maxIndex, bool online);
  void updateHistory(const char* history, int idx);
  void updateStatus(const char* status);
  void updateNetworkStatus(const char* ip, int wifiRssi);
  void updateDarkmode(bool darkmode);
  void setLayout(uint8_t layoutId);
  bool isSettingsOpen() const;

#if defined(ENABLE_TFT)
  GatewayOrientation orientation() const { return _orientation; }
  uint16_t lastTouchRawX() const { return _lastTouchRawX; }
  uint16_t lastTouchRawY() const { return _lastTouchRawY; }
  uint16_t lastTouchTransformedX() const { return _lastTouchTransformedX; }
  uint16_t lastTouchTransformedY() const { return _lastTouchTransformedY; }
#endif

  // LVGL methods
  bool getTouch(uint16_t* x, uint16_t* y);  // Check for touch callback
  void handleGestureEventEvent(char gesture);
  SemaphoreHandle_t getUISemaphore() {
#if defined(ENABLE_TFT)
    return _uiSemaphore;
#else
    return nullptr;
#endif
  }
};

// LVGL handlers and utilities
#if defined(ENABLE_TFT)
void updateLabel(lv_obj_t* obj, const char* label);
void touchScreenHandler(lv_indev_t* indev, lv_indev_data_t* data);
void gestureScreenHandler(lv_event_t* e);
void log_print(lv_log_level_t level, const char* buf);
void lvgl_loop_handler(void* parameter);
void btnLeftEventHandler(lv_event_t* e);
void btnRightEventHandler(lv_event_t* e);
void gestureLeft();
void gestureRight();
#endif
extern Display myDisplay;

#endif  // GATEWAY

#endif  // SRC_DISPLAY_HPP_

// EOF
