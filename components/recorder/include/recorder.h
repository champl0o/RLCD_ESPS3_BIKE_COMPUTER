#pragma once

#include "esp_err.h"
#include "fit.h"
#include <stdint.h>

esp_err_t recorder_init(void);
esp_err_t recorder_push(const fit_record_t *record);
esp_err_t recorder_start(uint32_t fit_time);
esp_err_t recorder_stop(void);
