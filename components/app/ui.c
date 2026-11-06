#include "ui.h"
#include "esp_timer.h"

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

void ui_draw_current_screen(void) {
  screen_data_t snapshot;
  model_snapshot(&snapshot);

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
