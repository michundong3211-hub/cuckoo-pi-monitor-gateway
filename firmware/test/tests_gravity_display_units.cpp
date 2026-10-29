#include <AUnit.h>

#include <math.h>

#include "gravity_display_units.hpp"

test(GravityDisplayUnits, ConvertsGravityAndTemperature) {
  assertNear(19.3f, gravitySgToPlato(1.080f), 0.2f);
  assertNear(1.080f, gravityPlatoToSg(19.3f), 0.001f);
  assertNear(73.4f, celsiusToFahrenheit(23.0f), 0.01f);
  assertNear(23.0f, fahrenheitToCelsius(73.4f), 0.01f);
}

test(GravityDisplayUnits, CalculatesFermentationAndAbv) {
  float attenuation = NAN;
  float abv = NAN;
  assertTrue(calculateFermentation(1.080f, 1.046f, &attenuation, &abv));
  // The repository specification defines apparent attenuation as
  // 100 * (OG - FG) / (OG - 1), which is 42.5% for this vector.
  assertNear(42.5f, attenuation, 0.2f);
  assertNear(4.464f, abv, 0.01f);
}

test(GravityDisplayUnits, RejectsInvalidInputs) {
  float attenuation = 0.0f;
  float abv = 0.0f;
  assertFalse(calculateFermentation(NAN, 1.046f, &attenuation, &abv));
  assertFalse(calculateFermentation(1.000f, 1.000f, &attenuation, &abv));
  assertFalse(calculateFermentation(1.046f, 1.080f, &attenuation, &abv));
}

test(GravityDisplayUnits, FormatsDashboardAbv) {
  char abv_text[16];
  formatDashboardAbvText(NAN, abv_text, sizeof(abv_text));
  assertEqual(String("--"), String(abv_text));

  formatDashboardAbvText(3.44f, abv_text, sizeof(abv_text));
  assertEqual(String("ABV 3.4%"), String(abv_text));
}

test(GravityDisplayUnits, EvaluatesTemperatureAndBatteryAlerts) {
  assertFalse(isTemperatureOutOfRangeC(23.0f, 18.0f, 28.0f));
  assertTrue(isTemperatureOutOfRangeC(29.0f, 18.0f, 28.0f));
  assertTrue(isTemperatureOutOfRangeC(17.0f, 18.0f, 28.0f));
  assertFalse(isTemperatureOutOfRangeC(NAN, 18.0f, 28.0f));

  assertFalse(isBatteryPercentageLow(25.0f, 20.0f));
  assertTrue(isBatteryPercentageLow(19.0f, 20.0f));
  assertFalse(isBatteryPercentageLow(NAN, 20.0f));
}
