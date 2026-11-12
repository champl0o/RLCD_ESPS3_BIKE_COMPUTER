#include "ui_text.h"
#include <stddef.h>

const char *ui_hint(ride_ui_state_t state) {
  switch (state) {
  case UI_RIDE_READY:
    return "KEY hold: START REC   BOOT: Main";
  case UI_RIDE_RIDING:
    return "KEY x2: PAUSE   BOOT hold: STOP";
  case UI_RIDE_PAUSED:
    return "KEY x2: RESUME   BOOT hold: STOP";
  case UI_RIDE_SUMMARY:
    return "any key: continue";
  }
  return NULL;
}

const char *ui_summary_title(void) { return "RIDE ENDED"; }

const char *ui_banner_title(banner_t banner) {
  switch (banner) {
  case BANNER_NO_CARD:
    return "NO CARD";
  case BANNER_NO_TIME:
    return "NO TIME";
  case BANNER_START_FAILED:
    return "CANNOT START";
  case BANNER_NONE:
    break;
  }
  return NULL;
}

const char *ui_banner_reason(banner_t banner) {
  switch (banner) {
  case BANNER_NO_CARD:
    return "insert card, then hold KEY";
  case BANNER_NO_TIME:
    return "waiting for GPS or RTC time";
  case BANNER_START_FAILED:
    return "recorder busy, try again";
  case BANNER_NONE:
    break;
  }
  return NULL;
}

bool ui_hint_visible(ride_ui_state_t state, int64_t now_us,
                     int64_t boot_grace_end_us, int64_t last_press_us) {
  if (state == UI_RIDE_SUMMARY || now_us < boot_grace_end_us)
    return true;
  if (last_press_us <= 0)
    return false;

  int64_t since_press_us = now_us - last_press_us;
  return since_press_us >= 0 && since_press_us < UI_HINT_AFTER_PRESS_US;
}
