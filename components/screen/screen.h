#pragma once

#include <stdint.h>
#include <stdbool.h>

/* Дані одного екрана. render() малює лише те, що тут є, і не знає,
 * звідки воно взялося — справжній датчик чи мок. valid-прапорці, щоб
 * відрізнити «0 км/год» від «ще не виміряли». */
typedef struct
{
    bool temp_valid;
    bool humidity_valid;
    bool battery_valid;
    int battery_mv;
    int battery_pct;
    float temp_c;
    float humidity_pct;
    uint32_t uptime_s;

    bool rtc_valid;
    uint8_t rtc_hour, rtc_min, rtc_sec, rtc_day, rtc_month, rtc_year;

    /* GPS + поїздка (поки з mock_gps) */
    bool gps_valid;
    bool rtc_synced;
    uint8_t utc_hh, utc_mm, utc_ss;
    uint8_t utc_day, utc_month, utc_year;
    uint8_t satellites;
    float hdop;
    double lat_deg, lon_deg;
    float speed_kmh;
    double dist_m; /* дистанція й час у русі валідні завжди — це накопичене */
    uint32_t moving_s;
} screen_data_t;

/* Ініціалізувати дисплей (SPI, ST7305, поворот). */
void screen_init(void);

/* Намалювати кадр і виштовхнути на панель.
 * Повертає, скільки мікросекунд зайняла передача по SPI. */
int64_t screen_draw(const screen_data_t *d);
