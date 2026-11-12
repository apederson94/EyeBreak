#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Force-include stub pebble.h so eye_timer.c's #include <pebble.h> resolves here. */
#include "pebble.h"

/* Stub implementations for platform APIs referenced by eye_timer.c. */
int persist_write_int(int key, int32_t value) { (void)key; (void)value; return 1; }
int persist_write_data(int key, const void *data, int size) { (void)key; (void)data; (void)size; return size; }
int persist_get_size(int key) { (void)key; return 0; }
int persist_read_data(int key, void *data, int size) { (void)key; (void)data; (void)size; return 0; }
int persist_read_int(int key) { (void)key; return 0; }
bool persist_exists(int key) { (void)key; return false; }

void wakeup_cancel(WakeupId id) { (void)id; }
void wakeup_cancel_all(void) {}
void wakeup_service_subscribe(void (*handler)(WakeupId, int32_t)) { (void)handler; }
WakeupId wakeup_schedule(time_t when, int32_t cookie, ...) { (void)when; (void)cookie; return -1; }

/* Include source under test; static functions are now visible. */
#include "../src/c/eye_timer.c"

/* ============================================================================
 * Minimal test runner
 * ============================================================================ */

static int s_tests_run = 0;
static int s_tests_passed = 0;

#define TEST(name)                                        \
  do {                                                    \
    printf("  TEST: %-45s ", name);                       \
    s_tests_run++;                                        \
  } while (0)

#define PASS()                                            \
  do {                                                    \
    printf("PASSED\n");                                   \
    s_tests_passed++;                                     \
  } while (0)

/* ============================================================================
 * Tests for prv_duration_for
 * ============================================================================ */

static void test_prv_duration_for(void) {
  TEST("prv_duration_for(TIMER_WORK) == 1200");
  assert(prv_duration_for(TIMER_WORK) == 20 * 60);
  PASS();

  TEST("prv_duration_for(TIMER_REST) == 20");
  assert(prv_duration_for(TIMER_REST) == 20);
  PASS();

  TEST("prv_duration_for(TIMER_OFF) == 0");
  assert(prv_duration_for(TIMER_OFF) == 0);
  PASS();
}

/* ============================================================================
 * Tests for prv_remaining_seconds
 * ============================================================================ */

static void test_prv_remaining_seconds(void) {
  time_t now = 1000;

  TEST("prv_remaining_seconds(TIMER_OFF) == 0");
  assert(prv_remaining_seconds(TIMER_OFF, 2000, now) == 0);
  PASS();

  TEST("prv_remaining_seconds(end_time == now) == 0");
  assert(prv_remaining_seconds(TIMER_WORK, now, now) == 0);
  PASS();

  TEST("prv_remaining_seconds(end_time < now) == 0");
  assert(prv_remaining_seconds(TIMER_WORK, now - 1, now) == 0);
  PASS();

  TEST("prv_remaining_seconds(TIMER_WORK) == 120");
  assert(prv_remaining_seconds(TIMER_WORK, 1120, now) == 120);
  PASS();

  TEST("prv_remaining_seconds(TIMER_REST) == 20");
  assert(prv_remaining_seconds(TIMER_REST, 1020, now) == 20);
  PASS();
}

/* ============================================================================
 * Tests for prv_advance_calc
 * ============================================================================ */

static void test_prv_advance_calc_off_to_work(void) {
  TimerState state = TIMER_OFF;
  time_t end_time = 0;
  time_t now = 1000;

  TEST("prv_advance_calc OFF -> WORK at now=1000");
  prv_advance_calc(&state, &end_time, now);
  assert(state == TIMER_WORK);
  assert(end_time == 20 * 60); /* 0 + 1200 = 1200 */
  assert(end_time > now);
  PASS();
}

static void test_prv_advance_calc_work_to_rest(void) {
  TimerState state = TIMER_WORK;
  time_t end_time = 1300;
  time_t now = 1300; /* exactly at end_time */

  TEST("prv_advance_calc WORK -> REST (end_time <= now)");
  prv_advance_calc(&state, &end_time, now);
  assert(state == TIMER_REST);
  assert(end_time == now + 20); /* 1320 */
  PASS();
}

static void test_prv_advance_calc_rest_to_work(void) {
  TimerState state = TIMER_REST;
  time_t end_time = 1320;
  time_t now = 1320;

  TEST("prv_advance_calc REST -> WORK");
  prv_advance_calc(&state, &end_time, now);
  assert(state == TIMER_WORK);
  assert(end_time == now + 20 * 60); /* 1340 */
  PASS();
}

static void test_prv_advance_calc_guard_stops_before_now(void) {
  TimerState state = TIMER_WORK;
  time_t end_time = 900; /* in the past */
  time_t now = 1000;

  TEST("prv_advance_calc advances until end_time > now");
  prv_advance_calc(&state, &end_time, now);
  assert(end_time > now);
  assert(state == TIMER_WORK || state == TIMER_REST);
  PASS();
}

static void test_prv_advance_calc_guard_past_due_fallback(void) {
  TimerState state = TIMER_OFF;
  time_t end_time = 0;
  time_t now = 5000;

  TEST("prv_advance_calc OFF with end_time=0 advances past now");
  prv_advance_calc(&state, &end_time, now);
  assert(end_time > now);
  assert(state == TIMER_WORK || state == TIMER_REST);
  PASS();
}

static void test_prv_advance_calc_rebases_to_now_when_delayed(void) {
  TimerState state = TIMER_WORK;
  time_t end_time = 1300; /* WORK ended 13s ago */
  time_t now = 1313;

  TEST("prv_advance_calc delayed WORK->REST rebases to now + duration");
  prv_advance_calc(&state, &end_time, now);
  assert(state == TIMER_REST);
  assert(end_time == now + 20); /* 1333, not 1320 */
  assert(end_time > now);
  PASS();
}

/* ============================================================================
 * main
 * ============================================================================ */

int main(void) {
  printf("EyeTimer unit tests\n");
  printf("===================\n\n");

  test_prv_duration_for();
  test_prv_remaining_seconds();
  test_prv_advance_calc_off_to_work();
  test_prv_advance_calc_work_to_rest();
  test_prv_advance_calc_rest_to_work();
  test_prv_advance_calc_guard_stops_before_now();
  test_prv_advance_calc_guard_past_due_fallback();
  test_prv_advance_calc_rebases_to_now_when_delayed();

  printf("\n===================\n");
  printf("Results: %d/%d tests passed\n", s_tests_passed, s_tests_run);
  return (s_tests_passed == s_tests_run) ? 0 : 1;
}
