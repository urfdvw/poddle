#pragma once

#include <stdint.h>

// Battery Saving (Clay): how often the face redraws when it shows seconds.
// Values are persisted and sent by the config page; keep them stable.
typedef enum {
  UPDATE_SCHEDULE_EXACT = 0,   // every X seconds, on the clock
  UPDATE_SCHEDULE_RANDOM = 1,  // after 0.5*X + rand(X) seconds, X on average
} UpdateSchedule;

#define UPDATE_INTERVAL_MIN 1
// New settings from the config page are capped at 40s, so a random redraw
// (at most 1.5*X apart) never skips a whole minute. 1.0.2 allowed up to 60;
// a value saved back then is kept as it is.
#define UPDATE_INTERVAL_MAX 40
#define UPDATE_INTERVAL_STORED_MAX 60
#define UPDATE_INTERVAL_DEFAULT 1

// Clamps an interval into [MIN, max]; out-of-range input that is not a
// number at all should be mapped to the default by the caller.
int update_interval_clamp(int seconds, int max);

// Milliseconds until the next redraw.
//   exact:  the next instant whose Unix time is a multiple of `interval`
//           (so updates are exactly `interval` apart and, when it divides
//           60, land on the same seconds every minute).
//   random: 0.5*interval + U[0, interval) seconds, from `rnd` (any uint32).
// `now_s`/`now_ms` are the current Unix time and its millisecond part.
uint32_t update_delay_ms(UpdateSchedule schedule, int interval, uint32_t now_s,
                         uint16_t now_ms, uint32_t rnd);
