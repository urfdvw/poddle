#include "time_words.h"

#include "assets.h"

static void prv_push(TwLine *line, uint8_t token) {
  line->tokens[line->count++] = token;
}

// 1..12 -> WORD_ONE..WORD_TWELVE (shared by hours and minute ones/teens)
static uint8_t prv_ones_word(int n) {
  return (uint8_t)(WORD_ONE + n - 1);
}

void time_words(int hour24, int minute, TwPhrase *out) {
  out->hour.count = 0;
  out->minute.count = 0;
  out->ampm = hour24 < 12 ? WORD_AM : WORD_PM;

  int hour12 = hour24 % 12;
  if (hour12 == 0) {
    hour12 = 12;
  }
  prv_push(&out->hour, prv_ones_word(hour12));

  TwLine *line = &out->minute;
  if (minute == 0) {
    prv_push(line, WORD_OCLOCK);
  } else if (minute < 10) {
    prv_push(line, WORD_OH);
    prv_push(line, TW_SPACE);
    prv_push(line, prv_ones_word(minute));
  } else if (minute <= 12) {
    prv_push(line, prv_ones_word(minute));
  } else if (minute < 20) {
    prv_push(line, (uint8_t)(WORD_THIRTEEN + minute - 13));
  } else {
    prv_push(line, (uint8_t)(WORD_TWENTY + minute / 10 - 2));
    if (minute % 10 != 0) {
      prv_push(line, TW_HYPHEN);
      prv_push(line, prv_ones_word(minute % 10));
    }
  }
}
