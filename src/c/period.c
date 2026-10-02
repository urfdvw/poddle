#include "period.h"

static bool prv_digits(const char *text, int count, int *out) {
  int value = 0;
  for (int i = 0; i < count; i++) {
    if (text[i] < '0' || text[i] > '9') {
      return false;
    }
    value = value * 10 + (text[i] - '0');
  }
  *out = value;
  return true;
}

int32_t period_parse_date(const char *text) {
  int year, month, mday;
  if (!text || !prv_digits(text, 4, &year) || text[4] != '-' || !prv_digits(text + 5, 2, &month) ||
      text[7] != '-' || !prv_digits(text + 8, 2, &mday) || text[10] != '\0') {
    return 0;
  }
  if (month < 1 || month > 12 || mday < 1 || mday > 31) {
    return 0;
  }
  return year * 10000 + month * 100 + mday;
}

int period_parse_time(const char *text) {
  int hour, minute;
  // HTML time inputs may add seconds ("09:30:00"); they are ignored.
  if (!text || !prv_digits(text, 2, &hour) || text[2] != ':' || !prv_digits(text + 3, 2, &minute) ||
      (text[5] != '\0' && text[5] != ':')) {
    return -1;
  }
  if (hour > 23 || minute > 59) {
    return -1;
  }
  return hour * 60 + minute;
}

bool period_active(const PeriodConfig *cfg, int year, int month, int mday, int wday,
                   int hour24, int minute) {
  if (cfg->start_min < 0 || cfg->end_min <= cfg->start_min) {
    return false;
  }
  switch (cfg->repeat) {
    case PERIOD_REPEAT_DATE:
      if (cfg->date != year * 10000 + month * 100 + mday) {
        return false;
      }
      break;
    case PERIOD_REPEAT_WEEKDAYS:
      if (!(cfg->weekdays & (1 << wday))) {
        return false;
      }
      break;
    case PERIOD_REPEAT_DAILY:
      break;
    default:
      return false;
  }
  const int now = hour24 * 60 + minute;
  return now >= cfg->start_min && now < cfg->end_min;
}

void period_progress(const PeriodConfig *cfg, int hour24, int minute, int second, bool is_24h,
                     bool hour_seconds, ProgressInfo *out) {
  const int32_t start_s = cfg->start_min * 60;
  const int32_t span_s = cfg->end_min * 60 - start_s;
  // Segment labels only change by the minute, and so does the bar then.
  const int32_t now_s =
      hour24 * 3600 + minute * 60 + (period_needs_seconds(cfg) ? second : 0);
  const int32_t elapsed = now_s - start_s;
  out->num = elapsed;
  out->den = span_s;
  if (cfg->format == LABEL_FORMAT_SEGMENT) {
    format_clock(out->left, cfg->start_min / 60, cfg->start_min % 60, is_24h);
    format_clock(out->right, cfg->end_min / 60, cfg->end_min % 60, is_24h);
  } else {
    format_duration(out->left, "", elapsed, hour_seconds);
    format_duration(out->right, "-", span_s - elapsed, hour_seconds);
  }
}

bool period_needs_seconds(const PeriodConfig *cfg) {
  return cfg->format == LABEL_FORMAT_ELAPSED;
}
