#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "labels.h"

// Custom period (Clay): between a start and an end time on the chosen days,
// the progress bar runs from start to end with its own label format;
// outside it the face follows the normal progress settings. Values are
// persisted and sent by the config page; keep them stable.
typedef enum {
  PERIOD_REPEAT_OFF = 0,
  PERIOD_REPEAT_DATE = 1,      // one calendar date
  PERIOD_REPEAT_WEEKDAYS = 2,  // the checked days of the week
  PERIOD_REPEAT_DAILY = 3,
} PeriodRepeat;

typedef struct {
  PeriodRepeat repeat;
  int32_t date;        // YYYYMMDD, for PERIOD_REPEAT_DATE
  uint8_t weekdays;    // bit 0 = Sunday ... bit 6 = Saturday
  int16_t start_min;   // minutes after midnight, [0, 1440)
  int16_t end_min;     // must be after start_min (no crossing midnight)
  LabelFormat format;
} PeriodConfig;

// "2026-10-02" -> 20261002; 0 when it does not parse.
int32_t period_parse_date(const char *text);
// "09:30" -> 570; -1 when it does not parse.
int period_parse_time(const char *text);

// Whether the period applies at this local time: start <= now < end, on a
// matching day. year is the full year, month 1-12.
bool period_active(const PeriodConfig *cfg, int year, int month, int mday, int wday,
                   int hour24, int minute);
// Bar and labels while the period is active.
void period_progress(const PeriodConfig *cfg, int hour24, int minute, int second, bool is_24h,
                     ProgressInfo *out);
// Whether the period's labels need a per-second redraw.
bool period_needs_seconds(const PeriodConfig *cfg);
