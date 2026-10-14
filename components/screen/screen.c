#include "screen.h"

#include <stdio.h>
#include "esp_timer.h"
#include "u8g2_hal.h"

static u8g2_t u8g2;

/* u8g2 малює рядок від лівого краю по базовій лінії.
 * Щоб відцентрувати — треба спершу спитати ширину вже вибраним шрифтом. */
static void draw_centered(const char *s, int cx, int y)
{
    int w = u8g2_GetStrWidth(&u8g2, s);
    u8g2_DrawStr(&u8g2, cx - w / 2, y, s);
}

/* Те саме, але притиснуте до правого краю x_right. */
static void draw_right(const char *s, int x_right, int y)
{
    int w = u8g2_GetStrWidth(&u8g2, s);
    u8g2_DrawStr(&u8g2, x_right - w, y, s);
}

/* Рядок «підпис ліворуч — значення праворуч». */
static void draw_row(const char *label, const char *value, int y)
{
    const int pad = 16;
    const int W = u8g2_GetDisplayWidth(&u8g2);
    u8g2_SetFont(&u8g2, u8g2_font_helvR10_tr);
    u8g2_DrawStr(&u8g2, pad, y, label);
    u8g2_SetFont(&u8g2, u8g2_font_helvB14_tr);
    draw_right(value, W - pad, y);
}

/* Компонування — варіант A з docs/superpowers/specs/2026-10-07-gps-trip-design.md:
 * рядок стану / місце під мапу / панель швидкості. */
static void render(const screen_data_t *d)
{
    const int W = u8g2_GetDisplayWidth(&u8g2);
    const int H = u8g2_GetDisplayHeight(&u8g2);
    const int status_h = 22;
    const int panel_top = 296; /* усе, що вище — місце під мапу */
    char buf[32];

    u8g2_ClearBuffer(&u8g2);

    /* --- Рядок стану: інверсія --- */
    u8g2_DrawBox(&u8g2, 0, 0, W, status_h);
    u8g2_SetDrawColor(&u8g2, 0);
    u8g2_SetFont(&u8g2, u8g2_font_helvB12_tr);
    if (d->rtc_valid)
        snprintf(buf, sizeof(buf), "%02u:%02u:%02u %02u/%02u/%02u UTC", d->rtc_hour, d->rtc_min, d->rtc_sec, d->rtc_day, d->rtc_month, d->rtc_year);
    else
        snprintf(buf, sizeof(buf), "--:-- UTC");
    u8g2_DrawStr(&u8g2, 6, 17, buf);
    snprintf(buf, sizeof(buf), "%u SAT", d->satellites);
    draw_centered(buf, W / 2 + 70, 17);
    if (d->battery_valid)
        snprintf(buf, sizeof(buf), "%d%%", d->battery_pct);
    else
        snprintf(buf, sizeof(buf), "--%%");
    draw_right(buf, W - 6, 17);
    u8g2_SetDrawColor(&u8g2, 1);

    /* --- Місце під мапу (пункт 6). Поки — стан GPS і датчики. --- */
    const int row_h = 40;
    int y = status_h + 34;

    draw_row("GPS", d->gps_valid ? "FIX" : "NO FIX", y);
    y += row_h;
    if (d->gps_valid)
        snprintf(buf, sizeof(buf), "%.1f", d->hdop);
    else
        snprintf(buf, sizeof(buf), "--");
    draw_row("HDOP", buf, y);
    y += row_h;
    if (d->gps_valid)
        snprintf(buf, sizeof(buf), "%.5f  %.5f", d->lat_deg, d->lon_deg);
    else
        snprintf(buf, sizeof(buf), "--");
    draw_row("POS", buf, y);
    y += row_h;

    u8g2_DrawHLine(&u8g2, 16, y - 26, W - 32); /* GPS зверху, датчики знизу */

    if (d->temp_valid)
        snprintf(buf, sizeof(buf), "%.1f C", d->temp_c);
    else
        snprintf(buf, sizeof(buf), "--.-");
    draw_row("TEMP", buf, y);
    y += row_h;
    if (d->humidity_valid)
        snprintf(buf, sizeof(buf), "%.0f%%", d->humidity_pct);
    else
        snprintf(buf, sizeof(buf), "--%%");
    draw_row("HUMIDITY", buf, y);

    /* --- Панель даних: подвійна лінія відділяє її від «мапи» --- */
    u8g2_DrawBox(&u8g2, 0, panel_top, W, 2);

    u8g2_SetFont(&u8g2, u8g2_font_logisoso50_tn);
    if (d->gps_valid)
        snprintf(buf, sizeof(buf), "%.1f", d->speed_kmh);
    else
        snprintf(buf, sizeof(buf), "--.-");
    u8g2_DrawStr(&u8g2, 10, panel_top + 64, buf);
    u8g2_SetFont(&u8g2, u8g2_font_helvR10_tr);
    u8g2_DrawStr(&u8g2, 12, panel_top + 90, "km/h");

    const int col_x = 172;
    u8g2_DrawVLine(&u8g2, col_x - 10, panel_top + 10, H - panel_top - 20);

    u8g2_SetFont(&u8g2, u8g2_font_helvR08_tr);
    u8g2_DrawStr(&u8g2, col_x, panel_top + 18, "DIST");
    u8g2_DrawStr(&u8g2, col_x, panel_top + 62, "MOVING");
    u8g2_SetFont(&u8g2, u8g2_font_helvB18_tr);
    snprintf(buf, sizeof(buf), "%.1f km", d->dist_m / 1000.0);
    u8g2_DrawStr(&u8g2, col_x, panel_top + 42, buf);
    snprintf(buf, sizeof(buf), "%lu:%02lu:%02lu",
             (unsigned long)(d->moving_s / 3600),
             (unsigned long)(d->moving_s / 60 % 60),
             (unsigned long)(d->moving_s % 60));
    u8g2_DrawStr(&u8g2, col_x, panel_top + 86, buf);

    u8g2_DrawFrame(&u8g2, 0, 0, W, H);
}

void screen_init(void)
{
    rlcd_init(&u8g2);
}

int64_t screen_draw(const screen_data_t *d)
{
    render(d);

    /* Скільки коштує виштовхнути кадр. Пам'ятай: на дроті ~30 КБ, бо
     * драйвер перепаковує 3 байти буфера в 6 байтів для контролера. */
    int64_t t0 = esp_timer_get_time();
    u8g2_SendBuffer(&u8g2);
    return esp_timer_get_time() - t0;
}
