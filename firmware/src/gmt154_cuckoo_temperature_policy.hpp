#ifndef SRC_GMT154_CUCKOO_TEMPERATURE_POLICY_HPP_
#define SRC_GMT154_CUCKOO_TEMPERATURE_POLICY_HPP_

namespace gmt154_cuckoo_temperature_policy {

struct TemperaturePresentation {
  bool fridge_constant_mode;
  bool has_temperature;
  float temperature_c;
  float display_temperature;
  const char* heading;
  const char* left_label;
  const char* left_value;
  bool unit_visible;
};

inline float formatTemperatureForDisplay(float temperature_c,
                                         bool fahrenheit) {
  return fahrenheit ? temperature_c * 9.0f / 5.0f + 32.0f : temperature_c;
}

inline TemperaturePresentation selectTemperaturePresentation(
    bool is_gmt154_06, bool has_mode, int mode_raw, bool has_fridge_temp,
    float fridge_temp_c, bool has_beer_temp, float beer_temp_c,
    bool fahrenheit) {
  const bool fridge_constant_mode =
      is_gmt154_06 && has_mode && mode_raw == 1;
  const bool has_temperature = fridge_constant_mode ? has_fridge_temp
                                                     : has_beer_temp;
  const float temperature_c = fridge_constant_mode ? fridge_temp_c : beer_temp_c;
  return {fridge_constant_mode,
          has_temperature,
          has_temperature ? temperature_c : 0.0f,
          has_temperature
              ? formatTemperatureForDisplay(temperature_c, fahrenheit)
              : 0.0f,
          fridge_constant_mode ? "FRIDGE TEMPERATURE" : "BEER TEMPERATURE",
          fridge_constant_mode ? "CURRENT MODE" : "BEER SET",
          fridge_constant_mode ? "Fridge Constant" : nullptr,
          has_temperature};
}

}  // namespace gmt154_cuckoo_temperature_policy

#endif  // SRC_GMT154_CUCKOO_TEMPERATURE_POLICY_HPP_
