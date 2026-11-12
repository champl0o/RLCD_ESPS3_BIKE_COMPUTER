#include "screen.h"

#include "detailed_screen.h"
#include "esp_timer.h"
#include "helper.h"
#include "map_screen.h"
#include "minimal_screen.h"
#include "ride_screens.h"
#include <stdio.h>

u8g2_t u8g2;

/* Block over the finished screen: a start error matters more than a pause. */
static void draw_state_overlay(const screen_data_t *d) {
  if (d->banner != BANNER_NONE)
    draw_state_block(&u8g2, ui_banner_title(d->banner),
                     ui_banner_reason(d->banner));
  else if (d->ui_state == UI_RIDE_PAUSED)
    draw_state_block(&u8g2, "PAUSED", "MANUAL");
}

/* Layout — variant
 * docs/superpowers/specs/2026-10-07-gps-trip-design.md: status bar / room for the
 * map / speed panel. */
static void render(const screen_data_t *d, current_screen_t screen) {
  if (d->ui_state == UI_RIDE_SUMMARY) {
    draw_summary_screen(&u8g2, d);
    return;
  }

  switch (screen) {
  case SCREEN_MINIMAL:
    draw_minimal_screen(&u8g2, d);
    break;
  case SCREEN_MAP:
    draw_map_screen(&u8g2, d);
    break;
  case SCREEN_DETAILED:
  default:
    draw_detailed_screen(&u8g2, d);
    break;
  }

  draw_state_overlay(d);

}

void screen_init(void) { rlcd_init(&u8g2); }

int64_t screen_draw(const screen_data_t *d, current_screen_t screen) {
  render(d, screen);

  /* How much it costs to push a frame out. Remember: ~30 KB on the wire, because
   * the driver repacks 3 buffer bytes into 6 bytes for the controller. */
  int64_t t0 = esp_timer_get_time();
  u8g2_SendBuffer(&u8g2);
  return esp_timer_get_time() - t0;
}
