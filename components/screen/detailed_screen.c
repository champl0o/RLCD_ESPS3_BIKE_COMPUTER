#include "detailed_screen.h"
#include <stdio.h>

void draw_detailed_screen(u8g2_t *u8g2, const screen_data_t *d)
{
    const int row_h = 24;
    char buf[32];
    int n = 1; /* наскрізний номер рядка даних */

    u8g2_ClearBuffer(u8g2);
    draw_status_bar(u8g2, d);

    /* Таблиця «підпис ліворуч — значення праворуч», рядки по 24 px, групи з підписом.
     * top — верх наступного елемента; базова лінія рядка на 16 px нижче. */
    int top = 36;

    draw_group_label(u8g2, "MOTION", top);
    top += 14;

    if (d->rtc_valid)
        snprintf(buf, sizeof(buf), "%02u:%02u:%02u", d->rtc_hour, d->rtc_min, d->rtc_sec);
    else
        snprintf(buf, sizeof(buf), "--:--:--");
    draw_row(u8g2, n++, "TIME", buf, top + 16);
    top += row_h;

    snprintf(buf, sizeof(buf), "%.2f km", d->dist_m / 1000.0);
    draw_row(u8g2, n++, "DIST", buf, top + 16);
    top += row_h;

    snprintf(buf, sizeof(buf), "%lu:%02lu:%02lu",
             (unsigned long)(d->moving_s / 3600),
             (unsigned long)(d->moving_s / 60 % 60),
             (unsigned long)(d->moving_s % 60));
    draw_row(u8g2, n++, "MOVING", buf, top + 16);
    top += row_h;

    snprintf(buf, sizeof(buf), "%.1f km/h", d->average_speed);
    draw_row(u8g2, n++, "AVG", buf, top + 16);
    top += row_h;

    snprintf(buf, sizeof(buf), "%.1f km/h", d->max_speed_kmh);
    draw_row(u8g2, n++, "MAX", buf, top + 16);
    top += row_h + 4;

    draw_group_label(u8g2, "CLIMATE", top);
    top += 14;

    if (d->temp_valid)
        snprintf(buf, sizeof(buf), "%.1f C", d->temp_c);
    else
        snprintf(buf, sizeof(buf), "--.-");
    draw_row(u8g2, n++, "TEMP", buf, top + 16);
    top += row_h;

    if (d->humidity_valid)
        snprintf(buf, sizeof(buf), "%.0f%%", d->humidity_pct);
    else
        snprintf(buf, sizeof(buf), "--%%");
    draw_row(u8g2, n++, "HUMIDITY", buf, top + 16);
    top += row_h + 4;

    draw_group_label(u8g2, "GPS", top);
    top += 14;

    if (d->gps_valid)
        snprintf(buf, sizeof(buf), "%u SAT", d->satellites);
    else
        snprintf(buf, sizeof(buf), "NO FIX");
    draw_row(u8g2, n++, "FIX", buf, top + 16);
    top += row_h;

    if (d->gps_valid)
        snprintf(buf, sizeof(buf), "%.1f", d->hdop);
    else
        snprintf(buf, sizeof(buf), "--");
    draw_row(u8g2, n++, "HDOP", buf, top + 16);
    top += row_h;

    if (d->gps_valid)
        snprintf(buf, sizeof(buf), "%.4f %.4f", d->lat_deg, d->lon_deg);
    else
        snprintf(buf, sizeof(buf), "--");
    draw_row(u8g2, n++, "POS", buf, top + 16);
    top += row_h + 4;

    draw_group_label(u8g2, "POWER", top);
    top += 14;

    if (d->battery_valid)
        snprintf(buf, sizeof(buf), "%d%%  %.2f V", d->battery_pct, d->battery_mv / 1000.0);
    else
        snprintf(buf, sizeof(buf), "--");
    draw_row_bar(u8g2, n++, "BATTERY", buf, top + 16, d->battery_valid ? d->battery_pct : 0);

    draw_page_tabs(u8g2, SCREEN_DETAILED);
}
