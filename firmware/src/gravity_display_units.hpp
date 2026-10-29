#ifndef SRC_GRAVITY_DISPLAY_UNITS_HPP_
#define SRC_GRAVITY_DISPLAY_UNITS_HPP_

#include <stddef.h>

float gravitySgToPlato(float sg);
float gravityPlatoToSg(float plato);
float celsiusToFahrenheit(float celsius);
float fahrenheitToCelsius(float fahrenheit);
bool calculateFermentation(float ogSg, float fgSg,
                           float* apparentAttenuation,
                           float* abvPercent);
bool isTemperatureOutOfRangeC(float tempC, float minTempC, float maxTempC);
bool isBatteryPercentageLow(float batteryPercent, float lowThresholdPercent);
void formatDashboardAbvText(float abvPercent, char* out, size_t outSize);

#endif  // SRC_GRAVITY_DISPLAY_UNITS_HPP_
