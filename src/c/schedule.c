#include "schedule.h"

int update_interval_clamp(int seconds) {
  if (seconds < UPDATE_INTERVAL_MIN) {
    return UPDATE_INTERVAL_MIN;
  }
  if (seconds > UPDATE_INTERVAL_MAX) {
    return UPDATE_INTERVAL_MAX;
  }
  return seconds;
}

uint32_t update_delay_ms(UpdateSchedule schedule, int interval, uint32_t now_s,
                         uint16_t now_ms, uint32_t rnd) {
  const int x = update_interval_clamp(interval);
  if (schedule == UPDATE_SCHEDULE_RANDOM) {
    if (x <= UPDATE_RANDOM_WIDE_MAX) {
      const uint32_t x_ms = (uint32_t)x * 1000;
      return x_ms / 2 + rnd % x_ms;
    }
    const uint32_t x_ms = (uint32_t)(x < UPDATE_RANDOM_MAX ? x : UPDATE_RANDOM_MAX) * 1000;
    const uint32_t y_ms = 60000 - x_ms;
    return x_ms - y_ms / 2 + rnd % y_ms;
  }
  const uint32_t period_ms = (uint32_t)x * 1000;
  const uint32_t into_period_ms = (now_s % (period_ms / 1000)) * 1000 + now_ms;
  return period_ms - into_period_ms;  // in [1, period_ms]
}
