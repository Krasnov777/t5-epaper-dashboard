#pragma once
#include <Arduino.h>
#include "config.h"

// Persistent device configuration, stored as /settings.json in LittleFS.
struct Settings {
    // Wi-Fi (station mode)
    String wifiSsid;
    String wifiPass;

    // Operating mode
    uint8_t  mode            = MODE_PHOTO;

    // Photo / slideshow
    uint32_t slideshowSec    = DEFAULT_SLIDESHOW_SEC;
    String   pinnedPhoto;          // "" = cycle through all photos

    // Metrics — location (city name is auto-resolved from lat/lon)
    float    lat             = 51.5074f;   // London (generic default; set via web UI)
    float    lon             = -0.1278f;
    String   locationName    = "London";

    // Two configurable news blocks (label + RSS/Atom feed URL). Region-agnostic.
    String   news1Label      = "Tech";
    String   news1Url        = "https://feeds.arstechnica.com/arstechnica/index";
    String   news2Label      = "World";
    String   news2Url        = "https://feeds.bbci.co.uk/news/world/rss.xml";

    uint32_t metricsRefresh  = DEFAULT_METRICS_REFRESH;  // minutes
    String   tz              = "GMT0BST,M3.5.0/1,M10.5.0";    // POSIX TZ (set via web UI)

    // Home Assistant integration (MODE_HOME — indoor climate zones)
    String   haUrl    = "";   // e.g. http://homeassistant.local:8123
    String   haToken  = "";   // long-lived access token (write-only; never returned)
    String   zoneLabel[NUM_ZONES] = {"Downstairs", "Living Room", "Upstairs", "Bedroom"};
    String   zoneTemp[NUM_ZONES]  = {"", "", "", ""};   // temperature sensor entity_ids
    String   zoneHum[NUM_ZONES]   = {"", "", "", ""};   // humidity sensor entity_ids

    bool     hasWifi() const { return wifiSsid.length() > 0; }
};

extern Settings g_settings;

// Load settings.json into g_settings (keeps defaults for missing keys).
bool settingsLoad();
// Persist g_settings to settings.json.
bool settingsSave();
// Serialize current settings to a JSON string (for the /api/status response).
String settingsToJson();
// Apply a JSON document (partial update) onto g_settings. Returns true if changed.
bool settingsApplyJson(const String &json);
