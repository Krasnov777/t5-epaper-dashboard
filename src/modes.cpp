#include "modes.h"
#include "config.h"
#include "settings.h"
#include "display.h"
#include "storage.h"
#include "metrics.h"
#include "homeassistant.h"
#include <WiFi.h>

namespace Modes {

static String   s_current;            // photo currently displayed (cycling)
static uint32_t s_lastPhoto   = 0;
static uint32_t s_lastMetrics = 0;

String currentPhotoName() { return s_current; }

static void placeholder(const String &line1, const String &line2) {
    Display::clearBuffer();
    int lh = Display::lineHeight();
    Display::textCentered(0, SCREEN_H / 2 - lh, SCREEN_W, line1);
    if (line2.length())
        Display::textCentered(0, SCREEN_H / 2 + lh, SCREEN_W, line2);
    Display::commit();
}

static void renderPhoto() {
    String name = g_settings.pinnedPhoto.length() ? g_settings.pinnedPhoto : s_current;
    if (name.length() == 0 || !Storage::exists(name)) name = Storage::firstPhoto();
    if (name.length() == 0) {
        placeholder("No photos yet", "Upload at  http://" MDNS_HOST ".local");
        return;
    }
    s_current = name;
    if (Storage::readPhoto(name, Display::fb())) {
        Display::commit();
    } else {
        placeholder("Could not read photo", name);
    }
    s_lastPhoto = millis();
}

static String currentIp() {
    return (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : String("offline");
}
static void renderMetrics() { Metrics::render(currentIp()); s_lastMetrics = millis(); }
static void renderHome()    { HomeMode::render(currentIp()); s_lastMetrics = millis(); }

void renderCurrent() {
    if      (g_settings.mode == MODE_METRICS) renderMetrics();
    else if (g_settings.mode == MODE_HOME)    renderHome();
    else                                      renderPhoto();
}

void setMode(uint8_t mode) {
    g_settings.mode = (mode > MODE_HOME) ? MODE_PHOTO : mode;
    settingsSave();
    renderCurrent();
}

void showPhoto(const String &name) {
    g_settings.pinnedPhoto = name;
    g_settings.mode = MODE_PHOTO;
    settingsSave();
    s_current = name;
    renderPhoto();
}

void cycleAll() {
    g_settings.pinnedPhoto = "";
    settingsSave();
    if (g_settings.mode == MODE_PHOTO) renderPhoto();
}

void refreshNow() { renderCurrent(); }

void tick() {
    uint32_t now = millis();
    if (g_settings.mode == MODE_PHOTO) {
        bool cycling = g_settings.pinnedPhoto.length() == 0;
        if (cycling && (now - s_lastPhoto) >= g_settings.slideshowSec * 1000UL) {
            String nxt = Storage::nextPhoto(s_current);
            if (nxt.length() && nxt != s_current) {
                s_current = nxt;
                renderPhoto();
            } else {
                s_lastPhoto = now;   // 0 or 1 photo: nothing to advance
            }
        }
    } else {  // MODE_METRICS or MODE_HOME — periodic refresh
        if ((now - s_lastMetrics) >= g_settings.metricsRefresh * 60UL * 1000UL) {
            renderCurrent();
        }
    }
}

}  // namespace Modes
