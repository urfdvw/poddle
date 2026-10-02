// Host-side dump of the pure logic modules, checked by tests/check_logic.py.
#include <stdio.h>

#include "../src/c/assets.h"
#include "../src/c/labels.h"
#include "../src/c/period.h"
#include "../src/c/schedule.h"
#include "../src/c/time_words.h"

static const char *WORD_TEXT[WORD_COUNT] = {
  "One", "Two", "Three", "Four", "Five", "Six", "Seven", "Eight", "Nine", "Ten", "Eleven",
  "Twelve", "Thirteen", "Fourteen", "Fifteen", "Sixteen", "Seventeen", "Eighteen", "Nineteen",
  "Twenty", "Thirty", "Forty", "Fifty", "Oh", "O'Clock", "AM", "PM",
};

static void prv_print_line(const TwLine *line) {
  for (int i = 0; i < line->count; i++) {
    uint8_t t = line->tokens[i];
    fputs(t == TW_SPACE ? " " : t == TW_HYPHEN ? "-" : WORD_TEXT[t], stdout);
  }
}

int main(void) {
  // Spoken time: every hour (24h, to cover AM/PM) x minute.
  for (int h = 0; h < 24; h++) {
    for (int m = 0; m < 60; m++) {
      TwPhrase p;
      time_words(h, m, &p);
      printf("words %02d:%02d|", h, m);
      prv_print_line(&p.hour);
      putchar('|');
      prv_print_line(&p.minute);
      printf("|%s\n", WORD_TEXT[p.ampm]);
    }
  }
  // Progress: all 4 combinations, both clock styles, every second of the day.
  for (int mode = 0; mode < 2; mode++) {
    for (int fmt = 0; fmt < 2; fmt++) {
      for (int is24 = 0; is24 < 2; is24++) {
        for (int t = 0; t < 86400; t++) {
          ProgressInfo pi;
          progress_info((ProgressMode)mode, (LabelFormat)fmt, t / 3600, t / 60 % 60, t % 60,
                        is24, &pi);
          printf("progress %d %d %d %05d|%ld/%ld|%s|%s\n", mode, fmt, is24, t, (long)pi.num,
                 (long)pi.den, pi.left, pi.right);
        }
      }
    }
  }
  for (int mo = 1; mo <= 12; mo++) {
    for (int d = 1; d <= 31; d++) {
      char buf[LABEL_BUF_SIZE];
      format_date(buf, mo, d);
      printf("date %d %d|%s\n", mo, d, buf);
    }
  }
  // Steps mode.
  static const int32_t STEPS[] = {-5, 0, 1, 7999, 8000, 8001, 12345, 999999, 1234567};
  static const int32_t TARGETS[] = {-1, 0, 1, 8000, 999999};
  for (unsigned i = 0; i < sizeof(STEPS) / sizeof(STEPS[0]); i++) {
    for (unsigned j = 0; j < sizeof(TARGETS) / sizeof(TARGETS[0]); j++) {
      ProgressInfo pi;
      steps_info(STEPS[i], TARGETS[j], &pi);
      printf("steps %ld %ld|%ld/%ld|%s|%s\n", (long)STEPS[i], (long)TARGETS[j], (long)pi.num,
             (long)pi.den, pi.left, pi.right);
    }
  }
  // Battery Saving schedule.
  for (int v = -5; v <= 70; v++) {
    printf("clamp %d|%d\n", v, update_interval_clamp(v));
  }
  static const uint16_t MS[] = {0, 1, 500, 999};
  for (int interval = 1; interval <= 60; interval++) {
    for (uint32_t now = 1790000000u; now < 1790000000u + 2 * 60; now++) {
      for (int m = 0; m < 4; m++) {
        printf("exact %d %lu %u|%lu\n", interval, (unsigned long)now, MS[m],
               (unsigned long)update_delay_ms(UPDATE_SCHEDULE_EXACT, interval, now, MS[m], 0));
      }
    }
    static const uint32_t RND[] = {0, 1, 499, 500, 999, 1000, 12345, 2147483647u, 4294967295u};
    for (int r = 0; r < 9; r++) {
      printf("random %d %lu|%lu\n", interval, (unsigned long)RND[r],
             (unsigned long)update_delay_ms(UPDATE_SCHEDULE_RANDOM, interval, 0, 0, RND[r]));
    }
  }
  // Custom period: parsing.
  static const char *DATES[] = {"2026-10-02", "1999-01-31", "2026-13-01", "2026-1-01",
                                "2026-10-02x", "", "abcd-ef-gh", "2026-00-10", "2026-12-32"};
  for (unsigned i = 0; i < sizeof(DATES) / sizeof(DATES[0]); i++) {
    printf("pdate %s|%ld\n", DATES[i], (long)period_parse_date(DATES[i]));
  }
  static const char *TIMES[] = {"00:00", "09:30", "23:59", "24:00", "12:60", "9:30",
                                "09:30:00", "09:30x", "", "ab:cd"};
  for (unsigned i = 0; i < sizeof(TIMES) / sizeof(TIMES[0]); i++) {
    printf("ptime %s|%d\n", TIMES[i], period_parse_time(TIMES[i]));
  }
  // Custom period: when it is active. 2026-10-01 is a Thursday (wday 4);
  // two weeks, every minute.
  static const PeriodConfig ACTIVE[] = {
    {PERIOD_REPEAT_OFF, 0, 0x7f, 540, 1020, LABEL_FORMAT_ELAPSED},
    {PERIOD_REPEAT_DATE, 20261005, 0, 540, 1020, LABEL_FORMAT_ELAPSED},
    {PERIOD_REPEAT_WEEKDAYS, 0, 0x3e, 0, 1, LABEL_FORMAT_SEGMENT},
    {PERIOD_REPEAT_WEEKDAYS, 0, 0x41, 1380, 1439, LABEL_FORMAT_ELAPSED},
    {PERIOD_REPEAT_DAILY, 0, 0, 0, 1439, LABEL_FORMAT_ELAPSED},
    {PERIOD_REPEAT_DAILY, 0, 0, 600, 600, LABEL_FORMAT_ELAPSED},   // empty
    {PERIOD_REPEAT_DAILY, 0, 0, 700, 600, LABEL_FORMAT_ELAPSED},   // end before start
    {PERIOD_REPEAT_DAILY, 0, 0, -1, 600, LABEL_FORMAT_ELAPSED},    // unparsed start
  };
  for (unsigned c = 0; c < sizeof(ACTIVE) / sizeof(ACTIVE[0]); c++) {
    const PeriodConfig *p = &ACTIVE[c];
    for (int day = 0; day < 14; day++) {
      const int mday = 1 + day, wday = (4 + day) % 7;
      for (int t = 0; t < 1440; t++) {
        printf("pactive %d %ld %d %d %d %d %d %d|%d\n", p->repeat, (long)p->date, p->weekdays,
               p->start_min, p->end_min, mday, wday, t,
               period_active(p, 2026, 10, mday, wday, t / 60, t % 60));
      }
    }
  }
  // Custom period: bar and labels, every second inside a few periods.
  static const int16_t SPANS[][2] = {{540, 1020}, {0, 1439}, {600, 601}, {600, 660}, {600, 661},
                                     {1380, 1439}};
  for (unsigned s = 0; s < sizeof(SPANS) / sizeof(SPANS[0]); s++) {
    for (int fmt = 0; fmt < 2; fmt++) {
      for (int is24 = 0; is24 < 2; is24++) {
        for (int hs = 0; hs < 2; hs++) {
          const PeriodConfig p = {PERIOD_REPEAT_DAILY, 0, 0, SPANS[s][0], SPANS[s][1],
                                  (LabelFormat)fmt};
          for (int t = SPANS[s][0] * 60; t < SPANS[s][1] * 60; t++) {
            ProgressInfo pi;
            period_progress(&p, t / 3600, t / 60 % 60, t % 60, is24, hs, &pi);
            printf("pprogress %d %d %d %d %d %d|%ld/%ld|%s|%s\n", SPANS[s][0], SPANS[s][1], fmt,
                   is24, hs, t, (long)pi.num, (long)pi.den, pi.left, pi.right);
          }
        }
      }
    }
  }
  return 0;
}
