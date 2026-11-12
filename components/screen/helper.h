#pragma once

#include "u8g2_hal.h"
#include "screen.h"

void draw_centered(u8g2_t *u8g2, const char *s, int cx, int y);
void draw_right(u8g2_t *u8g2, const char *s, int x_right, int y);
void draw_row(u8g2_t *u8g2, int n, const char *label, const char *value, int y);
void draw_row_bar(u8g2_t *u8g2, int n, const char *label, const char *value, int y, int pct);

#define LOW_BATTERY_PCT 20
#define SPEED_BAR_MAX_KMH 50.0f

/* There is something to show in the warning stripe: no fix or low battery. */
bool screen_has_warning(const screen_data_t *d);

/* Diagonal stripe of period px (half black/half white) — only for warning states. */
void draw_hazard(u8g2_t *u8g2, int x, int y, int w, int h, int period);

/* Inverted state chip; compute its width via chip_width to press it to the right. */
int chip_width(u8g2_t *u8g2, const char *s);
void draw_chip(u8g2_t *u8g2, const char *s, int x, int y);

/* NO FIX / LOW BAT chips under the warning stripe (Main screen only). */
void draw_warning_chips(u8g2_t *u8g2, const screen_data_t *d);

/* Speed scale 0..SPEED_BAR_MAX_KMH: filled segments + a maximum triangle. */
void draw_speed_bar(u8g2_t *u8g2, const screen_data_t *d, int x, int y, int w, int h);

/* Group caption with a thin line to the right edge; returns the height it took. */
void draw_group_label(u8g2_t *u8g2, const char *label, int y);

/* Shared parts of all screens: the inverted status bar (24 px), the warning stripe
 * below it (when needed) and the page tabs. */
void draw_status_bar(u8g2_t *u8g2, const screen_data_t *d);
void draw_page_tabs(u8g2_t *u8g2, current_screen_t current);

/* Inverted state block under the status bar (28 px): title on the left, explanation
 * on the right. Fully covers the NO FIX / LOW BAT chips beneath it. */
void draw_state_block(u8g2_t *u8g2, const char *title, const char *reason);

/* Status grammar: a hatched plate means "waiting / not valid yet", a solid black
 * plate means "ready". Diagonal stripes, 2 px on and 2 px off. */
void draw_hatch(u8g2_t *u8g2, int x, int y, int w, int h);

/* Value on a hatched band: the text sits right-aligned on a white plate, and
 * the stripes run from x_left to x_right, `margin` px above and below the plate
 * (the plate is the text box plus 2 px). The font must already be set. */
void draw_hatched_value(u8g2_t *u8g2, const char *text, int x_left, int x_right,
                        int baseline, int margin);

/* Button hint in a frame in place of the tabs. */
void draw_hint_strip(u8g2_t *u8g2, const char *text);

/* Bottom strip: the hint when it is time to show it (screen_data_t.hint_visible),
 * otherwise the page tabs. */
void draw_bottom_strip(u8g2_t *u8g2, const screen_data_t *d, current_screen_t current);
