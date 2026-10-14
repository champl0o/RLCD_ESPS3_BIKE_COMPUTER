#pragma once

#include "esp_err.h"
#include "driver/i2c_master.h"

/* SHTC3 — температура + вологість, I2C 0x70.
 * Шина створюється ззовні: на ній ще сидять RTC і аудіокодеки,
 * тож драйвер датчика не має нею володіти. */

typedef struct
{
    i2c_master_dev_handle_t dev;
} shtc3_t;

/* Додати датчик на шину, перевірити ID, приспати. */
esp_err_t shtc3_init(shtc3_t *s, i2c_master_bus_handle_t bus);

/* Розбудити → один вимір (~12 мс) → приспати. Блокує задачу на ~20–30 мс. */
esp_err_t shtc3_read(shtc3_t *s, float *temp_c, float *humidity_pct);
