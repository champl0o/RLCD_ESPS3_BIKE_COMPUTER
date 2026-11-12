#pragma once

#include "esp_err.h"
#include "driver/i2c_master.h"

/* SHTC3 — temperature + humidity, I2C 0x70.
 * The bus is created externally: the RTC and audio codecs also sit on it,
 * so the sensor driver must not own it. */

typedef struct
{
    i2c_master_dev_handle_t dev;
} shtc3_t;

/* Add the sensor to the bus, check the ID, put it to sleep. */
esp_err_t shtc3_init(shtc3_t *s, i2c_master_bus_handle_t bus);

/* Wake up → one measurement (~12 ms) → put to sleep. Blocks the task for ~20–30 ms. */
esp_err_t shtc3_read(shtc3_t *s, float *temp_c, float *humidity_pct);
