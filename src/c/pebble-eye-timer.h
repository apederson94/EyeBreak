#include <pebble.h>

typedef enum {
  TIMER_OFF,
  TIMER_REST,
  TIMER_SCREEN,
} TimerState;

static void prv_timer_status_message(TimerState *state);
static void prv_update_app_glance(AppGlanceReloadSession *session, size_t limit, void *context);
static void prv_timer_toggle_state();
static void prv_exit_delay();
static void prv_exit_application(void *data);
static void prv_vibe_alert();
static void prv_init(void);
static void prv_deinit(void);
static void prv_window_load(Window *window);
static void prv_window_unload(Window *window);
static void prv_setup_state();
static void prv_wakeup_handler(WakeupId wakeup_id, int32_t reason);
static void prv_schedule_wakeup();
static void prv_draw_main_text(Window *window);
static void prv_draw_action_bar(Window *window);
