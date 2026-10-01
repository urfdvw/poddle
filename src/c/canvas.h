#pragma once

#include <pebble.h>

// The face is designed on a 168x144 landscape canvas. The sprite sheets hold
// every glyph and icon already rotated 90 degrees clockwise, so drawing is
// plain blitting: a canvas rect only has its origin moved onto the physical
// 144x168 screen (canvas_to_screen), never any pixels rotated.
#define CANVAS_W 168
#define CANVAS_H 144

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

void canvas_init(void);
void canvas_deinit(void);

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
