#pragma once

#include "u8g2_hal.h"
#include "screen.h"

void draw_centered(u8g2_t *u8g2, const char *s, int cx, int y);
void draw_right(u8g2_t *u8g2, const char *s, int x_right, int y);
void draw_row(u8g2_t *u8g2, const char *label, const char *value, int y);

/* Спільні частини всіх екранів: інверсний рядок стану (24 px) і крапки сторінок. */
void draw_status_bar(u8g2_t *u8g2, const screen_data_t *d);
void draw_page_dots(u8g2_t *u8g2, current_screen_t current);
