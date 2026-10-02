#include "labels.h"

#include <stdio.h>

void format_clock(char *buf, int hour24, int minute, bool is_24h) {
  int hour = hour24 % 24;
  if (!is_24h) {
    hour %= 12;
    if (hour == 0) {
      hour = 12;
    }
  }
  snprintf(buf, LABEL_BUF_SIZE, "%d:%02d", hour, minute);
}

void format_date(char *buf, int month, int mday) {
  snprintf(buf, LABEL_BUF_SIZE, "%d/%d", month, mday);
}

static void prv_format_mmss(char *buf, const char *sign, int total_seconds) {
  snprintf(buf, LABEL_BUF_SIZE, "%s%02d:%02d", sign, total_seconds / 60, total_seconds % 60);
}

void format_duration(char *buf, const char *sign, int total_seconds) {
  if (total_seconds < 3600) {
    prv_format_mmss(buf, sign, total_seconds);
    return;
  }
  snprintf(buf, LABEL_BUF_SIZE, "%s%02d:%02d:%02d", sign, total_seconds / 3600,
           total_seconds / 60 % 60, total_seconds % 60);
}

void progress_info(ProgressMode mode, LabelFormat format, int hour24, int minute, int second,
                   bool is_24h, ProgressInfo *out) {
  if (mode == PROGRESS_MODE_MINUTE) {
    out->num = second;
    out->den = 60;
    if (format == LABEL_FORMAT_SEGMENT) {
      int next_hour = minute == 59 ? hour24 + 1 : hour24;
      format_clock(out->left, hour24, minute, is_24h);
      format_clock(out->right, next_hour, (minute + 1) % 60, is_24h);
    } else {
      // The minutes field stays "00", so the top of the minute reads -00:00.
      prv_format_mmss(out->left, "", second);
      prv_format_mmss(out->right, "-", (60 - second) % 60);
    }
  } else {
    // Without a seconds tick the bar only moves once a minute.
    int elapsed = minute * 60 + (progress_needs_seconds(mode, format) ? second : 0);
    out->num = elapsed;
    out->den = 3600;
    if (format == LABEL_FORMAT_SEGMENT) {
      format_clock(out->left, hour24, 0, is_24h);
      format_clock(out->right, hour24 + 1, 0, is_24h);
    } else {
      prv_format_mmss(out->left, "", elapsed);
      prv_format_mmss(out->right, "-", 3600 - elapsed);
    }
  }
}

bool progress_needs_seconds(ProgressMode mode, LabelFormat format) {
  return mode == PROGRESS_MODE_MINUTE || format == LABEL_FORMAT_ELAPSED;
}
