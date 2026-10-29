#include "gravity_display_units.hpp"

#include <math.h>
#include <stdio.h>

float gravitySgToPlato(float sg) {
  if (!isfinite(sg)) return NAN;
  return -616.868f + 1111.14f * sg - 630.272f * sg * sg +
         135.997f * sg * sg * sg;
}

float gravityPlatoToSg(float plato) {
  if (!isfinite(plato)) return NAN;
  const float denominator = 258.6f - (plato / 258.2f) * 227.1f;
  if (fabsf(denominator) < 0.0001f) return NAN;
  return 1.0f + plato / denominator;
}

float celsiusToFahrenheit(float celsius) {
  if (!isfinite(celsius)) return NAN;
  return celsius * 9.0f / 5.0f + 32.0f;
}

float fahrenheitToCelsius(float fahrenheit) {
  if (!isfinite(fahrenheit)) return NAN;
  return (fahrenheit - 32.0f) * 5.0f / 9.0f;
}

bool calculateFermentation(float ogSg, float fgSg,
                           float* apparentAttenuation, float* abvPercent) {
  if (!apparentAttenuation || !abvPercent || !isfinite(ogSg) ||
      !isfinite(fgSg) || ogSg <= 1.0f) {
    return false;
  }

  const float attenuation = 100.0f * (ogSg - fgSg) / (ogSg - 1.0f);
  const float abv = (ogSg - fgSg) * 131.3f;
  if (!isfinite(attenuation) || !isfinite(abv) || fgSg > ogSg) {
    return false;
  }

  *apparentAttenuation = attenuation < 0.0f
                             ? 0.0f
                             : (attenuation > 100.0f ? 100.0f : attenuation);
  *abvPercent = abv;
  return true;
}

bool isTemperatureOutOfRangeC(float tempC, float minTempC, float maxTempC) {
  if (!isfinite(tempC) || !isfinite(minTempC) || !isfinite(maxTempC)) {
    return false;
  }
  return tempC < minTempC || tempC > maxTempC;
}

bool isBatteryPercentageLow(float batteryPercent, float lowThresholdPercent) {
  if (!isfinite(batteryPercent) || !isfinite(lowThresholdPercent)) {
    return false;
  }
  return batteryPercent < lowThresholdPercent;
}

void formatDashboardAbvText(float abvPercent, char* out, size_t outSize) {
  if (!out || outSize == 0) return;

  if (!isfinite(abvPercent)) {
    snprintf(out, outSize, "--");
    return;
  }

  snprintf(out, outSize, "ABV %.1f%%", abvPercent);
}
