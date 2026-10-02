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
  const uint32_t period_ms = (uint32_t)update_interval_clamp(interval) * 1000;
  if (schedule == UPDATE_SCHEDULE_RANDOM) {
    return period_ms / 2 + rnd % period_ms;
  }
  const uint32_t into_period_ms = (now_s % (period_ms / 1000)) * 1000 + now_ms;
  return period_ms - into_period_ms;  // in [1, period_ms]
}
