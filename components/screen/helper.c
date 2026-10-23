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

/* Спільне тіло рядка: номер, підпис, значення праворуч, а між ними пунктир або,
 * якщо bar_pct >= 0, шкала заповнення. */
static void draw_row_impl(u8g2_t *u8g2, int n, const char *label, const char *value, int y, int bar_pct)
{
    const int pad = 14;
    const int label_x = pad + 16; /* місце під двоцифровий номер шрифтом 5x7 */
    const int W = u8g2_GetDisplayWidth(u8g2);
    char num[4];

    snprintf(num, sizeof(num), "%d", n);
    u8g2_SetFont(u8g2, u8g2_font_5x7_tr);
    u8g2_DrawStr(u8g2, pad, y, num);

    u8g2_SetFont(u8g2, u8g2_font_helvR10_tr);
    u8g2_DrawStr(u8g2, label_x, y, label);
    int x0 = label_x + u8g2_GetStrWidth(u8g2, label) + 6;
    u8g2_SetFont(u8g2, u8g2_font_helvB14_tr);
    draw_right(u8g2, value, W - pad, y);
    int x1 = W - pad - u8g2_GetStrWidth(u8g2, value) - 6;

    if (bar_pct >= 0 && x1 - x0 >= 12)
    {
        int w = x1 - x0;
        u8g2_DrawFrame(u8g2, x0, y - 9, w, 9);
        u8g2_DrawBox(u8g2, x0 + 2, y - 7, (w - 4) * bar_pct / 100, 5);
        return;
    }

    /* пунктир від підпису до значення, щоб око не губилось у порожньому проміжку */
    for (int x = x0; x < x1; x += 3)
        u8g2_DrawPixel(u8g2, x, y - 2);
}

/* Рядок «№ підпис ... значення». */
void draw_row(u8g2_t *u8g2, int n, const char *label, const char *value, int y)
{
    draw_row_impl(u8g2, n, label, value, y, -1);
}

/* Те саме, але між підписом і значенням шкала 0..100%. */
void draw_row_bar(u8g2_t *u8g2, int n, const char *label, const char *value, int y, int pct)
{
    if (pct < 0)
        pct = 0;
    if (pct > 100)
        pct = 100;
    draw_row_impl(u8g2, n, label, value, y, pct);
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

static bool low_battery(const screen_data_t *d)
{
    return d->battery_valid && d->battery_pct < LOW_BATTERY_PCT;
}

bool screen_has_warning(const screen_data_t *d)
{
    return !d->gps_valid || low_battery(d);
}

void draw_hazard(u8g2_t *u8g2, int x, int y, int w, int h, int period)
{
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++)
            if (((i + h - j) % period) < period / 2)
                u8g2_DrawPixel(u8g2, x + i, y + j);
}

int chip_width(u8g2_t *u8g2, const char *s)
{
    u8g2_SetFont(u8g2, u8g2_font_5x7_tr);
    return u8g2_GetStrWidth(u8g2, s) + 6;
}

void draw_chip(u8g2_t *u8g2, const char *s, int x, int y)
{
    int w = chip_width(u8g2, s);
    u8g2_DrawBox(u8g2, x, y, w, 13);
    u8g2_SetDrawColor(u8g2, 0);
    u8g2_DrawStr(u8g2, x + 3, y + 10, s);
    u8g2_SetDrawColor(u8g2, 1);
}

void draw_warning_chips(u8g2_t *u8g2, const screen_data_t *d)
{
    const int W = u8g2_GetDisplayWidth(u8g2);
    const int y = 36;
    char buf[16];

    if (!d->gps_valid)
        draw_chip(u8g2, "NO FIX", 8, y);
    if (low_battery(d))
    {
        snprintf(buf, sizeof(buf), "LOW BAT %d%%", d->battery_pct);
        draw_chip(u8g2, buf, W - 8 - chip_width(u8g2, buf), y);
    }
}

void draw_speed_bar(u8g2_t *u8g2, const screen_data_t *d, int x, int y, int w, int h)
{
    const int n = 20, gap = 2;
    const int seg_w = (w - (n - 1) * gap) / n;
    int filled = 0, peak = 0;

    if (d->gps_valid)
        filled = (int)(d->speed_kmh / SPEED_BAR_MAX_KMH * n + 0.5f);
    peak = (int)(d->max_speed_kmh / SPEED_BAR_MAX_KMH * n);
    if (filled > n)
        filled = n;
    if (peak > n - 1)
        peak = n - 1;

    for (int i = 0; i < n; i++)
    {
        int sx = x + i * (seg_w + gap);
        if (i < filled)
            u8g2_DrawBox(u8g2, sx, y, seg_w, h);
        else
            u8g2_DrawFrame(u8g2, sx, y, seg_w, h);
    }
    /* максимум поїздки: трикутник під шкалою, щоб бачити, наскільки ти нижче піку */
    if (d->max_speed_kmh > 0.0f)
    {
        int cx = x + peak * (seg_w + gap) + seg_w / 2;
        u8g2_DrawTriangle(u8g2, cx - 3, y + h + 5, cx + 3, y + h + 5, cx, y + h + 1);
    }
}

void draw_group_label(u8g2_t *u8g2, const char *label, int y)
{
    const int W = u8g2_GetDisplayWidth(u8g2);
    u8g2_SetFont(u8g2, u8g2_font_5x7_tr);
    u8g2_DrawStr(u8g2, 10, y + 10, label);
    u8g2_DrawHLine(u8g2, 14 + u8g2_GetStrWidth(u8g2, label), y + 6, W - 24 - u8g2_GetStrWidth(u8g2, label));
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
    if (d->gps_valid)
    {
        u8g2_SetFont(u8g2, u8g2_font_5x7_tr);
        snprintf(buf, sizeof(buf), "%02u", d->satellites);
        u8g2_DrawStr(u8g2, W / 2 + 16, 16, buf);
        u8g2_SetFont(u8g2, u8g2_font_helvB12_tr);
    }

    /* число % без знака праворуч, іконка батареї лівіше від нього */
    if (d->battery_valid)
        snprintf(buf, sizeof(buf), "%d", d->battery_pct);
    else
        snprintf(buf, sizeof(buf), "--");
    draw_right(u8g2, buf, W - 6, 17);
    draw_battery_icon(u8g2, W - 6 - u8g2_GetStrWidth(u8g2, buf) - 4 - 26, icon_y,
                      d->battery_valid, d->battery_pct);
    u8g2_SetDrawColor(u8g2, 1);

    /* hazard лише коли є що попередити: постійна смуга — це шум */
    if (screen_has_warning(d))
        draw_hazard(u8g2, 0, 24, W, 6, 8);
}

/* Три вкладки з назвами: активна інвертована, наступна (куди веде KEY) має ">". */
void draw_page_tabs(u8g2_t *u8g2, current_screen_t current)
{
    static const char *names[SCREEN_COUNT] = {"MAIN", "DATA", "MAP"};
    const int W = u8g2_GetDisplayWidth(u8g2);
    const int H = u8g2_GetDisplayHeight(u8g2);
    const int gap = 4;
    const int cw = (W - 12 - gap * (SCREEN_COUNT - 1)) / SCREEN_COUNT;
    char buf[8];

    u8g2_SetFont(u8g2, u8g2_font_5x7_tr);
    for (int i = 0; i < SCREEN_COUNT; i++)
    {
        int x = 6 + i * (cw + gap);
        int y = H - 22;
        if (i == (int)current)
            u8g2_DrawBox(u8g2, x, y, cw, 16);
        else
            u8g2_DrawFrame(u8g2, x, y, cw, 16);

        snprintf(buf, sizeof(buf), "%s%s", (i == (int)((current + 1) % SCREEN_COUNT)) ? "> " : "", names[i]);
        u8g2_SetDrawColor(u8g2, i == (int)current ? 0 : 1);
        draw_centered(u8g2, buf, x + cw / 2, y + 12);
        u8g2_SetDrawColor(u8g2, 1);
    }
}
