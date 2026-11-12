#pragma once

#include "helper.h"
#include "screen.h"

/* Summary after the recording stops: distance, moving time, average and maximum
 * speed from the accumulated trip (it is not updated while the summary is shown). */
void draw_summary_screen(u8g2_t *u8g2, const screen_data_t *d);
