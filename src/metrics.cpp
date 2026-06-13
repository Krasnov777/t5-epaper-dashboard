#include "metrics.h"
#include "display.h"
#include "settings.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <math.h>

namespace Metrics {

const char *weatherDesc(int code) {
    switch (code) {
        case 0:  return "Clear";
        case 1:  return "Mostly clear";
        case 2:  return "Partly cloudy";
        case 3:  return "Overcast";
        case 45: case 48: return "Fog";
        case 51: case 53: case 55: return "Drizzle";
        case 56: case 57: return "Freezing drizzle";
        case 61: case 63: case 65: return "Rain";
        case 66: case 67: return "Freezing rain";
        case 71: case 73: case 75: return "Snow";
        case 77: return "Snow grains";
        case 80: case 81: case 82: return "Rain showers";
        case 85: case 86: return "Snow showers";
        case 95: return "Thunderstorm";
        case 96: case 99: return "Thunderstorm";
        default: return "—";
    }
}

// ---------- weather icons (drawn in logical portrait coords) ----------
static void iconSun(int cx, int cy, int s) {
    Display::drawCircle(cx, cy, s, 0);
    Display::drawCircle(cx, cy, s - 1, 0);
    for (int a = 0; a < 360; a += 45) {
        float r = a * 3.14159f / 180.0f;
        int x0 = cx + (int)((s + 5) * cosf(r)), y0 = cy + (int)((s + 5) * sinf(r));
        int x1 = cx + (int)((s + 13) * cosf(r)), y1 = cy + (int)((s + 13) * sinf(r));
        Display::line(x0, y0, x1, y1, 0);
    }
}

static void iconCloud(int cx, int cy, int s, uint8_t gray) {
    Display::fillCircle(cx - s, cy, s * 2 / 3, gray);
    Display::fillCircle(cx + s, cy, s * 3 / 4, gray);
    Display::fillCircle(cx, cy - s / 2, s, gray);
    Display::fillRect(cx - s, cy, 2 * s, s * 3 / 4, gray);
    // soft outline along the bottom
    Display::hLine(cx - s, cy + s * 3 / 4 - 1, 2 * s, 0);
}

static void drawWeatherIcon(int cx, int cy, int s, int code) {
    switch (code) {
        case 0:
            iconSun(cx, cy, s); break;
        case 1: case 2:
            iconSun(cx - s / 2, cy - s / 2, s * 2 / 3);
            iconCloud(cx + s / 3, cy + s / 4, s * 2 / 3, 0xC0); break;
        case 3: case 45: case 48:
            iconCloud(cx, cy, s, 0x90); break;
        case 51: case 53: case 55: case 61: case 63: case 65:
        case 80: case 81: case 82: case 66: case 67:
            iconCloud(cx, cy - s / 4, s * 3 / 4, 0xB0);
            for (int i = -1; i <= 1; i++)
                Display::line(cx + i * s / 2, cy + s / 2, cx + i * s / 2 - 5, cy + s, 0);
            break;
        case 71: case 73: case 75: case 77: case 85: case 86:
            iconCloud(cx, cy - s / 4, s * 3 / 4, 0xB0);
            for (int i = -1; i <= 1; i++) Display::fillCircle(cx + i * s / 2, cy + s * 3 / 4, 3, 0);
            break;
        case 95: case 96: case 99:
            iconCloud(cx, cy - s / 4, s * 3 / 4, 0x80);
            Display::line(cx, cy + s / 3, cx - 7, cy + s * 3 / 4, 0);
            Display::line(cx - 7, cy + s * 3 / 4, cx + 4, cy + s * 3 / 4, 0);
            Display::line(cx + 4, cy + s * 3 / 4, cx - 3, cy + s + 6, 0);
            break;
        default:
            iconCloud(cx, cy, s, 0xA0); break;
    }
}

// ---------- date helpers ----------
static int dow(int y, int m, int d) {
    static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (m < 3) y -= 1;
    return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
}
static String weekdayLabel(const String &iso, int index) {
    if (index == 0) return "Today";
    if (iso.length() < 10) return "?";
    int y = iso.substring(0, 4).toInt(), m = iso.substring(5, 7).toInt(), d = iso.substring(8, 10).toInt();
    static const char *n[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    return n[dow(y, m, d)];
}

WeatherData fetchWeather() {
    WeatherData wd;
    if (WiFi.status() != WL_CONNECTED) return wd;
    char url[320];
    snprintf(url, sizeof(url),
             "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
             "&current=temperature_2m,relative_humidity_2m,weather_code,wind_speed_10m"
             "&daily=weather_code,temperature_2m_max,temperature_2m_min,sunrise,sunset"
             "&timezone=auto&forecast_days=%d",
             g_settings.lat, g_settings.lon, FORECAST_DAYS);

    WiFiClientSecure client; client.setInsecure();
    HTTPClient http; http.setTimeout(8000);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    if (!http.begin(client, url)) return wd;
    int rc = http.GET();
    if (rc != HTTP_CODE_OK) { log_e("weather HTTP %d", rc); http.end(); return wd; }
    String payload = http.getString();   // getString() decodes chunked transfer
    http.end();
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) { log_e("weather JSON: %s", err.c_str()); return wd; }

    JsonObject cur = doc["current"];
    wd.temp     = cur["temperature_2m"] | 0.0f;
    wd.humidity = cur["relative_humidity_2m"] | 0;
    wd.code     = cur["weather_code"] | 0;
    wd.wind     = cur["wind_speed_10m"] | 0.0f;

    JsonObject daily = doc["daily"];
    JsonArray dates = daily["time"], codes = daily["weather_code"];
    JsonArray tmax = daily["temperature_2m_max"], tmin = daily["temperature_2m_min"];
    JsonArray sr = daily["sunrise"], ss = daily["sunset"];
    {   // sun times: ISO "2026-06-13T05:18" -> "05:18"
        String a = sr[0].as<String>(), b = ss[0].as<String>(), c = sr[1].as<String>();
        if (a.length() >= 16) wd.sunrise         = a.substring(11, 16);
        if (b.length() >= 16) wd.sunset          = b.substring(11, 16);
        if (c.length() >= 16) wd.sunriseTomorrow = c.substring(11, 16);
    }
    for (int i = 0; i < FORECAST_DAYS && i < (int)dates.size(); i++) {
        wd.days[i].label = weekdayLabel(dates[i].as<String>(), i);
        wd.days[i].code  = codes[i] | 0;
        wd.days[i].tMax  = tmax[i] | 0.0f;
        wd.days[i].tMin  = tmin[i] | 0.0f;
    }
    wd.ok = true;
    return wd;
}

static void decodeEntities(String &s) {
    s.replace("&amp;", "&"); s.replace("&quot;", "\""); s.replace("&#39;", "'");
    s.replace("&apos;", "'"); s.replace("&lt;", "<"); s.replace("&gt;", ">");
    s.replace("&#8217;", "'"); s.replace("&#8216;", "'"); s.replace("&#8211;", "-");
    s.replace("&#8230;", "…");
}

int fetchNews(const String &url, String out[], int maxItems) {
    if (WiFi.status() != WL_CONNECTED || url.length() == 0) return 0;
    WiFiClientSecure client; client.setInsecure();
    HTTPClient http; http.setTimeout(8000);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    if (!http.begin(client, url)) return 0;
    int rc = http.GET();
    if (rc != HTTP_CODE_OK) { log_e("news HTTP %d", rc); http.end(); return 0; }

    WiFiClient *stream = http.getStreamPtr();
    String buf; buf.reserve(18000);
    unsigned long t0 = millis();
    while (http.connected() && buf.length() < 17000 && millis() - t0 < 6000) {
        while (stream->available() && buf.length() < 17000) buf += (char)stream->read();
        delay(2);
    }
    http.end();

    int n = 0;
    int pos = buf.indexOf("<item");
    if (pos < 0) pos = buf.indexOf("<entry");
    if (pos < 0) pos = 0;
    while (n < maxItems) {
        int ts = buf.indexOf("<title", pos);
        if (ts < 0) break;
        int gt = buf.indexOf('>', ts);
        if (gt < 0) break;
        int te = buf.indexOf("</title>", gt);
        if (te < 0) break;
        String t = buf.substring(gt + 1, te);
        t.replace("<![CDATA[", ""); t.replace("]]>", "");
        decodeEntities(t); t.trim();
        if (t.length()) out[n++] = t;
        pos = te + 8;
    }
    return n;
}

// ---------- time ----------
static String nowDate() {
    struct tm ti;
    if (!getLocalTime(&ti, 200)) return "";
    char b[40]; strftime(b, sizeof(b), "%A %d %B", &ti); return String(b);
}
static String nowClock() {
    struct tm ti;
    if (!getLocalTime(&ti, 200)) return "--:--";
    char b[8]; strftime(b, sizeof(b), "%H:%M", &ti); return String(b);
}

// ---------- news block (single, word-wrapped, rotating headline) ----------
static constexpr float TITLE_SCALE = 0.85f;
static constexpr float NEWS_SCALE  = 0.80f;

// Draw a headline word-wrapped to maxW, up to maxLines. Returns y after.
static int drawWrapped(int x, int y, const String &s, int maxW, float sc, int maxLines) {
    int step = (int)(Display::lineHeight(sc) * 0.80f);
    String line, rest = s;
    int lines = 0;
    while (rest.length() && lines < maxLines) {
        int sp = rest.indexOf(' ');
        String word = (sp < 0) ? rest : rest.substring(0, sp);
        String cand = line.length() ? line + " " + word : word;
        if (Display::textWidth(cand, sc) <= maxW) {
            line = cand;
            rest = (sp < 0) ? "" : rest.substring(sp + 1);
        } else if (line.length() == 0) {            // single word too long
            line = Display::fitText(word, maxW, sc);
            rest = (sp < 0) ? "" : rest.substring(sp + 1);
        } else {                                    // flush current line
            bool last = (lines == maxLines - 1);
            Display::text(x, y, last ? Display::fitText(line + " " + rest, maxW, sc) : line, false, sc);
            y += step; lines++;
            if (last) return y;
            line = "";
        }
    }
    if (line.length() && lines < maxLines) { Display::text(x, y, line, false, sc); y += step; }
    return y;
}

static int drawNewsBlock(int y, const String &title, const String &headline, int maxW, int M) {
    Display::text(M, y, title, true, TITLE_SCALE);
    Display::hLine(M, y + 10, maxW, 0);
    y += 46;
    if (headline.length() == 0) { Display::text(M, y, "(no items)", false, NEWS_SCALE); return y + 38; }
    return drawWrapped(M, y, headline, maxW, NEWS_SCALE, 3);
}

static int nowMinutes() {
    struct tm ti;
    if (!getLocalTime(&ti, 200)) return -1;
    return ti.tm_hour * 60 + ti.tm_min;
}
static int hhmmToMin(const String &t) {
    if (t.length() < 5) return -1;
    return t.substring(0, 2).toInt() * 60 + t.substring(3, 5).toInt();
}

// A modern sun-on-horizon icon. rising=true -> outline sun + up arrow (sunrise);
// rising=false -> filled sun + down arrow (sunset). cx,cy = icon centre.
static void drawSunEvent(int cx, int cy, int s, bool rising) {
    int hy = cy + s / 3;             // horizon line height
    int r  = s;
    if (rising) { Display::drawCircle(cx, hy, r, 0); Display::drawCircle(cx, hy, r - 1, 0); }
    else        { Display::fillCircle(cx, hy, r, 0x40); }
    // clip everything below the horizon
    Display::fillRect(cx - r - 14, hy + 1, 2 * r + 28, r + 24, 255);
    // rays over the upper hemisphere
    const float dirs[5][2] = {{0, -1}, {-0.72f, -0.72f}, {0.72f, -0.72f}, {-1, -0.18f}, {1, -0.18f}};
    for (auto &d : dirs)
        Display::line(cx + (int)(d[0] * (r + 5)), hy + (int)(d[1] * (r + 5)),
                      cx + (int)(d[0] * (r + 13)), hy + (int)(d[1] * (r + 13)), 0);
    // horizon
    Display::hLine(cx - r - 8, hy, 2 * r + 16, 0);
    // direction arrow below the horizon
    int by = hy + 9;
    Display::vLine(cx, by, 12, 0);
    if (rising) { Display::line(cx - 5, by + 5, cx, by, 0);      Display::line(cx, by, cx + 5, by + 5, 0); }
    else        { Display::line(cx - 5, by + 7, cx, by + 12, 0); Display::line(cx, by + 12, cx + 5, by + 7, 0); }
}

// Draw the NEXT sun event (icon + label + time) right-aligned in the header.
static void drawNextSunEvent(const WeatherData &wd, int M) {
    int nm = nowMinutes();
    int sr = hhmmToMin(wd.sunrise), ss = hhmmToMin(wd.sunset);
    bool rising; String t;
    if (nm >= 0 && sr >= 0 && ss >= 0) {
        if (nm < sr)      { rising = true;  t = wd.sunrise; }
        else if (nm < ss) { rising = false; t = wd.sunset; }
        else              { rising = true;  t = wd.sunriseTomorrow.length() ? wd.sunriseTomorrow : wd.sunrise; }
    } else { rising = false; t = wd.sunset; }
    if (t.length() == 0) return;

    int cx = SCREEN_W - M - 26, cy = 54;
    drawSunEvent(cx, cy, 18, rising);
    int tx = cx - 32;
    Display::textRight(tx, 42, rising ? "Sunrise" : "Sunset", false, 0.62f);
    Display::textRight(tx, 80, t, true, 0.95f);
}

// Reverse-geocode lat/lon -> city name (BigDataCloud, free, no key).
static String reverseGeocode(float lat, float lon) {
    if (WiFi.status() != WL_CONNECTED) return "";
    char url[200];
    snprintf(url, sizeof(url),
             "https://api.bigdatacloud.net/data/reverse-geocode-client?latitude=%.4f&longitude=%.4f&localityLanguage=en",
             lat, lon);
    WiFiClientSecure client; client.setInsecure();
    HTTPClient http; http.setTimeout(8000);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    if (!http.begin(client, url)) return "";
    int rc = http.GET();
    if (rc != HTTP_CODE_OK) { http.end(); return ""; }
    String payload = http.getString();
    http.end();
    JsonDocument doc;
    if (deserializeJson(doc, payload)) return "";
    String c = doc["city"].as<String>();
    if (c.length() == 0) c = doc["locality"].as<String>();
    if (c.length() == 0) c = doc["principalSubdivision"].as<String>();
    return c;
}

// City for the header — resolved from coordinates, cached, persisted to settings.
static String cityName() {
    static String s_city;
    static float  s_lat = 999, s_lon = 999;
    if (s_city.length() == 0 || s_lat != g_settings.lat || s_lon != g_settings.lon) {
        String c = reverseGeocode(g_settings.lat, g_settings.lon);
        if (c.length()) {
            s_city = c; s_lat = g_settings.lat; s_lon = g_settings.lon;
            if (c != g_settings.locationName) { g_settings.locationName = c; settingsSave(); }
        }
    }
    return s_city.length() ? s_city : g_settings.locationName;
}

void render(const String &ip) {
    static uint32_t s_rot = 0;     // rotates the displayed headline each refresh

    WeatherData wd = fetchWeather();
    String a[MAX_HEADLINES], b[MAX_HEADLINES];
    int nA = fetchNews(g_settings.news1Url, a, MAX_HEADLINES);
    int nB = fetchNews(g_settings.news2Url, b, MAX_HEADLINES);
    String line1 = nA ? a[s_rot % nA] : String();
    String line2 = nB ? b[s_rot % nB] : String();

    const int M = 24;
    const int W = SCREEN_W - 2 * M;   // 492

    Display::clearBuffer();

    // ===== Header =====
    Display::text(M, 50, cityName(), true);
    String d = nowDate();
    if (d.length()) Display::text(M, 88, d, false, 0.62f);
    if (wd.ok) drawNextSunEvent(wd, M);
    Display::hLine(M, 102, W, 0);

    // ===== Current weather =====
    if (wd.ok) {
        drawWeatherIcon(M + 55, 185, 44, wd.code);
        char t[16]; snprintf(t, sizeof(t), "%.0f°C", wd.temp);
        Display::text(M + 135, 178, String(t), true, 1.5f);       // big temperature
        Display::text(M + 135, 225, weatherDesc(wd.code), false, 0.85f);
        char e[40];
        snprintf(e, sizeof(e), "Humidity  %d%%", wd.humidity);
        Display::text(M, 282, String(e), false, 0.75f);
        snprintf(e, sizeof(e), "Wind  %.0f km/h", wd.wind);
        Display::textRight(SCREEN_W - M, 282, String(e), false, 0.75f);
    } else {
        Display::text(M, 185, "Weather unavailable", false, 0.85f);
        Display::text(M, 225, "check Wi-Fi / location", false, 0.7f);
    }

    // ===== 3-day forecast =====
    Display::hLine(M, 300, W, 0);
    if (wd.ok) {
        int colW = W / FORECAST_DAYS;
        for (int i = 0; i < FORECAST_DAYS; i++) {
            int cx = M + i * colW + colW / 2;
            Display::textCentered(M + i * colW, 340, colW, wd.days[i].label, true, 0.72f);
            drawWeatherIcon(cx, 378, 16, wd.days[i].code);
            char r[24]; snprintf(r, sizeof(r), "%.0f° / %.0f°", wd.days[i].tMax, wd.days[i].tMin);
            Display::textCentered(M + i * colW, 432, colW, String(r), false, 0.68f);
        }
    }

    // ===== News (one rotating headline per block) =====
    int y = 500;
    y = drawNewsBlock(y, g_settings.news1Label, line1, W, M);
    y += 30;
    y = drawNewsBlock(y, g_settings.news2Label, line2, W, M);

    // ===== Footer (centered update time) =====
    (void)ip;
    Display::hLine(M, SCREEN_H - 40, W, 0);
    Display::textCentered(0, SCREEN_H - 14, SCREEN_W, "Updated " + nowClock(), false, 0.6f);

    Display::commit();
    s_rot++;   // advance headline rotation for the next refresh
}

}  // namespace Metrics
