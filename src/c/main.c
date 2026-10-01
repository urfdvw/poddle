#include <pebble.h>

#include "assets.h"
#include "canvas.h"
#include "labels.h"
#include "time_words.h"

// Layout on the design canvas (y values are cap tops for text). Rows above
// the spoken time hang from the top, the progress row from the bottom;
// x positions follow the canvas width.
//
// The spoken-time block (hour cap top to AM/PM baseline) is centered in the
// space between the date row and the progress track, lifted slightly the
// way the mockup sits it.
#if ASSET_LARGE
// Pebble Time 2 (200x228), 22px sprites (13px caps): the base layout scaled
// by 22/16 and rounded.
#define MARGIN_TEXT 11
#define STATUS_CAP_Y 16
#define STATUS_ICON_Y 17
#define STATUS_ICON_X 11
#define BATTERY_INSET 33  // 25px body ends 8px from the right, nub hangs past it
#define SEPARATOR_Y 46
#define DATE_CAP_Y 50
#define TRACK_X 10
#define TRACK_INSET_BOTTOM 38
#define TRACK_H 7
#define LABEL_INSET_BOTTOM 28
#define WORDS_REGION_TOP 69
#define WORDS_BLOCK_H 67
#define WORDS_MINUTE_DY 26
#define WORDS_AMPM_DY 54
#define WORDS_LIFT 3
#define BATTERY_FILL_DX 2
#define BATTERY_FILL_DY 2
#define BATTERY_FILL_W 21
#define BATTERY_FILL_H 8
#define BATTERY_INNER_W 23  // color theme: fill the whole interior
#define BATTERY_INNER_H 10
#else
// 144x168 screens, 16px sprites (9px caps). Values measured off the mockup:
// Frames 1-2 for portrait, Frame 3 for landscape; the spoken-time caps land
// at y=69/88/108 in portrait and 57/76/96 in landscape.
#define MARGIN_TEXT 8  // text ink keeps 8px from either edge
#define STATUS_CAP_Y 11
#define STATUS_ICON_Y 12  // 9px icons centered on the 34px status row
#define STATUS_ICON_X 8
#define BATTERY_INSET 24  // 18px body ends 6px from the right, nub hangs past it
#define SEPARATOR_Y 33
#define DATE_CAP_Y 36
#define TRACK_X 7
#define TRACK_INSET_BOTTOM 28
#define TRACK_H 5
#define LABEL_INSET_BOTTOM 20
#define WORDS_REGION_TOP 50
#define WORDS_BLOCK_H 48
#define WORDS_MINUTE_DY 19
#define WORDS_AMPM_DY 39
#define WORDS_LIFT 2
// Battery fill area inside the BATTERY icon.
#define BATTERY_FILL_DX 2
#define BATTERY_FILL_DY 2
#define BATTERY_FILL_W 14
#define BATTERY_FILL_H 5
#define BATTERY_INNER_W 17  // color theme: fill the whole interior
#define BATTERY_INNER_H 7
#endif

#define PERSIST_KEY_PROGRESS_MODE 1
#define PERSIST_KEY_LABEL_FORMAT 2
#define PERSIST_KEY_ORIENTATION 3
#define PERSIST_KEY_THEME 4

// Theme (Clay). The color theme only exists on color screens.
typedef enum {
  THEME_BW = 0,
  THEME_COLOR = 1,
} Theme;

#ifdef PBL_COLOR
// Silver title bar: white fading to light gray (the reference bar runs
// #feffff -> #b1b6b9; these are the nearest palette colors).
#define COLOR_STATUS_TOP GColorWhite
#define COLOR_STATUS_BOTTOM GColorLightGray
#define COLOR_ACCENT GColorPictonBlue  // quiet-time icon (tinted sheet) and progress
// Battery: dark gray frame (in the tinted icon sheet), the charge split into
// a light upper half and a darker lower half, after the reference's
// #A5E07F fill under its highlight/shade gradient.
#define COLOR_BATTERY_TOP GColorMintGreen
#define COLOR_BATTERY_BOTTOM GColorMayGreen
#define COLOR_BATTERY_EMPTY GColorDarkGray  // the reference's #54585b
#endif

static Window *s_window;
static ProgressMode s_progress_mode = PROGRESS_MODE_HOUR;
static LabelFormat s_label_format = LABEL_FORMAT_ELAPSED;
static Orientation s_orientation = ORIENTATION_PORTRAIT;
static Theme s_theme = THEME_BW;
static uint8_t s_battery_percent = 100;
static bool s_connected = true;
static TimeUnits s_tick_units;

static bool prv_is_24h(void) {
#ifdef DEMO_24H
  return DEMO_24H;
#endif
  return clock_is_24h_style();
}

static void prv_draw_text(GContext *ctx, const char *text, int x, int cap_y, GAlign align) {
  Glyph run[LABEL_BUF_SIZE];
  int n = canvas_digits(text, run, LABEL_BUF_SIZE);
  if (align == GAlignLeft) {
    canvas_draw_left(ctx, run, n, x, cap_y);
  } else if (align == GAlignRight) {
    canvas_draw_right(ctx, run, n, x, cap_y);
  } else {
    canvas_draw_centered(ctx, run, n, x, cap_y);
  }
}

static int prv_line_glyphs(const TwLine *line, Glyph *out) {
  for (int i = 0; i < line->count; i++) {
    uint8_t t = line->tokens[i];
    if (t == TW_SPACE) {
      out[i] = (Glyph){SHEET_SPACE, 0};
    } else if (t == TW_HYPHEN) {
      out[i] = (Glyph){SHEET_DIGITS, DIGIT_MINUS};
    } else {
      out[i] = (Glyph){SHEET_WORDS, t};
    }
  }
  return line->count;
}

// Aplite's frozen SDK stubs quiet_time_is_active() to false (its firmware
// predates Quiet Time), so the icon simply never shows "quiet" there.
static bool prv_quiet_time(void) {
#ifdef DEMO_QUIET
  return DEMO_QUIET;
#endif
  return quiet_time_is_active();
}

static bool prv_color_theme(void) {
#ifdef PBL_COLOR
  return s_theme == THEME_COLOR;
#else
  return false;
#endif
}

static void prv_draw_status(GContext *ctx, const struct tm *t) {
  const int w = canvas_width();
  const bool color = prv_color_theme();
#ifdef PBL_COLOR
  if (color) {
    // Gray gradient down to the separator row, which it replaces.
    canvas_draw_gradient(ctx, GRect(0, 0, w, SEPARATOR_Y + 1), COLOR_STATUS_TOP,
                         COLOR_STATUS_BOTTOM);
  }
#endif

  // One top-left icon: disconnected while the phone is away, otherwise the
  // quiet-time state.
  int status_icon = ICON_DISCONNECTED;
  if (s_connected) {
    status_icon = prv_quiet_time() ? ICON_QUIET_ON : ICON_QUIET_OFF;
  }
  canvas_draw_icon(ctx, status_icon, STATUS_ICON_X, STATUS_ICON_Y);

  char buf[LABEL_BUF_SIZE];
  format_clock(buf, t->tm_hour, t->tm_min, prv_is_24h());
  prv_draw_text(ctx, buf, w / 2, STATUS_CAP_Y, GAlignCenter);

  const int battery_x = w - BATTERY_INSET;
  canvas_draw_icon(ctx, ICON_BATTERY, battery_x, STATUS_ICON_Y);
#ifdef PBL_COLOR
  if (color) {
    const int fill = (s_battery_percent * BATTERY_INNER_W + 50) / 100;
    const int top_h = (BATTERY_INNER_H + 1) / 2;
    graphics_context_set_fill_color(ctx, COLOR_BATTERY_EMPTY);
    canvas_fill_rect(ctx, battery_x + 1, STATUS_ICON_Y + 1, BATTERY_INNER_W, BATTERY_INNER_H);
    graphics_context_set_fill_color(ctx, COLOR_BATTERY_TOP);
    canvas_fill_rect(ctx, battery_x + 1, STATUS_ICON_Y + 1, fill, top_h);
    graphics_context_set_fill_color(ctx, COLOR_BATTERY_BOTTOM);
    canvas_fill_rect(ctx, battery_x + 1, STATUS_ICON_Y + 1 + top_h, fill,
                     BATTERY_INNER_H - top_h);
    graphics_context_set_fill_color(ctx, GColorBlack);
  } else
#endif
  {
    const int fill = (s_battery_percent * BATTERY_FILL_W + 50) / 100;
    canvas_fill_rect(ctx, battery_x + BATTERY_FILL_DX, STATUS_ICON_Y + BATTERY_FILL_DY, fill,
                     BATTERY_FILL_H);
  }

  if (!color) {
    canvas_fill_rect(ctx, 0, SEPARATOR_Y, w, 1);
  }
}

static void prv_draw_date(GContext *ctx, const struct tm *t) {
  char buf[LABEL_BUF_SIZE];
  format_date(buf, t->tm_mon + 1, t->tm_mday);
  prv_draw_text(ctx, buf, MARGIN_TEXT, DATE_CAP_Y, GAlignLeft);

  Glyph wday = {SHEET_WEEKDAYS, (uint8_t)(WDAY_SU + t->tm_wday)};
  canvas_draw_right(ctx, &wday, 1, canvas_width() - MARGIN_TEXT, DATE_CAP_Y);
}

static void prv_draw_words(GContext *ctx, const struct tm *t) {
  TwPhrase phrase;
  time_words(t->tm_hour, t->tm_min, &phrase);

  const int region_h = canvas_height() - TRACK_INSET_BOTTOM - WORDS_REGION_TOP;
  const int hour_y = WORDS_REGION_TOP + (region_h - WORDS_BLOCK_H) / 2 - WORDS_LIFT;
  const int center_x = canvas_width() / 2;
  Glyph run[TW_MAX_TOKENS];
  int n = prv_line_glyphs(&phrase.hour, run);
  canvas_draw_centered(ctx, run, n, center_x, hour_y);
  n = prv_line_glyphs(&phrase.minute, run);
  canvas_draw_centered(ctx, run, n, center_x, hour_y + WORDS_MINUTE_DY);

  Glyph ampm = {SHEET_WORDS, phrase.ampm};
  canvas_draw_centered(ctx, &ampm, 1, center_x, hour_y + WORDS_AMPM_DY);
}

static void prv_draw_progress(GContext *ctx, const struct tm *t) {
  ProgressInfo info;
  progress_info(s_progress_mode, s_label_format, t->tm_hour, t->tm_min, t->tm_sec,
                prv_is_24h(), &info);

  const int w = canvas_width();
  const int h = canvas_height();
  const int track_w = w - 2 * TRACK_X;
  const int track_y = h - TRACK_INSET_BOTTOM;
  const int label_y = h - LABEL_INSET_BOTTOM;

  // Outline with clipped corners, then the fill over the left edge.
  canvas_fill_rect(ctx, TRACK_X + 1, track_y, track_w - 2, 1);
  canvas_fill_rect(ctx, TRACK_X + 1, track_y + TRACK_H - 1, track_w - 2, 1);
  canvas_fill_rect(ctx, TRACK_X, track_y + 1, 1, TRACK_H - 2);
  canvas_fill_rect(ctx, TRACK_X + track_w - 1, track_y + 1, 1, TRACK_H - 2);
  const int fill_w = track_w * info.num / info.den;
#ifdef PBL_COLOR
  if (prv_color_theme()) {
    // Inside the outline only, so the black frame stays intact.
    graphics_context_set_fill_color(ctx, COLOR_ACCENT);
    int inner = fill_w - 1;
    if (inner > track_w - 2) {
      inner = track_w - 2;
    }
    canvas_fill_rect(ctx, TRACK_X + 1, track_y + 1, inner, TRACK_H - 2);
    graphics_context_set_fill_color(ctx, GColorBlack);
  } else
#endif
  {
    canvas_fill_rect(ctx, TRACK_X, track_y + 1, fill_w, TRACK_H - 2);
  }

  prv_draw_text(ctx, info.left, MARGIN_TEXT, label_y, GAlignLeft);
  prv_draw_text(ctx, info.right, w - MARGIN_TEXT, label_y, GAlignRight);
}

static void prv_update_proc(Layer *layer, GContext *ctx) {
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
#ifdef DEMO_HOUR
  // Screenshot builds: pin the clock (2026-10-01 is a Thursday).
  t->tm_year = 2026 - 1900;
  t->tm_mon = DEMO_MON - 1;
  t->tm_mday = DEMO_MDAY;
  t->tm_wday = DEMO_WDAY;
  t->tm_hour = DEMO_HOUR;
  t->tm_min = DEMO_MIN;
  t->tm_sec = DEMO_SEC;
#endif

  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);
  graphics_context_set_fill_color(ctx, GColorBlack);

  prv_draw_status(ctx, t);
  prv_draw_date(ctx, t);
  prv_draw_words(ctx, t);
  prv_draw_progress(ctx, t);
}

static void prv_tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  layer_mark_dirty(window_get_root_layer(s_window));
}

static void prv_subscribe_ticks(void) {
  TimeUnits units =
      progress_needs_seconds(s_progress_mode, s_label_format) ? SECOND_UNIT : MINUTE_UNIT;
  if (units != s_tick_units) {
    tick_timer_service_subscribe(units, prv_tick_handler);
    s_tick_units = units;
  }
}

static void prv_connection_handler(bool connected) {
  s_connected = connected;
#ifdef DEMO_DISCONNECTED
  s_connected = !DEMO_DISCONNECTED;
#endif
  if (s_window) {
    layer_mark_dirty(window_get_root_layer(s_window));
  }
}

static void prv_battery_handler(BatteryChargeState state) {
  s_battery_percent = state.charge_percent;
#ifdef DEMO_BATTERY
  s_battery_percent = DEMO_BATTERY;
#endif
  if (s_window) {
    layer_mark_dirty(window_get_root_layer(s_window));
  }
}

static int32_t prv_tuple_int(const Tuple *t) {
  // Clay sends select values as strings.
  if (t->type == TUPLE_CSTRING) {
    return atoi(t->value->cstring);
  }
  return t->value->int32;
}

static void prv_inbox_received(DictionaryIterator *iter, void *context) {
  Tuple *mode = dict_find(iter, MESSAGE_KEY_ProgressMode);
  if (mode) {
    s_progress_mode =
        prv_tuple_int(mode) == PROGRESS_MODE_MINUTE ? PROGRESS_MODE_MINUTE : PROGRESS_MODE_HOUR;
    persist_write_int(PERSIST_KEY_PROGRESS_MODE, s_progress_mode);
  }
  Tuple *format = dict_find(iter, MESSAGE_KEY_LabelFormat);
  if (format) {
    s_label_format = prv_tuple_int(format) == LABEL_FORMAT_SEGMENT ? LABEL_FORMAT_SEGMENT
                                                                    : LABEL_FORMAT_ELAPSED;
    persist_write_int(PERSIST_KEY_LABEL_FORMAT, s_label_format);
  }
  Tuple *orientation = dict_find(iter, MESSAGE_KEY_Orientation);
  if (orientation) {
    s_orientation = prv_tuple_int(orientation) == ORIENTATION_LANDSCAPE ? ORIENTATION_LANDSCAPE
                                                                         : ORIENTATION_PORTRAIT;
    persist_write_int(PERSIST_KEY_ORIENTATION, s_orientation);
    canvas_init(s_orientation);
  }
  Tuple *theme = dict_find(iter, MESSAGE_KEY_Theme);
  if (theme) {
    s_theme = prv_tuple_int(theme) == THEME_COLOR ? THEME_COLOR : THEME_BW;
    persist_write_int(PERSIST_KEY_THEME, s_theme);
    canvas_set_color_icons(prv_color_theme());
  }
  prv_subscribe_ticks();
  layer_mark_dirty(window_get_root_layer(s_window));
}

static void prv_load_settings(void) {
#ifdef DEMO_PROGRESS_MODE
  s_progress_mode = DEMO_PROGRESS_MODE;
  s_label_format = DEMO_LABEL_FORMAT;
  s_orientation = DEMO_ORIENTATION;
  s_theme = DEMO_THEME;
  return;
#endif
  if (persist_exists(PERSIST_KEY_PROGRESS_MODE)) {
    s_progress_mode = persist_read_int(PERSIST_KEY_PROGRESS_MODE) == PROGRESS_MODE_MINUTE
                          ? PROGRESS_MODE_MINUTE
                          : PROGRESS_MODE_HOUR;
  }
  if (persist_exists(PERSIST_KEY_LABEL_FORMAT)) {
    s_label_format = persist_read_int(PERSIST_KEY_LABEL_FORMAT) == LABEL_FORMAT_SEGMENT
                         ? LABEL_FORMAT_SEGMENT
                         : LABEL_FORMAT_ELAPSED;
  }
  if (persist_exists(PERSIST_KEY_ORIENTATION)) {
    s_orientation = persist_read_int(PERSIST_KEY_ORIENTATION) == ORIENTATION_LANDSCAPE
                        ? ORIENTATION_LANDSCAPE
                        : ORIENTATION_PORTRAIT;
  }
  if (persist_exists(PERSIST_KEY_THEME)) {
    s_theme = persist_read_int(PERSIST_KEY_THEME) == THEME_COLOR ? THEME_COLOR : THEME_BW;
  }
}

static void prv_init(void) {
  prv_load_settings();
  canvas_init(s_orientation);
  canvas_set_color_icons(prv_color_theme());

  s_window = window_create();
  layer_set_update_proc(window_get_root_layer(s_window), prv_update_proc);
  window_stack_push(s_window, false);

  prv_battery_handler(battery_state_service_peek());
  battery_state_service_subscribe(prv_battery_handler);
  prv_connection_handler(connection_service_peek_pebble_app_connection());
  connection_service_subscribe((ConnectionHandlers){
    .pebble_app_connection_handler = prv_connection_handler,
  });
  prv_subscribe_ticks();

  app_message_register_inbox_received(prv_inbox_received);
  app_message_open(128, 64);
}

static void prv_deinit(void) {
  tick_timer_service_unsubscribe();
  battery_state_service_unsubscribe();
  connection_service_unsubscribe();
  window_destroy(s_window);
  canvas_deinit();
}

int main(void) {
  prv_init();
  app_event_loop();
  prv_deinit();
}
