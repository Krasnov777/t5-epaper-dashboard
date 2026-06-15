#pragma once
#include <Arduino.h>
#include "config.h"

// Portrait rendering layer over LilyGo-EPD47.
//
// All coordinates are LOGICAL PORTRAIT (0..SCREEN_W-1, 0..SCREEN_H-1 =
// 540 x 960). They are rotated 90 deg into the native 960x540 panel
// framebuffer. Text is drawn with a custom rotated blitter (the stock library
// can't rotate and hard-clips at y<540), reusing the bundled FiraSans font.
namespace Display {

bool begin();
uint8_t *fb();                 // raw panel framebuffer (FB_SIZE bytes)

void clearBuffer();            // fill white (no panel I/O)
void flashClean();             // flash panel clean (de-ghost)
void render();                 // push framebuffer to panel
void commit();                 // clean + push

// ---- primitives (logical portrait coords; color 0=black..255=white) ----
void setPixel(int x, int y, uint8_t val4 /*0..15*/);
void fillRect(int x, int y, int w, int h, uint8_t color);
void drawRect(int x, int y, int w, int h, uint8_t color);
void hLine(int x, int y, int w, uint8_t color);
void vLine(int x, int y, int h, uint8_t color);
void line(int x0, int y0, int x1, int y1, uint8_t color);
void drawCircle(int cx, int cy, int r, uint8_t color);
void fillCircle(int cx, int cy, int r, uint8_t color);

// ---- text (default FiraSans; y is the baseline; scale<1 shrinks glyphs) ----
int  text(int x, int y, const String &s, bool bold = false, float scale = 1.0f);
void textCentered(int x, int y, int w, const String &s, bool bold = false, float scale = 1.0f);
void textRight(int rightX, int y, const String &s, bool bold = false, float scale = 1.0f);
int  textWidth(const String &s, float scale = 1.0f);
int  lineHeight(float scale = 1.0f);   // font line advance * scale

// ---- icons (Material Design Icons subset; the glyph box is centred at cx,cy) ----
void icon(uint32_t codepoint, int cx, int cy, float scale);
namespace Icon {
enum : uint32_t {
    HOME = 0xF02DC, BED = 0xF02E3, SOFA = 0xF04B9, THERMO = 0xF050F, HUMIDITY = 0xF058E,
    CLOUDY = 0xF0590, FOG = 0xF0591, LIGHTNING = 0xF0593, PARTLY = 0xF0595, POURING = 0xF0596,
    RAINY = 0xF0597, SNOWY = 0xF0598, SUNNY = 0xF0599, SUNSET_DOWN = 0xF059B, SUNSET_UP = 0xF059C,
    LIGHTNING_RAINY = 0xF067E, STAIRS_UP = 0xF12BD, STAIRS_DOWN = 0xF12BE,
};
}
// Truncate s with an ellipsis to fit maxWidth px at the given scale.
String fitText(const String &s, int maxWidth, float scale = 1.0f);

}  // namespace Display
