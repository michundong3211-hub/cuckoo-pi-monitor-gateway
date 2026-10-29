#include <assert.h>

#include <fstream>
#include <sstream>
#include <string>

#include "../../src/gateway35_audio_codec.hpp"
#include "../../src/waveshare_35_profile.hpp"

namespace {

std::string readFile(const char* path) {
  std::ifstream input(path, std::ios::binary);
  assert(input.good());
  std::ostringstream contents;
  contents << input.rdbuf();
  return contents.str();
}

void require(const std::string& source, const char* needle) {
  assert(source.find(needle) != std::string::npos);
}

void reject(const std::string& source, const char* needle) {
  assert(source.find(needle) == std::string::npos);
}

void testProfileAndStartupContract() {
  static_assert(gateway35::kAudioMclkFrequencyHz ==
                    gateway35::kAudioSampleRateHz * 256,
                "3.5 MCLK must be 256 times the PCM sample rate");
  assert(gateway35::kAudioCodecI2cAddress == 0x18);
  assert(gateway35::kAudioSdaPin == 8);
  assert(gateway35::kAudioSclPin == 7);
  assert(gateway35::kAudioMclkPin == 12);
  assert(gateway35::kAudioBclkPin == 13);
  assert(gateway35::kAudioLrckPin == 15);
  assert(gateway35::kAudioDoutPin == 16);
  assert(gateway35::kAudioSampleRateHz == 16000);
  assert(gateway35::kAudioMclkFrequencyHz == 4096000);

  assert(gateway35AudioStartupReady(true, true, true, true));
  assert(!gateway35AudioStartupReady(false, true, true, true));
  assert(!gateway35AudioStartupReady(true, false, true, true));
  assert(!gateway35AudioStartupReady(true, true, false, true));
  assert(!gateway35AudioStartupReady(true, true, true, false));
}

}  // namespace

int main() {
  testProfileAndStartupContract();

  const std::string audioHeader = readFile("src/gateway_audio.hpp");
  const std::string audio = readFile("src/gateway_audio.cpp");
  const std::string profile = readFile("src/waveshare_35_profile.hpp");
  const std::string platform = readFile("platformio.ini");
  const size_t env32Start = platform.find("[env:gateway32s3wave-tft-sd]");
  const size_t env35Start = platform.find("[env:gateway35s3wave-tft-sd]");
  assert(env32Start != std::string::npos);
  assert(env35Start != std::string::npos);
  assert(env32Start < env35Start);
  const std::string env32 = platform.substr(env32Start, env35Start - env32Start);
  const std::string env35 = platform.substr(env35Start);

  require(env32, "-D ENABLE_GATEWAY_AUDIO=1");
  require(env35, "-D ENABLE_GATEWAY_AUDIO=1");
  reject(env32, "-D GATEWAY_35=1");
  require(profile, "kTca9554Address = 0x20");
  require(profile, "kTca9554OutputRegister = 0x01");
  require(profile, "kTca9554ConfigRegister = 0x03");
  require(profile, "kPowerAmplifierEnableBit = 0x80");
  require(audioHeader, "Gateway35AudioCodec _codec");
  require(audio, "_codec.begin(");
  require(audio, "std_config.gpio_cfg.mclk");
  require(audio, "gateway35AudioStartupReady");
  require(audio, "setGateway35PowerAmplifierEnabled");
  require(audio, "setGateway35PowerAmplifierEnabled(Wire, true)");
  require(audio, "kPowerAmplifierEnableBit");
  require(audio, "kTca9554OutputRegister");
  require(audio, "kTca9554ConfigRegister");
  reject(audio, "Wire.begin(");
  return 0;
}
