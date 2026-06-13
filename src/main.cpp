#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <DNSServer.h>
#include <ArduinoOTA.h>
#include <Button2.h>
#include "utilities.h"      // BUTTON_1 etc. (from LilyGo-EPD47)
#include "config.h"
#include "settings.h"
#include "display.h"
#include "storage.h"
#include "modes.h"
#include "web.h"

static DNSServer s_dns;
static Button2   s_btn;
static bool      s_apMode = false;

// ---- small on-screen messages ----
static void screenMessage(const String &l1, const String &l2 = "", const String &l3 = "") {
    Display::clearBuffer();
    int lh = Display::lineHeight();
    int cy = SCREEN_H / 2 - lh;
    Display::textCentered(0, cy, SCREEN_W, l1);
    if (l2.length()) Display::textCentered(0, cy + lh + 6, SCREEN_W, l2);
    if (l3.length()) Display::textCentered(0, cy + 2 * (lh + 6), SCREEN_W, l3);
    Display::commit();
}

static bool connectSTA() {
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(MDNS_HOST);
    WiFi.begin(g_settings.wifiSsid.c_str(), g_settings.wifiPass.c_str());
    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 20000) {
        delay(250);
    }
    return WiFi.status() == WL_CONNECTED;
}

static void startApMode() {
    s_apMode = true;
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASSWORD);
    IPAddress ip = WiFi.softAPIP();
    s_dns.start(53, "*", ip);           // captive portal
    Web::begin(/*setupMode=*/true);
    screenMessage("Wi-Fi setup needed",
                  "Join Wi-Fi '" AP_SSID "'  (pass: " AP_PASSWORD ")",
                  "then open  http://" + ip.toString());
    log_i("AP mode at %s", ip.toString().c_str());
}

static void startOTA() {
    ArduinoOTA.setHostname(MDNS_HOST);
    // ArduinoOTA.setPassword("...");   // uncomment to require a push password
    ArduinoOTA.onStart([](){
        screenMessage("Firmware update", "Receiving over Wi-Fi…", "Do not power off");
    });
    ArduinoOTA.onError([](ota_error_t){
        screenMessage("Update failed", "Will reboot…");
    });
    ArduinoOTA.begin();
}

static void startNormalMode() {
    configTzTime(g_settings.tz.c_str(), "pool.ntp.org", "time.google.com", "time.cloudflare.com");
    if (MDNS.begin(MDNS_HOST)) MDNS.addService("http", "tcp", 80);
    startOTA();
    Web::begin(/*setupMode=*/false);
    Modes::renderCurrent();
    log_i("Online: http://%s.local  (%s)  fw %s", MDNS_HOST,
          WiFi.localIP().toString().c_str(), FW_VERSION);
}

static void onTap(Button2 &) {
    if (s_apMode) return;
    Modes::setMode(g_settings.mode == MODE_PHOTO ? MODE_METRICS : MODE_PHOTO);
}

static void onLong(Button2 &) {
    // Forget Wi-Fi and reboot into setup mode.
    g_settings.wifiSsid = "";
    g_settings.wifiPass = "";
    settingsSave();
    screenMessage("Wi-Fi credentials cleared", "Rebooting into setup mode…");
    delay(800);
    ESP.restart();
}

void setup() {
    Serial.begin(115200);
    delay(200);
    log_i("LilyGo T5 4.7\" Smart Frame booting");

    if (!Display::begin()) log_e("Display init failed");
    Storage::begin();
    settingsLoad();

    s_btn.begin(BUTTON_1);
    s_btn.setLongClickTime(2500);
    s_btn.setTapHandler(onTap);
    s_btn.setLongClickDetectedHandler(onLong);

    screenMessage("Smart E-Paper Frame", "Starting up…");

    if (g_settings.hasWifi()) {
        screenMessage("Connecting to Wi-Fi", g_settings.wifiSsid);
        if (connectSTA()) startNormalMode();
        else              startApMode();
    } else {
        startApMode();
    }
}

void loop() {
    s_btn.loop();
    Web::loopTasks();
    if (s_apMode) {
        s_dns.processNextRequest();
    } else {
        ArduinoOTA.handle();
        Modes::tick();
        // Auto-recover if Wi-Fi drops for a while.
        static uint32_t downSince = 0;
        if (WiFi.status() != WL_CONNECTED) {
            if (downSince == 0) downSince = millis();
            else if (millis() - downSince > 60000) { WiFi.reconnect(); downSince = millis(); }
        } else {
            downSince = 0;
        }
    }
    delay(5);
}
