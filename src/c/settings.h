#pragma once

#include <pebble.h>

#include "canvas.h"
#include "labels.h"
#include "period.h"
#include "schedule.h"

// Theme (Clay). The color theme only exists on color screens.
typedef enum {
  THEME_BW = 0,
  THEME_COLOR = 1,
} Theme;

// Everything the config page sets. Values are persisted and sent by the
// config page; keep them stable.
typedef struct {
  ProgressMode progress_mode;
  LabelFormat label_format;
  Orientation orientation;
  Theme theme;
  UpdateSchedule update_schedule;
  int update_interval;
  int32_t step_target;  // steps mode
  PeriodConfig period;
} Settings;

// Defaults, then whatever was persisted (or the DEMO_* pins of a
// screenshot build).
void settings_load(Settings *s);
// Applies and persists the keys present in a config page message.
void settings_apply_message(Settings *s, DictionaryIterator *iter);
