#ifndef MONITOR_RTC_H
#define MONITOR_RTC_H

#include <stdint.h>
#include "driver/i2c_master.h"
#include "esp_err.h"
#include <time.h>

typedef struct
{
    i2c_master_dev_handle_t dev;
} monitor_rtc_t;

esp_err_t monitor_rtc_init(monitor_rtc_t *rtc, i2c_master_bus_handle_t bus);
esp_err_t monitor_rtc_get_time(monitor_rtc_t *rtc, struct tm *out);
esp_err_t monitor_rtc_set_time(monitor_rtc_t *rtc, const struct tm *in);

#endif // MONITOR_RTC_H
