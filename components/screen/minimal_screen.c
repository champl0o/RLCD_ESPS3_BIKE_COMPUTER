#include "minimal_screen.h"
#include <stdio.h>

/* Клітинка «підпис зверху, значення посередині, одиниця знизу», значення притиснуте до x_right. */
static void draw_cell(u8g2_t *u8g2, const char *label, const char *value, const char *unit,
                      int x_left, int x_right, int y_top)
{
    u8g2_SetFont(u8g2, u8g2_font_5x7_tr);
    u8g2_DrawStr(u8g2, x_left, y_top + 16, label);
    draw_right(u8g2, unit, x_right, y_top + 90);
    u8g2_SetFont(u8g2, u8g2_font_logisoso32_tn);
    draw_right(u8g2, value, x_right, y_top + 66);
}

#define HR_MAX_BPM 190 /* TODO: 220 - вік, потім у налаштування */
#define HR_ZONES 5

/* Зона 1..5 за відсотком від HR_MAX_BPM (50/60/70/80/90%); 0 — нижче зони 1. */
static int hr_zone(uint16_t bpm)
{
    int pct = bpm * 100 / HR_MAX_BPM;
    if (pct < 50)
        return 0;
    if (pct >= 90)
        return 5;
    return (pct - 50) / 10 + 1;
}

/* Клітинка пульсу: значення, шкала зон і підпис стану. «---» ніколи не схоже на 0. */
static void draw_hr_cell(u8g2_t *u8g2, const screen_data_t *d, int x_left, int x_right, int y_top)
{
    const bool no_skin = d->hr_contact_supported && !d->hr_contact_detected;
    const bool show = d->hr_valid && !no_skin;
    const int zone = show ? hr_zone(d->hr_bpm) : 0;
    const int gap = 2;
    const int seg_w = (x_right - x_left - (HR_ZONES - 1) * gap) / HR_ZONES;
    char buf[8];

    u8g2_SetFont(u8g2, u8g2_font_5x7_tr);
    u8g2_DrawStr(u8g2, x_left, y_top + 16, no_skin && d->hr_valid ? "HR NO SKIN" : "HR");
    draw_right(u8g2, "bpm", x_right, y_top + 90);

    if (show)
        snprintf(buf, sizeof(buf), "%u", (unsigned)d->hr_bpm);
    else
        snprintf(buf, sizeof(buf), "---");
    u8g2_SetFont(u8g2, u8g2_font_logisoso32_tn);
    draw_right(u8g2, buf, x_right, y_top + 66);

    for (int i = 0; i < HR_ZONES; i++)
    {
        int sx = x_left + i * (seg_w + gap);
        if (i < zone - 1)
            u8g2_DrawFrame(u8g2, sx, y_top + 74, seg_w, 6);
        else if (i == zone - 1)
            u8g2_DrawBox(u8g2, sx, y_top + 74, seg_w, 6);
        else
            u8g2_DrawHLine(u8g2, sx, y_top + 77, seg_w);
    }
}

void draw_minimal_screen(u8g2_t *u8g2, const screen_data_t *d)
{
    const int W = u8g2_GetDisplayWidth(u8g2);
    const int pad = 10;
    const int mid_x = W / 2;
    const int grid_y = 172;
    const int cell_h = 100;
    char buf[32];

    u8g2_ClearBuffer(u8g2);

    draw_status_bar(u8g2, d);
    draw_warning_chips(u8g2, d);

    /* --- Швидкість: найбільше число, права рука вирівняна --- */
    u8g2_SetFont(u8g2, u8g2_font_logisoso92_tn);
    if (d->gps_valid)
        snprintf(buf, sizeof(buf), "%.1f", d->speed_kmh);
    else
        snprintf(buf, sizeof(buf), "--.-");
    draw_right(u8g2, buf, W - pad, 144);

    /* шкала ліворуч від підпису: швидкість відносно 0..50 км/год і пік поїздки */
    u8g2_SetFont(u8g2, u8g2_font_5x7_tr);
    draw_right(u8g2, "KM/H", W - pad, 164);
    draw_speed_bar(u8g2, d, pad, 152, W - pad - 40 - pad, 8);

    /* --- Сітка 2x2: DIST | MOVING, AVG | HR --- */
    u8g2_DrawHLine(u8g2, 0, grid_y, W);
    u8g2_DrawHLine(u8g2, 0, grid_y + cell_h, W);
    u8g2_DrawHLine(u8g2, 0, grid_y + 2 * cell_h, W);
    u8g2_DrawVLine(u8g2, mid_x, grid_y, 2 * cell_h);

    snprintf(buf, sizeof(buf), "%.1f", d->dist_m / 1000.0);
    draw_cell(u8g2, "DIST", buf, "km", pad, mid_x - pad, grid_y);
    snprintf(buf, sizeof(buf), "%lu:%02lu",
             (unsigned long)(d->moving_s / 3600),
             (unsigned long)(d->moving_s / 60 % 60));
    draw_cell(u8g2, "MOVING", buf, "h:mm", mid_x + pad, W - pad, grid_y);
    snprintf(buf, sizeof(buf), "%.1f", d->average_speed);
    draw_cell(u8g2, "AVG", buf, "km/h", pad, mid_x - pad, grid_y + cell_h);
    draw_hr_cell(u8g2, d, mid_x + pad, W - pad, grid_y + cell_h);

    draw_page_tabs(u8g2, SCREEN_MINIMAL);
}
