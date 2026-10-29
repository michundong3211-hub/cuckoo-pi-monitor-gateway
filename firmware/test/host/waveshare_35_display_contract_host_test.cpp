#include <fstream>
#include <string>

namespace {
int failures = 0;

void expect(bool condition) {
  if (!condition) ++failures;
}

std::string readFile(const char* path) {
  std::ifstream input(path);
  if (!input) {
    ++failures;
    return {};
  }
  return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

void require(const std::string& source, const char* token) {
  expect(source.find(token) != std::string::npos);
}

void forbid(const std::string& source, const char* token) {
  expect(source.find(token) == std::string::npos);
}
}  // namespace

int main() {
  const std::string environment = readFile("platformio.ini");
  const std::string adapter_header = readFile("src/gateway35_display.hpp");
  const std::string adapter_source = readFile("src/gateway35_display.cpp");
  const std::string display_header = readFile("src/display.hpp");
  const std::string display_source = readFile("src/display.cpp");
  const std::string main_source = readFile("src/main_gateway.cpp");

  require(environment, "[env:gateway35s3wave-tft-sd]");
  require(environment, "Arduino_GFX.git#v1.5.5");
  require(adapter_header, "class Gateway35DisplayAdapter");
  require(adapter_source, "Arduino_ESP32SPI");
  require(adapter_source, "Arduino_ST7796");
  require(adapter_source, "kSpiMosiPin");
  require(adapter_source, "kSpiMisoPin");
  require(adapter_source, "kSpiSclkPin");
  require(adapter_source, "kLcdDcPin");
  require(adapter_source, "kLcdCsPin");
  require(adapter_source, "kLcdResetPin");
  require(adapter_source, "kLcdResetPin, 0, true");
  require(adapter_source, "kDisplayWidth");
  require(adapter_source, "kDisplayHeight");
  require(adapter_source, "lv_display_create(kDisplayWidth, kDisplayHeight)");
  require(adapter_source, "LV_COLOR_FORMAT_RGB565");
  require(adapter_source, "LV_DISPLAY_RENDER_MODE_PARTIAL");
  require(adapter_source, "draw16bitRGBBitmap");
  require(adapter_source, "finishFlush");
  require(adapter_source, "lv_display_flush_ready(display)");
  require(adapter_source, "initializePanelHardware(Wire, hooks)");
  require(adapter_source, "probeTouch");

  require(display_header, "#if defined(GATEWAY_35)");
  require(display_header, "Gateway35DisplayAdapter");
  require(display_header, "return SPI;");
  require(display_header, "return _tft->getSPIinstance();");
  require(display_source, "#if defined(GATEWAY_35)");
  require(display_source, "beginOnInitializedBus(Wire)");
  require(display_source, "_gateway35->setRotation");
  require(display_source,
          "gateway35NativeTouchPoint(raw_x, raw_y, _orientation)");
  require(display_source, "gateway35_display");
  forbid(adapter_source, "lv_tft_espi_create");
  forbid(adapter_source, "QMI8658");
  forbid(adapter_source, "CST3530");
  require(adapter_source, "setRotation");
  require(display_source, "lv_tft_espi_create");
  require(display_source, "QMI8658");
  require(display_source, "CST3530");
  require(display_source, "_tft->setRotation");
  require(main_source, "myDisplay.setFont(FontSize::FONT_18);");
  return failures == 0 ? 0 : 1;
}
