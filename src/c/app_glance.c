#include <pebble.h>

#include "app_glance.h"
#include "eye_timer.h"

static char s_subtitle[40];

static void prv_update(AppGlanceReloadSession *session, size_t limit, void *context) {
  if (limit < 1) {
    return;
  }

  TimerState state = eye_timer_state();
  PublishedId icon = APP_GLANCE_SLICE_DEFAULT_ICON;

  if (state == TIMER_OFF) {
    snprintf(s_subtitle, sizeof(s_subtitle), "Ready");
  } else {
    icon = (state == TIMER_WORK) ? PUBLISHED_ID_GLANCE_WORK
                                 : PUBLISHED_ID_GLANCE_REST;
    char time_buf[16];
    time_t end = eye_timer_end_time();
    struct tm *tm = localtime(&end);

    if (tm && clock_is_24h_style()) {
      strftime(time_buf, sizeof(time_buf), "%H:%M", tm);
    } else if (tm) {
      strftime(time_buf, sizeof(time_buf), "%I:%M %p", tm);
    } else {
      time_buf[0] = '\0';
    }

    snprintf(s_subtitle, sizeof(s_subtitle), "%s %s",
             state == TIMER_WORK ? "WORK" : "REST", time_buf);
  }

  AppGlanceSlice entry = (AppGlanceSlice) {
    .layout = {
      .icon = icon,
      .subtitle_template_string = s_subtitle,
    },
    .expiration_time = (state == TIMER_OFF)
                           ? APP_GLANCE_SLICE_NO_EXPIRATION
                           : eye_timer_end_time(),
  };

  AppGlanceResult result = app_glance_add_slice(session, entry);
  if (result != APP_GLANCE_RESULT_SUCCESS) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "AppGlance Error: %d", result);
  }
}

void app_glance_update(void) {
  app_glance_reload(prv_update, NULL);
}
