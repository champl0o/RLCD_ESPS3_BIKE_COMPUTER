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

/* Скільки стовпчиків GPS світиться: 0 = немає фіксу. Пороги HDOP — пропозиція
 * з screens-plan.md §6.1, підібрати на реальній поїздці. */
static int gps_bars(const screen_data_t *d)
{
    if (!d->gps_valid)
        return 0;
    if (d->hdop < 2.0f)
        return 4;
    if (d->hdop < 5.0f)
        return 3;
    if (d->hdop < 10.0f)
        return 2;
    return 1;
}

/* 4 стовпчики 3 px, крок 5, висоти 4/8/12/16 від нижньої лінії (x, y — лівий верх
 * 16x16). Неактивний стовпчик — «пеньок» 3x2. */
static void draw_gps_icon(u8g2_t *u8g2, int x, int y, int bars)
{
    for (int i = 0; i < 4; i++)
    {
        int h = (i < bars) ? 4 * (i + 1) : 2;
        u8g2_DrawBox(u8g2, x + i * 5, y + 16 - h, 3, h);
    }
}

/* Корпус 24x12 з рамкою 2 px, носик 2x6, до 4 суцільних сегментів 3x6 (по 25%).
 * x, y — лівий верх 26x16; valid == false дає порожній корпус. */
static void draw_battery_icon(u8g2_t *u8g2, int x, int y, bool valid, int pct)
{
    int segs = valid ? (pct + 24) / 25 : 0;
    if (segs > 4)
        segs = 4;

    u8g2_DrawFrame(u8g2, x, y + 2, 24, 12);
    u8g2_DrawFrame(u8g2, x + 1, y + 3, 22, 10);
    u8g2_DrawBox(u8g2, x + 24, y + 5, 2, 6);
    for (int i = 0; i < segs; i++)
        u8g2_DrawBox(u8g2, x + 4 + i * 4, y + 5, 3, 6);
}

void draw_status_bar(u8g2_t *u8g2, const screen_data_t *d)
{
    const int W = u8g2_GetDisplayWidth(u8g2);
    const int icon_y = 4; /* 16 px іконка по центру рядка 24 px */
    char buf[16];

    u8g2_DrawBox(u8g2, 0, 0, W, 24);
    u8g2_SetDrawColor(u8g2, 0);
    u8g2_SetFont(u8g2, u8g2_font_helvB12_tr);
    if (d->rtc_valid)
        snprintf(buf, sizeof(buf), "%02u:%02u", d->rtc_hour, d->rtc_min);
    else
        snprintf(buf, sizeof(buf), "--:--");
    u8g2_DrawStr(u8g2, 6, 17, buf);

    draw_gps_icon(u8g2, W / 2 - 9, icon_y, gps_bars(d));

    /* число % без знака праворуч, іконка батареї лівіше від нього */
    if (d->battery_valid)
        snprintf(buf, sizeof(buf), "%d", d->battery_pct);
    else
        snprintf(buf, sizeof(buf), "--");
    draw_right(u8g2, buf, W - 6, 17);
    draw_battery_icon(u8g2, W - 6 - u8g2_GetStrWidth(u8g2, buf) - 4 - 26, icon_y,
                      d->battery_valid, d->battery_pct);
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
