#pragma once

#include <ArduinoJson.h>

#include "gateway_device_settings.hpp"

void append_gateway_device_settings_json(
    JsonArray devices, const GravityDeviceSettings* settings, uint8_t count);
