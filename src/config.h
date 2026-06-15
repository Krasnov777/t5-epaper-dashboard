#pragma once
#include <Arduino.h>

// Firmware version (shown in the UI / status, handy for confirming OTA worked).
#define FW_VERSION "1.9.0"

// ---- Display geometry (LilyGo T5 4.7" V2.3 / ED047TC1) ----
// The panel is physically 960 x 540, 4-bit grayscale (16 levels), addressed in
// landscape. We mount it VERTICALLY, so all drawing/layout uses a logical
// PORTRAIT canvas (540 wide x 960 tall) which is rotated 90 deg into the panel
// framebuffer (see display.cpp / the browser packer).
static constexpr int     PANEL_W = 960;     // native long axis
static constexpr int     PANEL_H = 540;     // native short axis
static constexpr int     SCREEN_W = 540;    // logical portrait width
static constexpr int     SCREEN_H = 960;    // logical portrait height

static constexpr size_t  FB_SIZE    = (size_t)PANEL_W / 2 * PANEL_H;  // 259200 bytes
// A "photo" file is exactly one panel framebuffer: 2 px/byte, even-x = low
// nibble, odd-x = high nibble, value 0..15 (0 = black, 15 = white).
static constexpr size_t  PHOTO_BYTES = FB_SIZE;

// Portrait mount direction. If the image comes out upside-down, flip this.
// 1 = board rotated clockwise, 0 = counter-clockwise.
#define PORTRAIT_CW 1

// ---- Filesystem layout (LittleFS) ----
#define PHOTO_DIR      "/photos"
#define SETTINGS_PATH  "/settings.json"

// ---- Soft-AP / onboarding ----
#define AP_SSID        "T5-Frame-Setup"
#define AP_PASSWORD    "lilygo123"        // >= 8 chars required by WPA2
#define MDNS_HOST      "t5frame"          // reachable as http://t5frame.local

// ---- Operating modes ----
enum DisplayMode : uint8_t {
    MODE_PHOTO   = 0,
    MODE_METRICS = 1,
    MODE_HOME    = 2,   // weather top + Home Assistant indoor zones
};

static constexpr int NUM_ZONES = 4;

// ---- Defaults ----
static constexpr uint32_t DEFAULT_SLIDESHOW_SEC    = 600;   // 10 min
static constexpr uint32_t DEFAULT_METRICS_REFRESH  = 15;    // minutes
static constexpr int      MAX_HEADLINES            = 8;     // fetched per feed (for rotation)
static constexpr int      FORECAST_DAYS            = 3;
