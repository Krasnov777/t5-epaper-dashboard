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

// ---------- zone icons (logical portrait coords) ----------
static void iconSofa(int cx, int cy, int s) {
    int w = 2 * s, h = s;
    Display::drawRect(cx - s, cy - h / 2 - 6, w, 10, 0);     // backrest
    Display::drawRect(cx - s, cy - 2, w, h, 0);              // seat block
    Display::fillRect(cx - s, cy - 8, 5, h + 6, 0);       // left arm
    Display::fillRect(cx + s - 5, cy - 8, 5, h + 6, 0);   // right arm
    Display::vLine(cx - s + 5, cy + h - 2, 5, 0);
    Display::vLine(cx + s - 5, cy + h - 2, 5, 0);
}
static void iconBed(int cx, int cy, int s) {
    int w = 2 * s, h = s;
    Display::fillRect(cx - s, cy - h / 2 - 4, 5, h + 10, 0);  // headboard
    Display::drawRect(cx - s, cy, w, h, 0);                       // mattress
    Display::drawRect(cx - s + 7, cy + 3, s - 2, h / 2, 0);       // pillow
    Display::vLine(cx - s + 2, cy + h, 5, 0);
    Display::vLine(cx + s - 2, cy + h, 5, 0);
}
static void iconStairs(int cx, int cy, int s, bool up) {
    int steps = 3, u = (2 * s) / steps;
    for (int k = 0; k < steps; k++) {
        int x = cx - s + k * u;
        int y = up ? (cy + s - k * u) : (cy - s + k * u);
        Display::hLine(x, y, u, 0);                       // tread
        if (up) Display::vLine(x, y - u, u, 0);           // riser
        else    Display::vLine(x + u, y, u, 0);
    }
    int ax = cx - s - 3;                                  // direction chevron
    if (up) { Display::line(ax, cy + s, ax + 5, cy + s - 6, 0); Display::line(ax + 5, cy + s - 6, ax + 10, cy + s, 0); }
    else    { Display::line(ax, cy - s, ax + 5, cy - s + 6, 0); Display::line(ax + 5, cy - s + 6, ax + 10, cy - s, 0); }
}
static void drawZoneIcon(int kind, int cx, int cy, int s) {
    switch (kind) {
        case 0: iconStairs(cx, cy, s, false); break;  // Downstairs
        case 1: iconSofa(cx, cy, s);          break;  // Living Room
        case 2: iconStairs(cx, cy, s, true);  break;  // Upstairs
        default: iconBed(cx, cy, s);          break;  // Bedroom
    }
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
