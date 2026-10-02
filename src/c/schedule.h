#pragma once

#include <stdint.h>

// Battery Saving (Clay): how often the face redraws when it shows seconds.
// Values are persisted and sent by the config page; keep them stable.
typedef enum {
  UPDATE_SCHEDULE_EXACT = 0,   // every X seconds, on the clock
  UPDATE_SCHEDULE_RANDOM = 1,  // X on average, never a minute without a redraw
} UpdateSchedule;

#define UPDATE_INTERVAL_MIN 1
#define UPDATE_INTERVAL_MAX 60
#define UPDATE_INTERVAL_DEFAULT 1
// Random: up to this period the spread is X itself; above it the spread
// narrows to 60 - X so no gap reaches a minute.
#define UPDATE_RANDOM_WIDE_MAX 40
// Random silently uses at most 55s, so its spread (60 - X) stays positive.
#define UPDATE_RANDOM_MAX 55

// Clamps a configured interval into [MIN, MAX]; out-of-range input that is
// not a number at all should be mapped to the default by the caller.
int update_interval_clamp(int seconds);

// Milliseconds until the next redraw.
//   exact:  the next instant whose Unix time is a multiple of `interval`
//           (so updates are exactly `interval` apart and, when it divides
//           60, land on the same seconds every minute).
//   random: from `rnd` (any uint32), X on average and always < 60s apart.
//           X <= 40: 0.5*X + U[0, X) seconds.
//           X > 40:  X = min(X, 55), Y = 60 - X, X - 0.5*Y + U[0, Y) seconds.
// `now_s`/`now_ms` are the current Unix time and its millisecond part.
uint32_t update_delay_ms(UpdateSchedule schedule, int interval, uint32_t now_s,
                         uint16_t now_ms, uint32_t rnd);
