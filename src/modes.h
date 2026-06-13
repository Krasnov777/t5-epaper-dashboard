#pragma once
#include <Arduino.h>

// High-level controller: owns the photo-slideshow + metrics timers and drives
// the display according to g_settings.mode.
namespace Modes {

void renderCurrent();              // redraw whatever the current mode shows
void tick();                       // call frequently from loop(); handles timers
void setMode(uint8_t mode);        // switch mode, persist, redraw
void showPhoto(const String &name);// pin + display a specific photo now
void cycleAll();                   // un-pin so the slideshow cycles all photos
void refreshNow();                 // force an immediate redraw/refetch

String currentPhotoName();

}  // namespace Modes
