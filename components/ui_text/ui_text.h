#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Ride state from the screen's point of view. Pure logic without ESP-IDF so the texts and
 * the hint display rules can be tested on the host. */
typedef enum {
  UI_RIDE_READY = 0, /* no recording */
  UI_RIDE_RIDING,    /* recording in progress */
  UI_RIDE_PAUSED,    /* recording paused */
  UI_RIDE_SUMMARY,   /* summary after stopping */
} ride_ui_state_t;

typedef enum {
  BANNER_NONE = 0,
  BANNER_NO_CARD,
  BANNER_NO_TIME,
  BANNER_START_FAILED,
} banner_t;

#define UI_HINT_AFTER_PRESS_US 3000000 /* how long the hint lives after a press */
#define UI_HINT_AT_BOOT_US 10000000    /* and how long after device start */
#define UI_BANNER_US 3000000           /* how long the start-error banner stays */

const char *ui_hint(ride_ui_state_t state);

/* Title of the summary screen. The recorder reports no result yet, so it must
 * not claim the ride was saved. */
const char *ui_summary_title(void);

/* NULL for BANNER_NONE. */
const char *ui_banner_title(banner_t banner);
const char *ui_banner_reason(banner_t banner);

/* The summary always shows its hint; the other states — at start and for 3 s after
 * a press. last_press_us == 0 means "never pressed". */
bool ui_hint_visible(ride_ui_state_t state, int64_t now_us,
                     int64_t boot_grace_end_us, int64_t last_press_us);
