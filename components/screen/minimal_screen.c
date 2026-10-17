#include "minimal_screen.h"
#include <stdio.h>

/* Клітинка «підпис зверху, значення знизу», притиснута до правого краю x_right. */
static void draw_cell(u8g2_t *u8g2, const char *label, const char *value, int x_left, int x_right, int y_top)
{
    u8g2_SetFont(u8g2, u8g2_font_helvR08_tr);
    u8g2_DrawStr(u8g2, x_left, y_top + 14, label);
    u8g2_SetFont(u8g2, u8g2_font_helvB24_tr);
    draw_right(u8g2, value, x_right, y_top + 52);
}

void draw_minimal_screen(u8g2_t *u8g2, const screen_data_t *d)
{
    const int W = u8g2_GetDisplayWidth(u8g2);
    const int H = u8g2_GetDisplayHeight(u8g2);
    const int status_h = 24;
    const int pad = 12;
    const int mid_x = W / 2;
    char buf[32];

    u8g2_ClearBuffer(u8g2);

    draw_status_bar(u8g2, d);

    /* --- Швидкість: найбільше число, права рука вирівняна --- */
    u8g2_SetFont(u8g2, u8g2_font_logisoso78_tn);
    if (d->gps_valid)
        snprintf(buf, sizeof(buf), "%.1f", d->speed_kmh);
    else
        snprintf(buf, sizeof(buf), "--.-");
    draw_right(u8g2, buf, W - pad, status_h + 110);
    u8g2_SetFont(u8g2, u8g2_font_helvR10_tr);
    draw_right(u8g2, "km/h", W - pad, status_h + 140);

    /* --- Ряд 1: DIST | MOVING --- */
    const int row1_y = 192;
    u8g2_DrawHLine(u8g2, 0, row1_y - 2, W);
    snprintf(buf, sizeof(buf), "%.1f", d->dist_m / 1000.0);
    draw_cell(u8g2, "DIST km", buf, pad, mid_x - pad, row1_y + 6);
    snprintf(buf, sizeof(buf), "%lu:%02lu",
             (unsigned long)(d->moving_s / 3600),
             (unsigned long)(d->moving_s / 60 % 60));
    draw_cell(u8g2, "MOVING", buf, mid_x + pad, W - pad, row1_y + 6);
    u8g2_DrawVLine(u8g2, mid_x, row1_y + 6, 66);

    /* --- Ряд 2: AVG | MAX --- */
    const int row2_y = 290;
    u8g2_DrawHLine(u8g2, 0, row2_y - 2, W);
    snprintf(buf, sizeof(buf), "%.1f", d->average_speed);
    draw_cell(u8g2, "AVG km/h", buf, pad, mid_x - pad, row2_y + 6);
    snprintf(buf, sizeof(buf), "%.1f", d->max_speed_kmh);
    draw_cell(u8g2, "MAX km/h", buf, mid_x + pad, W - pad, row2_y + 6);
    u8g2_DrawVLine(u8g2, mid_x, row2_y + 6, 66);

    draw_page_dots(u8g2, SCREEN_MINIMAL);
    u8g2_DrawFrame(u8g2, 0, 0, W, H);
}
