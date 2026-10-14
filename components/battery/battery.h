#pragma once

#include <stdint.h>
#include "esp_err.h"
#include <esp_adc/adc_oneshot.h>

/* Батарея 18650 на ADC1_CH3 (GPIO4) через дільник 1:3. */
typedef struct
{
    adc_oneshot_unit_handle_t adc;
    adc_cali_handle_t cali;
} battery_t;

esp_err_t battery_init(battery_t *battery);

/* Напруга на клемах батареї в мВ — уже з урахуванням дільника. */
esp_err_t battery_read_mv(battery_t *battery, int *mv);

/* Чиста функція: мВ → 0..100 % за кривою розряду Li-ion. Без заліза. */
uint8_t battery_percent_from_mv(int mv);
