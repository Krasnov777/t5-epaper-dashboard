# T5 Smart E-Paper Frame — Architecture

Internal developer documentation. For user-facing setup/usage see [`../README.md`](../README.md).
Public repo: <https://github.com/Krasnov777/t5-epaper-dashboard> · current firmware `FW_VERSION` 1.4.1.

---

## 1. Overview

Firmware for a **LilyGo T5 4.7" E-Paper V2.3 (ESP32-S3)** mounted **vertically**.
It's a Wi-Fi appliance with a built-in web UI and two display modes:

- **Photo frame** — browser-dithered images shown as a slideshow.
- **Metrics dashboard** — Open-Meteo weather + sunrise/sunset + two rotating RSS
  headlines from user-chosen feeds, drawn natively on the panel.

Design principles:
- **Self-contained**: the device fetches its own data; no companion server.
- **Heavy work in the browser**: image resize/dither/pack happens client-side,
  so the device only stores and blits ready framebuffers.
- **One-command deploy**: the web UI is embedded in PROGMEM; LittleFS holds only
  photos + `settings.json`. After the first USB flash, updates go over Wi-Fi (OTA).

---

## 2. Hardware

| | |
|---|---|
| Board | LilyGo T5 4.7" E-Paper **V2.3** (`boards/T5-ePaper-S3.json`) |
| MCU | ESP32-S3-WROOM-1-N16R8 — 16 MB flash, 8 MB PSRAM, native USB |
| Panel | ED047TC1, **960×540 landscape**, 16-level grayscale, parallel (epdiy-style) |
| Mount | **Portrait** → logical canvas is **540×960** |
| Button | `BUTTON_1` = GPIO21 (short press = toggle mode; 2.5 s hold = forget Wi-Fi) |

The 4.7" panel is **not** SPI/GxEPD2 — it's driven by the parallel `LilyGo-EPD47`
library (esp32s3 branch), which also bundles the FiraSans font and pin map
(`utilities.h`).

---

## 3. Build / flash / OTA

```bash
pio run -e T5-ePaper-S3              # build
pio run -e T5-ePaper-S3 -t upload    # USB flash (board on /dev/cu.usbmodem*)
pio device monitor                   # serial @115200
```

- A bare `pio run` builds **both** envs; the `-ota` env errors if handed a USB
  port — always pass `-e T5-ePaper-S3` for USB.
- **OTA (preferred after first flash):**
  - Web: Settings → Firmware update → pick `.pio/build/T5-ePaper-S3/firmware.bin`.
  - CLI: `curl -X POST --data-binary @firmware.bin http://t5frame.local/api/update`
  - PlatformIO espota: `pio run -e T5-ePaper-S3-ota -t upload` — **blocked by the
    macOS application firewall** (the device must connect back to the espota
    server). Allow incoming for Python or use web OTA instead.
- Dual-OTA partitions (`partitions_custom.csv`): app0/app1 (3.5 MB each) +
  ~8.8 MB LittleFS. A bad image falls back to the previous slot.
- USB: ESP32-S3 native USB needs a **USB 2.0 data cable** — Thunderbolt/charge-only
  cables don't enumerate it. Force download mode with BOOT+RST if needed.

---

## 4. Module map (`src/`)

| File | Responsibility |
|---|---|
| `main.cpp` | Boot, Wi-Fi onboarding (STA → SoftAP captive portal), NTP, mDNS, ArduinoOTA, button, loop |
| `config.h` | Geometry (panel vs logical), `PORTRAIT_CW`, defaults, `FW_VERSION` |
| `settings.{h,cpp}` | `Settings` struct ↔ `/settings.json` (LittleFS); legacy-key migration |
| `display.{h,cpp}` | **Portrait engine**: logical→panel transform, primitives, custom rotated/scaled text blitter |
| `storage.{h,cpp}` | LittleFS photo store (list/read/delete, FS usage) |
| `metrics.{h,cpp}` | Open-Meteo, reverse geocode, RSS, and the portrait dashboard layout + weather icons |
| `modes.{h,cpp}` | Mode state machine + slideshow / metrics-refresh timers |
| `web.{h,cpp}` | Async HTTP server: JSON API, photo upload, OTA upload |
| `web_assets.h` | Embedded SPA (`INDEX_HTML`) + onboarding page (`SETUP_HTML`) in PROGMEM |

---

## 5. Portrait rendering engine (`display.cpp`)

The panel framebuffer is **960×540** (4-bit, 2 px/byte). The library's text
functions can't rotate and hard-clip at `y < 540`, and FiraSans is zlib-compressed
— so we implement our own:

- **Coordinate transform** — all drawing uses logical portrait coords
  `(0..539, 0..959)`. `setPixel(x,y,v)` maps them to the native panel:
  ```
  PORTRAIT_CW: PX = y;            PY = PANEL_H-1 - x;
  else:        PX = PANEL_W-1 - y; PY = x;
  ```
  Flip `PORTRAIT_CW` (config.h) if the image is upside-down. **The browser photo
  packer mirrors this exact transform.**
- **Primitives** — pixel/line/rect/circle/triangle, all via `setPixel`.
- **Custom text blitter** — `get_glyph()` (public) + zlib `uncompress()` (linked
  via the bundled zlib) decode each FiraSans glyph; pixels are blitted through the
  rotation transform. Supports a `scale` arg (one font → many sizes): news ≈0.62,
  body ≈0.75–0.85, big temperature ≈1.5. Glyph format: `byte_width = w/2 + w%2`,
  4-bit alpha, even-x = low nibble; black ink value = `15 - bm`.

---

## 6. Photo pipeline

Browser (`web_assets.h process()`):
1. Draw the image into a **540×960** canvas (cover/contain), grayscale,
   brightness/contrast.
2. **Floyd–Steinberg dither** to 16 levels.
3. **Rotate + pack** into a 960×540 panel framebuffer using the same transform as
   `Display::setPixel` (`PORTRAIT_CW` kept in sync).
4. `POST /api/upload?name=x.bin` (raw `application/octet-stream`).

Device: stores the 259 200-byte blob in `/photos/`, and `Storage::readPhoto()`
reads it straight into the framebuffer for `Display::commit()`. No on-device
decode/rotation.

**Photo `.bin` format:** exactly `960*540/2 = 259200` bytes, 4-bit grayscale,
2 px/byte, **even-x = low nibble, odd-x = high nibble**, value `0..15`
(0 = black, 15 = white). Matches `epd_draw_grayscale_image()`.

---

## 7. Metrics pipeline (`metrics.cpp`)

Each render (every `metricsRefresh` minutes, default 15):

1. **Weather** — Open-Meteo `/v1/forecast` (current + 3-day daily +
   sunrise/sunset). No API key. Must use `http.getString()` (response is
   **chunked**; `getStream()` + ArduinoJson → `InvalidInput`) and
   `setFollowRedirects`.
2. **City** — BigDataCloud reverse-geocode (lat/lon → city), no key. Cached per
   coordinate; persisted into `settings.locationName`.
3. **News** — two user-configured RSS/Atom feeds (label + URL each, chosen from a
   category catalog or custom). Streamed first ~17 KB, naive
   `<item>…<title>` scan, entity decode. `setFollowRedirects` (feeds 301).
4. **Layout** (portrait 540×960): header (city + **next** sun event icon/time),
   big temperature + condition icon, humidity/wind, 3-day forecast (hand-drawn
   weather icons), then the **two configured news blocks** — one **rotating**
   word-wrapped headline each — and a centered footer with the update time. A
   static counter advances the headline index each refresh; the "next sun event"
   is sunrise/sunset/next-day-sunrise depending on the current time.

All HTTPS via `WiFiClientSecure::setInsecure()`.

---

## 8. Web server & API (`web.cpp`)

`ESPAsyncWebServer` on :80. Heavy work (display renders, reboot) is deferred from
the async-tcp task to `loop()` via flags (`Web::loopTasks()`).

| Method | Path | Body | Action |
|---|---|---|---|
| GET  | `/api/status` | — | settings, photos, ip, heap, fs, version, current photo |
| GET  | `/api/fb` | — | live 960×540 4-bit framebuffer (web UI → portrait PNG screenshot) |
| POST | `/api/upload?name=x.bin` | raw bytes | store a 259 200-byte framebuffer |
| POST | `/api/update` | raw `firmware.bin` | OTA (Update.h) then reboot |
| POST | `/api/photo/show` | `{name}` | pin + display |
| POST | `/api/photo/delete` | `{name}` | delete |
| POST | `/api/photo/cycle` | `{}` | un-pin → slideshow |
| POST | `/api/mode` | `{mode:0\|1}` | 0 photo / 1 metrics |
| POST | `/api/settings` | partial JSON | update + persist |
| POST | `/api/refresh` | `{}` | repaint current mode |
| POST | `/api/wifi` | `{wifiSsid,wifiPass}` | set Wi-Fi + reboot |
| POST | `/api/reboot` | `{}` | reboot |

> Upload route is `/api/upload`, **not** `/api/photo` — the latter prefix-collided
> with `/api/photo/*` in ESPAsyncWebServer's route matching.

**Screenshots:** `GET /api/fb` returns the raw framebuffer; the web UI (Settings →
📷 Screenshot) reverses the portrait transform and exports a PNG. The repo image
`docs/screenshot-metrics.png` was generated the same way via a pure-Python PNG
writer fetching `/api/fb`.

---

## 9. Boot & onboarding (`main.cpp`)

```
setup(): Display.begin → Storage.begin → settingsLoad
         hasWifi? → connectSTA (20 s) → startNormalMode | startApMode
normal : NTP (configTzTime) · mDNS (t5frame.local) · ArduinoOTA · web(false) · render
ap     : SoftAP "T5-Frame-Setup"/lilygo123 · DNS captive portal · web(true) · setup screen
loop()  : button · Web::loopTasks · Modes::tick · Wi-Fi auto-recover
```

---

## 10. Gotchas / lessons learned

- **USB cable**: ESP32-S3 native USB is USB 2.0 FS only; Thunderbolt/charge cables
  won't enumerate. Use BOOT+RST to force the ROM bootloader.
- **Lib deps**: must list `Wire` + `SPI` or SensorLib (transitive) fails on `SPI.h`.
- **Route collision**: `/api/photo` vs `/api/photo/*` — keep upload on `/api/upload`.
- **Open-Meteo chunked**: parse `getString()`, not `getStream()`.
- **Feeds 301**: `setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS)`.
- **Portrait text**: stock `writeln` can't rotate / clips at y<540 → custom blitter.
- **`pio run` builds both envs**: use `-e T5-ePaper-S3` for USB.
- **OTA version race**: the device answers `/api/status` for ~0.5 s on the old
  firmware before it reboots — re-poll after a moment to confirm the new version.
