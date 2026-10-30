#pragma once

#include "esp_err.h"
#include "esp_event.h"
#include "esp_event_base.h"

ESP_EVENT_DECLARE_BASE(APP_EVENT_ORCHESTRATOR);

typedef enum {
  EVT_BUTTON = 0,
  EVT_GPS_UPDATED,
  EVT_RTC_UPDATED,
  EVT_CLIMATE_UPDATED,
  EVT_BATTERY_UPDATED,
  EVT_SD_MOUNTED
} app_event_id_t;

esp_err_t event_orchestrator_init(void);
esp_event_loop_handle_t event_orchestrator_get_loop(void);
esp_err_t event_orchestrator_add_event_handler(
    esp_event_base_t event_base, int32_t event_id,
    esp_event_handler_t event_handler, void *event_handler_arg);
