#pragma once
#include <Arduino.h>

// Async HTTP server: serves the embedded SPA and the JSON/upload API.
namespace Web {

// setupMode = true  -> Soft-AP onboarding (only Wi-Fi config endpoints + setup page)
// setupMode = false -> full UI + API
void begin(bool setupMode);

// Call from loop(): performs deferred display renders + reboots requested by
// HTTP handlers (kept off the async-tcp task on purpose).
void loopTasks();

}  // namespace Web
