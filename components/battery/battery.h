#pragma once

#include <stdint.h>
#include "esp_err.h"
#include <esp_adc/adc_oneshot.h>

/* 18650 battery on ADC1_CH3 (GPIO4) through a 1:3 divider. 1:3. */
typedef struct
{
    adc_oneshot_unit_handle_t adc;
    adc_cali_handle_t cali;
} battery_t;

esp_err_t battery_init(battery_t *battery);

/* Battery terminal voltage in mV, already corrected for the divider. */
esp_err_t battery_read_mv(battery_t *battery, int *mv);

/* Pure function: mV → 0..100 % along the Li-ion discharge curve. No hardware. */
uint8_t battery_percent_from_mv(int mv);
