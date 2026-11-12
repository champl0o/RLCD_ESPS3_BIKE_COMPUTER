#include "helper.h"
#include <stdio.h>
#include <string.h>

/* u8g2 draws a string from the left edge along the baseline.
 * To centre it, first ask for the width using the already selected font. */
void draw_centered(u8g2_t *u8g2, const char *s, int cx, int y)
{
    int w = u8g2_GetStrWidth(u8g2, s);
    u8g2_DrawStr(u8g2, cx - w / 2, y, s);
}

/* The same, but pressed against the right edge x_right. */
void draw_right(u8g2_t *u8g2, const char *s, int x_right, int y)
{
    int w = u8g2_GetStrWidth(u8g2, s);
    u8g2_DrawStr(u8g2, x_right - w, y, s);
}

/* Shared row body: number, label, value on the right, and between them a dotted line or,
 * if bar_pct >= 0, a fill scale. */
static void draw_row_impl(u8g2_t *u8g2, int n, const char *label, const char *value, int y, int bar_pct)
{
    const int pad = 14;
    const int label_x = pad + 16; /* room for a two-digit number in the 5x7 font */
    const int W = u8g2_GetDisplayWidth(u8g2);
    char num[4];

    snprintf(num, sizeof(num), "%d", n);
    u8g2_SetFont(u8g2, u8g2_font_5x7_tr);
    u8g2_DrawStr(u8g2, pad, y, num);

    u8g2_SetFont(u8g2, u8g2_font_helvR10_tr);
    u8g2_DrawStr(u8g2, label_x, y, label);
    int x0 = label_x + u8g2_GetStrWidth(u8g2, label) + 6;
    u8g2_SetFont(u8g2, u8g2_font_helvB14_tr);
    int x1 = W - pad - u8g2_GetStrWidth(u8g2, value) - 6;

    /* an invalid value is a hatched plate, never a bare dash */
    if (strncmp(value, "--", 2) == 0 || strcmp(value, "NO FIX") == 0)
    {
        draw_hatched_value(u8g2, value, x0, W - pad, y, 2);
        return;
    }
    draw_right(u8g2, value, W - pad, y);

    if (bar_pct >= 0 && x1 - x0 >= 12)
    {
        int w = x1 - x0;
        u8g2_DrawFrame(u8g2, x0, y - 9, w, 9);
        u8g2_DrawBox(u8g2, x0 + 2, y - 7, (w - 4) * bar_pct / 100, 5);
        return;
    }

    /* dotted line from the label to the value so the eye does not get lost in the empty gap */
    for (int x = x0; x < x1; x += 3)
        u8g2_DrawPixel(u8g2, x, y - 2);
}

/* Row "No. label ... value". */
void draw_row(u8g2_t *u8g2, int n, const char *label, const char *value, int y)
{
    draw_row_impl(u8g2, n, label, value, y, -1);
}

/* The same, but with a 0..100% scale between the label and the value. */
void draw_row_bar(u8g2_t *u8g2, int n, const char *label, const char *value, int y, int pct)
{
    if (pct < 0)
        pct = 0;
    if (pct > 100)
        pct = 100;
    draw_row_impl(u8g2, n, label, value, y, pct);
}

/* How many GPS bars are lit: 0 = no fix. The HDOP thresholds are a proposal
 * from screens-plan.md §6.1, to be tuned on a real ride. */
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

/* 4 bars 3 px wide, step 5, heights 4/8/12/16 from the bottom line (x, y — top left
 * 16x16). An inactive bar is a "stub" 3x2. */
static void draw_gps_icon(u8g2_t *u8g2, int x, int y, int bars)
{
    for (int i = 0; i < 4; i++)
    {
        int h = (i < bars) ? 4 * (i + 1) : 2;
        u8g2_DrawBox(u8g2, x + i * 5, y + 16 - h, 3, h);
    }
}

/* microSD card 12x16 (x, y — top left of the 16x16). mounted: solid with contacts;
 * otherwise an outline + a diagonal. Contacts and the halo are drawn in the colour opposite to the current one. */
static void draw_sd_icon(u8g2_t *u8g2, int x, int y, bool mounted)
{
    const uint8_t fg = u8g2->draw_color;
    const uint8_t bg = !fg;

    if (mounted)
    {
        u8g2_DrawBox(u8g2, x + 2, y, 8, 16);
        u8g2_DrawBox(u8g2, x + 10, y + 4, 4, 12);
        for (int i = 0; i < 4; i++) /* cut corner */
            u8g2_DrawHLine(u8g2, x + 10, y + i, i + 1);
        u8g2_SetDrawColor(u8g2, bg);
        for (int cx = 4; cx <= 8; cx += 2) /* 3 contacts */
            u8g2_DrawVLine(u8g2, x + cx, y + 2, 5);
        u8g2_SetDrawColor(u8g2, fg);
    }
    else
    {
        u8g2_DrawHLine(u8g2, x + 2, y, 9);
        u8g2_DrawLine(u8g2, x + 10, y, x + 13, y + 3);
        u8g2_DrawVLine(u8g2, x + 13, y + 3, 13);
        u8g2_DrawHLine(u8g2, x + 2, y + 15, 12);
        u8g2_DrawVLine(u8g2, x + 2, y, 16);
        u8g2_SetDrawColor(u8g2, bg); /* halo under the diagonal */
        for (int d = -1; d <= 1; d++)
            u8g2_DrawLine(u8g2, x + 1, y + 15 + d, x + 14, y + 2 + d);
        u8g2_SetDrawColor(u8g2, fg);
        u8g2_DrawLine(u8g2, x + 1, y + 15, x + 14, y + 2);
        u8g2_DrawLine(u8g2, x + 1, y + 14, x + 14, y + 1);
    }
}

/* Body 24x12 with a 2 px frame, a 2x6 nub, up to 4 solid segments 3x6 (25% each). 25%).
 * x, y — top left of the 26x16 area; valid == false gives an empty body. */
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
    /* ride maximum: a triangle under the scale to see how far below the peak you are */
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
    const int icon_y = 4; /* 16 px icon centred in the row of 24 px */
    char buf[16];

    u8g2_DrawBox(u8g2, 0, 0, W, 24);
    u8g2_SetDrawColor(u8g2, 0);
    u8g2_SetFont(u8g2, u8g2_font_helvB12_tr);
    if (d->rtc_valid)
        snprintf(buf, sizeof(buf), "%02u:%02u", d->rtc_hour, d->rtc_min);
    else
        snprintf(buf, sizeof(buf), "--:--");
    u8g2_DrawStr(u8g2, 6, 17, buf);

    /* recording state between the clock and GPS: a solid dot — recording, two
     * bars — paused. A word next to it, since there is no colour. */
    u8g2_SetFont(u8g2, u8g2_font_helvB10_tr);
    if (d->recording && !d->paused)
    {
        u8g2_DrawDisc(u8g2, 68, 12, 4, U8G2_DRAW_ALL);
        u8g2_DrawStr(u8g2, 77, 16, "REC");
    }
    else if (d->recording && d->paused)
    {
        u8g2_DrawBox(u8g2, 64, 7, 3, 10);
        u8g2_DrawBox(u8g2, 70, 7, 3, 10);
        u8g2_DrawStr(u8g2, 77, 16, "PAUSE");
    }
    u8g2_SetFont(u8g2, u8g2_font_helvB12_tr);

    draw_gps_icon(u8g2, W / 2 - 9, icon_y, gps_bars(d));
    if (d->gps_valid)
    {
        u8g2_SetFont(u8g2, u8g2_font_5x7_tr);
        snprintf(buf, sizeof(buf), "%02u", d->satellites);
        u8g2_DrawStr(u8g2, W / 2 + 16, 16, buf);
        u8g2_SetFont(u8g2, u8g2_font_helvB12_tr);
    }

    draw_sd_icon(u8g2, 200, icon_y, d->sd_mounted);

    /* the number without a % sign on the right, the battery icon to the left of it */
    if (d->battery_valid)
        snprintf(buf, sizeof(buf), "%d", d->battery_pct);
    else
        snprintf(buf, sizeof(buf), "--");
    draw_right(u8g2, buf, W - 6, 17);
    draw_battery_icon(u8g2, W - 6 - u8g2_GetStrWidth(u8g2, buf) - 4 - 26, icon_y,
                      d->battery_valid, d->battery_pct);
    u8g2_SetDrawColor(u8g2, 1);

    /* hazard only when there is something to warn about: a permanent stripe is noise */
    if (screen_has_warning(d))
        draw_hazard(u8g2, 0, 24, W, 6, 8);
}

/* Three tabs with names: the active one is inverted, the next one (where KEY leads) has ">". */
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

void draw_hatch(u8g2_t *u8g2, int x, int y, int w, int h)
{
    /* 2 px on, 2 px off along every row, shifted by one per row: diagonal stripes.
     * Runs are drawn as lines, not pixel by pixel. */
    for (int j = 0; j < h; j++)
    {
        int i = 0;
        while (i < w)
        {
            if (((x + i + y + j) % 4) < 2)
            {
                const int start = i;
                while (i < w && ((x + i + y + j) % 4) < 2)
                    i++;
                u8g2_DrawHLine(u8g2, x + start, y + j, i - start);
            }
            else
                i++;
        }
    }
}

void draw_hatched_value(u8g2_t *u8g2, const char *text, int x_left, int x_right,
                        int baseline, int margin)
{
    const int tw = u8g2_GetStrWidth(u8g2, text);
    const int asc = u8g2_GetAscent(u8g2);
    const int desc = -u8g2_GetDescent(u8g2);
    const int plate_y = baseline - asc - 2;
    const int plate_h = asc + desc + 4;

    draw_hatch(u8g2, x_left, plate_y - margin, x_right - x_left, plate_h + 2 * margin);
    u8g2_SetDrawColor(u8g2, 0);
    u8g2_DrawBox(u8g2, x_right - tw - 10, plate_y, tw + 10, plate_h); /* ends at the hatch edge */
    u8g2_SetDrawColor(u8g2, 1);
    u8g2_DrawStr(u8g2, x_right - tw - 5, baseline, text);
}

void draw_state_block(u8g2_t *u8g2, const char *title, const char *reason)
{
    const int W = u8g2_GetDisplayWidth(u8g2);
    const int y = 26;
    const int h = 26; /* ends at row 51: the speed digits of Main start at row 52 */

    u8g2_SetDrawColor(u8g2, 1);
    u8g2_DrawBox(u8g2, 0, y, W, h);
    u8g2_SetDrawColor(u8g2, 0);
    u8g2_SetFont(u8g2, u8g2_font_helvB14_tr);
    u8g2_DrawStr(u8g2, 8, y + 20, title);
    if (reason)
    {
        u8g2_SetFont(u8g2, u8g2_font_5x7_tr);
        draw_right(u8g2, reason, W - 8, y + 18);
    }
    u8g2_SetDrawColor(u8g2, 1);
}

void draw_hint_strip(u8g2_t *u8g2, const char *text)
{
    const int W = u8g2_GetDisplayWidth(u8g2);
    const int H = u8g2_GetDisplayHeight(u8g2);
    const int y = H - 22;

    u8g2_DrawFrame(u8g2, 6, y, W - 12, 16);
    u8g2_SetFont(u8g2, u8g2_font_6x12_tr);
    draw_centered(u8g2, text, W / 2, y + 12);
}

void draw_bottom_strip(u8g2_t *u8g2, const screen_data_t *d, current_screen_t current)
{
    const char *hint = ui_hint(d->ui_state);

    if (d->hint_visible && hint)
        draw_hint_strip(u8g2, hint);
    else
        draw_page_tabs(u8g2, current);
}
