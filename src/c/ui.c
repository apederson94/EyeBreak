#include <pebble.h>

#include "app_glance.h"
#include "config.h"
#include "eye_timer.h"
#include "feedback.h"
#include "ui.h"

static Window *s_window;
static TextLayer *s_phase_layer;
static TextLayer *s_time_layer;
static TextLayer *s_footer_layer;
static Layer *s_progress_layer;
static ActionBarLayer *s_action_bar;
static GBitmap *s_icon_play;
static GBitmap *s_icon_pause;
static AppTimer *s_exit_timer;
static TimerState s_theme_state = TIMER_OFF;
static bool s_theme_valid = false;
static char s_time_buf[32];

static GColor prv_window_color(TimerState state) {
  switch (state) {
    case TIMER_WORK:
      return PBL_IF_COLOR_ELSE(GColorCeleste, GColorWhite);
    case TIMER_REST:
      return PBL_IF_COLOR_ELSE(GColorMintGreen, GColorBlack);
    default:
      return GColorWhite;
  }
}

static GColor prv_text_color(TimerState state) {
  if (state == TIMER_REST) {
    return PBL_IF_COLOR_ELSE(GColorBlack, GColorWhite);
  }
  return GColorBlack;
}

static GColor prv_progress_fill_color(TimerState state) {
  switch (state) {
    case TIMER_WORK:
      return PBL_IF_COLOR_ELSE(GColorCobaltBlue, GColorBlack);
    case TIMER_REST:
      return PBL_IF_COLOR_ELSE(GColorJaegerGreen, GColorWhite);
    default:
      return GColorClear;
  }
}

static GColor prv_progress_track_color(TimerState state) {
  if (state == TIMER_REST) {
    return PBL_IF_COLOR_ELSE(GColorLightGray, GColorDarkGray);
  }
  return GColorLightGray;
}

static void prv_progress_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  TimerState state = eye_timer_state();

  graphics_context_set_fill_color(ctx, prv_progress_track_color(state));
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  if (state == TIMER_OFF) {
    return;
  }

  int total = eye_timer_phase_total_seconds();
  if (total <= 0) {
    return;
  }

  int elapsed = total - eye_timer_remaining_seconds();
  if (elapsed < 0) {
    elapsed = 0;
  } else if (elapsed > total) {
    elapsed = total;
  }

  int width = (bounds.size.w * elapsed) / total;
  if (width > 0) {
    graphics_context_set_fill_color(ctx, prv_progress_fill_color(state));
    graphics_fill_rect(ctx, GRect(0, 0, width, bounds.size.h), 0, GCornerNone);
  }
}

static void prv_refresh_action_bar(TimerState state) {
  if (!s_action_bar) {
    return;
  }

  GBitmap *icon = (state == TIMER_OFF) ? s_icon_play : s_icon_pause;
  if (icon) {
    action_bar_layer_set_icon(s_action_bar, BUTTON_ID_SELECT, icon);
  }
}

static void prv_apply_theme(TimerState state) {
  GColor text_color = prv_text_color(state);

  window_set_background_color(s_window, prv_window_color(state));

  if (s_phase_layer) {
    text_layer_set_text_color(s_phase_layer, text_color);
  }
  if (s_time_layer) {
    text_layer_set_text_color(s_time_layer, text_color);
  }
  if (s_footer_layer) {
    text_layer_set_text_color(s_footer_layer, text_color);
  }
  if (s_action_bar) {
    action_bar_layer_set_background_color(
        s_action_bar, PBL_IF_COLOR_ELSE(GColorLightGray, GColorWhite));
  }
}

static void prv_refresh(void) {
  TimerState state = eye_timer_state();
  bool theme_changed = !s_theme_valid || state != s_theme_state;

  if (theme_changed) {
    if (state == TIMER_OFF) {
      text_layer_set_font(s_time_layer,
                          fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
      text_layer_set_text(s_phase_layer, "EYETIMER");
      snprintf(s_time_buf, sizeof(s_time_buf), "%dm work / %ds rest",
               EYE_WORK_SECONDS / 60, EYE_REST_SECONDS);
      text_layer_set_text(s_footer_layer, s_time_buf);
      layer_set_hidden(text_layer_get_layer(s_footer_layer), false);
      layer_set_hidden(s_progress_layer, true);
    } else {
      text_layer_set_font(
          s_time_layer,
          fonts_get_system_font(
              PBL_IF_COLOR_ELSE(FONT_KEY_LECO_42_NUMBERS,
                                FONT_KEY_LECO_36_BOLD_NUMBERS)));
      text_layer_set_text(s_phase_layer,
                          state == TIMER_WORK ? "WORK" : "REST");
      text_layer_set_text(s_footer_layer, "");
      layer_set_hidden(text_layer_get_layer(s_footer_layer), true);
      layer_set_hidden(s_progress_layer, false);
    }

    prv_refresh_action_bar(state);
    prv_apply_theme(state);
    s_theme_state = state;
    s_theme_valid = true;
  }

  if (state == TIMER_OFF) {
    text_layer_set_text(s_time_layer, "Ready");
  } else {
    int remaining = eye_timer_remaining_seconds();
    snprintf(s_time_buf, sizeof(s_time_buf), "%02d:%02d",
             remaining / 60, remaining % 60);
    text_layer_set_text(s_time_layer, s_time_buf);
  }

  if (s_progress_layer) {
    layer_mark_dirty(s_progress_layer);
  }
}

static void prv_feedback_for_state(void) {
  switch (eye_timer_state()) {
    case TIMER_WORK:
      feedback_work();
      break;
    case TIMER_REST:
      feedback_rest();
      break;
    default:
      break;
  }
}

static void prv_tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  eye_timer_advance_if_due();
  eye_timer_ensure_wakeup();
  bool changed = eye_timer_consume_changed();

  prv_refresh();

  if (changed) {
    prv_feedback_for_state();
    app_glance_update();
  }
}

static void prv_select_click(ClickRecognizerRef recognizer, void *context) {
  bool running = eye_timer_toggle();
  prv_refresh();

  if (running) {
    prv_feedback_for_state();
  }

  app_glance_update();
}

static void prv_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_click);
}

static void prv_exit_application(void *data) {
  s_exit_timer = NULL;
  exit_reason_set(APP_EXIT_ACTION_PERFORMED_SUCCESSFULLY);
  window_stack_remove(s_window, false);
}

static void prv_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);
  int content_width = bounds.size.w - ACTION_BAR_WIDTH;

  s_phase_layer = text_layer_create(
      GRect(0, bounds.size.h / 4 - 10, content_width, 24));
  text_layer_set_background_color(s_phase_layer, GColorClear);
  text_layer_set_font(s_phase_layer,
                      fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(s_phase_layer, GTextAlignmentCenter);
  layer_add_child(root, text_layer_get_layer(s_phase_layer));

  s_time_layer = text_layer_create(
      GRect(0, bounds.size.h / 2 - 30, content_width, 64));
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
  layer_add_child(root, text_layer_get_layer(s_time_layer));

  s_footer_layer = text_layer_create(
      GRect(0, bounds.size.h - 26, content_width, 20));
  text_layer_set_background_color(s_footer_layer, GColorClear);
  text_layer_set_font(s_footer_layer,
                      fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_footer_layer, GTextAlignmentCenter);
  layer_add_child(root, text_layer_get_layer(s_footer_layer));

  s_progress_layer = layer_create(
      GRect(8, bounds.size.h - 14, content_width - 16, 6));
  layer_set_update_proc(s_progress_layer, prv_progress_update_proc);
  layer_add_child(root, s_progress_layer);

  s_icon_play = gbitmap_create_with_resource(RESOURCE_ID_ICON_PLAY);
  s_icon_pause = gbitmap_create_with_resource(RESOURCE_ID_ICON_PAUSE);

  s_action_bar = action_bar_layer_create();
  action_bar_layer_set_click_config_provider(s_action_bar,
                                             prv_click_config_provider);
  action_bar_layer_add_to_window(s_action_bar, window);

  s_theme_valid = false;
  prv_refresh();

  tick_timer_service_subscribe(SECOND_UNIT, prv_tick_handler);

  if (launch_reason() == APP_LAUNCH_WAKEUP) {
    prv_feedback_for_state();
    s_exit_timer = app_timer_register(EYE_WAKEUP_DISPLAY_MS,
                                      prv_exit_application, NULL);
  }
}

static void prv_window_unload(Window *window) {
  tick_timer_service_unsubscribe();

  if (s_exit_timer) {
    app_timer_cancel(s_exit_timer);
    s_exit_timer = NULL;
  }

  if (s_phase_layer) {
    text_layer_destroy(s_phase_layer);
    s_phase_layer = NULL;
  }

  if (s_time_layer) {
    text_layer_destroy(s_time_layer);
    s_time_layer = NULL;
  }

  if (s_footer_layer) {
    text_layer_destroy(s_footer_layer);
    s_footer_layer = NULL;
  }

  if (s_progress_layer) {
    layer_destroy(s_progress_layer);
    s_progress_layer = NULL;
  }

  if (s_action_bar) {
    action_bar_layer_destroy(s_action_bar);
    s_action_bar = NULL;
  }

  if (s_icon_play) {
    gbitmap_destroy(s_icon_play);
    s_icon_play = NULL;
  }

  if (s_icon_pause) {
    gbitmap_destroy(s_icon_pause);
    s_icon_pause = NULL;
  }
}

void ui_init(void) {
  s_window = window_create();
  window_set_background_color(s_window, GColorWhite);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });
  window_stack_push(s_window, true);
}

void ui_deinit(void) {
  if (s_window) {
    window_destroy(s_window);
    s_window = NULL;
  }
}
