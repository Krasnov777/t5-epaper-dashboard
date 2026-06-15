#pragma once
#include <Arduino.h>

// MODE_HOME: same weather top block as metrics, but the lower area shows a 2x2
// grid of indoor zones (temperature + humidity) pulled from Home Assistant's
// REST API (/api/states/<entity> with a long-lived token).
namespace HomeMode {

void render(const String &ip);

}  // namespace HomeMode
