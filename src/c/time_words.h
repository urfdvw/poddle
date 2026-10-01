#pragma once

#include <stdint.h>

// Spoken-time phrase as a sequence of tokens: WORD_* ids from assets.h,
// or one of the separators below.
#define TW_SPACE 0xFE
#define TW_HYPHEN 0xFD
#define TW_MAX_TOKENS 3

typedef struct {
  uint8_t tokens[TW_MAX_TOKENS];
  uint8_t count;
} TwLine;

typedef struct {
  TwLine hour;    // line 1, e.g. "Three"
  TwLine minute;  // line 2, e.g. "Oh Five", "Twenty-Nine", "O'Clock"
  uint8_t ampm;   // WORD_AM or WORD_PM
} TwPhrase;

// Digits-only style, always 12-hour (independent of clock_is_24h_style()).
void time_words(int hour24, int minute, TwPhrase *out);
