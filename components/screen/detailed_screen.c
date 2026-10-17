#include "detailed_screen.h"
#include <stdio.h>

void draw_detailed_screen(u8g2_t *u8g2, const screen_data_t *d)
{
    const int W = u8g2_GetDisplayWidth(u8g2);
    const int H = u8g2_GetDisplayHeight(u8g2);
    const int row_h = 30;
    char buf[32];

    u8g2_ClearBuffer(u8g2);
    draw_status_bar(u8g2, d);

    /* Таблиця «підпис ліворуч — значення праворуч», рядки по 30 px. */
    int y = 24 + 24;

    if (d->rtc_valid)
        snprintf(buf, sizeof(buf), "%02u:%02u:%02u", d->rtc_hour, d->rtc_min, d->rtc_sec);
    else
        snprintf(buf, sizeof(buf), "--:--:--");
    draw_row(u8g2, "TIME", buf, y);
    y += row_h;

    snprintf(buf, sizeof(buf), "%.2f km", d->dist_m / 1000.0);
    draw_row(u8g2, "DIST", buf, y);
    y += row_h;

    snprintf(buf, sizeof(buf), "%lu:%02lu:%02lu",
             (unsigned long)(d->moving_s / 3600),
             (unsigned long)(d->moving_s / 60 % 60),
             (unsigned long)(d->moving_s % 60));
    draw_row(u8g2, "MOVING", buf, y);
    y += row_h;

    snprintf(buf, sizeof(buf), "%.1f km/h", d->average_speed);
    draw_row(u8g2, "AVG", buf, y);
    y += row_h;

    snprintf(buf, sizeof(buf), "%.1f km/h", d->max_speed_kmh);
    draw_row(u8g2, "MAX", buf, y);
    y += row_h;

    u8g2_DrawHLine(u8g2, 16, y - 20, W - 32); /* рух зверху, середовище знизу */

    if (d->temp_valid)
        snprintf(buf, sizeof(buf), "%.1f C", d->temp_c);
    else
        snprintf(buf, sizeof(buf), "--.-");
    draw_row(u8g2, "TEMP", buf, y);
    y += row_h;

    if (d->humidity_valid)
        snprintf(buf, sizeof(buf), "%.0f%%", d->humidity_pct);
    else
        snprintf(buf, sizeof(buf), "--%%");
    draw_row(u8g2, "HUMIDITY", buf, y);
    y += row_h;

    u8g2_DrawHLine(u8g2, 16, y - 20, W - 32); /* середовище зверху, GPS знизу */

    draw_row(u8g2, "GPS", d->gps_valid ? "FIX" : "NO FIX", y);
    y += row_h;

    if (d->gps_valid)
        snprintf(buf, sizeof(buf), "%.1f", d->hdop);
    else
        snprintf(buf, sizeof(buf), "--");
    draw_row(u8g2, "HDOP", buf, y);
    y += row_h;

    if (d->gps_valid)
        snprintf(buf, sizeof(buf), "%.4f %.4f", d->lat_deg, d->lon_deg);
    else
        snprintf(buf, sizeof(buf), "--");
    draw_row(u8g2, "POS", buf, y);
    y += row_h;

    if (d->battery_valid)
        snprintf(buf, sizeof(buf), "%d%%  %.2f V", d->battery_pct, d->battery_mv / 1000.0);
    else
        snprintf(buf, sizeof(buf), "--");
    draw_row(u8g2, "BATTERY", buf, y);

    draw_page_dots(u8g2, SCREEN_DETAILED);
    u8g2_DrawFrame(u8g2, 0, 0, W, H);
}
