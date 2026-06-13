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
// Truncate s with an ellipsis to fit maxWidth px at the given scale.
String fitText(const String &s, int maxWidth, float scale = 1.0f);

}  // namespace Display
