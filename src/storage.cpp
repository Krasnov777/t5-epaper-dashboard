#include "storage.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <algorithm>

namespace Storage {

bool begin() {
    if (!LittleFS.begin(true)) {   // format on first run
        log_e("LittleFS mount failed");
        return false;
    }
    if (!LittleFS.exists(PHOTO_DIR)) LittleFS.mkdir(PHOTO_DIR);
    return true;
}

String pathFor(const String &name) { return String(PHOTO_DIR) + "/" + name; }

String safeName(const String &raw) {
    String base = raw;
    int slash = base.lastIndexOf('/');
    if (slash >= 0) base = base.substring(slash + 1);
    String out;
    for (size_t i = 0; i < base.length() && out.length() < 48; i++) {
        char c = base[i];
        if (isalnum(c) || c == '.' || c == '_' || c == '-') out += c;
    }
    if (out.length() == 0) out = "photo";
    if (!out.endsWith(".bin")) out += ".bin";
    return out;
}

std::vector<String> listPhotos() {
    std::vector<String> names;
    File dir = LittleFS.open(PHOTO_DIR);
    if (dir && dir.isDirectory()) {
        for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
            if (!f.isDirectory()) {
                String n = String(f.name());
                int slash = n.lastIndexOf('/');
                if (slash >= 0) n = n.substring(slash + 1);
                names.push_back(n);
            }
            f.close();
        }
    }
    if (dir) dir.close();
    std::sort(names.begin(), names.end());
    return names;
}

String photosJson() {
    JsonDocument doc;
    JsonArray arr = doc["photos"].to<JsonArray>();
    File dir = LittleFS.open(PHOTO_DIR);
    if (dir && dir.isDirectory()) {
        for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
            if (!f.isDirectory()) {
                String n = String(f.name());
                int slash = n.lastIndexOf('/');
                if (slash >= 0) n = n.substring(slash + 1);
                JsonObject o = arr.add<JsonObject>();
                o["name"] = n;
                o["size"] = f.size();
            }
            f.close();
        }
    }
    if (dir) dir.close();
    doc["fsTotal"] = fsTotal();
    doc["fsUsed"]  = fsUsed();
    String out;
    serializeJson(doc, out);
    return out;
}

bool readPhoto(const String &name, uint8_t *dest) {
    String path = pathFor(name);
    File f = LittleFS.open(path, "r");
    if (!f) return false;
    if (f.size() != PHOTO_BYTES) {
        log_e("Photo %s wrong size %u (want %u)", name.c_str(),
              (unsigned)f.size(), (unsigned)PHOTO_BYTES);
        f.close();
        return false;
    }
    size_t got = f.read(dest, PHOTO_BYTES);
    f.close();
    return got == PHOTO_BYTES;
}

bool deletePhoto(const String &name) { return LittleFS.remove(pathFor(name)); }
bool exists(const String &name)      { return LittleFS.exists(pathFor(name)); }

String firstPhoto() {
    auto v = listPhotos();
    return v.empty() ? String() : v.front();
}

String nextPhoto(const String &current) {
    auto v = listPhotos();
    if (v.empty()) return String();
    for (size_t i = 0; i < v.size(); i++) {
        if (v[i] == current) return v[(i + 1) % v.size()];
    }
    return v.front();
}

size_t fsTotal() { return LittleFS.totalBytes(); }
size_t fsUsed()  { return LittleFS.usedBytes(); }

}  // namespace Storage
