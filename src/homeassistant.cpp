#include "homeassistant.h"
#include "config.h"
#include "settings.h"
#include "display.h"
#include "metrics.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

namespace HomeMode {

struct Reading { bool tOk = false; float t = 0; bool hOk = false; float h = 0; };

// GET {haUrl}/api/states/{entity} with bearer token; out = numeric state.
static bool getState(const String &entity, float &out) {
    if (entity.length() == 0 || g_settings.haUrl.length() == 0 || g_settings.haToken.length() == 0) return false;
    if (WiFi.status() != WL_CONNECTED) return false;

    String url = g_settings.haUrl;
    while (url.endsWith("/")) url.remove(url.length() - 1);
    url += "/api/states/" + entity;

    HTTPClient http;
    http.setTimeout(6000);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    WiFiClient plain;
    WiFiClientSecure secure;
    bool begun;
    if (url.startsWith("https")) { secure.setInsecure(); begun = http.begin(secure, url); }
    else                        { begun = http.begin(plain, url); }
    if (!begun) return false;

    http.addHeader("Authorization", "Bearer " + g_settings.haToken);
    int rc = http.GET();
    if (rc != HTTP_CODE_OK) { log_e("HA %s -> HTTP %d", entity.c_str(), rc); http.end(); return false; }
    String payload = http.getString();
    http.end();

    JsonDocument doc;
    if (deserializeJson(doc, payload)) return false;
    String st = doc["state"].as<String>();
    if (st.length() == 0 || st == "unavailable" || st == "unknown") return false;
    out = st.toFloat();
    return true;
}

// ---------- metric-type catalog (keep keys in sync with web_assets.h) ----------
struct MetricType {
    const char *key;
    uint32_t    icon;
    const char *unit;
    int         decimals;
    bool        secondary;   // show entity2 as a "%" line (humidity-style)
};
static const MetricType TYPES[] = {
    {"room_living", Display::Icon::SOFA,        "°",     1, true},
    {"room_bed",    Display::Icon::BED,         "°",     1, true},
    {"room_down",   Display::Icon::STAIRS_DOWN, "°",     1, true},
    {"room_up",     Display::Icon::STAIRS_UP,   "°",     1, true},
    {"temperature", Display::Icon::THERMO,      "°",     1, false},
    {"humidity",    Display::Icon::WATER,       "%",     0, false},
    {"storage",     Display::Icon::HARDDISK,    "%",     0, false},
    {"storage_gb",  Display::Icon::HARDDISK,    " GB",   0, false},
    {"voltage",     Display::Icon::FLASH,       " V",    1, false},
    {"power",       Display::Icon::POWER_PLUG,  " W",    0, false},
    {"battery",     Display::Icon::BATTERY,     "%",     0, false},
    {"co2",         Display::Icon::CO2,         " ppm",  0, false},
    {"pressure",    Display::Icon::GAUGE,       " hPa",  0, false},
    {"custom",      Display::Icon::GAUGE,       "",      1, false},
};
static const MetricType &typeFor(const String &key) {
    for (auto &t : TYPES) if (key == t.key) return t;
    return TYPES[sizeof(TYPES) / sizeof(TYPES[0]) - 1];   // custom
}

static void drawDroplet(int x, int y, int s) {
    Display::drawCircle(x, y, s, 0);
    Display::line(x - s + 1, y - 1, x, y - 2 * s, 0);
    Display::line(x + s - 1, y - 1, x, y - 2 * s, 0);
}

static void drawTileCell(int cx, int cy, int cw, const String &label, const MetricType &mt,
                         bool ok, float v, bool secOk, float sv) {
    Display::icon(mt.icon, cx + 34, cy + 30, 0.6f);
    Display::text(cx + 66, cy + 24, Display::fitText(label, cw - 74, 0.62f), false, 0.62f);

    char val[24];
    if (ok) snprintf(val, sizeof(val), "%.*f%s", mt.decimals, v, mt.unit);
    else    snprintf(val, sizeof(val), "--");
    Display::text(cx + 16, cy + 100, String(val), true, 1.4f);

    if (mt.secondary && secOk) {
        drawDroplet(cx + 22, cy + 138, 6);
        char hh[12]; snprintf(hh, sizeof(hh), "%d%%", (int)(sv + 0.5f));
        Display::text(cx + 38, cy + 146, String(hh), false, 0.72f);
    }
}

void render(const String &ip) {
    (void)ip;
    Metrics::WeatherData wd = Metrics::fetchWeather();

    const int M = 24, W = SCREEN_W - 2 * M;
    Display::clearBuffer();
    int yTop = Metrics::drawTopBlock(wd);

    // ===== Metric tiles (2x2) =====
    Display::text(M, yTop + 30, "Home", true, 0.85f);
    Display::hLine(M, yTop + 40, W, 0);

    int gridTop = yTop + 60;
    int gridBot = SCREEN_H - 52;
    int cellW = W / 2, cellH = (gridBot - gridTop) / 2;
    Display::vLine(M + cellW, gridTop, gridBot - gridTop, 0x80);
    Display::hLine(M, gridTop + cellH, W, 0x80);
    for (int i = 0; i < NUM_ZONES; i++) {
        const MetricType &mt = typeFor(g_settings.tileType[i]);
        float v = 0, sv = 0;
        bool ok = getState(g_settings.tileEntity[i], v);
        bool secOk = mt.secondary && getState(g_settings.tileEntity2[i], sv);
        int col = i % 2, row = i / 2;
        drawTileCell(M + col * cellW + 6, gridTop + row * cellH, cellW - 12,
                     g_settings.tileLabel[i], mt, ok, v, secOk, sv);
    }

    Metrics::drawFooter();
    Display::commit();
}

}  // namespace HomeMode
