#include "web.h"
#include "config.h"
#include "settings.h"
#include "storage.h"
#include "modes.h"
#include "display.h"
#include "web_assets.h"
#include <WiFi.h>
#include <LittleFS.h>
#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>
#include <ArduinoJson.h>
#include <Update.h>

namespace Web {

static AsyncWebServer s_server(80);
static bool   s_setup        = false;
static bool   s_reboot       = false;
static bool   s_renderDirty  = false;

// ---- photo upload (raw octet-stream body) ----
static File   s_upFile;
static bool   s_upOk         = false;
static String s_upName;

static String statusJson() {
    JsonDocument doc;
    doc["ip"]       = WiFi.localIP().toString();
    doc["version"]  = FW_VERSION;
    doc["current"]  = Modes::currentPhotoName();   // photo currently on the panel
    doc["freeHeap"] = ESP.getFreeHeap();
    doc["fsTotal"]  = Storage::fsTotal();
    doc["fsUsed"]   = Storage::fsUsed();

    JsonDocument sd;
    deserializeJson(sd, settingsToJson());
    doc["settings"] = sd;

    JsonArray arr = doc["photos"].to<JsonArray>();
    for (auto &name : Storage::listPhotos()) {
        File f = LittleFS.open(Storage::pathFor(name), "r");
        JsonObject o = arr.add<JsonObject>();
        o["name"] = name;
        o["size"] = f ? f.size() : 0;
        if (f) f.close();
    }
    String out;
    serializeJson(doc, out);
    return out;
}

static void onPhotoBody(AsyncWebServerRequest *req, uint8_t *data, size_t len,
                        size_t index, size_t total) {
    if (index == 0) {
        s_upOk = false;
        if (total != PHOTO_BYTES) {
            log_e("upload wrong size %u (want %u)", (unsigned)total, (unsigned)PHOTO_BYTES);
            return;
        }
        s_upName = req->hasParam("name")
                       ? Storage::safeName(req->getParam("name")->value())
                       : String("photo.bin");
        s_upFile = LittleFS.open(Storage::pathFor(s_upName), "w");
        s_upOk = (bool)s_upFile;
    }
    if (s_upOk && s_upFile) s_upFile.write(data, len);
    if (index + len >= total && s_upFile) s_upFile.close();
}

// ---- firmware OTA (raw .bin body via /api/update) ----
static bool s_otaOk = false;
static void onUpdateBody(AsyncWebServerRequest *req, uint8_t *data, size_t len,
                         size_t index, size_t total) {
    if (index == 0) {
        s_otaOk = false;
        size_t sz = total > 0 ? total : UPDATE_SIZE_UNKNOWN;
        if (!Update.begin(sz)) {
            log_e("OTA begin failed");
            Update.printError(Serial);
            return;
        }
        log_i("OTA start (%u bytes)", (unsigned)total);
    }
    if (Update.isRunning()) {
        if (Update.write(data, len) != len) Update.printError(Serial);
    }
    if (index + len >= total && Update.isRunning()) {
        s_otaOk = Update.end(true);
        if (!s_otaOk) Update.printError(Serial);
        else log_i("OTA done, will reboot");
    }
}

static void addJson(const char *uri,
                    std::function<void(AsyncWebServerRequest *, JsonVariant &)> fn) {
    auto *h = new AsyncCallbackJsonWebHandler(uri, fn);
    s_server.addHandler(h);
}

static void registerSetupRoutes() {
    s_server.on("/", HTTP_GET, [](AsyncWebServerRequest *r) {
        r->send(200, "text/html", SETUP_HTML);
    });
    addJson("/api/wifi", [](AsyncWebServerRequest *r, JsonVariant &j) {
        g_settings.wifiSsid = j["wifiSsid"].as<String>();
        g_settings.wifiPass = j["wifiPass"].as<String>();
        settingsSave();
        r->send(200, "application/json", "{\"ok\":true}");
        s_reboot = true;
    });
    // Captive-portal: any other URL shows the setup page.
    s_server.onNotFound([](AsyncWebServerRequest *r) {
        r->send(200, "text/html", SETUP_HTML);
    });
}

static void registerMainRoutes() {
    s_server.on("/", HTTP_GET, [](AsyncWebServerRequest *r) {
        r->send(200, "text/html", INDEX_HTML);
    });
    s_server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *r) {
        r->send(200, "application/json", statusJson());
    });

    // Raw current framebuffer (960x540, 4-bit) — the web UI converts it to a
    // portrait PNG for screenshots.
    s_server.on("/api/fb", HTTP_GET, [](AsyncWebServerRequest *r) {
        uint8_t *fb = Display::fb();
        if (!fb) { r->send(503); return; }
        r->send(r->beginResponse_P(200, "application/octet-stream", fb, FB_SIZE));
    });

    // Photo upload: POST raw bytes to /api/upload?name=foo.bin
    // (kept off the /api/photo/* namespace to avoid route prefix collisions)
    s_server.on(
        "/api/upload", HTTP_POST,
        [](AsyncWebServerRequest *r) {
            if (s_upOk) r->send(200, "application/json", "{\"ok\":true,\"name\":\"" + s_upName + "\"}");
            else        r->send(400, "application/json", "{\"ok\":false}");
        },
        nullptr, onPhotoBody);

    // Firmware OTA: POST raw firmware.bin to /api/update
    s_server.on(
        "/api/update", HTTP_POST,
        [](AsyncWebServerRequest *r) {
            r->send(s_otaOk ? 200 : 500, "application/json",
                    s_otaOk ? "{\"ok\":true}" : "{\"ok\":false}");
            if (s_otaOk) s_reboot = true;
        },
        nullptr, onUpdateBody);

    addJson("/api/settings", [](AsyncWebServerRequest *r, JsonVariant &j) {
        String s; serializeJson(j, s);
        settingsApplyJson(s);
        r->send(200, "application/json", "{\"ok\":true}");
    });
    addJson("/api/mode", [](AsyncWebServerRequest *r, JsonVariant &j) {
        g_settings.mode = j["mode"].as<uint8_t>() > MODE_METRICS ? MODE_PHOTO : j["mode"].as<uint8_t>();
        settingsSave();
        s_renderDirty = true;
        r->send(200, "application/json", "{\"ok\":true}");
    });
    addJson("/api/photo/show", [](AsyncWebServerRequest *r, JsonVariant &j) {
        g_settings.pinnedPhoto = j["name"].as<String>();
        g_settings.mode = MODE_PHOTO;
        settingsSave();
        s_renderDirty = true;
        r->send(200, "application/json", "{\"ok\":true}");
    });
    addJson("/api/photo/cycle", [](AsyncWebServerRequest *r, JsonVariant &j) {
        g_settings.pinnedPhoto = "";
        settingsSave();
        s_renderDirty = true;
        r->send(200, "application/json", "{\"ok\":true}");
    });
    addJson("/api/photo/delete", [](AsyncWebServerRequest *r, JsonVariant &j) {
        String name = j["name"].as<String>();
        Storage::deletePhoto(name);
        if (name == Modes::currentPhotoName() || name == g_settings.pinnedPhoto) {
            if (name == g_settings.pinnedPhoto) g_settings.pinnedPhoto = "";
            s_renderDirty = true;
        }
        r->send(200, "application/json", "{\"ok\":true}");
    });
    addJson("/api/refresh", [](AsyncWebServerRequest *r, JsonVariant &j) {
        s_renderDirty = true;
        r->send(200, "application/json", "{\"ok\":true}");
    });
    addJson("/api/wifi", [](AsyncWebServerRequest *r, JsonVariant &j) {
        g_settings.wifiSsid = j["wifiSsid"].as<String>();
        g_settings.wifiPass = j["wifiPass"].as<String>();
        settingsSave();
        r->send(200, "application/json", "{\"ok\":true}");
        s_reboot = true;
    });
    addJson("/api/reboot", [](AsyncWebServerRequest *r, JsonVariant &j) {
        r->send(200, "application/json", "{\"ok\":true}");
        s_reboot = true;
    });
    s_server.onNotFound([](AsyncWebServerRequest *r) {
        r->send(404, "text/plain", "Not found");
    });
}

void begin(bool setupMode) {
    s_setup = setupMode;
    if (setupMode) registerSetupRoutes();
    else           registerMainRoutes();
    s_server.begin();
    log_i("HTTP server started (%s mode)", setupMode ? "setup" : "main");
}

void loopTasks() {
    if (s_renderDirty) {
        s_renderDirty = false;
        Modes::renderCurrent();
    }
    if (s_reboot) {
        delay(400);
        ESP.restart();
    }
}

}  // namespace Web
