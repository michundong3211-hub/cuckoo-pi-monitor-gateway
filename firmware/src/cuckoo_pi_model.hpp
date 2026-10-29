#ifndef SRC_CUCKOO_PI_MODEL_HPP_
#define SRC_CUCKOO_PI_MODEL_HPP_

#include <cstddef>
#include <cstdint>

struct CuckooPiTopics {
  char status[96];
  char mode[96];
  char beer_set[96];
  char fridge_set[96];
  char ack[96];
};

struct CuckooPiSnapshot {
  char device_id[13];
  int mode_raw;
  int state_raw;
  float beer_temp_c;
  float beer_set_c;
  float fridge_temp_c;
  float room_temp_c;
  float fridge_set_c;
  uint32_t status_time_seconds;
  int profile_step;
  int profile_steps;
  bool has_mode;
  bool has_state;
  bool has_beer_temp;
  bool has_beer_set;
  bool has_fridge_temp;
  bool has_room_temp;
  bool has_fridge_set;
  bool has_status_time;
  bool has_profile_step;
  bool has_profile_steps;
};

bool normalizeCuckooPiId(const char* input, char output[13]);
bool buildCuckooPiTopics(const char* device_id, CuckooPiTopics* output);
const char* cuckooPiModeLabel(int mode);
const char* cuckooPiStateLabel(int state);
bool parseCuckooPiSnapshot(const char* device_id, const char* json,
                           size_t length, CuckooPiSnapshot* output);

#endif  // SRC_CUCKOO_PI_MODEL_HPP_
