#include "map_screen.h"
#include <stdio.h>

/* Заглушка: справжня векторна мапа — етап 6. Поки порожня рамка, чесний стан
 * «мапи немає» і швидкість, щоб екран лишався корисним. */
void draw_map_screen(u8g2_t *u8g2, const screen_data_t *d)
{
    const int W = u8g2_GetDisplayWidth(u8g2);
    char buf[32];

    u8g2_ClearBuffer(u8g2);
    draw_status_bar(u8g2, d);

    u8g2_DrawFrame(u8g2, 6, 34, W - 12, 267);

    /* стрічка зі справжнім станом: модуля мапи ще немає */
    u8g2_DrawFrame(u8g2, 20, 150, W - 40, 41);
    draw_hazard(u8g2, 20, 150, W - 40, 6, 8);
    draw_hazard(u8g2, 20, 185, W - 40, 6, 8);
    u8g2_SetFont(u8g2, u8g2_font_5x7_tr);
    draw_centered(u8g2, "MAP NOT AVAILABLE", W / 2, 174);

    /* швидкість у нижній інверсній смузі */
    u8g2_DrawBox(u8g2, 6, 308, W - 12, 61);
    u8g2_SetDrawColor(u8g2, 0);
    u8g2_SetFont(u8g2, u8g2_font_5x7_tr);
    u8g2_DrawStr(u8g2, 16, 358, "KM/H");
    if (d->gps_valid)
        snprintf(buf, sizeof(buf), "%.1f", d->speed_kmh);
    else
        snprintf(buf, sizeof(buf), "--.-");
    u8g2_SetFont(u8g2, u8g2_font_logisoso32_tn);
    draw_right(u8g2, buf, W - 16, 358);
    u8g2_SetDrawColor(u8g2, 1);

    draw_page_tabs(u8g2, SCREEN_MAP);
}
