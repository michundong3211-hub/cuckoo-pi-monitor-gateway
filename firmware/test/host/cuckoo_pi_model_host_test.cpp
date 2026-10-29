#include "cuckoo_pi_model.hpp"

#include <cassert>
#include <cmath>
#include <cstring>

namespace {
constexpr float kTolerance = 0.0005f;

bool near(float actual, float expected) {
  return std::fabs(actual - expected) < kTolerance;
}
}

int main() {
  char normalized[13] = {};
  assert(normalizeCuckooPiId(" 00:11:22:33:44:55 ", normalized));
  assert(std::strcmp(normalized, "001122334455") == 0);
  assert(normalizeCuckooPiId("00-11-22-33-44-55", normalized));
  assert(std::strcmp(normalized, "001122334455") == 0);
  assert(!normalizeCuckooPiId("cc4bc282c34", normalized));
  assert(!normalizeCuckooPiId("cc4bc282c34z", normalized));

  CuckooPiTopics topics{};
  assert(buildCuckooPiTopics("001122334455", &topics));
  assert(std::strcmp(topics.status,
                     "/cuckoo/wenkong/001122334455") == 0);
  assert(std::strcmp(topics.mode, "/cuckoo/mode/001122334455") == 0);
  assert(std::strcmp(topics.beer_set, "/cuckoo/beer/001122334455") == 0);
  assert(std::strcmp(topics.fridge_set, "/cuckoo/ice/001122334455") == 0);
  assert(std::strcmp(topics.ack,
                     "/cuckoo/mode/ack/001122334455") == 0);

  assert(std::strcmp(cuckooPiModeLabel(0), "Off") == 0);
  assert(std::strcmp(cuckooPiModeLabel(1), "Fridge Constant") == 0);
  assert(std::strcmp(cuckooPiModeLabel(2), "Beer Constant") == 0);
  assert(std::strcmp(cuckooPiModeLabel(3), "Beer Profile") == 0);
  assert(std::strcmp(cuckooPiModeLabel(99), "Unknown") == 0);
  assert(std::strcmp(cuckooPiStateLabel(4), "Cooling") == 0);
  assert(std::strcmp(cuckooPiStateLabel(5), "Waiting to Cool") == 0);
  assert(std::strcmp(cuckooPiStateLabel(99), "Unknown") == 0);

  const char* complete_json =
      R"json({"state":4,"statusTime":15387,"beerTemp":24.18000031,"beerSet":15,"fridgeTemp":24.18799973,"fridgeSet":5,"roomTemp":23.81299973,"mode":3,"profileStep":0,"profileSteps":3,"profileStart":1788157311,"profileStepStart":1788157311,"ip":"192.0.2.10","pressure":0,"gravity":0.981299996,"plato":-4.92530489,"voltage":4.71999979,"auxTemp":34.13000107,"tilt":2.599999905,"hydrometerRssi":-57,"hydrometerName":"cuckootilt0588fc","gravityUpdated":1788208323,"hydrometerMode":"http","hydrometerIdentifier":"001122334455"})json";
  CuckooPiSnapshot complete{};
  assert(parseCuckooPiSnapshot("001122334455", complete_json,
                               std::strlen(complete_json), &complete));
  assert(std::strcmp(complete.device_id, "001122334455") == 0);
  assert(complete.has_mode && complete.mode_raw == 3);
  assert(complete.has_state && complete.state_raw == 4);
  assert(complete.has_beer_temp && near(complete.beer_temp_c, 24.18000031f));
  assert(complete.has_beer_set && near(complete.beer_set_c, 15.0f));
  assert(complete.has_fridge_temp && near(complete.fridge_temp_c, 24.18799973f));
  assert(complete.has_fridge_set && near(complete.fridge_set_c, 5.0f));
  assert(complete.has_room_temp && near(complete.room_temp_c, 23.81299973f));
  assert(complete.has_status_time && complete.status_time_seconds == 15387U);

  const char* optional_json =
      R"json({"state":3,"beerTemp":24.06299973,"beerSet":26,"fridgeTemp":24.06299973,"fridgeSet":30,"mode":3,"profileStep":2,"profileSteps":7})json";
  CuckooPiSnapshot optional{};
  assert(parseCuckooPiSnapshot("001122334455", optional_json,
                               std::strlen(optional_json), &optional));
  assert(optional.has_mode && optional.has_state);
  assert(optional.has_beer_temp && optional.has_beer_set);
  assert(optional.has_fridge_temp && optional.has_fridge_set);
  assert(!optional.has_status_time);

  const char* ranged_json =
      R"json({"state":77,"mode":88,"statusTime":-1,"beerTemp":200,"beerSet":15,"fridgeTemp":-60,"fridgeSet":5})json";
  CuckooPiSnapshot ranged{};
  assert(parseCuckooPiSnapshot("001122334455", ranged_json,
                               std::strlen(ranged_json), &ranged));
  assert(ranged.has_mode && ranged.mode_raw == 88);
  assert(ranged.has_state && ranged.state_raw == 77);
  assert(!ranged.has_status_time);
  assert(!ranged.has_beer_temp);
  assert(ranged.has_beer_set);
  assert(!ranged.has_fridge_temp);
  assert(ranged.has_fridge_set);

  const char* boundary_json =
      R"json({"beerTemp":-40,"beerSet":100,"fridgeTemp":-40.01,"fridgeSet":100.01})json";
  CuckooPiSnapshot boundary{};
  assert(parseCuckooPiSnapshot("001122334455", boundary_json,
                               std::strlen(boundary_json), &boundary));
  assert(boundary.has_beer_temp);
  assert(boundary.has_beer_set);
  assert(!boundary.has_fridge_temp);
  assert(!boundary.has_fridge_set);

  const char* room_temperature_json =
      R"json({"roomTemp":31.31299973,"fridgeTemp":4.5,"fridgeSet":8.25})json";
  CuckooPiSnapshot room_temperature{};
  assert(parseCuckooPiSnapshot("001122334455", room_temperature_json,
                               std::strlen(room_temperature_json),
                               &room_temperature));
  assert(room_temperature.has_room_temp);
  assert(near(room_temperature.room_temp_c, 31.31299973f));
  assert(room_temperature.has_fridge_temp);
  assert(near(room_temperature.fridge_temp_c, 4.5f));
  assert(room_temperature.has_fridge_set);
  assert(near(room_temperature.fridge_set_c, 8.25f));

  const char* room_temperature_missing_json = R"json({"state":4})json";
  assert(parseCuckooPiSnapshot(
      "001122334455", room_temperature_missing_json,
      std::strlen(room_temperature_missing_json), &room_temperature));
  assert(!room_temperature.has_room_temp);
  assert(room_temperature.room_temp_c == 0.0f);
  assert(!room_temperature.has_fridge_temp);
  assert(room_temperature.fridge_temp_c == 0.0f);
  assert(!room_temperature.has_fridge_set);
  assert(room_temperature.fridge_set_c == 0.0f);

  assert(parseCuckooPiSnapshot("001122334455", room_temperature_json,
                               std::strlen(room_temperature_json),
                               &room_temperature));
  assert(room_temperature.has_room_temp);
  assert(near(room_temperature.room_temp_c, 31.31299973f));

  const struct {
    const char* json;
    float want;
  } valid_room_boundary_cases[] = {
      {R"json({"roomTemp":0})json", 0.0f},
      {R"json({"roomTemp":0.0})json", 0.0f},
      {R"json({"roomTemp":-1.25})json", -1.25f},
      {R"json({"roomTemp":-40})json", -40.0f},
      {R"json({"roomTemp":100})json", 100.0f},
  };
  for (const auto& test_case : valid_room_boundary_cases) {
    assert(parseCuckooPiSnapshot("001122334455", test_case.json,
                                 std::strlen(test_case.json),
                                 &room_temperature));
    assert(room_temperature.has_room_temp);
    assert(near(room_temperature.room_temp_c, test_case.want));
  }

  const char* invalid_room_jsons[] = {
      R"json({"roomTemp":-40.01})json",
      R"json({"roomTemp":100.01})json",
      R"json({"roomTemp":null})json",
      R"json({"roomTemp":"31.31"})json",
      R"json({"roomTemp":false})json",
      R"json({"roomTemp":true})json",
      R"json({"roomTemp":[]})json",
      R"json({"roomTemp":{}})json",
      R"json({"roomTemp":1e999})json",
  };
  for (const char* invalid_json : invalid_room_jsons) {
    assert(parseCuckooPiSnapshot("001122334455", room_temperature_json,
                                 std::strlen(room_temperature_json),
                                 &room_temperature));
    assert(room_temperature.has_room_temp);
    assert(near(room_temperature.room_temp_c, 31.31299973f));
    assert(parseCuckooPiSnapshot("001122334455", invalid_json,
                                 std::strlen(invalid_json), &room_temperature));
    assert(!room_temperature.has_room_temp);
    assert(room_temperature.room_temp_c == 0.0f);
  }

  const char* fridge_set_only_json = R"json({"fridgeSet":8.25})json";
  assert(parseCuckooPiSnapshot("001122334455", fridge_set_only_json,
                               std::strlen(fridge_set_only_json),
                               &room_temperature));
  assert(!room_temperature.has_room_temp);
  assert(room_temperature.room_temp_c == 0.0f);

  CuckooPiSnapshot unchanged{};
  std::strcpy(unchanged.device_id, "unchanged");
  unchanged.mode_raw = 42;
  assert(!parseCuckooPiSnapshot("001122334455", "{bad json", 9,
                                &unchanged));
  assert(std::strcmp(unchanged.device_id, "unchanged") == 0);
  assert(unchanged.mode_raw == 42);
  assert(!parseCuckooPiSnapshot("invalid-id", complete_json,
                                std::strlen(complete_json), &unchanged));
  assert(!parseCuckooPiSnapshot("001122334455", complete_json,
                                std::strlen(complete_json), nullptr));
  return 0;
}
