#include "handlers.h"
#include "ble_parse.h"
#include "esp_event_base.h"
#include "esp_timer.h"
#include "event_orchestrator.h"
#include "fit.h"
#include "model.h"
#include "recorder.h"
#include "sdcard.h"
#include "ui.h"
#include <stdint.h>

#define NEXT_CLIMATE_DELAY_US 10000000 // 10 seconds
#define NEXT_RTC_DELAY_US 1000000      // 1 second
#define NEXT_BATTERY_DELAY_US 5000000  // 5 seconds

static const char *TAG = "handlers";

static esp_timer_handle_t climate_timer;
static esp_timer_handle_t rtc_timer;
static esp_timer_handle_t battery_timer;

static esp_event_loop_handle_t loop_handle;

static void on_gps_event_handler(void *handler_arg, esp_event_base_t base,
                                 int32_t id, void *event_data);
static void on_sd_card_event_handler(void *handler_arg, esp_event_base_t base,
                                     int32_t id, void *event_data);
static void on_button_event_handler(void *handler_arg, esp_event_base_t base,
                                    int32_t id, void *event_data);
static void on_hr_event_handler(void *handler_arg, esp_event_base_t base,
                                int32_t id, void *event_data);
static void on_hr_lost_event_handler(void *handler_arg, esp_event_base_t base,
                                     int32_t id, void *event_data);

static void on_sensor_timer(void *arg) { ui_notify((uint32_t)(uintptr_t)arg); }

void timers_init(void) {
  const esp_timer_create_args_t climate_timer_args = {
      .callback = on_sensor_timer,
      .arg = (void *)(uintptr_t)UI_TICK_CLIMATE,
      .name = "climate_timer",
  };

  const esp_timer_create_args_t rtc_timer_args = {
      .callback = on_sensor_timer,
      .arg = (void *)(uintptr_t)UI_TICK_RTC,
      .name = "rtc_timer",
  };

  const esp_timer_create_args_t battery_timer_args = {
      .callback = on_sensor_timer,
      .arg = (void *)(uintptr_t)UI_TICK_BATTERY,
      .name = "battery_timer",
  };

  ESP_ERROR_CHECK(esp_timer_create(&climate_timer_args, &climate_timer));
  ESP_ERROR_CHECK(esp_timer_create(&rtc_timer_args, &rtc_timer));
  ESP_ERROR_CHECK(esp_timer_create(&battery_timer_args, &battery_timer));

  ESP_ERROR_CHECK(
      esp_timer_start_periodic(climate_timer, NEXT_CLIMATE_DELAY_US));
  ESP_ERROR_CHECK(esp_timer_start_periodic(rtc_timer, NEXT_RTC_DELAY_US));
  ESP_ERROR_CHECK(
      esp_timer_start_periodic(battery_timer, NEXT_BATTERY_DELAY_US));
}

esp_err_t handlers_init(void) {
  esp_err_t err = event_orchestrator_init();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize event orchestrator: %s",
             esp_err_to_name(err));
    return err;
  }

  loop_handle = event_orchestrator_get_loop();

  event_orchestrator_add_event_handler(APP_EVENT_ORCHESTRATOR, EVT_GPS_UPDATED,
                                       on_gps_event_handler, NULL);
  event_orchestrator_add_event_handler(APP_EVENT_ORCHESTRATOR, EVT_SD_MOUNTED,
                                       on_sd_card_event_handler, NULL);
  event_orchestrator_add_event_handler(APP_EVENT_ORCHESTRATOR, EVT_BUTTON,
                                       on_button_event_handler, NULL);
  event_orchestrator_add_event_handler(APP_EVENT_ORCHESTRATOR, EVT_HR_UPDATED,
                                       on_hr_event_handler, NULL);
  event_orchestrator_add_event_handler(APP_EVENT_ORCHESTRATOR, EVT_HR_LOST,
                                       on_hr_lost_event_handler, NULL);

  return ESP_OK;
}

void on_gps_data_handler(const gps_data_t *data) {
  esp_err_t err =
      esp_event_post_to(loop_handle, APP_EVENT_ORCHESTRATOR, EVT_GPS_UPDATED,
                        (void *)data, sizeof(gps_data_t), 0);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Failed to post GPS event: %s", esp_err_to_name(err));
  }
}

static void on_gps_event_handler(void *handler_arg, esp_event_base_t base,
                                 int32_t id, void *event_data) {
  if (id == EVT_GPS_UPDATED) {
    gps_data_t *gps_data = (gps_data_t *)event_data;

    double now =
        esp_timer_get_time() / 1000000.0; // Convert microseconds to seconds
    trip_update(gps_data->lat, gps_data->lon, gps_data->speed_kmh, now,
                gps_data->valid);
    const trip_t *trip_data = trip_get();

    model_apply_gps(gps_data, trip_data);

    uint32_t timestamp = 0;
    bool has_time = model_fit_time(&timestamp);
    uint8_t hr_bpm = FIT_INVALID_UINT8;
    if (!model_hr_bpm(&hr_bpm))
      hr_bpm = FIT_INVALID_UINT8;

    if (has_time) {
      fit_record_t record = fit_record_make(
          timestamp, gps_data->valid, gps_data->lat, gps_data->lon,
          gps_data->speed_kmh, trip_data->dist_km, hr_bpm, FIT_INVALID_UINT8);
      recorder_push(&record);
    }

    // d->stopped_time = trip_data.stopped_time;

    ui_notify(UI_REDRAW);

    ESP_LOGI(TAG, "GPS updated: lat=%.6f, lon=%.6f, speed=%.2f km/h",
             gps_data->lat, gps_data->lon, gps_data->speed_kmh);
  }
}

void on_sd_card_mounted_handler(void *arg) {
  esp_err_t err = esp_event_post_to(loop_handle, APP_EVENT_ORCHESTRATOR,
                                    EVT_SD_MOUNTED, NULL, 0, 0);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Failed to post SD card event: %s", esp_err_to_name(err));
  }
}

static void on_sd_card_event_handler(void *handler_arg, esp_event_base_t base,
                                     int32_t id, void *event_data) {
  if (id == EVT_SD_MOUNTED) {
    bool sd_mounted = sd_card_is_mounted();

    model_set_sd_mounted(sd_mounted);
  }
}

void on_button_callback_handler(btn_event_t event) {
  esp_err_t err = esp_event_post_to(loop_handle, APP_EVENT_ORCHESTRATOR,
                                    EVT_BUTTON, &event, sizeof(btn_event_t), 0);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Failed to post button event: %s", esp_err_to_name(err));
  }
}

static void on_button_event_handler(void *handler_arg, esp_event_base_t base,
                                    int32_t id, void *event_data) {
  if (id == EVT_BUTTON) {
    btn_event_t *event = (btn_event_t *)event_data;

    current_screen_t local_current_screen;
    local_current_screen = model_current_screen();

    if (event->btn_id == BTN_KEY && event->gesture == PRESS_SHORT) {
      model_next_screen();
    }

    if (event->btn_id == BTN_BOOT && event->gesture == PRESS_SHORT) {
      model_reset_screen();
    }

    if (event->btn_id == BTN_KEY && event->gesture == PRESS_LONG) {
      uint32_t start_time = 0;
      bool sd_mounted = model_sd_mounted();
      bool has_time = model_fit_time(&start_time);

      if (!sd_mounted || !has_time) {
        ESP_LOGW(TAG, "Cannot start recording: sd_mounted=%d, time known=%d",
                 sd_mounted, has_time);
      } else if (recorder_start(start_time) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to queue recording start");
      }
    }

    if (event->btn_id == BTN_BOOT && event->gesture == PRESS_LONG) {
      if (recorder_stop() != ESP_OK)
        ESP_LOGW(TAG, "Failed to queue recording stop");
    }

    if (local_current_screen != model_current_screen()) {
      ui_notify(UI_REDRAW);

      ESP_LOGI(TAG, "Screen changed to %d", model_current_screen());
    }
  }
}

void on_hr_data_handler(hr_data_t hr_data) {
  esp_err_t err =
      esp_event_post_to(loop_handle, APP_EVENT_ORCHESTRATOR, EVT_HR_UPDATED,
                        &hr_data, sizeof(hr_data_t), 0);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Failed to post HR event: %s", esp_err_to_name(err));
  }
}

static void on_hr_event_handler(void *handler_arg, esp_event_base_t base,
                                int32_t id, void *event_data) {
  if (id == EVT_HR_UPDATED) {
    hr_data_t *hr_data = (hr_data_t *)event_data;

    model_set_hr(hr_data->bpm, hr_data->contact_supported,
                 hr_data->contact_detected);

    ui_notify(UI_REDRAW);

    ESP_LOGI(
        TAG, "HR updated: bpm=%d, contact_supported=%d, contact_detected=%d",
        hr_data->bpm, hr_data->contact_supported, hr_data->contact_detected);
  }
}

void on_hr_lost_handler(void) {
  esp_err_t err = esp_event_post_to(loop_handle, APP_EVENT_ORCHESTRATOR,
                                    EVT_HR_LOST, NULL, 0, 0);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Failed to post HR lost event: %s", esp_err_to_name(err));
  }
}

static void on_hr_lost_event_handler(void *handler_arg, esp_event_base_t base,
                                     int32_t id, void *event_data) {
  if (id == EVT_HR_LOST) {
    model_clear_hr();
    ui_notify(UI_REDRAW);
    ESP_LOGI(TAG, "HR sensor lost");
  }
}
