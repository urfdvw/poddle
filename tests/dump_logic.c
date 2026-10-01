// Host-side dump of the pure logic modules, checked by tests/check_logic.py.
#include <stdio.h>

#include "../src/c/assets.h"
#include "../src/c/labels.h"
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
  return 0;
}
