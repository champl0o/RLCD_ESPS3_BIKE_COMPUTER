#include "screen.h"

#include <stdio.h>
#include "esp_timer.h"
#include "helper.h"
#include "detailed_screen.h"
#include "minimal_screen.h"

u8g2_t u8g2;

/* Компонування — варіант A з docs/superpowers/specs/2026-10-07-gps-trip-design.md:
 * рядок стану / місце під мапу / панель швидкості. */
static void render(const screen_data_t *d, current_screen_t screen)
{
    switch (screen)
    {
    case SCREEN_MINIMAL:
        draw_minimal_screen(&u8g2, d);
        break;
    case SCREEN_DETAILED:
    default:
        draw_detailed_screen(&u8g2, d);
        break;
    }
}

void screen_init(void)
{
    rlcd_init(&u8g2);
}

int64_t screen_draw(const screen_data_t *d, current_screen_t screen)
{
    render(d, screen);

    /* Скільки коштує виштовхнути кадр. Пам'ятай: на дроті ~30 КБ, бо
     * драйвер перепаковує 3 байти буфера в 6 байтів для контролера. */
    int64_t t0 = esp_timer_get_time();
    u8g2_SendBuffer(&u8g2);
    return esp_timer_get_time() - t0;
}
