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

// ---------- zone icons (Material Design Icons, by zone index) ----------
static void drawZoneIcon(int kind, int cx, int cy, int s) {
    uint32_t cp;
    switch (kind) {
        case 0:  cp = Display::Icon::STAIRS_DOWN; break;  // Downstairs
        case 1:  cp = Display::Icon::SOFA;        break;  // Living Room
        case 2:  cp = Display::Icon::STAIRS_UP;   break;  // Upstairs
        default: cp = Display::Icon::BED;         break;  // Bedroom
    }
    Display::icon(cp, cx, cy, s / 30.0f);
}
static void drawDroplet(int x, int y, int s) {
    Display::drawCircle(x, y, s, 0);
    Display::line(x - s + 1, y - 1, x, y - 2 * s, 0);
    Display::line(x + s - 1, y - 1, x, y - 2 * s, 0);
}

static void drawZoneCell(int cx, int cy, int cw, const String &label, int kind, const Reading &r) {
    drawZoneIcon(kind, cx + 32, cy + 30, 18);
    Display::text(cx + 64, cy + 24, Display::fitText(label, cw - 72, 0.62f), false, 0.62f);

    char t[16];
    if (r.tOk) snprintf(t, sizeof(t), "%.1f°", r.t); else snprintf(t, sizeof(t), "--");
    Display::text(cx + 16, cy + 100, String(t), true, 1.4f);

    if (r.hOk) {
        drawDroplet(cx + 22, cy + 138, 6);
        char hh[12]; snprintf(hh, sizeof(hh), "%d%%", (int)(r.h + 0.5f));
        Display::text(cx + 38, cy + 146, String(hh), false, 0.72f);
    }
}

void render(const String &ip) {
    (void)ip;
    Metrics::WeatherData wd = Metrics::fetchWeather();

    Reading z[NUM_ZONES];
    for (int i = 0; i < NUM_ZONES; i++) {
        z[i].tOk = getState(g_settings.zoneTemp[i], z[i].t);
        z[i].hOk = getState(g_settings.zoneHum[i],  z[i].h);
    }

    const int M = 24, W = SCREEN_W - 2 * M;
    Display::clearBuffer();
    int yTop = Metrics::drawTopBlock(wd);

    // ===== Indoor zones (2x2) =====
    Display::text(M, yTop + 30, "Indoor", true, 0.85f);
    Display::hLine(M, yTop + 40, W, 0);

    int gridTop = yTop + 60;
    int gridBot = SCREEN_H - 52;
    int cellW = W / 2, cellH = (gridBot - gridTop) / 2;
    Display::vLine(M + cellW, gridTop, gridBot - gridTop, 0x80);          // column separator (gray)
    Display::hLine(M, gridTop + cellH, W, 0x80);                          // row separator (gray)
    for (int i = 0; i < NUM_ZONES; i++) {
        int col = i % 2, row = i / 2;
        drawZoneCell(M + col * cellW + 6, gridTop + row * cellH, cellW - 12,
                     g_settings.zoneLabel[i], i, z[i]);
    }

    Metrics::drawFooter();
    Display::commit();
}

}  // namespace HomeMode
