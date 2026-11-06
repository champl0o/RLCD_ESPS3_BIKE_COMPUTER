#pragma once

#include "model.h"
#include "screen.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void ui_init_screen(void);
void ui_notify(uint32_t bits);
void ui_draw_current_screen(void);

#define UI_REDRAW (1u << 0)
#define UI_TICK_RTC (1u << 1)
#define UI_TICK_CLIMATE (1u << 2)
#define UI_TICK_BATTERY (1u << 3)
