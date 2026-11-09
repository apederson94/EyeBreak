#pragma once

#include <pebble.h>

typedef enum {
  TIMER_OFF = 0,
  TIMER_WORK,
  TIMER_REST,
} TimerState;

TimerState eye_timer_init(void);
void eye_timer_deinit(void);

TimerState eye_timer_state(void);
bool eye_timer_is_running(void);
int eye_timer_remaining_seconds(void);
int eye_timer_phase_total_seconds(void);
time_t eye_timer_end_time(void);

void eye_timer_start(void);
void eye_timer_stop(void);
bool eye_timer_toggle(void);
bool eye_timer_advance_if_due(void);
bool eye_timer_consume_changed(void);
