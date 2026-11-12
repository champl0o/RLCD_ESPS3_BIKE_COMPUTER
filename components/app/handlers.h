#pragma once
#include "button.h"
#include "esp_err.h"
#include "gps.h"
#include "ble_parse.h"

esp_err_t handlers_init(void);
void timers_init(void);

void on_gps_data_handler(const gps_data_t *data);
void on_sd_card_mounted_handler(void *arg);
void on_button_callback_handler(btn_event_t event);
void on_hr_data_handler(hr_data_t hr_data);
void on_hr_lost_handler(void);
