#pragma once
#include <Arduino.h>
#include "config.h"

// Weather (Open-Meteo, no API key) + news (RSS) fetching and native rendering.
namespace Metrics {

struct DayForecast {
    String  label;     // "Today", "Mon", ...
    int     code = 0;
    float   tMax = 0;
    float   tMin = 0;
};

struct WeatherData {
    bool   ok = false;
    float  temp = 0;
    int    humidity = 0;
    int    code = 0;
    float  wind = 0;   // km/h
    String sunrise;          // today "HH:MM"
    String sunset;           // today "HH:MM"
    String sunriseTomorrow;  // tomorrow "HH:MM"
    DayForecast days[FORECAST_DAYS];
};

// Short human description for a WMO weather code.
const char *weatherDesc(int code);

// Fetch current weather + forecast. Returns ok=false on network/parse error.
WeatherData fetchWeather();

// Fetch up to maxItems RSS/Atom item titles from `url` into `out`. Returns count.
int fetchNews(const String &url, String out[], int maxItems);

// Compose the metrics screen into the framebuffer and push it to the panel.
// `ip` is shown in the footer.
void render(const String &ip);

// Shared building blocks (also used by the Home/zones mode):
// draws header + current weather + 3-day forecast into the (cleared) buffer and
// returns the y where the content area below begins.
int  drawTopBlock(const WeatherData &wd);
// Centered "Updated HH:MM" footer.
void drawFooter();

}  // namespace Metrics
