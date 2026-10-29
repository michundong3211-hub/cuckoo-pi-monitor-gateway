#include "cuckoo_pi_model.hpp"

#include <ArduinoJson.h>

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

namespace {
constexpr float kMinimumTemperatureC = -40.0f;
constexpr float kMaximumTemperatureC = 100.0f;

bool readInteger(JsonVariantConst value, int* output) {
  if (!output || !value.is<int>()) return false;
  *output = value.as<int>();
  return true;
}

bool readTemperature(JsonVariantConst value, float* output) {
  if (!output || !value.is<float>()) return false;
  const float parsed = value.as<float>();
  if (!std::isfinite(parsed) || parsed < kMinimumTemperatureC ||
      parsed > kMaximumTemperatureC) {
    return false;
  }
  *output = parsed;
  return true;
}

bool readDuration(JsonVariantConst value, uint32_t* output) {
  if (!output || !value.is<unsigned long>()) return false;
  const unsigned long parsed = value.as<unsigned long>();
  if (parsed > std::numeric_limits<uint32_t>::max()) return false;
  *output = static_cast<uint32_t>(parsed);
  return true;
}
}

bool normalizeCuckooPiId(const char* input, char output[13]) {
  if (!input || !output) return false;
  char normalized[13] = {};
  size_t count = 0;
  for (const unsigned char* current =
           reinterpret_cast<const unsigned char*>(input);
       *current; ++current) {
    if (std::isspace(*current)) continue;
    if (*current == ':' || *current == '-') continue;
    if (!std::isxdigit(*current) || count >= 12) return false;
    normalized[count++] = static_cast<char>(std::tolower(*current));
  }
  if (count != 12) return false;
  std::memcpy(output, normalized, sizeof(normalized));
  return true;
}

bool buildCuckooPiTopics(const char* device_id, CuckooPiTopics* output) {
  if (!output) return false;
  char id[13] = {};
  if (!normalizeCuckooPiId(device_id, id)) return false;

  CuckooPiTopics topics{};
  std::snprintf(topics.status, sizeof(topics.status),
                "/cuckoo/wenkong/%s", id);
  std::snprintf(topics.mode, sizeof(topics.mode), "/cuckoo/mode/%s", id);
  std::snprintf(topics.beer_set, sizeof(topics.beer_set),
                "/cuckoo/beer/%s", id);
  std::snprintf(topics.fridge_set, sizeof(topics.fridge_set),
                "/cuckoo/ice/%s", id);
  std::snprintf(topics.ack, sizeof(topics.ack),
                "/cuckoo/mode/ack/%s", id);
  *output = topics;
  return true;
}

const char* cuckooPiModeLabel(int mode) {
  switch (mode) {
    case 0:
      return "Off";
    case 1:
      return "Fridge Constant";
    case 2:
      return "Beer Constant";
    case 3:
      return "Beer Profile";
    default:
      return "Unknown";
  }
}

const char* cuckooPiStateLabel(int state) {
  switch (state) {
    case 0:
      return "Idle";
    case 1:
      return "Off";
    case 2:
      return "Door Open";
    case 3:
      return "Heating";
    case 4:
      return "Cooling";
    case 5:
      return "Waiting to Cool";
    case 6:
      return "Waiting to Heat";
    case 7:
      return "Waiting for Peak";
    case 8:
      return "Cooling Minimum Time";
    case 9:
      return "Heating Minimum Time";
    default:
      return "Unknown";
  }
}

bool parseCuckooPiSnapshot(const char* device_id, const char* json,
                           size_t length, CuckooPiSnapshot* output) {
  if (!json || length == 0 || !output) return false;

  char normalized_id[13] = {};
  if (!normalizeCuckooPiId(device_id, normalized_id)) return false;

  JsonDocument document;
  const DeserializationError error = deserializeJson(document, json, length);
  if (error || !document.is<JsonObject>()) return false;
  const JsonObjectConst root = document.as<JsonObjectConst>();

  CuckooPiSnapshot parsed{};
  std::memcpy(parsed.device_id, normalized_id, sizeof(parsed.device_id));
  parsed.mode_raw = -1;
  parsed.state_raw = -1;
  parsed.has_mode = readInteger(root["mode"], &parsed.mode_raw);
  parsed.has_state = readInteger(root["state"], &parsed.state_raw);
  parsed.has_beer_temp =
      readTemperature(root["beerTemp"], &parsed.beer_temp_c);
  parsed.has_beer_set =
      readTemperature(root["beerSet"], &parsed.beer_set_c);
  parsed.has_fridge_temp =
      readTemperature(root["fridgeTemp"], &parsed.fridge_temp_c);
  parsed.has_room_temp =
      readTemperature(root["roomTemp"], &parsed.room_temp_c);
  parsed.has_fridge_set =
      readTemperature(root["fridgeSet"], &parsed.fridge_set_c);
  parsed.has_status_time =
      readDuration(root["statusTime"], &parsed.status_time_seconds);
  parsed.has_profile_step =
      readInteger(root["profileStep"], &parsed.profile_step);
  parsed.has_profile_steps =
      readInteger(root["profileSteps"], &parsed.profile_steps);
  if (!parsed.has_profile_step || !parsed.has_profile_steps ||
      parsed.profile_step < 0 || parsed.profile_steps <= 0 ||
      parsed.profile_step >= parsed.profile_steps) {
    parsed.has_profile_step = false;
    parsed.has_profile_steps = false;
  }

  *output = parsed;
  return true;
}
