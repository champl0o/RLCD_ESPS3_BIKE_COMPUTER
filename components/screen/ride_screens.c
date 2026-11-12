#include "ride_screens.h"
#include <stdio.h>

/* Cell "label on top, value below", value pressed to x_right. */
static void draw_stat(u8g2_t *u8g2, const char *label, const char *value,
                      int x_left, int x_right, int y_top)
{
    u8g2_SetFont(u8g2, u8g2_font_5x7_tr);
    u8g2_DrawStr(u8g2, x_left, y_top + 16, label);
    /* 24 px digits: "10:05" and "100.0" are 73 px wide, the cell has 80. */
    u8g2_SetFont(u8g2, u8g2_font_logisoso24_tn);
    draw_right(u8g2, value, x_right, y_top + 60);
}

void draw_summary_screen(u8g2_t *u8g2, const screen_data_t *d)
{
    const int W = u8g2_GetDisplayWidth(u8g2);
    const int pad = 10;
    const int col_w = W / 3;
    const int grid_y = 190;
    char buf[32];

    u8g2_ClearBuffer(u8g2);
    draw_status_bar(u8g2, d);
    draw_state_block(u8g2, ui_summary_title(), NULL);

    /* distance is the only big number, the rest are half the size */
    snprintf(buf, sizeof(buf), "%.1f", d->dist_m / 1000.0);
    u8g2_SetFont(u8g2, u8g2_font_logisoso92_tn);
    draw_right(u8g2, buf, W - pad, 160);
    u8g2_SetFont(u8g2, u8g2_font_5x7_tr);
    draw_right(u8g2, "KM", W - pad, 178);

    u8g2_DrawHLine(u8g2, 0, grid_y, W);
    u8g2_DrawHLine(u8g2, 0, grid_y + 100, W);
    u8g2_DrawVLine(u8g2, col_w, grid_y, 100);
    u8g2_DrawVLine(u8g2, 2 * col_w, grid_y, 100);

    snprintf(buf, sizeof(buf), "%lu:%02lu", (unsigned long)(d->moving_s / 3600),
             (unsigned long)(d->moving_s / 60 % 60));
    draw_stat(u8g2, "TIME h:mm", buf, pad, col_w - pad, grid_y);
    snprintf(buf, sizeof(buf), "%.1f", d->average_speed);
    draw_stat(u8g2, "AVG km/h", buf, col_w + pad, 2 * col_w - pad, grid_y);
    snprintf(buf, sizeof(buf), "%.1f", d->max_speed_kmh);
    draw_stat(u8g2, "MAX km/h", buf, 2 * col_w + pad, W - pad, grid_y);

    draw_bottom_strip(u8g2, d, SCREEN_MINIMAL);
}
