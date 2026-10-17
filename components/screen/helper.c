#include "helper.h"
#include <stdio.h>

/* u8g2 малює рядок від лівого краю по базовій лінії.
 * Щоб відцентрувати — треба спершу спитати ширину вже вибраним шрифтом. */
void draw_centered(u8g2_t *u8g2, const char *s, int cx, int y)
{
    int w = u8g2_GetStrWidth(u8g2, s);
    u8g2_DrawStr(u8g2, cx - w / 2, y, s);
}

/* Те саме, але притиснуте до правого краю x_right. */
void draw_right(u8g2_t *u8g2, const char *s, int x_right, int y)
{
    int w = u8g2_GetStrWidth(u8g2, s);
    u8g2_DrawStr(u8g2, x_right - w, y, s);
}

/* Рядок «підпис ліворуч — значення праворуч». */
void draw_row(u8g2_t *u8g2, const char *label, const char *value, int y)
{
    const int pad = 16;
    const int W = u8g2_GetDisplayWidth(u8g2);
    u8g2_SetFont(u8g2, u8g2_font_helvR10_tr);
    u8g2_DrawStr(u8g2, pad, y, label);
    u8g2_SetFont(u8g2, u8g2_font_helvB14_tr);
    draw_right(u8g2, value, W - pad, y);
}

void draw_status_bar(u8g2_t *u8g2, const screen_data_t *d)
{
    const int W = u8g2_GetDisplayWidth(u8g2);
    char buf[16];

    u8g2_DrawBox(u8g2, 0, 0, W, 24);
    u8g2_SetDrawColor(u8g2, 0);
    u8g2_SetFont(u8g2, u8g2_font_helvB12_tr);
    if (d->rtc_valid)
        snprintf(buf, sizeof(buf), "%02u:%02u", d->rtc_hour, d->rtc_min);
    else
        snprintf(buf, sizeof(buf), "--:--");
    u8g2_DrawStr(u8g2, 6, 17, buf);
    snprintf(buf, sizeof(buf), "%u SAT", d->satellites);
    draw_centered(u8g2, buf, W / 2, 17);
    if (d->battery_valid)
        snprintf(buf, sizeof(buf), "%d%%", d->battery_pct);
    else
        snprintf(buf, sizeof(buf), "--%%");
    draw_right(u8g2, buf, W - 6, 17);
    u8g2_SetDrawColor(u8g2, 1);
}

/* Заповнена крапка = поточний екран, порожнє кільце = решта. */
void draw_page_dots(u8g2_t *u8g2, current_screen_t current)
{
    const int W = u8g2_GetDisplayWidth(u8g2);
    const int H = u8g2_GetDisplayHeight(u8g2);

    for (int i = 0; i < SCREEN_COUNT; i++)
    {
        int cx = W / 2 + (i - SCREEN_COUNT / 2) * 16;
        if (i == (int)current)
            u8g2_DrawDisc(u8g2, cx, H - 8, 3, U8G2_DRAW_ALL);
        else
            u8g2_DrawCircle(u8g2, cx, H - 8, 3, U8G2_DRAW_ALL);
    }
}
