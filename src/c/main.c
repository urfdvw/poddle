#include <pebble.h>

#include "assets.h"
#include "canvas.h"
#include "labels.h"
#include "time_words.h"

// Layout on the 168x144 design canvas (y values are cap tops for text).
#define MARGIN_TEXT 8  // text ink starts at x=8 and ends before x=160
#define CENTER_X (CANVAS_W / 2)

#define STATUS_CAP_Y 11
#define STATUS_ICON_Y 12  // 9px icons centered on the 34px status row
#define STATUS_ICON_X 8
#define BATTERY_X 144  // 18px body ends at x=161, nub hangs past it
#define SEPARATOR_Y 33

#define DATE_CAP_Y 36

#define HOUR_CAP_Y 57
#define MINUTE_CAP_Y 76
#define AMPM_CAP_Y 96

#define TRACK_X 7
#define TRACK_Y 116
#define TRACK_W 154
#define TRACK_H 5
#define PROGRESS_LABEL_CAP_Y 124

// Battery fill area inside the BATTERY icon.
#define BATTERY_FILL_DX 2
#define BATTERY_FILL_DY 2
#define BATTERY_FILL_W 14
#define BATTERY_FILL_H 5

#define PERSIST_KEY_PROGRESS_MODE 1
#define PERSIST_KEY_LABEL_FORMAT 2

static Window *s_window;
static ProgressMode s_progress_mode = PROGRESS_MODE_HOUR;
static LabelFormat s_label_format = LABEL_FORMAT_ELAPSED;
static uint8_t s_battery_percent = 100;
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

static void prv_draw_status(GContext *ctx, const struct tm *t) {
  canvas_draw_icon(ctx, prv_quiet_time() ? ICON_QUIET_ON : ICON_QUIET_OFF, STATUS_ICON_X,
                   STATUS_ICON_Y);

  char buf[LABEL_BUF_SIZE];
  format_clock(buf, t->tm_hour, t->tm_min, prv_is_24h());
  prv_draw_text(ctx, buf, CENTER_X, STATUS_CAP_Y, GAlignCenter);

  canvas_draw_icon(ctx, ICON_BATTERY, BATTERY_X, STATUS_ICON_Y);
  int fill = (s_battery_percent * BATTERY_FILL_W + 50) / 100;
  canvas_fill_rect(ctx, BATTERY_X + BATTERY_FILL_DX, STATUS_ICON_Y + BATTERY_FILL_DY, fill,
                   BATTERY_FILL_H);

  canvas_fill_rect(ctx, 0, SEPARATOR_Y, CANVAS_W, 1);
}

static void prv_draw_date(GContext *ctx, const struct tm *t) {
  char buf[LABEL_BUF_SIZE];
  format_date(buf, t->tm_mon + 1, t->tm_mday);
  prv_draw_text(ctx, buf, MARGIN_TEXT, DATE_CAP_Y, GAlignLeft);

  Glyph wday = {SHEET_WEEKDAYS, (uint8_t)(WDAY_SU + t->tm_wday)};
  canvas_draw_right(ctx, &wday, 1, CANVAS_W - MARGIN_TEXT, DATE_CAP_Y);
}

static void prv_draw_words(GContext *ctx, const struct tm *t) {
  TwPhrase phrase;
  time_words(t->tm_hour, t->tm_min, &phrase);

  Glyph run[TW_MAX_TOKENS];
  int n = prv_line_glyphs(&phrase.hour, run);
  canvas_draw_centered(ctx, run, n, CENTER_X, HOUR_CAP_Y);
  n = prv_line_glyphs(&phrase.minute, run);
  canvas_draw_centered(ctx, run, n, CENTER_X, MINUTE_CAP_Y);

  Glyph ampm = {SHEET_WORDS, phrase.ampm};
  canvas_draw_centered(ctx, &ampm, 1, CENTER_X, AMPM_CAP_Y);
}

static void prv_draw_progress(GContext *ctx, const struct tm *t) {
  ProgressInfo info;
  progress_info(s_progress_mode, s_label_format, t->tm_hour, t->tm_min, t->tm_sec,
                prv_is_24h(), &info);

  // Outline with clipped corners, then the fill over the left edge.
  canvas_fill_rect(ctx, TRACK_X + 1, TRACK_Y, TRACK_W - 2, 1);
  canvas_fill_rect(ctx, TRACK_X + 1, TRACK_Y + TRACK_H - 1, TRACK_W - 2, 1);
  canvas_fill_rect(ctx, TRACK_X, TRACK_Y + 1, 1, TRACK_H - 2);
  canvas_fill_rect(ctx, TRACK_X + TRACK_W - 1, TRACK_Y + 1, 1, TRACK_H - 2);
  canvas_fill_rect(ctx, TRACK_X, TRACK_Y + 1, TRACK_W * info.num / info.den, TRACK_H - 2);

  prv_draw_text(ctx, info.left, MARGIN_TEXT, PROGRESS_LABEL_CAP_Y, GAlignLeft);
  prv_draw_text(ctx, info.right, CANVAS_W - MARGIN_TEXT, PROGRESS_LABEL_CAP_Y, GAlignRight);
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
  prv_subscribe_ticks();
  layer_mark_dirty(window_get_root_layer(s_window));
}

static void prv_load_settings(void) {
#ifdef DEMO_PROGRESS_MODE
  s_progress_mode = DEMO_PROGRESS_MODE;
  s_label_format = DEMO_LABEL_FORMAT;
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
}

static void prv_init(void) {
  prv_load_settings();
  canvas_init();

  s_window = window_create();
  layer_set_update_proc(window_get_root_layer(s_window), prv_update_proc);
  window_stack_push(s_window, false);

  prv_battery_handler(battery_state_service_peek());
  battery_state_service_subscribe(prv_battery_handler);
  prv_subscribe_ticks();

  app_message_register_inbox_received(prv_inbox_received);
  app_message_open(128, 64);
}

static void prv_deinit(void) {
  tick_timer_service_unsubscribe();
  battery_state_service_unsubscribe();
  window_destroy(s_window);
  canvas_deinit();
}

int main(void) {
  prv_init();
  app_event_loop();
  prv_deinit();
}
