#pragma once

#include <pebble.h>

// The face is laid out on a design canvas: the screen itself in portrait,
// or the screen turned sideways in landscape (e.g. 168x144 for a 144x168
// screen). Landscape loads sprite sheets whose glyphs and icons were
// rotated 90 degrees clockwise at build time, so a canvas rect only has its
// origin moved onto the physical screen; portrait loads
// the upright sheets and draws 1:1. Either way no pixels are rotated at
// runtime.
#define SCREEN_W PBL_DISPLAY_WIDTH
#define SCREEN_H PBL_DISPLAY_HEIGHT

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

// Loads the sheets for the orientation (reloads if it changed).
void canvas_init(Orientation orientation);
// Color theme: icons come from the tinted icon sheet (color platforms only;
// a no-op elsewhere).
void canvas_set_color_icons(bool enabled);
void canvas_deinit(void);
int canvas_width(void);
int canvas_height(void);

void canvas_fill_rect(GContext *ctx, int x, int y, int w, int h);

// Glyph runs, placed by their ink (left edge, right edge or center) with
// the cap top at canvas row cap_top.
void canvas_draw_left(GContext *ctx, const Glyph *run, int count, int ink_x, int cap_top);
void canvas_draw_right(GContext *ctx, const Glyph *run, int count, int ink_right, int cap_top);
void canvas_draw_centered(GContext *ctx, const Glyph *run, int count, int center_x,
                          int cap_top);
// Icons are drawn by their top-left corner.
void canvas_draw_icon(GContext *ctx, int icon, int x, int y);

#ifdef PBL_COLOR
// Vertical gradient (top row `top`, bottom row `bottom`), ordered-dithered
// between the two colors. Rendered once into a cached bitmap per rect.
void canvas_draw_gradient(GContext *ctx, GRect r, GColor top, GColor bottom);
#endif

// Converts a string of "0-9 : / -" into digit glyphs; returns the count.
int canvas_digits(const char *text, Glyph *out, int max);
