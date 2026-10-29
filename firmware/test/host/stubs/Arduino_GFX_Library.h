#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

constexpr uint8_t OUTPUT = 1;
constexpr uint8_t LOW = 0;
constexpr uint8_t HIGH = 1;

struct FakeArduinoGfxState {
  struct Rect {
    int16_t x;
    int16_t y;
    int16_t width;
    int16_t height;
  };

  bool ps_malloc_fails = false;
  int ps_malloc_calls = 0;
  int free_calls = 0;
  bool gfx_begin_result = true;
  int gfx_begin_calls = 0;
  int fill_screen_calls = 0;
  int fill_rect_calls = 0;
  std::vector<Rect> fill_rects;
  int set_cursor_calls = 0;
  int16_t last_cursor_x = 0;
  int16_t last_cursor_y = 0;
  int print_calls = 0;
  std::string last_printed_text;
  int get_text_bounds_calls = 0;
  uint16_t text_bounds_width = 60;
  uint16_t text_bounds_height = 12;
  int esp32spi_construct_calls = 0;
  int8_t esp32spi_dc = 0;
  int8_t esp32spi_cs = 0;
  int8_t esp32spi_sclk = 0;
  int8_t esp32spi_mosi = 0;
  int8_t esp32spi_miso = 0;
  int st7796_construct_calls = 0;
  int8_t st7796_reset = 0;
  uint8_t st7796_rotation = 0;
  bool st7796_ips = false;
  int16_t st7796_width = 0;
  int16_t st7796_height = 0;
  int set_rotation_calls = 0;
  uint8_t last_rotation = 0;
  int draw_calls = 0;
  int last_draw_x = 0;
  int last_draw_y = 0;
  int last_draw_width = 0;
  int last_draw_height = 0;
  uint16_t* last_draw_pixels = nullptr;
  int pin_mode_calls = 0;
  std::vector<int> digital_values;
  bool (*before_gfx_begin)() = nullptr;
  bool (*before_backlight_on)() = nullptr;
  bool ordering_ok = true;
};

inline FakeArduinoGfxState g_fake_arduino_gfx;

inline void fakeArduinoGfxReset() { g_fake_arduino_gfx = FakeArduinoGfxState{}; }

inline void* ps_malloc(size_t bytes) {
  ++g_fake_arduino_gfx.ps_malloc_calls;
  if (g_fake_arduino_gfx.ps_malloc_fails) return nullptr;
  return new uint8_t[bytes];
}

inline void free(void* memory) {
  ++g_fake_arduino_gfx.free_calls;
  delete[] static_cast<uint8_t*>(memory);
}

inline void pinMode(int, uint8_t) { ++g_fake_arduino_gfx.pin_mode_calls; }

inline void digitalWrite(int, uint8_t value) {
  if (value == HIGH && g_fake_arduino_gfx.before_backlight_on != nullptr &&
      !g_fake_arduino_gfx.before_backlight_on()) {
    g_fake_arduino_gfx.ordering_ok = false;
  }
  g_fake_arduino_gfx.digital_values.push_back(value);
}

inline void delay(uint32_t) {}

class Arduino_DataBus {
 public:
  virtual ~Arduino_DataBus() = default;
};

class Arduino_ESP32SPI : public Arduino_DataBus {
 public:
  Arduino_ESP32SPI(int8_t dc, int8_t cs, int8_t sclk, int8_t mosi,
                   int8_t miso)
      : dc(dc), cs(cs), sclk(sclk), mosi(mosi), miso(miso) {
    ++g_fake_arduino_gfx.esp32spi_construct_calls;
    g_fake_arduino_gfx.esp32spi_dc = dc;
    g_fake_arduino_gfx.esp32spi_cs = cs;
    g_fake_arduino_gfx.esp32spi_sclk = sclk;
    g_fake_arduino_gfx.esp32spi_mosi = mosi;
    g_fake_arduino_gfx.esp32spi_miso = miso;
  }

  int8_t dc;
  int8_t cs;
  int8_t sclk;
  int8_t mosi;
  int8_t miso;
};

class Arduino_GFX {
 public:
  virtual ~Arduino_GFX() = default;

  virtual bool begin(int32_t = 0) {
    ++g_fake_arduino_gfx.gfx_begin_calls;
    if (g_fake_arduino_gfx.before_gfx_begin != nullptr &&
        !g_fake_arduino_gfx.before_gfx_begin()) {
      g_fake_arduino_gfx.ordering_ok = false;
    }
    return g_fake_arduino_gfx.gfx_begin_result;
  }

  virtual void fillScreen(uint16_t) { ++g_fake_arduino_gfx.fill_screen_calls; }
  virtual void fillRect(int16_t x, int16_t y, int16_t width, int16_t height,
                        uint16_t) {
    ++g_fake_arduino_gfx.fill_rect_calls;
    g_fake_arduino_gfx.fill_rects.push_back({x, y, width, height});
  }
  virtual void setTextSize(uint8_t) {}
  virtual void setTextColor(uint16_t) {}
  virtual void setRotation(uint8_t rotation) {
    ++g_fake_arduino_gfx.set_rotation_calls;
    g_fake_arduino_gfx.last_rotation = rotation;
  }
  virtual void setCursor(int16_t x, int16_t y) {
    ++g_fake_arduino_gfx.set_cursor_calls;
    g_fake_arduino_gfx.last_cursor_x = x;
    g_fake_arduino_gfx.last_cursor_y = y;
  }
  virtual size_t print(const char* text) {
    ++g_fake_arduino_gfx.print_calls;
    g_fake_arduino_gfx.last_printed_text = text == nullptr ? "" : text;
    return g_fake_arduino_gfx.last_printed_text.size();
  }
  virtual void getTextBounds(const char*, int16_t x, int16_t y, int16_t* x1,
                             int16_t* y1, uint16_t* width,
                             uint16_t* height) {
    ++g_fake_arduino_gfx.get_text_bounds_calls;
    *x1 = x;
    *y1 = y;
    *width = g_fake_arduino_gfx.text_bounds_width;
    *height = g_fake_arduino_gfx.text_bounds_height;
  }

  virtual void draw16bitRGBBitmap(int16_t x, int16_t y, uint16_t* pixels,
                                  int16_t width, int16_t height) {
    ++g_fake_arduino_gfx.draw_calls;
    g_fake_arduino_gfx.last_draw_x = x;
    g_fake_arduino_gfx.last_draw_y = y;
    g_fake_arduino_gfx.last_draw_width = width;
    g_fake_arduino_gfx.last_draw_height = height;
    g_fake_arduino_gfx.last_draw_pixels = pixels;
  }
};

class Arduino_ST7796 : public Arduino_GFX {
 public:
  Arduino_ST7796(Arduino_DataBus* bus, int8_t reset, uint8_t rotation,
                 bool ips, int16_t width, int16_t height)
      : bus(bus), reset(reset), rotation(rotation), ips(ips), width(width),
        height(height) {
    ++g_fake_arduino_gfx.st7796_construct_calls;
    g_fake_arduino_gfx.st7796_reset = reset;
    g_fake_arduino_gfx.st7796_rotation = rotation;
    g_fake_arduino_gfx.st7796_ips = ips;
    g_fake_arduino_gfx.st7796_width = width;
    g_fake_arduino_gfx.st7796_height = height;
  }

  Arduino_DataBus* bus;
  int8_t reset;
  uint8_t rotation;
  bool ips;
  int16_t width;
  int16_t height;
};
