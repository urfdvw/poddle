#pragma once

#include <pebble.h>

// The face is laid out on a design canvas: 144x168 in portrait (the screen
// itself) or 168x144 in landscape. Landscape loads sprite sheets whose
// glyphs and icons were rotated 90 degrees clockwise at build time, so a
// canvas rect only has its origin moved onto the physical 144x168 screen
// (canvas_to_screen); portrait loads the upright sheets and draws 1:1.
// Either way no pixels are rotated at runtime.
#define SCREEN_W 144
#define SCREEN_H 168

typedef enum {
  ORIENTATION_PORTRAIT = 0,
  ORIENTATION_LANDSCAPE = 1,
} Orientation;

typedef enum {
  SHEET_DIGITS,
  SHEET_WEEKDAYS,
  SHEET_WORDS,
  SHEET_ICONS,
  SHEET_COUNT,
  SHEET_SPACE = SHEET_COUNT,  // pseudo-entry: advance only
} SheetId;

typedef struct {
  uint8_t sheet;
  uint8_t index;
} Glyph;

typedef struct {
  int adv;    // pen advance of the whole run
  int ink_l;  // ink extent relative to the pen start: [ink_l, ink_r)
  int ink_r;
} RunMetrics;

// Loads the sheets for the orientation (reloads if it changed).
void canvas_init(Orientation orientation);
void canvas_deinit(void);
int canvas_width(void);
int canvas_height(void);

GRect canvas_to_screen(GRect r);
void canvas_fill_rect(GContext *ctx, int x, int y, int w, int h);

RunMetrics canvas_measure(const Glyph *run, int count);
// Draws a run with its cap top at canvas row cap_top.
void canvas_draw_run(GContext *ctx, const Glyph *run, int count, int pen_x, int cap_top);
void canvas_draw_left(GContext *ctx, const Glyph *run, int count, int ink_x, int cap_top);
void canvas_draw_right(GContext *ctx, const Glyph *run, int count, int ink_right, int cap_top);
void canvas_draw_centered(GContext *ctx, const Glyph *run, int count, int center_x,
                          int cap_top);
// Icons are drawn by their top-left corner.
void canvas_draw_icon(GContext *ctx, int icon, int x, int y);

// Converts a string of "0-9 : / -" into digit glyphs; returns the count.
int canvas_digits(const char *text, Glyph *out, int max);
