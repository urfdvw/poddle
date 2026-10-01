#include "canvas.h"

#include "assets.h"

static GBitmap *s_sheets[SHEET_COUNT];

static const uint32_t SHEET_RESOURCES[SHEET_COUNT] = {
  RESOURCE_ID_SHEET_DIGITS,
  RESOURCE_ID_SHEET_WEEKDAYS,
  RESOURCE_ID_SHEET_WORDS,
  RESOURCE_ID_SHEET_ICONS,
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

void canvas_init(void) {
  for (int i = 0; i < SHEET_COUNT; i++) {
    s_sheets[i] = gbitmap_create_with_resource(SHEET_RESOURCES[i]);
  }
}

void canvas_deinit(void) {
  for (int i = 0; i < SHEET_COUNT; i++) {
    gbitmap_destroy(s_sheets[i]);
    s_sheets[i] = NULL;
  }
}

GRect canvas_to_screen(GRect r) {
  return GRect(CANVAS_H - r.origin.y - r.size.h, r.origin.x, r.size.h, r.size.w);
}

void canvas_fill_rect(GContext *ctx, int x, int y, int w, int h) {
  if (w <= 0 || h <= 0) {
    return;
  }
  graphics_fill_rect(ctx, canvas_to_screen(GRect(x, y, w, h)), 0, GCornerNone);
}

static void prv_blit(GContext *ctx, int sheet, const SheetEntry *e, int x, int y) {
  GBitmap *bmp = s_sheets[sheet];
  if (!bmp) {
    return;
  }
  // In the sheet the entry is stored rotated: h columns wide, w rows tall.
  gbitmap_set_bounds(bmp, GRect(e->px, e->py, e->h, e->w));
  graphics_context_set_compositing_mode(ctx, GCompOpAnd);
  graphics_draw_bitmap_in_rect(ctx, bmp, canvas_to_screen(GRect(x, y, e->w, e->h)));
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
