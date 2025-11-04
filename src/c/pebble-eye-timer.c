#include <pebble.h>
#include "pebble-eye-timer.h"

// Virtual Lockitron settings (https://api.lockitron.com/v1/getting_started/virtual_locks)
// #define LOCKITRON_LOCK_UUID "95c22a11-4c9e-4420-adf0-11f1b36575f2"
// #define LOCKITRON_ACCESS_TOKEN "99e75a775fe737bb716caf88f161460bb623d283c3561c833480f0834335668b"
// #define MESSAGE_KEY_LOCK_STATE = "pebble_eye_timer_lock_state"
// #define MESSAGE_KEY_APP_READY = "pebble_eye_timer_ready"
static uint32_t KEY_STATE = 0;

static Window *s_window;
static TimerState s_timer_state;
static TextLayer *s_txt_layer;

static char s_glance[32];

int main(void) {
  prv_init();
  app_event_loop();
  prv_deinit();
}

static void prv_setup_state() {
    if (!persist_exists(KEY_STATE)) {
        persist_write_int(KEY_STATE, TIMER_OFF);
    }

    s_timer_state = persist_read_int(KEY_STATE);
    APP_LOG(APP_LOG_LEVEL_INFO, "STATE: s_timer_state = %d", s_timer_state);
}

static void prv_init() {
  prv_setup_state();
  wakeup_service_subscribe(prv_wakeup_handler);

  prv_timer_toggle_state();
  app_message_open(256, 256);
  prv_schedule_wakeup();

  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });
  window_stack_push(s_window, false);
}

static void prv_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  //Create a TextLayer to show the result
  s_txt_layer = text_layer_create(GRect(0, (bounds.size.h/2)-30, bounds.size.w, 60));
  text_layer_set_background_color(s_txt_layer, GColorClear);
  text_layer_set_text_color(s_txt_layer, GColorBlack);
  text_layer_set_font(s_txt_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(s_txt_layer, GTextAlignmentCenter);

  prv_timer_status_message(&s_timer_state);
  text_layer_set_text(s_txt_layer, s_glance);
  layer_add_child(window_layer, text_layer_get_layer(s_txt_layer));

  prv_vibe_alert();

  prv_exit_delay();
}

static void prv_exit_delay() {
  // Get the system timeout duration
  int timeout = preferred_result_display_duration();

  // After the timeout, exit the application
  app_timer_register(timeout, prv_exit_application, NULL);
}

static void prv_exit_application(void *data) {
  // App can exit to return directly to their default watchface
  exit_reason_set(APP_EXIT_ACTION_PERFORMED_SUCCESSFULLY);

  // Exit the application by unloading the only window
  window_stack_remove(s_window, false);
}

// Request a state change for the Timer (SCREEN/OFF)
static void prv_timer_toggle_state() {
  switch(s_timer_state) {
    case TIMER_SCREEN:
    case TIMER_REST:
      s_timer_state = TIMER_OFF;
      break;
    case TIMER_OFF:
      s_timer_state = TIMER_SCREEN;
    default:
      break; // do nothing
  }
}

static void prv_vibe_alert() {
    switch(s_timer_state) {
        case TIMER_SCREEN:
          vibes_double_pulse();
          break;
        case TIMER_REST:
          vibes_long_pulse();
          break;
        default:
          break;
    }
}

static void prv_window_unload(Window *window) {
  window_destroy(s_window);
}

static void prv_deinit(void) {
  // Before the application terminates, setup the AppGlance and store state
  prv_update_stored_state();
  app_glance_reload(prv_update_app_glance, &s_timer_state);
}

static void prv_update_stored_state() {
    persist_write_int(KEY_STATE, s_timer_state);
}

// Create the AppGlance displayed in the system launcher
static void prv_update_app_glance(AppGlanceReloadSession *session, size_t limit, void *context) {
  // Check we haven't exceeded system limit of AppGlance's
  if (limit < 1) return;

  char time_buffer[8];
  clock_copy_time_string(time_buffer, sizeof(time_buffer));

  // Generate a friendly message for the current Lockitron state
  prv_timer_status_message(&s_timer_state);
  APP_LOG(APP_LOG_LEVEL_INFO, "STATE: %s", s_glance);

  // Create the AppGlanceSlice (no icon, no expiry)
  AppGlanceSlice entry = (AppGlanceSlice) {
    .layout = {
      .subtitle_template_string = s_glance
    },
    .expiration_time = APP_GLANCE_SLICE_NO_EXPIRATION
  };

  // Add the slice, and check the result
  const AppGlanceResult result = app_glance_add_slice(session, entry);
  if (result != APP_GLANCE_RESULT_SUCCESS) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "AppGlance Error: %d", result);
  }
}

// Generate a string to display the Lockitron state
static void prv_timer_status_message(TimerState *state) {
  time_t future = time(NULL);

  switch(*state) {
    case TIMER_SCREEN:
      future += 20 * 60;  // 20 min ahead
      break;
    case TIMER_REST:
      future += 30;  // 30 sec ahead
      break;
    default:
      strncpy(s_glance, "OFF", sizeof(s_glance));
      return;
  }

  struct tm *tm = localtime(&future);

  switch(*state) {
    case TIMER_SCREEN:
      if (clock_is_24h_style()) {
        strftime(s_glance, sizeof(s_glance), "SCREEN %R", tm);  // “17:30”
      } else {
        strftime(s_glance, sizeof(s_glance), "SCREEN %I:%M %p", tm);  // “5:30 PM”
      }
      break;
    case TIMER_REST:
      if (clock_is_24h_style()) {
        strftime(s_glance, sizeof(s_glance), "REST %R", tm);  // “17:30”
      } else {
        strftime(s_glance, sizeof(s_glance), "REST %I:%M %p", tm);  // “5:30 PM”
      }
      break;
    case TIMER_OFF:
    default:
      strncpy(s_glance, "OFF", sizeof(s_glance));
      return;
  }
}

// figure out why wakeup also turns off the app. probably something I don't understand about wakeups
// might need to create some var that holds a flag that says whether or not this was a wakeup or something like that
// wakeups shouldn't do the same thing as non-wakeups, so will need to figure out that piece
// probably needs to go in prv_init
// should also explore using notifications to see if that is any better, but wakeups will work for now
static void prv_wakeup_handler(WakeupId wakeup_id, int32_t cookie) {
    APP_LOG(APP_LOG_LEVEL_INFO, "STATE: WOKE UP %d", cookie);
    s_timer_state = cookie;
    prv_schedule_wakeup();
    app_glance_reload(prv_update_app_glance, &s_timer_state);
}

static void prv_schedule_wakeup() {
    time_t wakeup_time = time(NULL);
    int32_t new_state;
    switch (s_timer_state) {
        case TIMER_SCREEN:
          APP_LOG(APP_LOG_LEVEL_INFO, "STATE: WAKEUP TIMER_SCREEN");
          wakeup_time += 20; // 20 minutes
          new_state = TIMER_REST;
          break;
        case TIMER_REST:
          APP_LOG(APP_LOG_LEVEL_INFO, "STATE: WAKEUP TIMER_REST");
          wakeup_time += 30; // 30 seconds
          new_state = TIMER_SCREEN;
          break;
        default:
          APP_LOG(APP_LOG_LEVEL_INFO, "STATE: WAKEUP DEFAULT / CANCEL");
          return;
    }

    WakeupId id = wakeup_schedule(wakeup_time, new_state, false);
    APP_LOG(APP_LOG_LEVEL_INFO, "STATE: wakeup id %d", id);
}
