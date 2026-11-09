#include "feedback.h"
#include "config.h"

static const uint32_t WORK_SEGMENTS[] = {180, 120, 180, 120, 180};
static const uint32_t REST_SEGMENTS[] = {800, 250, 200};

static void prv_play(const uint32_t *segments, uint32_t count) {
  vibes_cancel();

  VibePattern pattern = {
    .durations = segments,
    .num_segments = count,
  };
  vibes_enqueue_custom_pattern(pattern);
}

void feedback_work(void) {
  prv_play(WORK_SEGMENTS, ARRAY_LENGTH(WORK_SEGMENTS));
}

void feedback_rest(void) {
  prv_play(REST_SEGMENTS, ARRAY_LENGTH(REST_SEGMENTS));
}
