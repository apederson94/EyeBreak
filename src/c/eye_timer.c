#include "eye_timer.h"
#include "config.h"

static TimerState s_state = TIMER_OFF;
static time_t s_end_time = 0;
static WakeupId s_wakeup_id = -1;
static bool s_changed = false;

static int32_t prv_duration_for(TimerState state) {
  switch (state) {
    case TIMER_WORK:
      return EYE_WORK_SECONDS;
    case TIMER_REST:
      return EYE_REST_SECONDS;
    default:
      return 0;
  }
}

static bool prv_is_valid_state(TimerState state) {
  return state == TIMER_OFF || state == TIMER_WORK || state == TIMER_REST;
}

static void prv_save(void) {
  persist_write_int(EYE_PERSIST_KEY_STATE, (int32_t)s_state);
  persist_write_int(EYE_PERSIST_KEY_END_TIME, (int32_t)s_end_time);
}

static void prv_cancel_wakeup(void) {
  if (s_wakeup_id >= 0) {
    wakeup_cancel(s_wakeup_id);
    s_wakeup_id = -1;
  }
}

static void prv_schedule_wakeup(void) {
  prv_cancel_wakeup();

  if (s_state == TIMER_OFF) {
    return;
  }

  s_wakeup_id = wakeup_schedule(s_end_time, 0, false);
  if (s_wakeup_id < 0) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "wakeup_schedule failed: %d", (int)s_wakeup_id);
  } else {
    EYE_LOG("scheduled wakeup id=%d at=%ld", (int)s_wakeup_id, (long)s_end_time);
  }
}

static void prv_advance(void) {
  time_t now = time(NULL);
  int guard = 0;

  do {
    s_state = (s_state == TIMER_WORK) ? TIMER_REST : TIMER_WORK;
    s_end_time += prv_duration_for(s_state);
    guard++;
  } while (s_end_time <= now && guard < 1000);

  if (s_end_time <= now) {
    s_end_time = now + prv_duration_for(s_state);
  }

  s_changed = true;
  EYE_LOG("advanced to %s, end=%ld",
          s_state == TIMER_WORK ? "WORK" : "REST", (long)s_end_time);

  prv_save();
  prv_schedule_wakeup();
}

static void prv_wakeup_handler(WakeupId wakeup_id, int32_t reason) {
  if (!eye_timer_advance_if_due()) {
    prv_schedule_wakeup();
  }
}

TimerState eye_timer_init(void) {
  wakeup_cancel_all();
  s_wakeup_id = -1;
  wakeup_service_subscribe(prv_wakeup_handler);

  s_state = persist_exists(EYE_PERSIST_KEY_STATE)
                ? (TimerState)persist_read_int(EYE_PERSIST_KEY_STATE)
                : TIMER_OFF;
  s_end_time = persist_exists(EYE_PERSIST_KEY_END_TIME)
                   ? (time_t)persist_read_int(EYE_PERSIST_KEY_END_TIME)
                   : 0;

  if (!prv_is_valid_state(s_state)) {
    s_state = TIMER_OFF;
  }

  if (s_state != TIMER_OFF && s_end_time <= 0) {
    s_state = TIMER_OFF;
  }

  if (s_state != TIMER_OFF && s_end_time <= time(NULL)) {
    prv_advance();
  } else {
    prv_save();
    prv_schedule_wakeup();
  }

  s_changed = false;

  EYE_LOG("init state=%d end=%ld", (int)s_state, (long)s_end_time);
  return s_state;
}

void eye_timer_deinit(void) {
  prv_save();
  prv_schedule_wakeup();
}

TimerState eye_timer_state(void) {
  return s_state;
}

bool eye_timer_is_running(void) {
  return s_state != TIMER_OFF;
}

time_t eye_timer_end_time(void) {
  return s_end_time;
}

int eye_timer_remaining_seconds(void) {
  if (s_state == TIMER_OFF) {
    return 0;
  }

  time_t now = time(NULL);
  if (s_end_time <= now) {
    return 0;
  }

  return (int)(s_end_time - now);
}

int eye_timer_phase_total_seconds(void) {
  return (int)prv_duration_for(s_state);
}

void eye_timer_start(void) {
  if (s_state != TIMER_OFF) {
    return;
  }

  s_state = TIMER_WORK;
  s_end_time = time(NULL) + prv_duration_for(s_state);
  s_changed = false;
  EYE_LOG("start, end=%ld", (long)s_end_time);
  prv_save();
  prv_schedule_wakeup();
}

void eye_timer_stop(void) {
  s_state = TIMER_OFF;
  s_end_time = 0;
  s_changed = false;
  EYE_LOG("stop");
  prv_save();
  prv_schedule_wakeup();
}

bool eye_timer_toggle(void) {
  if (s_state == TIMER_OFF) {
    eye_timer_start();
  } else {
    eye_timer_stop();
  }

  return s_state != TIMER_OFF;
}

bool eye_timer_advance_if_due(void) {
  if (s_state == TIMER_OFF) {
    return false;
  }

  if (time(NULL) < s_end_time) {
    return false;
  }

  prv_advance();
  return true;
}

bool eye_timer_consume_changed(void) {
  bool changed = s_changed;
  s_changed = false;
  return changed;
}
