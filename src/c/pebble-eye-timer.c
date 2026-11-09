#include <pebble.h>

#include "app_glance.h"
#include "eye_timer.h"
#include "ui.h"

int main(void) {
  eye_timer_init();
  app_glance_update();
  ui_init();

  app_event_loop();

  app_glance_update();
  eye_timer_deinit();
  ui_deinit();
}
