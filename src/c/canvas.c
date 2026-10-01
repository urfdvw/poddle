#include "canvas.h"

#include "assets.h"

static GBitmap *s_sheets[SHEET_COUNT];
static int16_t s_sheet_h[SHEET_COUNT];  // portrait sheets: full height, before set_bounds
static Orientation s_orientation = ORIENTATION_PORTRAIT;
static bool s_loaded;
#ifdef PBL_COLOR
static GBitmap *s_color_icons;  // tinted icons, see ASSET_ICON_COLOR_ENTRIES
static int16_t s_color_icons_h;
static bool s_use_color_icons;
static GBitmap *s_gradient;
static GRect s_gradient_rect;
static GColor s_gradient_colors[2];

static const uint32_t COLOR_ICON_RESOURCES[2] = {
  [ORIENTATION_PORTRAIT] = RESOURCE_ID_SHEET_ICONS_COLOR_PORTRAIT,
  [ORIENTATION_LANDSCAPE] = RESOURCE_ID_SHEET_ICONS_COLOR,
};
#endif

static const uint32_t SHEET_RESOURCES[2][SHEET_COUNT] = {
  [ORIENTATION_PORTRAIT] = {
    RESOURCE_ID_SHEET_DIGITS_PORTRAIT,
    RESOURCE_ID_SHEET_WEEKDAYS_PORTRAIT,
    RESOURCE_ID_SHEET_WORDS_PORTRAIT,
    RESOURCE_ID_SHEET_ICONS_PORTRAIT,
  },
  [ORIENTATION_LANDSCAPE] = {
    RESOURCE_ID_SHEET_DIGITS,
    RESOURCE_ID_SHEET_WEEKDAYS,
    RESOURCE_ID_SHEET_WORDS,
    RESOURCE_ID_SHEET_ICONS,
  },
};

static const SheetEntry *const SHEET_ENTRIES[SHEET_COUNT] = {
  ASSET_DIGIT_ENTRIES,
  ASSET_WDAY_ENTRIES,
  ASSET_WORD_ENTRIES,
  ASSET_ICON_ENTRIES,
};

static const int8_t SHEET_CAP_OFFSET[SHEET_COUNT] = {
  ASSET_DIGIT_CAP_OFFSET,
  ASSET_WDAY_CAP_OFFSET,
  ASSET_WORD_CAP_OFFSET,
  0,
};

void canvas_init(Orientation orientation) {
  if (s_loaded && orientation == s_orientation) {
    return;
  }
  canvas_deinit();
  s_orientation = orientation;
  for (int i = 0; i < SHEET_COUNT; i++) {
    s_sheets[i] = gbitmap_create_with_resource(SHEET_RESOURCES[orientation][i]);
    s_sheet_h[i] = s_sheets[i] ? gbitmap_get_bounds(s_sheets[i]).size.h : 0;
  }
#ifdef PBL_COLOR
  s_color_icons = gbitmap_create_with_resource(COLOR_ICON_RESOURCES[orientation]);
  s_color_icons_h = s_color_icons ? gbitmap_get_bounds(s_color_icons).size.h : 0;
#endif
  s_loaded = true;
}

void canvas_set_color_icons(bool enabled) {
#ifdef PBL_COLOR
  s_use_color_icons = enabled;
#endif
}

void canvas_deinit(void) {
  for (int i = 0; i < SHEET_COUNT; i++) {
    if (s_sheets[i]) {
      gbitmap_destroy(s_sheets[i]);
      s_sheets[i] = NULL;
    }
  }
#ifdef PBL_COLOR
  if (s_color_icons) {
    gbitmap_destroy(s_color_icons);
    s_color_icons = NULL;
  }
  if (s_gradient) {
    gbitmap_destroy(s_gradient);
    s_gradient = NULL;
  }
#endif
  s_loaded = false;
}

int canvas_width(void) {
  return s_orientation == ORIENTATION_LANDSCAPE ? SCREEN_H : SCREEN_W;
}

int canvas_height(void) {
  return s_orientation == ORIENTATION_LANDSCAPE ? SCREEN_W : SCREEN_H;
}

GRect canvas_to_screen(GRect r) {
  if (s_orientation == ORIENTATION_PORTRAIT) {
    return r;
  }
  return GRect(SCREEN_W - r.origin.y - r.size.h, r.origin.x, r.size.h, r.size.w);
}

void canvas_fill_rect(GContext *ctx, int x, int y, int w, int h) {
  if (w <= 0 || h <= 0) {
    return;
  }
  graphics_fill_rect(ctx, canvas_to_screen(GRect(x, y, w, h)), 0, GCornerNone);
}

static void prv_blit_bitmap(GContext *ctx, GBitmap *bmp, int sheet_h, GCompOp op,
                            const SheetEntry *e, int x, int y) {
  if (!bmp) {
    return;
  }
  if (s_orientation == ORIENTATION_LANDSCAPE) {
    // Rotated sheet: the entry is h columns wide, w rows tall.
    gbitmap_set_bounds(bmp, GRect(e->px, e->py, e->h, e->w));
  } else {
    // Upright sheet: the rotated sheet turned back, so its columns are rows.
    gbitmap_set_bounds(bmp, GRect(e->py, sheet_h - e->px - e->h, e->w, e->h));
  }
  graphics_context_set_compositing_mode(ctx, op);
  graphics_draw_bitmap_in_rect(ctx, bmp, canvas_to_screen(GRect(x, y, e->w, e->h)));
}

static void prv_blit(GContext *ctx, int sheet, const SheetEntry *e, int x, int y) {
  // 1-bit sheets: only the black ink lands.
  prv_blit_bitmap(ctx, s_sheets[sheet], s_sheet_h[sheet], GCompOpAnd, e, x, y);
}

static int prv_adv(const Glyph *g) {
  if (g->sheet == SHEET_SPACE) {
    return ASSET_SPACE_ADVANCE;
  }
  return SHEET_ENTRIES[g->sheet][g->index].adv;
}

RunMetrics canvas_measure(const Glyph *run, int count) {
  RunMetrics m = {0, 0, 0};
  for (int i = 0; i < count; i++) {
    m.adv += prv_adv(&run[i]);
  }
  if (count > 0) {
    if (run[0].sheet != SHEET_SPACE) {
      m.ink_l = SHEET_ENTRIES[run[0].sheet][run[0].index].lsb;
    }
    m.ink_r = m.adv;
    if (run[count - 1].sheet != SHEET_SPACE) {
      m.ink_r -= SHEET_ENTRIES[run[count - 1].sheet][run[count - 1].index].rsb;
    }
  }
  return m;
}

void canvas_draw_run(GContext *ctx, const Glyph *run, int count, int pen_x, int cap_top) {
  for (int i = 0; i < count; i++) {
    const Glyph *g = &run[i];
    if (g->sheet != SHEET_SPACE) {
      const SheetEntry *e = &SHEET_ENTRIES[g->sheet][g->index];
      prv_blit(ctx, g->sheet, e, pen_x + e->ox, cap_top - SHEET_CAP_OFFSET[g->sheet]);
    }
    pen_x += prv_adv(g);
  }
}

void canvas_draw_left(GContext *ctx, const Glyph *run, int count, int ink_x, int cap_top) {
  RunMetrics m = canvas_measure(run, count);
  canvas_draw_run(ctx, run, count, ink_x - m.ink_l, cap_top);
}

void canvas_draw_right(GContext *ctx, const Glyph *run, int count, int ink_right,
                       int cap_top) {
  RunMetrics m = canvas_measure(run, count);
  canvas_draw_run(ctx, run, count, ink_right - m.ink_r, cap_top);
}

void canvas_draw_centered(GContext *ctx, const Glyph *run, int count, int center_x,
                          int cap_top) {
  RunMetrics m = canvas_measure(run, count);
  canvas_draw_run(ctx, run, count, center_x - (m.ink_l + m.ink_r) / 2, cap_top);
}

void canvas_draw_icon(GContext *ctx, int icon, int x, int y) {
#ifdef PBL_COLOR
  if (s_use_color_icons && s_color_icons) {
    // Tinted sheet: palettized with transparency, each icon padded by 1px
    // for its halo.
    const SheetEntry *e = &ASSET_ICON_COLOR_ENTRIES[icon];
    prv_blit_bitmap(ctx, s_color_icons, s_color_icons_h, GCompOpSet, e, x + e->ox, y - 1);
    return;
  }
#endif
  prv_blit(ctx, SHEET_ICONS, &ASSET_ICON_ENTRIES[icon], x, y);
}

int canvas_digits(const char *text, Glyph *out, int max) {
  int n = 0;
  for (; *text && n < max; text++) {
    char c = *text;
    int index;
    if (c >= '0' && c <= '9') {
      index = DIGIT_0 + (c - '0');
    } else if (c == ':') {
      index = DIGIT_COLON;
    } else if (c == '/') {
      index = DIGIT_SLASH;
    } else if (c == '-') {
      index = DIGIT_MINUS;
    } else {
      continue;
    }
    out[n++] = (Glyph){SHEET_DIGITS, (uint8_t)index};
  }
  return n;
}

#ifdef PBL_COLOR
// 4x4 Bayer matrix for ordered dithering.
static const uint8_t BAYER4[4][4] = {
  {0, 8, 2, 10},
  {12, 4, 14, 6},
  {3, 11, 1, 9},
  {15, 7, 13, 5},
};

static void prv_render_gradient(GRect r, GColor top, GColor bottom) {
  GRect screen = canvas_to_screen(r);
  if (s_gradient) {
    gbitmap_destroy(s_gradient);
  }
  s_gradient = gbitmap_create_blank(screen.size, GBitmapFormat8Bit);
  s_gradient_rect = r;
  s_gradient_colors[0] = top;
  s_gradient_colors[1] = bottom;
  if (!s_gradient) {
    return;
  }
  uint8_t *data = gbitmap_get_data(s_gradient);
  const int stride = gbitmap_get_bytes_per_row(s_gradient);
  const int steps = r.size.h > 1 ? r.size.h - 1 : 1;
  for (int row = 0; row < r.size.h; row++) {
    const int level = row * 16 / steps;  // 0 = all top, 16 = all bottom
    for (int col = 0; col < r.size.w; col++) {
      const int cx = r.origin.x + col;
      const int cy = r.origin.y + row;
      const GColor c = BAYER4[cy & 3][cx & 3] < level ? bottom : top;
      // Same placement as canvas_to_screen, per pixel within the bitmap.
      int bx = col, by = row;
      if (s_orientation == ORIENTATION_LANDSCAPE) {
        bx = r.size.h - 1 - row;
        by = col;
      }
      data[by * stride + bx] = c.argb;
    }
  }
}

void canvas_draw_gradient(GContext *ctx, GRect r, GColor top, GColor bottom) {
  if (!s_gradient || !grect_equal(&r, &s_gradient_rect) ||
      !gcolor_equal(top, s_gradient_colors[0]) || !gcolor_equal(bottom, s_gradient_colors[1])) {
    prv_render_gradient(r, top, bottom);
  }
  if (s_gradient) {
    graphics_context_set_compositing_mode(ctx, GCompOpAssign);
    graphics_draw_bitmap_in_rect(ctx, s_gradient, canvas_to_screen(r));
  }
}
#endif
