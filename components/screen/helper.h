#pragma once

#include "u8g2_hal.h"
#include "screen.h"

void draw_centered(u8g2_t *u8g2, const char *s, int cx, int y);
void draw_right(u8g2_t *u8g2, const char *s, int x_right, int y);
void draw_row(u8g2_t *u8g2, int n, const char *label, const char *value, int y);
void draw_row_bar(u8g2_t *u8g2, int n, const char *label, const char *value, int y, int pct);

#define LOW_BATTERY_PCT 20
#define SPEED_BAR_MAX_KMH 50.0f

/* Є що показати смугою-попередженням: немає фіксу або низька батарея. */
bool screen_has_warning(const screen_data_t *d);

/* Діагональна смуга period px (чорне/біле навпіл) — лише для станів-попереджень. */
void draw_hazard(u8g2_t *u8g2, int x, int y, int w, int h, int period);

/* Інверсна плашка зі станом; ширину рахуй через chip_width, щоб притиснути праворуч. */
int chip_width(u8g2_t *u8g2, const char *s);
void draw_chip(u8g2_t *u8g2, const char *s, int x, int y);

/* Плашки NO FIX / LOW BAT під смугою-попередженням (лише Головний екран). */
void draw_warning_chips(u8g2_t *u8g2, const screen_data_t *d);

/* Шкала швидкості 0..SPEED_BAR_MAX_KMH: заповнені сегменти + трикутник максимуму. */
void draw_speed_bar(u8g2_t *u8g2, const screen_data_t *d, int x, int y, int w, int h);

/* Підпис групи з тонкою лінією до правого краю; повертає висоту, яку зайняла. */
void draw_group_label(u8g2_t *u8g2, const char *label, int y);

/* Спільні частини всіх екранів: інверсний рядок стану (24 px), смуга-попередження
 * під ним (за потреби) і вкладки сторінок. */
void draw_status_bar(u8g2_t *u8g2, const screen_data_t *d);
void draw_page_tabs(u8g2_t *u8g2, current_screen_t current);
