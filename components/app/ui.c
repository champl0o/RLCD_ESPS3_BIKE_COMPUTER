#include "ui.h"
#include "esp_timer.h"
#include <stdint.h>

#define SCREEN_TAG "screen"

static uint32_t frame = 0;

static TaskHandle_t ui_task_handle = NULL;

void ui_init_screen(void) {
  screen_init();

  ui_task_handle = xTaskGetCurrentTaskHandle();
}

void ui_notify(uint32_t bits) {
  if (ui_task_handle != NULL)
    xTaskNotify(ui_task_handle, bits, eSetBits);
}

#ifdef UI_DEMO_STATES
/* Bench-only screen preview (build with -DUI_DEMO_STATES): every 3 s
 * cycles through states and banners that no logic produces yet. */
static void demo_override(screen_data_t *s) {
  static const struct {
    ride_ui_state_t state;
    banner_t banner;
    bool recording;
    bool paused;
  } steps[] = {
      {UI_RIDE_READY, BANNER_NONE, false, false},
      {UI_RIDE_READY, BANNER_NO_CARD, false, false},
      {UI_RIDE_READY, BANNER_NO_TIME, false, false},
      {UI_RIDE_READY, BANNER_START_FAILED, false, false},
      {UI_RIDE_RIDING, BANNER_NONE, true, false},
      {UI_RIDE_PAUSED, BANNER_NONE, true, true},
      {UI_RIDE_SUMMARY, BANNER_NONE, false, false},
  };
  const int n = sizeof(steps) / sizeof(steps[0]);
  const int i = (int)((esp_timer_get_time() / 3000000LL) % n);

  s->ui_state = steps[i].state;
  s->banner = steps[i].banner;
  s->recording = steps[i].recording;
  s->paused = steps[i].paused;
  s->hint_visible = true;
}
#endif

void ui_draw_current_screen(void) {
  screen_data_t snapshot;
  model_snapshot(&snapshot);
#ifdef UI_DEMO_STATES
  demo_override(&snapshot);
#endif

  snapshot.uptime_s = (uint32_t)(esp_timer_get_time() / 1000000LL);

  int64_t dt =
      screen_draw(&snapshot, snapshot.current_screen); // draw current later

  if ((frame % 10) == 0) {
    ESP_LOGI(
        SCREEN_TAG,
        "frame %lu: SendBuffer took %lld us (max %.1f fps), %.2f C, %.1f %%RH",
        (unsigned long)frame, (long long)dt,
        dt > 0 ? 1000000.0 / (double)dt : 0.0, snapshot.temp_c,
        snapshot.humidity_pct);
  }
  frame++;
}
