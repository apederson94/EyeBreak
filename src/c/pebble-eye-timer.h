#include <pebble.h>

typedef enum {
  TIMER_SCREEN,
  TIMER_REST,
  TIMER_OFF
} TimerState;

static void prv_timer_status_message(TimerState *state);
static void prv_update_app_glance(AppGlanceReloadSession *session, size_t limit, void *context);
static void prv_update_stored_state();
static void prv_timer_toggle_state();
static void prv_exit_delay();
static void prv_exit_application(void *data);
static void prv_vibe_alert();
static void prv_init(void);
static void prv_deinit(void);
static void prv_window_load(Window *window);
static void prv_window_unload(Window *window);
static void prv_setup_state();
