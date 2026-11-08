#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "ble_sensors.h"
#include "button.h"
#include "gps.h"
#include "handlers.h"
#include "model.h"
#include "recorder.h"
#include "sdcard.h"
#include "sensors.h"
#include "ui.h"

static const char *TAG = "main";

void app_main(void) {
  esp_err_t err = model_init();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize model: %s", esp_err_to_name(err));
    return;
  }

  ui_init_screen();

  err = handlers_init();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize handlers: %s", esp_err_to_name(err));
    return;
  }

  sensors_init();

  timers_init();

  recorder_init();

  ble_sensors_init(on_hr_data_handler, on_hr_lost_handler);

  gps_start(on_gps_data_handler);

  sd_card_mount(on_sd_card_mounted_handler);

  button_init(on_button_callback_handler);

  /* First update of all sensors to populate the model with initial values */
  uint32_t bits = UI_TICK_CLIMATE | UI_TICK_RTC | UI_TICK_BATTERY;

  while (1) {
    if (bits & UI_TICK_CLIMATE)
      sensors_update_climate();
    if (bits & UI_TICK_RTC)
      sensors_update_rtc();
    if (bits & UI_TICK_BATTERY)
      sensors_update_battery();

    ui_draw_current_screen();

    xTaskNotifyWait(0, UINT32_MAX, &bits, portMAX_DELAY);
  }
}
