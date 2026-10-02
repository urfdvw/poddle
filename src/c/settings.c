#include "settings.h"

#define PERSIST_KEY_PROGRESS_MODE 1
#define PERSIST_KEY_LABEL_FORMAT 2
#define PERSIST_KEY_ORIENTATION 3
#define PERSIST_KEY_THEME 4
#define PERSIST_KEY_UPDATE_SCHEDULE 5
#define PERSIST_KEY_UPDATE_INTERVAL 6
#define PERSIST_KEY_PERIOD 7  // the whole PeriodConfig
#define PERSIST_KEY_STEP_TARGET 8

// Each value is normalized the same way whether it comes from storage or
// from the config page: anything unexpected falls back to the default.
static ProgressMode prv_progress_mode(int32_t v) {
#if defined(PBL_HEALTH)
  if (v == PROGRESS_MODE_STEPS) {
    return PROGRESS_MODE_STEPS;
  }
#endif
  // Watches without Health (aplite) never offer steps; hour mode it is.
  return v == PROGRESS_MODE_MINUTE ? PROGRESS_MODE_MINUTE : PROGRESS_MODE_HOUR;
}

// Blank, non-numeric or below 1: the default; huge values are capped.
static int32_t prv_step_target(int32_t v) {
  if (v < 1) {
    return STEP_TARGET_DEFAULT;
  }
  return v > STEP_TARGET_MAX ? STEP_TARGET_MAX : v;
}

static LabelFormat prv_label_format(int32_t v) {
  return v == LABEL_FORMAT_SEGMENT ? LABEL_FORMAT_SEGMENT : LABEL_FORMAT_ELAPSED;
}

static Orientation prv_orientation(int32_t v) {
  return v == ORIENTATION_LANDSCAPE ? ORIENTATION_LANDSCAPE : ORIENTATION_PORTRAIT;
}

static Theme prv_theme(int32_t v) {
  return v == THEME_COLOR ? THEME_COLOR : THEME_BW;
}

static UpdateSchedule prv_update_schedule(int32_t v) {
  return v == UPDATE_SCHEDULE_RANDOM ? UPDATE_SCHEDULE_RANDOM : UPDATE_SCHEDULE_EXACT;
}

static PeriodRepeat prv_period_repeat(int32_t v) {
  return v >= PERIOD_REPEAT_DATE && v <= PERIOD_REPEAT_DAILY ? (PeriodRepeat)v
                                                             : PERIOD_REPEAT_OFF;
}

void settings_load(Settings *s) {
  *s = (Settings){
    .progress_mode = PROGRESS_MODE_HOUR,
    .label_format = LABEL_FORMAT_ELAPSED,
    .orientation = ORIENTATION_PORTRAIT,
    .theme = THEME_BW,
    .update_schedule = UPDATE_SCHEDULE_EXACT,
    .update_interval = UPDATE_INTERVAL_DEFAULT,
    .step_target = STEP_TARGET_DEFAULT,
    .period = {
      .repeat = PERIOD_REPEAT_OFF,
      .weekdays = 0x3e,  // Monday to Friday
      .start_min = 9 * 60,
      .end_min = 17 * 60,
      .format = LABEL_FORMAT_ELAPSED,
    },
  };
#ifdef DEMO_PROGRESS_MODE
  s->progress_mode = prv_progress_mode(DEMO_PROGRESS_MODE);
  s->label_format = DEMO_LABEL_FORMAT;
  s->orientation = DEMO_ORIENTATION;
  s->theme = DEMO_THEME;
#ifdef DEMO_STEP_TARGET
  s->step_target = DEMO_STEP_TARGET;
#endif
#ifdef DEMO_PERIOD_START
  s->period.repeat = PERIOD_REPEAT_DAILY;
  s->period.start_min = DEMO_PERIOD_START;
  s->period.end_min = DEMO_PERIOD_END;
  s->period.format = DEMO_PERIOD_FORMAT;
#endif
  return;
#endif
  if (persist_exists(PERSIST_KEY_PROGRESS_MODE)) {
    s->progress_mode = prv_progress_mode(persist_read_int(PERSIST_KEY_PROGRESS_MODE));
  }
  if (persist_exists(PERSIST_KEY_LABEL_FORMAT)) {
    s->label_format = prv_label_format(persist_read_int(PERSIST_KEY_LABEL_FORMAT));
  }
  if (persist_exists(PERSIST_KEY_ORIENTATION)) {
    s->orientation = prv_orientation(persist_read_int(PERSIST_KEY_ORIENTATION));
  }
  if (persist_exists(PERSIST_KEY_THEME)) {
    s->theme = prv_theme(persist_read_int(PERSIST_KEY_THEME));
  }
  if (persist_exists(PERSIST_KEY_UPDATE_SCHEDULE)) {
    s->update_schedule = prv_update_schedule(persist_read_int(PERSIST_KEY_UPDATE_SCHEDULE));
  }
  if (persist_exists(PERSIST_KEY_UPDATE_INTERVAL)) {
    s->update_interval = update_interval_clamp(persist_read_int(PERSIST_KEY_UPDATE_INTERVAL));
  }
  if (persist_exists(PERSIST_KEY_STEP_TARGET)) {
    s->step_target = prv_step_target(persist_read_int(PERSIST_KEY_STEP_TARGET));
  }
  if (persist_get_size(PERSIST_KEY_PERIOD) == (int)sizeof(s->period)) {
    persist_read_data(PERSIST_KEY_PERIOD, &s->period, sizeof(s->period));
  }
}

static int32_t prv_tuple_int(const Tuple *t) {
  // Clay sends select values as strings.
  if (t->type == TUPLE_CSTRING) {
    return atoi(t->value->cstring);
  }
  return t->value->int32;
}

static const char *prv_tuple_string(const Tuple *t) {
  return t->type == TUPLE_CSTRING ? t->value->cstring : NULL;
}

// Reads the custom period keys into `p`; returns whether any were present.
static bool prv_read_period(PeriodConfig *p, DictionaryIterator *iter) {
  bool found = false;
  Tuple *t = dict_find(iter, MESSAGE_KEY_PeriodRepeat);
  if (t) {
    p->repeat = prv_period_repeat(prv_tuple_int(t));
    found = true;
  }
  t = dict_find(iter, MESSAGE_KEY_PeriodDate);
  if (t) {
    p->date = period_parse_date(prv_tuple_string(t));
    found = true;
  }
  // The checkboxes arrive as seven keys, Sunday first.
  for (int day = 0; day < 7; day++) {
    t = dict_find(iter, MESSAGE_KEY_PeriodWeekdays + day);
    if (t) {
      if (prv_tuple_int(t)) {
        p->weekdays |= 1 << day;
      } else {
        p->weekdays &= ~(1 << day);
      }
      found = true;
    }
  }
  t = dict_find(iter, MESSAGE_KEY_PeriodStart);
  if (t) {
    p->start_min = period_parse_time(prv_tuple_string(t));
    found = true;
  }
  t = dict_find(iter, MESSAGE_KEY_PeriodEnd);
  if (t) {
    p->end_min = period_parse_time(prv_tuple_string(t));
    found = true;
  }
  t = dict_find(iter, MESSAGE_KEY_PeriodLabelFormat);
  if (t) {
    p->format = prv_label_format(prv_tuple_int(t));
    found = true;
  }
  return found;
}

void settings_apply_message(Settings *s, DictionaryIterator *iter) {
  Tuple *t = dict_find(iter, MESSAGE_KEY_ProgressMode);
  if (t) {
    s->progress_mode = prv_progress_mode(prv_tuple_int(t));
    persist_write_int(PERSIST_KEY_PROGRESS_MODE, s->progress_mode);
  }
  t = dict_find(iter, MESSAGE_KEY_LabelFormat);
  if (t) {
    s->label_format = prv_label_format(prv_tuple_int(t));
    persist_write_int(PERSIST_KEY_LABEL_FORMAT, s->label_format);
  }
  t = dict_find(iter, MESSAGE_KEY_Orientation);
  if (t) {
    s->orientation = prv_orientation(prv_tuple_int(t));
    persist_write_int(PERSIST_KEY_ORIENTATION, s->orientation);
  }
  t = dict_find(iter, MESSAGE_KEY_Theme);
  if (t) {
    s->theme = prv_theme(prv_tuple_int(t));
    persist_write_int(PERSIST_KEY_THEME, s->theme);
  }
  t = dict_find(iter, MESSAGE_KEY_UpdateSchedule);
  if (t) {
    s->update_schedule = prv_update_schedule(prv_tuple_int(t));
    persist_write_int(PERSIST_KEY_UPDATE_SCHEDULE, s->update_schedule);
  }
  t = dict_find(iter, MESSAGE_KEY_UpdateInterval);
  if (t) {
    // Blank or non-numeric input reads as 0: fall back to the default.
    const int32_t seconds = prv_tuple_int(t);
    s->update_interval =
        seconds < UPDATE_INTERVAL_MIN ? UPDATE_INTERVAL_DEFAULT : update_interval_clamp(seconds);
    persist_write_int(PERSIST_KEY_UPDATE_INTERVAL, s->update_interval);
  }
  t = dict_find(iter, MESSAGE_KEY_StepTarget);
  if (t) {
    s->step_target = prv_step_target(prv_tuple_int(t));
    persist_write_int(PERSIST_KEY_STEP_TARGET, s->step_target);
  }
  if (prv_read_period(&s->period, iter)) {
    persist_write_data(PERSIST_KEY_PERIOD, &s->period, sizeof(s->period));
  }
}
