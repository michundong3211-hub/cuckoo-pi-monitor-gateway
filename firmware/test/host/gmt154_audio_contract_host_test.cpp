#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {

int failures = 0;

std::string readFile(const char* path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) return {};
  std::ostringstream contents;
  contents << input.rdbuf();
  return contents.str();
}

void require(const std::string& source, const char* needle,
             const char* description) {
  if (source.find(needle) == std::string::npos) {
    std::cerr << "FAIL: " << description << " (missing '" << needle << "')\n";
    ++failures;
  }
}

void reject(const std::string& source, const char* needle,
            const char* description) {
  if (source.find(needle) != std::string::npos) {
    std::cerr << "FAIL: " << description << " (unexpected '" << needle
              << "')\n";
    ++failures;
  }
}

void requireOrdered(const std::string& source,
                    const char* const* needles, size_t count,
                    const char* description) {
  size_t cursor = 0;
  for (size_t i = 0; i < count; ++i) {
    const size_t found = source.find(needles[i], cursor);
    if (found == std::string::npos) {
      std::cerr << "FAIL: " << description << " (missing/out of order '"
                << needles[i] << "')\n";
      ++failures;
      return;
    }
    cursor = found + std::char_traits<char>::length(needles[i]);
  }
}

std::string bracedBlockAfter(const std::string& source, const char* marker) {
  const size_t marker_start = source.find(marker);
  if (marker_start == std::string::npos) return {};
  const size_t open = source.find('{', marker_start);
  if (open == std::string::npos) return {};
  size_t depth = 0;
  for (size_t cursor = open; cursor < source.size(); ++cursor) {
    if (source[cursor] == '{') {
      ++depth;
    } else if (source[cursor] == '}' && --depth == 0) {
      return source.substr(open, cursor - open + 1);
    }
  }
  return {};
}

std::string sectionFrom(const std::string& source, const char* header) {
  size_t start = source.find(header);
  while (start != std::string::npos && start != 0 && source[start - 1] != '\n') {
    start = source.find(header, start + std::char_traits<char>::length(header));
  }
  if (start == std::string::npos) return {};
  const size_t end = source.find("\n[env:", start + std::char_traits<char>::length(header));
  return source.substr(start, end == std::string::npos ? end : end - start);
}

void requireSingleOccurrence(const std::string& source, const char* needle,
                             const char* description) {
  const size_t first = source.find(needle);
  if (first == std::string::npos ||
      source.find(needle, first + std::char_traits<char>::length(needle)) !=
          std::string::npos) {
    std::cerr << "FAIL: " << description << " (expected exactly one '"
              << needle << "')\n";
    ++failures;
  }
}

}  // namespace

int main() {
  const std::string platform = readFile("platformio.ini");
  const std::string profile = readFile("src/gmt154_audio_profile.hpp");
  const std::string codecHeader = readFile("src/gmt154_audio_codec.hpp");
  const std::string codec = readFile("src/gmt154_audio_codec.cpp");
  const std::string codecDriver = readFile("lib/es8311/es8311.cpp");
  const std::string audioHeader = readFile("src/gateway_audio.hpp");
  const std::string audio = readFile("src/gateway_audio.cpp");
  const std::string mainGateway = readFile("src/main_gateway.cpp");
  const std::string uiGateway = readFile("src/ui_gravitymon_gateway.cpp");

  require(platform, "[env:gmt154-06]", "GMT154 target environment exists");
  const std::string env = sectionFrom(platform, "[env:gmt154-06]");
  const std::string sharedEnv = sectionFrom(platform, "[gmt154_env_data]");
  require(env, "${gmt154_env_data.build_flags}",
          "GMT154 target uses the shared feature flags");
  require(sharedEnv, "-D ENABLE_GATEWAY_AUDIO=1",
          "GMT154 shared target flags enable gateway audio");
  reject(env, "GMT154_AUDIO_STD_INIT_DIAG",
         "normal GMT154 target remains outside C2b diagnostics");

  const std::string c2bEnv = sectionFrom(platform, "[env:gmt154-06-c2b-diag]");
  require(c2bEnv, "[env:gmt154-06-c2b-diag]",
          "GMT154 C2b has a dedicated reproducible PlatformIO environment");
  require(c2bEnv, "extends = env:gmt154-06",
          "C2b inherits the complete production GMT154 environment");
  require(c2bEnv, "-D GMT154_AUDIO_STD_INIT_DIAG=1",
          "C2b environment selects only the standard-mode diagnostic");
  require(c2bEnv, "pre:script/board.py",
          "C2b retains board setup patch");
  require(c2bEnv, "pre:script/patch_tft_espi_gmt154.py",
          "C2b retains TFT_eSPI no-MISO patch baseline");
  require(c2bEnv, "pre:script/patch_espframework_gmt154_logging.py",
          "C2b retains espframework logging guard baseline");
  require(c2bEnv, "firmware-gmt154-06-c2b-diag.bin",
          "C2b firmware artifact has a unique runtime-visible filename");
  reject(c2bEnv, "-D GMT154_AUDIO_HW_INIT_BYPASS=1",
         "C2b does not bypass the allocated-only baseline");

  const std::string c2cEnv = sectionFrom(platform, "[env:gmt154-06-c2c-diag]");
  require(c2cEnv, "[env:gmt154-06-c2c-diag]",
          "GMT154 C2c has a dedicated reproducible PlatformIO environment");
  require(c2cEnv, "extends = env:gmt154-06",
          "C2c inherits the complete production GMT154 environment");
  require(c2cEnv, "-D GMT154_AUDIO_STD_ENABLE_DIAG=1",
          "C2c selects only the standard-mode enable diagnostic");
  require(c2cEnv, "pre:script/board.py",
          "C2c retains board setup patch");
  require(c2cEnv, "pre:script/patch_tft_espi_gmt154.py",
          "C2c retains TFT_eSPI no-MISO patch baseline");
  require(c2cEnv, "pre:script/patch_espframework_gmt154_logging.py",
          "C2c retains espframework logging guard baseline");
  require(c2cEnv, "firmware-gmt154-06-c2c-diag.bin",
          "C2c firmware artifact has a unique runtime-visible filename");

  require(profile, "kAudioDoutPin = 4", "GMT154 I2S DOUT pin");
  require(profile, "kAudioWsPin = 7", "GMT154 I2S WS pin");
  require(profile, "kAudioBclkPin = 16", "GMT154 I2S BCLK pin");
  require(profile, "kAudioMclkPin = 38", "GMT154 I2S MCLK pin");
  require(profile, "kAudioSdaPin = 40", "GMT154 codec I2C SDA pin");
  require(profile, "kAudioSclPin = 39", "GMT154 codec I2C SCL pin");
  require(profile, "kAudioCodecI2cAddress = 0x18",
          "GMT154 ES8311 seven-bit address");
  require(profile, "kAmplifierControlPin = 8",
          "GMT154 NS4150B control pin");
  require(profile, "kAudioSampleRateHz = 16000",
          "GMT154 selected sample rate");
  require(profile, "kAudioMclkFrequencyHz = 4096000",
          "GMT154 selected MCLK frequency");
  require(profile, "kAudioSampleRateHz * 256",
          "GMT154 MCLK is 256 times the sample rate");

  require(codecHeader, "class Gmt154AudioCodec",
          "GMT154 has a target-specific ES8311 codec adapter");
  require(codec, "es8311_create(I2C_NUM_0, gmt154::kAudioCodecI2cAddress)",
          "GMT154 codec uses the approved I2C address");
  require(codec, "gmt154::kAudioMclkFrequencyHz",
          "GMT154 codec receives the selected MCLK");
  require(codec, "gmt154::kAudioSampleRateHz",
          "GMT154 codec receives the selected sample rate");
  require(codec, "ES8311_RESOLUTION_16", "GMT154 codec uses 16-bit PCM");
  const char* codecInitOrder[] = {
      "Wire.begin(gmt154::kAudioSdaPin, gmt154::kAudioSclPin",
      "es8311_create(I2C_NUM_0, gmt154::kAudioCodecI2cAddress)",
      "es8311_init(",
  };
  requireOrdered(codec, codecInitOrder,
                 sizeof(codecInitOrder) / sizeof(codecInitOrder[0]),
                 "GMT154 initializes its I2C bus before the codec");
  require(codecDriver, "{4096000, 16000,",
          "bundled ES8311 driver supports the selected clock pair");

  require(audioHeader, "Gmt154AudioCodec _gmt154_codec",
          "GatewayAudio owns the GMT154 codec adapter");
  require(audio, "std_config.gpio_cfg.dout", "GMT154 I2S TX output configured");
  require(audio, "gmt154::kAudioDoutPin", "GMT154 I2S DOUT maps to GPIO4");
  require(audio, "gmt154::kAudioWsPin", "GMT154 I2S WS maps to GPIO7");
  require(audio, "gmt154::kAudioBclkPin", "GMT154 I2S BCLK maps to GPIO16");
  require(audio, "gmt154::kAudioMclkPin", "GMT154 I2S MCLK maps to GPIO38");
  require(audio, "mclk_multiple = I2S_MCLK_MULTIPLE_256",
          "GMT154 I2S MCLK is configured as 256 times Fs");
  require(audio, "std_config.gpio_cfg.din = I2S_GPIO_UNUSED",
          "GMT154 configures playback only");
  reject(audio, "GPIO_NUM_15", "GMT154 does not configure the mic pin");

  const char* startupOrder[] = {
      "initializeGmt154AmplifierDisabled())",
      "i2s_new_channel(",
      "const bool i2s_ready =",
      "if (!i2s_ready)",
      "const bool codec_ready = _gmt154_codec.begin();",
      "if (!codec_ready)",
      "setGmt154PowerAmplifierEnabled(true)",
      "xTaskCreatePinnedToCore(",
  };
  requireOrdered(audio, startupOrder,
                 sizeof(startupOrder) / sizeof(startupOrder[0]),
                 "GMT154 startup keeps the amplifier off until codec and I2S are ready");

  require(audio, "AUD : Unable to initialize GMT154 ES8311 I2S output.",
          "GMT154 I2S initialization failure identifies its ES8311 target");
  const char* i2sFailureMessageOrder[] = {
      "if (!i2s_ready) {",
      "#if defined(GATEWAY_35)",
      "AUD : Unable to initialize ES8311 I2S output.",
      "#elif defined(GMT154_06)",
      "AUD : Unable to initialize GMT154 ES8311 I2S output.",
      "#else",
      "AUD : Unable to initialize PCM5101 I2S output.",
      "#endif",
  };
  requireOrdered(audio, i2sFailureMessageOrder,
                 sizeof(i2sFailureMessageOrder) /
                     sizeof(i2sFailureMessageOrder[0]),
                 "GMT154 selects its ES8311 I2S error before the PCM5101 fallback");

  require(audio, "setGmt154PowerAmplifierEnabled(false)",
          "GMT154 teardown disables the amplifier on startup failure");
  require(audio, "releaseRuntime();",
          "GMT154 startup failures release audio resources");

  require(mainGateway, "const bool offlineActive = evaluation.offline_active;",
          "GMT154 offline alarm ignores the dismiss flag");
  require(uiGateway, "offline_alarm_active || beer_deviation_alarm_active",
          "GMT154 device ID reflects either active device alarm");
  require(uiGateway, "device_alarm_active ? lv_color_hex(0xFF5D62)",
          "GMT154 device ID turns red while an alarm is active");
  const char* noPopupOrder[] = {
      "static void gravitymon_gateway_update_alert_ui() {",
      "#if defined(GMT154_06)",
      "gravitymon_gateway_destroy_alert_ui();",
      "return;",
  };
  requireOrdered(uiGateway, noPopupOrder,
                 sizeof(noPopupOrder) / sizeof(noPopupOrder[0]),
                 "GMT154 never displays an alert popup");
  require(uiGateway, "gravitymon_gateway_create_alert_ui(event);",
          "non-GMT154 targets retain the existing popup creation path");

  const char* amplifierSafeOrder[] = {
      "esp_err_t initializeGmt154AmplifierDisabled(const char** failure_stage) {",
      "*failure_stage = \"gpio_set_level(preload-disabled)\";",
      "err = gpio_set_level(pin, gmt154::kAmplifierDisabledLevel);",
      "err = gpio_set_direction(pin, GPIO_MODE_OUTPUT);",
      "(void)gpio_set_level(pin, gmt154::kAmplifierDisabledLevel);",
      "*failure_stage = \"gpio_set_level(confirm-disabled)\";",
      "return gpio_set_level(pin, gmt154::kAmplifierDisabledLevel);",
  };
  requireOrdered(audio, amplifierSafeOrder,
                 sizeof(amplifierSafeOrder) / sizeof(amplifierSafeOrder[0]),
                 "GMT154 preloads amp control low before output mode and reasserts low");

  const char* retryStartupOrder[] = {
      "void GatewayAudio::begin() {",
      "if (_started) return;",
      "if (!initializeGmt154AmplifierDisabled())",
      "if (!i2s_ready)",
      "if (!codec_ready)",
      "setGmt154PowerAmplifierEnabled(true)",
      "if (!i2s_ready || !codec_ready || !amplifier_ready || !task_ready)",
      "releaseRuntime();",
      "return;",
      "_available = true;",
      "_started = true;",
  };
  requireOrdered(audio, retryStartupOrder,
                 sizeof(retryStartupOrder) / sizeof(retryStartupOrder[0]),
                 "GMT154 remains retryable after failed startup and marks started only after success");
  require(audio,
          "#error \"GMT154 audio diagnostic modes are mutually exclusive.\"",
          "GMT154 full-bypass and GPIO-only diagnostics cannot both be enabled");
  const size_t beginOffset = audio.find("void GatewayAudio::begin() {");
  const size_t gpioOnlyFlagOffset = audio.find(
      "defined(GMT154_AUDIO_GPIO_ONLY_DIAG) && GMT154_AUDIO_GPIO_ONLY_DIAG",
      beginOffset);
  if (gpioOnlyFlagOffset == std::string::npos) {
    std::cerr << "FAIL: GMT154 GPIO-only diagnostic branch exists\n";
    ++failures;
  } else {
    const size_t branchStart = audio.rfind("#if", gpioOnlyFlagOffset);
    const size_t branchEnd = audio.find("#endif", gpioOnlyFlagOffset);
    const std::string gpioOnlyBranch =
        branchStart == std::string::npos || branchEnd == std::string::npos
            ? std::string()
            : audio.substr(branchStart, branchEnd - branchStart);
    const char* gpioOnlyOrder[] = {
        "defined(GMT154_06)",
        "defined(ARDUINO_ARCH_ESP32)",
        "defined(ENABLE_GATEWAY_AUDIO)",
        "const bool amplifier_disabled = initializeGmt154AmplifierDisabled();",
        "amplifier_disabled ? \"ok\" : \"failed\"",
        "_started = true;",
        "return;",
    };
    requireOrdered(gpioOnlyBranch, gpioOnlyOrder,
                   sizeof(gpioOnlyOrder) / sizeof(gpioOnlyOrder[0]),
                   "GPIO-only diagnostic reports one safe-init attempt and returns");
    reject(gpioOnlyBranch, "releaseRuntime()",
           "GPIO-only diagnostic does not run audio teardown");
    reject(gpioOnlyBranch, "i2s_",
           "GPIO-only diagnostic does not initialize I2S");
    reject(gpioOnlyBranch, "_gmt154_codec",
           "GPIO-only diagnostic does not initialize the codec");
    reject(gpioOnlyBranch, "setGmt154PowerAmplifierEnabled(true)",
           "GPIO-only diagnostic never enables the amplifier");
    reject(gpioOnlyBranch, "xTaskCreatePinnedToCore",
           "GPIO-only diagnostic does not create an audio task");
  }
  const char* hardwareBypassOrder[] = {
      "void GatewayAudio::begin() {",
      "defined(GMT154_06) && defined(ARDUINO_ARCH_ESP32)",
      "defined(GMT154_AUDIO_HW_INIT_BYPASS) && GMT154_AUDIO_HW_INIT_BYPASS",
      "_started = true;",
      "AUD : GMT154 audio hardware initialization bypassed.",
      "return;",
      "#endif",
      "initializeGmt154AmplifierDisabled()",
  };
  requireOrdered(audio, hardwareBypassOrder,
                 sizeof(hardwareBypassOrder) /
                     sizeof(hardwareBypassOrder[0]),
                 "GMT154 diagnostic bypass exits before audio hardware initialization");

  require(audio,
          "defined(GMT154_AUDIO_ALLOC_ONLY_DIAG) && GMT154_AUDIO_ALLOC_ONLY_DIAG",
          "GMT154 allocation-only diagnostic mode exists");
  require(audio,
          "defined(GMT154_AUDIO_STD_INIT_DIAG) && GMT154_AUDIO_STD_INIT_DIAG",
          "GMT154 C2b standard-mode diagnostic exists");
  require(audio,
          "defined(GMT154_AUDIO_STD_ENABLE_DIAG) && GMT154_AUDIO_STD_ENABLE_DIAG",
          "GMT154 C2c standard-mode enable diagnostic exists");
  require(audio,
          "(defined(GMT154_AUDIO_STD_INIT_DIAG) && GMT154_AUDIO_STD_INIT_DIAG && defined(GMT154_AUDIO_ALLOC_ONLY_DIAG) && GMT154_AUDIO_ALLOC_ONLY_DIAG)",
          "C2b and C2a diagnostic modes cannot be enabled together");
  require(audio,
          "(defined(GMT154_AUDIO_STD_ENABLE_DIAG) && GMT154_AUDIO_STD_ENABLE_DIAG && defined(GMT154_AUDIO_STD_INIT_DIAG) && GMT154_AUDIO_STD_INIT_DIAG)",
          "C2c and C2b diagnostic modes cannot be enabled together");
  require(audio,
          "(defined(GMT154_AUDIO_STD_ENABLE_DIAG) && GMT154_AUDIO_STD_ENABLE_DIAG && defined(GMT154_AUDIO_HW_INIT_BYPASS) && GMT154_AUDIO_HW_INIT_BYPASS)",
          "C2c and full-bypass diagnostic modes cannot be enabled together");
  require(audio,
          "(defined(GMT154_AUDIO_STD_ENABLE_DIAG) && GMT154_AUDIO_STD_ENABLE_DIAG && defined(GMT154_AUDIO_GPIO_ONLY_DIAG) && GMT154_AUDIO_GPIO_ONLY_DIAG)",
          "C2c and GPIO-only diagnostic modes cannot be enabled together");
  require(audio,
          "(defined(GMT154_AUDIO_STD_ENABLE_DIAG) && GMT154_AUDIO_STD_ENABLE_DIAG && defined(GMT154_AUDIO_ALLOC_ONLY_DIAG) && GMT154_AUDIO_ALLOC_ONLY_DIAG)",
          "C2c and allocation-only diagnostic modes cannot be enabled together");
  require(audio,
          "(defined(GMT154_AUDIO_HW_INIT_BYPASS) && GMT154_AUDIO_HW_INIT_BYPASS && defined(GMT154_AUDIO_ALLOC_ONLY_DIAG) && GMT154_AUDIO_ALLOC_ONLY_DIAG)",
          "GMT154 full-bypass and allocation-only diagnostics cannot both be enabled");
  require(audio,
          "(defined(GMT154_AUDIO_GPIO_ONLY_DIAG) && GMT154_AUDIO_GPIO_ONLY_DIAG && defined(GMT154_AUDIO_ALLOC_ONLY_DIAG) && GMT154_AUDIO_ALLOC_ONLY_DIAG)",
          "GMT154 GPIO-only and allocation-only diagnostics cannot both be enabled");
  require(audio,
          "esp_err_t initializeGmt154AmplifierDisabled(const char** failure_stage)",
          "GMT154 amplifier-disable helper exposes the failing stage and ESP error");
  require(audio, "gpio_set_level(preload-disabled)",
          "GMT154 amplifier-disable diagnostics identify the preload GPIO call");
  require(audio, "gpio_set_direction(output)",
          "GMT154 amplifier-disable diagnostics identify the direction GPIO call");
  require(audio, "gpio_set_level(confirm-disabled)",
          "GMT154 amplifier-disable diagnostics identify the final GPIO call");

  const size_t allocOnlyFlagOffset = audio.find(
      "defined(GMT154_AUDIO_ALLOC_ONLY_DIAG) && GMT154_AUDIO_ALLOC_ONLY_DIAG",
      beginOffset);
  if (allocOnlyFlagOffset == std::string::npos) {
    std::cerr << "FAIL: GMT154 allocation-only diagnostic branch exists\n";
    ++failures;
  } else {
    const size_t branchStart = audio.rfind("#if", allocOnlyFlagOffset);
    const size_t branchEnd = audio.find("#endif", allocOnlyFlagOffset);
    const std::string allocOnlyBranch =
        branchStart == std::string::npos || branchEnd == std::string::npos
            ? std::string()
            : audio.substr(branchStart, branchEnd - branchStart);
    const char* allocOnlyOrder[] = {
        "defined(GMT154_06)",
        "defined(ARDUINO_ARCH_ESP32)",
        "defined(ENABLE_GATEWAY_AUDIO)",
        "_started = true;",
        "_available = false;",
        "initializeGmt154AmplifierDisabled(&failure_stage)",
        "if (disable_err != ESP_OK) {",
        "stage=%s esp_err_t=%d; unavailable.",
        "return;",
        "const i2s_chan_config_t diagnostic_channel_config =",
        "i2s_new_channel(&diagnostic_channel_config, &_tx_handle, nullptr)",
        "if (alloc_err != ESP_OK) {",
        "stage=i2s_new_channel esp_err_t=%d; unavailable.",
        "return;",
        "allocated-only; clocks-not-started; codec skipped; amp enable skipped; task skipped; unavailable.",
        "return;",
    };
    requireOrdered(allocOnlyBranch, allocOnlyOrder,
                   sizeof(allocOnlyOrder) / sizeof(allocOnlyOrder[0]),
                   "allocation-only mode disables the amplifier, allocates TX once, and reports terminal unavailable status");
    reject(allocOnlyBranch, "i2s_channel_init_std_mode(",
           "allocation-only diagnostic does not initialize I2S standard mode");
    reject(allocOnlyBranch, "i2s_channel_enable(",
           "allocation-only diagnostic does not enable I2S clocks");
    reject(allocOnlyBranch, "_gmt154_codec.begin()",
           "allocation-only diagnostic skips codec initialization");
    reject(allocOnlyBranch, "setGmt154PowerAmplifierEnabled(true)",
           "allocation-only diagnostic never enables the amplifier");
    reject(allocOnlyBranch, "xTaskCreatePinnedToCore(",
           "allocation-only diagnostic does not create an audio task");
    reject(allocOnlyBranch, "releaseRuntime();",
           "allocation-only diagnostic keeps the allocated channel on failure");
    reject(allocOnlyBranch, "i2s_channel_disable(",
           "allocation-only diagnostic leaves the channel allocated until restart");
    reject(allocOnlyBranch, "i2s_del_channel(",
           "allocation-only diagnostic does not delete the allocated channel");
  }

  const size_t c2bFlagOffset = audio.find(
      "defined(GMT154_AUDIO_STD_INIT_DIAG) && GMT154_AUDIO_STD_INIT_DIAG",
      beginOffset);
  if (c2bFlagOffset == std::string::npos) {
    std::cerr << "FAIL: GMT154 C2b standard-mode diagnostic branch exists\n";
    ++failures;
  } else {
    const size_t branchStart = audio.rfind("#if", c2bFlagOffset);
    const size_t branchEnd = audio.find("#endif", c2bFlagOffset);
    const std::string c2bBranch =
        branchStart == std::string::npos || branchEnd == std::string::npos
            ? std::string()
            : audio.substr(branchStart, branchEnd - branchStart);
    const char* c2bOrder[] = {
        "defined(GMT154_06)",
        "defined(ARDUINO_ARCH_ESP32)",
        "defined(ENABLE_GATEWAY_AUDIO)",
        "_started = true;",
        "_available = false;",
        "initializeGmt154AmplifierDisabled(&failure_stage)",
        "i2s_new_channel(&diagnostic_channel_config, &_tx_handle, nullptr)",
        "heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)",
        "heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)",
        "heap_caps_get_free_size(MALLOC_CAP_DMA | MALLOC_CAP_8BIT)",
        "heap_caps_get_largest_free_block(MALLOC_CAP_DMA | MALLOC_CAP_8BIT)",
        "const int64_t init_start_us = esp_timer_get_time();",
        "i2s_channel_init_std_mode(_tx_handle, &diagnostic_std_config)",
        "const int64_t init_duration_us = esp_timer_get_time() - init_start_us;",
        "esp_err_t=%d",
        "duration_us=%lld",
        "internal free=%zu largest=%zu -> free=%zu largest=%zu; DMA free=%zu largest=%zu -> free=%zu largest=%zu.",
        "I2S channel remains disabled; MCLK may be running on GPIO38; codec skipped; amp enable skipped; task skipped; playback skipped; unavailable.",
        "return;",
    };
    requireOrdered(c2bBranch, c2bOrder,
                   sizeof(c2bOrder) / sizeof(c2bOrder[0]),
                   "C2b starts with C2a and records I2S std-init result, duration, and heap before/after");
    requireSingleOccurrence(c2bBranch,
                            "i2s_channel_init_std_mode(_tx_handle, &diagnostic_std_config)",
                            "C2b performs exactly one standard-mode initialization");
    reject(c2bBranch, "i2s_channel_enable(",
           "C2b does not enable the I2S channel (MCLK may be active after std init)");
    reject(c2bBranch, "_gmt154_codec.begin()",
           "C2b skips ES8311 codec initialization");
    reject(c2bBranch, "setGmt154PowerAmplifierEnabled(true)",
           "C2b never enables the amplifier");
    reject(c2bBranch, "xTaskCreatePinnedToCore(",
           "C2b does not create an audio task");
    reject(c2bBranch, "playSingleTone(",
           "C2b does not play tones");
    reject(c2bBranch, "i2s_channel_write(",
           "C2b does not write playback samples");
    reject(c2bBranch, "releaseRuntime();",
           "C2b leaves the allocation/init result available for observation");
    require(c2bBranch, "snprintf(",
            "C2b formats size and int64 diagnostics with printf semantics");
    require(c2bBranch, "Log.notice(F(\"%s\" CR), diagnostic_log)",
            "C2b passes only a string to ArduinoLog after formatting");
    require(c2bBranch, "before init; MCLK may run after std-init",
            "C2b pre-init log warns that MCLK can run after initialization");
    require(c2bBranch, "diagnostic_before_internal_free",
            "C2b snapshots internal heap before std init");
    require(c2bBranch, "diagnostic_before_dma_free",
            "C2b snapshots DMA heap before std init");
    reject(c2bBranch,
           "Log.notice(F(\"AUD : GMT154 C2b std-init esp_err_t=%d (%s); duration_us=%lld.",
           "C2b does not pass unsupported int64 format directly to ArduinoLog");
    reject(c2bBranch,
           "Log.notice(F(\"AUD : GMT154 C2b heap internal free=%zu",
           "C2b does not pass size_t heap values directly to ArduinoLog");
  }

  require(mainGateway, "C2B",
          "boot banner identifies the dedicated C2B diagnostic firmware");
  require(mainGateway, "C2C",
          "boot banner identifies the dedicated C2C diagnostic firmware");

  const size_t c2cFlagOffset = audio.find(
      "defined(GMT154_AUDIO_STD_ENABLE_DIAG) && GMT154_AUDIO_STD_ENABLE_DIAG",
      beginOffset);
  if (c2cFlagOffset == std::string::npos) {
    std::cerr << "FAIL: GMT154 C2c standard-mode enable branch exists\n";
    ++failures;
  } else {
    const size_t branchStart = audio.rfind("#if", c2cFlagOffset);
    const size_t branchEnd = audio.find("#endif", c2cFlagOffset);
    const std::string c2cBranch =
        branchStart == std::string::npos || branchEnd == std::string::npos
            ? std::string()
            : audio.substr(branchStart, branchEnd - branchStart);
    const char* c2cOrder[] = {
        "defined(GMT154_06)",
        "defined(ARDUINO_ARCH_ESP32)",
        "defined(ENABLE_GATEWAY_AUDIO)",
        "_started = true;",
        "_available = false;",
        "initializeGmt154AmplifierDisabled(&failure_stage)",
        "if (disable_err != ESP_OK) {",
        "i2s_new_channel(&diagnostic_channel_config, &_tx_handle, nullptr)",
        "if (alloc_err != ESP_OK) {",
        "i2s_channel_init_std_mode(_tx_handle, &diagnostic_std_config)",
        "if (init_err != ESP_OK) {",
        "C2c pre-enable;",
        "const int64_t enable_start_us = esp_timer_get_time();",
        "i2s_channel_enable(_tx_handle)",
        "const int64_t enable_duration_us = esp_timer_get_time() - enable_start_us;",
        "C2c post-enable",
        "if (enable_err != ESP_OK) {",
        "C2c channel enabled; codec skipped; amp enable skipped; task skipped; playback skipped; unavailable.",
        "return;",
    };
    requireOrdered(c2cBranch, c2cOrder,
                   sizeof(c2cOrder) / sizeof(c2cOrder[0]),
                   "C2c disables amp, allocates and initializes before one measured enable, then remains unavailable");
    requireSingleOccurrence(c2cBranch,
                            "i2s_new_channel(&diagnostic_channel_config, &_tx_handle, nullptr)",
                            "C2c allocates exactly one I2S TX channel");
    requireSingleOccurrence(c2cBranch,
                            "i2s_channel_init_std_mode(_tx_handle, &diagnostic_std_config)",
                            "C2c performs exactly one standard-mode initialization");
    requireSingleOccurrence(c2cBranch, "i2s_channel_enable(_tx_handle)",
                            "C2c enables the I2S channel exactly once");

    const char* failureMarkers[] = {
        "if (disable_err != ESP_OK) {",
        "if (alloc_err != ESP_OK) {",
        "if (init_err != ESP_OK) {",
        "if (enable_err != ESP_OK) {",
    };
    const char* failureDescriptions[] = {
        "C2c amplifier-disable failure returns before enable",
        "C2c allocation failure returns before enable",
        "C2c standard-init failure returns before enable",
        "C2c enable failure returns without falling through",
    };
    for (size_t index = 0; index < sizeof(failureMarkers) / sizeof(failureMarkers[0]);
         ++index) {
      const std::string failureBlock =
          bracedBlockAfter(c2cBranch, failureMarkers[index]);
      require(failureBlock, "return;", failureDescriptions[index]);
      reject(failureBlock, "i2s_channel_enable(", failureDescriptions[index]);
    }

    require(c2cBranch, "diagnostic_before_internal_free",
            "C2c records internal heap before standard-mode initialization");
    require(c2cBranch, "diagnostic_before_dma_free",
            "C2c records DMA heap before standard-mode initialization");
    require(c2cBranch, "diagnostic_internal_free_before_enable",
            "C2c records internal heap before channel enable");
    require(c2cBranch, "diagnostic_dma_free_before_enable",
            "C2c records DMA heap before channel enable");
    require(c2cBranch, "diagnostic_internal_free_after_enable",
            "C2c records internal heap after channel enable");
    require(c2cBranch, "diagnostic_dma_free_after_enable",
            "C2c records DMA heap after channel enable");
    require(c2cBranch, "enable_duration_us=%lld",
            "C2c reports measured enable duration");
    require(c2cBranch, "enable_err), esp_err_to_name(enable_err)",
            "C2c reports enable result and ESP error name");
    require(c2cBranch, "internal free=%zu largest=%zu -> free=%zu largest=%zu; DMA free=%zu largest=%zu -> free=%zu largest=%zu.",
            "C2c reports internal and DMA heap before and after enable");
    require(c2cBranch, "_available = false;",
            "C2c remains unavailable throughout its diagnostic");
    reject(c2cBranch, "_available = true;",
           "C2c never advertises playback availability");
    reject(c2cBranch, "_gmt154_codec.begin()",
           "C2c skips ES8311 codec initialization");
    reject(c2cBranch, "setGmt154PowerAmplifierEnabled(true)",
           "C2c never enables the amplifier");
    reject(c2cBranch, "xTaskCreatePinnedToCore(",
           "C2c does not create an audio task");
    reject(c2cBranch, "playSingleTone(",
           "C2c does not play tones");
    reject(c2cBranch, "i2s_channel_write(",
           "C2c does not write playback samples");
    require(c2cBranch, "snprintf(",
            "C2c formats duration and heap diagnostics with printf semantics");
    reject(c2cBranch,
           "Log.notice(F(\"AUD : GMT154 C2c post-enable esp_err_t=%d (%s); enable_duration_us=%lld.",
           "C2c does not pass int64 enable timing directly to ArduinoLog");
  }

  const char* nonGmt154StartedOrder[] = {
      "#if !defined(ENABLE_GATEWAY_AUDIO)",
      "!defined(GMT154_06)",
      "_started = true;",
      "#endif",
  };
  requireOrdered(audio, nonGmt154StartedOrder,
                 sizeof(nonGmt154StartedOrder) /
                     sizeof(nonGmt154StartedOrder[0]),
                 "non-GMT154 startup keeps its existing eager started state");

  if (failures != 0) {
    std::cerr << "GMT154 audio contract: " << failures << " failure(s)\n";
    return 1;
  }
  std::cout << "GMT154 audio contract: PASS\n";
  return 0;
}
