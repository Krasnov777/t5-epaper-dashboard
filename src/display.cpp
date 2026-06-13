#include "display.h"
#include "epd_driver.h"
#include "firasans.h"          // bundled FiraSans GFXfont (compressed)
#include <esp_heap_caps.h>

// zlib symbol (linked via LilyGo-EPD47's bundled zlib) — used to decompress
// FiraSans glyph bitmaps so we can blit them rotated.
extern "C" int uncompress(unsigned char *dest, unsigned long *destLen,
                          const unsigned char *source, unsigned long sourceLen);

namespace Display {

static uint8_t *s_fb = nullptr;
static const GFXfont *FONT = &FiraSans;

bool begin() {
    epd_init();
    s_fb = (uint8_t *)heap_caps_malloc(FB_SIZE, MALLOC_CAP_SPIRAM);
    if (!s_fb) {
        log_e("Framebuffer alloc failed (%u bytes PSRAM)", (unsigned)FB_SIZE);
        return false;
    }
    clearBuffer();
    return true;
}

uint8_t *fb() { return s_fb; }
void clearBuffer() { if (s_fb) memset(s_fb, 0xFF, FB_SIZE); }

void flashClean() {
    epd_poweron(); epd_clear(); epd_poweroff();
}
void render() {
    if (!s_fb) return;
    Rect_t area = {0, 0, PANEL_W, PANEL_H};
    epd_poweron(); epd_draw_grayscale_image(area, s_fb); epd_poweroff();
}
void commit() {
    if (!s_fb) return;
    Rect_t area = {0, 0, PANEL_W, PANEL_H};
    epd_poweron(); epd_clear(); epd_draw_grayscale_image(area, s_fb); epd_poweroff();
}

// ---- portrait pixel: logical (x,y) -> native panel framebuffer ----
void setPixel(int x, int y, uint8_t v) {
    if (!s_fb || x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H) return;
#if PORTRAIT_CW
    int PX = y;
    int PY = PANEL_H - 1 - x;
#else
    int PX = PANEL_W - 1 - y;
    int PY = x;
#endif
    size_t idx = (size_t)PY * (PANEL_W / 2) + (PX >> 1);
    if (PX & 1) s_fb[idx] = (s_fb[idx] & 0x0F) | (v << 4);
    else        s_fb[idx] = (s_fb[idx] & 0xF0) | (v & 0x0F);
}

void fillRect(int x, int y, int w, int h, uint8_t color) {
    uint8_t v = color >> 4;
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++) setPixel(x + i, y + j, v);
}
void drawRect(int x, int y, int w, int h, uint8_t color) {
    hLine(x, y, w, color); hLine(x, y + h - 1, w, color);
    vLine(x, y, h, color); vLine(x + w - 1, y, h, color);
}
void hLine(int x, int y, int w, uint8_t color) { fillRect(x, y, w, 1, color); }
void vLine(int x, int y, int h, uint8_t color) { fillRect(x, y, 1, h, color); }

void line(int x0, int y0, int x1, int y1, uint8_t color) {
    uint8_t v = color >> 4;
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        setPixel(x0, y0, v);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void drawCircle(int cx, int cy, int r, uint8_t color) {
    uint8_t v = color >> 4;
    int x = r, y = 0, err = 1 - r;
    while (x >= y) {
        setPixel(cx + x, cy + y, v); setPixel(cx - x, cy + y, v);
        setPixel(cx + x, cy - y, v); setPixel(cx - x, cy - y, v);
        setPixel(cx + y, cy + x, v); setPixel(cx - y, cy + x, v);
        setPixel(cx + y, cy - x, v); setPixel(cx - y, cy - x, v);
        y++;
        if (err < 0) err += 2 * y + 1;
        else { x--; err += 2 * (y - x) + 1; }
    }
}
void fillCircle(int cx, int cy, int r, uint8_t color) {
    uint8_t v = color >> 4;
    for (int j = -r; j <= r; j++)
        for (int i = -r; i <= r; i++)
            if (i * i + j * j <= r * r) setPixel(cx + i, cy + j, v);
}

// ---- text ----
static uint32_t utf8Next(const char *&p) {
    uint8_t c = (uint8_t)*p++;
    if (c < 0x80) return c;
    uint32_t cp; int n;
    if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; n = 1; }
    else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; n = 2; }
    else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; n = 3; }
    else return c;  // invalid lead byte
    for (int i = 0; i < n; i++) {
        if ((*p & 0xC0) != 0x80) return cp;
        cp = (cp << 6) | (*p++ & 0x3F);
    }
    return cp;
}

static void drawGlyph(GFXglyph *g, int penX, int baseY, float scale) {
    int bw = g->width / 2 + g->width % 2;
    unsigned long sz = (unsigned long)bw * g->height;
    if (sz == 0) return;
    uint8_t *bmp;
    bool comp = FONT->compressed;
    if (comp) {
        bmp = (uint8_t *)malloc(sz);
        if (!bmp) return;
        uncompress(bmp, &sz, &FONT->bitmap[g->data_offset], g->compressed_size);
    } else {
        bmp = (uint8_t *)&FONT->bitmap[g->data_offset];
    }
    if (scale == 1.0f) {
        for (int gy = 0; gy < g->height; gy++) {
            int ly = baseY - g->top + gy;
            for (int gx = 0; gx < g->width; gx++) {
                uint8_t byte = bmp[gy * bw + gx / 2];
                uint8_t bm = (gx & 1) ? (byte >> 4) : (byte & 0x0F);
                if (bm) setPixel(penX + g->left + gx, ly, 15 - bm);
            }
        }
    } else {
        int ow = (int)(g->width * scale + 0.5f);
        int oh = (int)(g->height * scale + 0.5f);
        int ox0 = penX + (int)(g->left * scale + 0.5f);
        int oy0 = baseY - (int)(g->top * scale + 0.5f);
        for (int oy = 0; oy < oh; oy++) {
            int gy = (int)(oy / scale);
            if (gy >= g->height) gy = g->height - 1;
            for (int ox = 0; ox < ow; ox++) {
                int gx = (int)(ox / scale);
                if (gx >= g->width) gx = g->width - 1;
                uint8_t byte = bmp[gy * bw + gx / 2];
                uint8_t bm = (gx & 1) ? (byte >> 4) : (byte & 0x0F);
                if (bm) setPixel(ox0 + ox, oy0 + oy, 15 - bm);
            }
        }
    }
    if (comp) free(bmp);
}

int text(int x, int y, const String &s, bool bold, float scale) {
    if (!s_fb) return x;
    const char *p = s.c_str();
    float cx = x;
    while (*p) {
        uint32_t cp = utf8Next(p);
        GFXglyph *g = nullptr;
        get_glyph(FONT, cp, &g);
        if (!g) continue;
        drawGlyph(g, (int)(cx + 0.5f), y, scale);
        if (bold) drawGlyph(g, (int)(cx + 0.5f) + 1, y, scale);
        cx += g->advance_x * scale;
    }
    return (int)(cx + 0.5f);
}

int textWidth(const String &s, float scale) {
    const char *p = s.c_str();
    float w = 0;
    while (*p) {
        uint32_t cp = utf8Next(p);
        GFXglyph *g = nullptr;
        get_glyph(FONT, cp, &g);
        if (g) w += g->advance_x * scale;
    }
    return (int)(w + 0.5f);
}

int lineHeight(float scale) { return (int)(FONT->advance_y * scale + 0.5f); }

void textCentered(int x, int y, int w, const String &s, bool bold, float scale) {
    text(x + (w - textWidth(s, scale)) / 2, y, s, bold, scale);
}
void textRight(int rightX, int y, const String &s, bool bold, float scale) {
    text(rightX - textWidth(s, scale), y, s, bold, scale);
}

String fitText(const String &s, int maxWidth, float scale) {
    if (textWidth(s, scale) <= maxWidth) return s;
    String t = s;
    while (t.length() > 1 && textWidth(t + "…", scale) > maxWidth) t.remove(t.length() - 1);
    t.trim();
    return t + "…";
}

}  // namespace Display
