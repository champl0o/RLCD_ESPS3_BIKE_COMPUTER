#include "map_screen.h"
#include <stdio.h>

/* Заглушка: справжня векторна мапа — етап 6. Поки лише рамка, текст і швидкість. */
void draw_map_screen(u8g2_t *u8g2, const screen_data_t *d)
{
    const int W = u8g2_GetDisplayWidth(u8g2);
    const int H = u8g2_GetDisplayHeight(u8g2);
    char buf[32];

    u8g2_ClearBuffer(u8g2);
    draw_status_bar(u8g2, d);

    u8g2_SetFont(u8g2, u8g2_font_helvB18_tr);
    draw_centered(u8g2, "MAP", W / 2, H / 2 - 8);
    u8g2_SetFont(u8g2, u8g2_font_helvR10_tr);
    draw_centered(u8g2, "coming soon", W / 2, H / 2 + 14);

    if (d->gps_valid)
        snprintf(buf, sizeof(buf), "%.1f km/h", d->speed_kmh);
    else
        snprintf(buf, sizeof(buf), "--.- km/h");
    u8g2_SetFont(u8g2, u8g2_font_helvB14_tr);
    draw_centered(u8g2, buf, W / 2, H - 40);

    draw_page_dots(u8g2, SCREEN_MAP);
    u8g2_DrawFrame(u8g2, 0, 0, W, H);
}
