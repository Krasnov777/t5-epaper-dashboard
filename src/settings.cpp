#include "settings.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

Settings g_settings;

static void docToSettings(JsonDocument &doc) {
    if (doc["wifiSsid"].is<const char *>())     g_settings.wifiSsid     = doc["wifiSsid"].as<String>();
    if (doc["wifiPass"].is<const char *>())     g_settings.wifiPass     = doc["wifiPass"].as<String>();
    if (doc["mode"].is<int>())                  g_settings.mode         = doc["mode"].as<uint8_t>();
    if (doc["slideshowSec"].is<uint32_t>())     g_settings.slideshowSec = doc["slideshowSec"].as<uint32_t>();
    if (doc["pinnedPhoto"].is<const char *>())  g_settings.pinnedPhoto  = doc["pinnedPhoto"].as<String>();
    if (doc["lat"].is<float>())                 g_settings.lat          = doc["lat"].as<float>();
    if (doc["lon"].is<float>())                 g_settings.lon          = doc["lon"].as<float>();
    if (doc["locationName"].is<const char *>()) g_settings.locationName = doc["locationName"].as<String>();
    // News blocks (with migration from older key names)
    if (doc["rssUrl"].is<const char *>())     g_settings.news1Url   = doc["rssUrl"].as<String>();    // legacy
    if (doc["techRss"].is<const char *>())    g_settings.news1Url   = doc["techRss"].as<String>();   // legacy
    if (doc["localRss"].is<const char *>())   g_settings.news2Url   = doc["localRss"].as<String>();  // legacy
    if (doc["news1Label"].is<const char *>()) g_settings.news1Label = doc["news1Label"].as<String>();
    if (doc["news1Url"].is<const char *>())   g_settings.news1Url   = doc["news1Url"].as<String>();
    if (doc["news2Label"].is<const char *>()) g_settings.news2Label = doc["news2Label"].as<String>();
    if (doc["news2Url"].is<const char *>())   g_settings.news2Url   = doc["news2Url"].as<String>();
    if (doc["metricsRefresh"].is<uint32_t>())   g_settings.metricsRefresh = doc["metricsRefresh"].as<uint32_t>();
    if (doc["tz"].is<const char *>())           g_settings.tz           = doc["tz"].as<String>();

    // Home Assistant
    if (doc["haUrl"].is<const char *>())   g_settings.haUrl   = doc["haUrl"].as<String>();
    if (doc["haToken"].is<const char *>()) g_settings.haToken = doc["haToken"].as<String>();
    // Legacy zones (temp+humidity) -> tiles
    if (doc["zones"].is<JsonArray>()) {
        JsonArray z = doc["zones"].as<JsonArray>();
        for (int i = 0; i < NUM_ZONES && i < (int)z.size(); i++) {
            if (z[i]["label"].is<const char *>()) g_settings.tileLabel[i]   = z[i]["label"].as<String>();
            if (z[i]["temp"].is<const char *>())  g_settings.tileEntity[i]  = z[i]["temp"].as<String>();
            if (z[i]["hum"].is<const char *>())   g_settings.tileEntity2[i] = z[i]["hum"].as<String>();
        }
    }
    if (doc["tiles"].is<JsonArray>()) {
        JsonArray t = doc["tiles"].as<JsonArray>();
        for (int i = 0; i < NUM_ZONES && i < (int)t.size(); i++) {
            if (t[i]["type"].is<const char *>())    g_settings.tileType[i]    = t[i]["type"].as<String>();
            if (t[i]["icon"].is<const char *>())    g_settings.tileIcon[i]    = t[i]["icon"].as<String>();
            if (t[i]["label"].is<const char *>())   g_settings.tileLabel[i]   = t[i]["label"].as<String>();
            if (t[i]["entity"].is<const char *>())  g_settings.tileEntity[i]  = t[i]["entity"].as<String>();
            if (t[i]["entity2"].is<const char *>()) g_settings.tileEntity2[i] = t[i]["entity2"].as<String>();
        }
    }
    // Migrate old per-room types -> single "climate" type + a room icon
    for (int i = 0; i < NUM_ZONES; i++) {
        if (g_settings.tileType[i].startsWith("room_")) {
            if (g_settings.tileIcon[i].length() == 0) {
                String r = g_settings.tileType[i];
                g_settings.tileIcon[i] = (r == "room_living") ? "sofa"
                                       : (r == "room_bed")    ? "bed"
                                       : (r == "room_up")     ? "stairs_up" : "stairs_down";
            }
            g_settings.tileType[i] = "climate";
        }
    }

    if (g_settings.slideshowSec < 5)   g_settings.slideshowSec = 5;
    if (g_settings.metricsRefresh < 1) g_settings.metricsRefresh = 1;
    if (g_settings.mode > MODE_HOME) g_settings.mode = MODE_PHOTO;
}

static void settingsToDoc(JsonDocument &doc, bool includeSecrets) {
    doc["wifiSsid"]       = g_settings.wifiSsid;
    if (includeSecrets) doc["wifiPass"] = g_settings.wifiPass;
    doc["mode"]           = g_settings.mode;
    doc["slideshowSec"]   = g_settings.slideshowSec;
    doc["pinnedPhoto"]    = g_settings.pinnedPhoto;
    doc["lat"]            = g_settings.lat;
    doc["lon"]            = g_settings.lon;
    doc["locationName"]   = g_settings.locationName;
    doc["news1Label"]     = g_settings.news1Label;
    doc["news1Url"]       = g_settings.news1Url;
    doc["news2Label"]     = g_settings.news2Label;
    doc["news2Url"]       = g_settings.news2Url;
    doc["metricsRefresh"] = g_settings.metricsRefresh;
    doc["tz"]             = g_settings.tz;

    doc["haUrl"] = g_settings.haUrl;
    if (includeSecrets) doc["haToken"] = g_settings.haToken;
    JsonArray t = doc["tiles"].to<JsonArray>();
    for (int i = 0; i < NUM_ZONES; i++) {
        JsonObject o = t.add<JsonObject>();
        o["type"]    = g_settings.tileType[i];
        o["icon"]    = g_settings.tileIcon[i];
        o["label"]   = g_settings.tileLabel[i];
        o["entity"]  = g_settings.tileEntity[i];
        o["entity2"] = g_settings.tileEntity2[i];
    }
}

bool settingsLoad() {
    if (!LittleFS.exists(SETTINGS_PATH)) {
        log_i("No settings.json yet, using defaults");
        return false;
    }
    File f = LittleFS.open(SETTINGS_PATH, "r");
    if (!f) return false;
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) {
        log_e("settings.json parse error: %s", err.c_str());
        return false;
    }
    docToSettings(doc);
    return true;
}

bool settingsSave() {
    JsonDocument doc;
    settingsToDoc(doc, /*includeSecrets=*/true);
    File f = LittleFS.open(SETTINGS_PATH, "w");
    if (!f) return false;
    bool ok = serializeJson(doc, f) > 0;
    f.close();
    return ok;
}

String settingsToJson() {
    // Used by the web UI — omit the Wi-Fi password.
    JsonDocument doc;
    settingsToDoc(doc, /*includeSecrets=*/false);
    String out;
    serializeJson(doc, out);
    return out;
}

bool settingsApplyJson(const String &json) {
    JsonDocument doc;
    if (deserializeJson(doc, json)) return false;
    docToSettings(doc);
    settingsSave();
    return true;
}
