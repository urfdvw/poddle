#pragma once

#include <stdbool.h>
#include <stdint.h>

// Progress bar settings (Clay config). Values are persisted and sent by the
// config page, so keep them stable; new modes go at the end.
typedef enum {
  PROGRESS_MODE_MINUTE = 0,  // within the current minute
  PROGRESS_MODE_HOUR = 1,    // within the current hour
  PROGRESS_MODE_STEPS = 2,   // today's steps toward a target (Health watches)
} ProgressMode;

#define STEP_TARGET_DEFAULT 8000
#define STEP_TARGET_MAX 999999

typedef enum {
  LABEL_FORMAT_SEGMENT = 0,  // segment start / segment end
  LABEL_FORMAT_ELAPSED = 1,  // elapsed / -remaining, MM:SS
} LabelFormat;

#define LABEL_BUF_SIZE 16

typedef struct {
  int32_t num;  // progress fraction num/den
  int32_t den;
  char left[LABEL_BUF_SIZE];
  char right[LABEL_BUF_SIZE];
} ProgressInfo;

// "3:29" (12h) or "15:29" (24h).
void format_clock(char *buf, int hour24, int minute, bool is_24h);
// "6/18": month 1-12, day 1-31, no zero padding.
void format_date(char *buf, int month, int mday);
// sign + "MM:SS"; from one hour up "H:MM:SS", or "H:MM" (seconds dropped,
// not rounded) without `hour_seconds`.
void format_duration(char *buf, const char *sign, int total_seconds, bool hour_seconds);
void progress_info(ProgressMode mode, LabelFormat format, int hour24, int minute, int second,
                   bool is_24h, ProgressInfo *out);
// Steps mode: the bar fills toward `target` (full once reached); left label
// today's steps, right label the difference: "-N" still to go, "+N" past
// the target, "0" exactly on it.
void steps_info(int32_t steps, int32_t target, ProgressInfo *out);
// Whether the configuration needs a per-second redraw.
bool progress_needs_seconds(ProgressMode mode, LabelFormat format);
