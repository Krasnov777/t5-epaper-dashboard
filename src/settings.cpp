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
    if (doc["zones"].is<JsonArray>()) {
        JsonArray z = doc["zones"].as<JsonArray>();
        for (int i = 0; i < NUM_ZONES && i < (int)z.size(); i++) {
            if (z[i]["label"].is<const char *>()) g_settings.zoneLabel[i] = z[i]["label"].as<String>();
            if (z[i]["temp"].is<const char *>())  g_settings.zoneTemp[i]  = z[i]["temp"].as<String>();
            if (z[i]["hum"].is<const char *>())   g_settings.zoneHum[i]   = z[i]["hum"].as<String>();
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
    JsonArray z = doc["zones"].to<JsonArray>();
    for (int i = 0; i < NUM_ZONES; i++) {
        JsonObject o = z.add<JsonObject>();
        o["label"] = g_settings.zoneLabel[i];
        o["temp"]  = g_settings.zoneTemp[i];
        o["hum"]   = g_settings.zoneHum[i];
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
