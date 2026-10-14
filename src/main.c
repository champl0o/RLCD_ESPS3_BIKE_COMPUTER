/*
 * Велокомп: тут лише збирання модулів докупи й основний цикл.
 */

#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "driver/i2c_master.h"

#include "screen.h"
#include "shtc3.h"
#include "monitor_rtc.h"
#include "battery.h"
#include "gps.h"
#include "button.h"

#define I2C_SDA_GPIO 13
#define I2C_SCL_GPIO 14

static const char *TAG = "main";

/* Одна шина на всю плату: SHTC3, PCF85063, ES8311, ES7210.
 * Хендл повертаємо назовні — RTC потім додасться на неї ж. */
static i2c_master_bus_handle_t i2c_bus_init(void)
{
    const i2c_master_bus_config_t cfg = {
        .i2c_port = -1, /* драйвер сам вибере вільний порт */
        .sda_io_num = I2C_SDA_GPIO,
        .scl_io_num = I2C_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus;
    ESP_ERROR_CHECK(i2c_new_master_bus(&cfg, &bus));
    return bus;
}

/* NULL замість датчика = його немає, на екрані лишаються прочерки. */
static void update_climate(shtc3_t *shtc3, screen_data_t *d)
{
    if (!shtc3)
        return;
    /* Невдале читання гасить valid — інакше на екрані висіло б
     * старе значення, ніби свіже. */
    esp_err_t err = shtc3_read(shtc3, &d->temp_c, &d->humidity_pct);
    d->temp_valid = d->humidity_valid = (err == ESP_OK);
    if (err != ESP_OK)
        ESP_LOGW(TAG, "SHTC3 read failed: %s", esp_err_to_name(err));
}

static void update_battery(battery_t *battery, screen_data_t *d)
{
    if (!battery)
        return;
    int mv;
    d->battery_valid = battery_read_mv(battery, &mv) == ESP_OK;
    if (d->battery_valid)
    {
        d->battery_mv = mv;
        d->battery_pct = battery_percent_from_mv(mv);
    }
}

static void update_rtc(monitor_rtc_t *rtc, screen_data_t *d)
{
    if (!rtc)
        return;
    struct tm now;
    esp_err_t err = monitor_rtc_get_time(rtc, &now);
    d->rtc_valid = (err == ESP_OK);
    if (d->rtc_valid)
    {
        d->rtc_hour = now.tm_hour;
        d->rtc_min = now.tm_min;
        d->rtc_sec = now.tm_sec;
        d->rtc_day = now.tm_mday;
        d->rtc_month = now.tm_mon + 1;
        d->rtc_year = now.tm_year % 100;
    }
    if (err != ESP_OK)
        ESP_LOGW(TAG, "RTC read failed: %s", esp_err_to_name(err));
}

static void update_gps(screen_data_t *d, monitor_rtc_t *rtc)
{
    gps_data_t gps = gps_get();
    d->gps_valid = gps.valid;
    if (!gps.valid)
        return;

    if (rtc && !d->rtc_synced)
    {
        struct tm t = {
            .tm_year = gps.year + 2000 - 1900,
            .tm_mon = gps.month - 1,
            .tm_mday = gps.day,
            .tm_hour = gps.hh,
            .tm_min = gps.mm,
            .tm_sec = gps.ss,
        };
        mktime(&t); /* лише щоб порахувати tm_wday */
        if (monitor_rtc_set_time(rtc, &t) == ESP_OK)
            d->rtc_synced = true;
    }

    d->satellites = gps.satellites;
    d->hdop = gps.hdop;
    d->lat_deg = gps.lat;
    d->lon_deg = gps.lon;
    d->speed_kmh = gps.speed_kmh;

    d->utc_day = gps.day;
    d->utc_month = gps.month;
    d->utc_year = gps.year;
    d->utc_hh = gps.hh;
    d->utc_mm = gps.mm;
    d->utc_ss = gps.ss;
}

void app_main(void)
{
    screen_init();

    i2c_master_bus_handle_t bus = i2c_bus_init();
    static shtc3_t shtc3_dev;
    shtc3_t *shtc3 = shtc3_init(&shtc3_dev, bus) == ESP_OK ? &shtc3_dev : NULL;
    if (!shtc3)
        ESP_LOGW(TAG, "SHTC3 not found, temp/humidity will show dashes");
    static monitor_rtc_t rtc_dev;
    monitor_rtc_t *rtc = monitor_rtc_init(&rtc_dev, bus) == ESP_OK ? &rtc_dev : NULL;
    if (!rtc)
        ESP_LOGW(TAG, "RTC not found, time will not be set");

    static battery_t battery_dev;
    battery_t *battery = battery_init(&battery_dev) == ESP_OK ? &battery_dev : NULL;
    if (!battery)
        ESP_LOGW(TAG, "Battery ADC init failed, battery will show dashes");

    gps_init();

    screen_data_t data = {0}; /* усі valid = false → на екрані прочерки */
    uint32_t frame = 0;
    button_init();

    while (1)
    {
        data.uptime_s = (uint32_t)(esp_timer_get_time() / 1000000LL);

        update_climate(shtc3, &data);
        update_battery(battery, &data);
        update_rtc(rtc, &data);
        update_gps(&data, rtc);

        if (gps_poll() == 0)
            ESP_LOGW(TAG, "GPS: no bytes this second — check TX/RX crossover and power");

        int64_t dt = screen_draw(&data);

        if ((frame % 10) == 0)
        {
            ESP_LOGI(TAG, "frame %lu: SendBuffer took %lld us (max %.1f fps), %.2f C, %.1f %%RH",
                     (unsigned long)frame, (long long)dt,
                     dt > 0 ? 1000000.0 / (double)dt : 0.0,
                     data.temp_c, data.humidity_pct);
        }
        frame++;

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
