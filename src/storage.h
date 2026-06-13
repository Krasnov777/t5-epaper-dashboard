#pragma once
#include <Arduino.h>
#include <vector>
#include "config.h"

// LittleFS-backed photo store. Photos live in /photos/<name>.bin and are each
// exactly PHOTO_BYTES (one 4-bit framebuffer).
namespace Storage {

bool begin();

// Filenames (without directory) of all stored photos, sorted ascending.
std::vector<String> listPhotos();

// JSON: {"photos":[{"name":"a.bin","size":259200}, ...],
//        "fsTotal":..,"fsUsed":..}
String photosJson();

// Read a photo into dest (must hold PHOTO_BYTES). Returns false if missing or
// wrong size.
bool readPhoto(const String &name, uint8_t *dest);

bool deletePhoto(const String &name);
bool exists(const String &name);

// First photo after `current` (wraps). Returns "" if no photos.
String nextPhoto(const String &current);
// First photo overall, or "".
String firstPhoto();

// Sanitise a user-supplied filename to a safe "*.bin" basename.
String safeName(const String &raw);

// Full path helper.
String pathFor(const String &name);

size_t fsTotal();
size_t fsUsed();

}  // namespace Storage
