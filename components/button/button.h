#pragma once

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <stdint.h>
#include <stdbool.h>
#include "esp_log.h"
#include "esp_timer.h"

#define BOOT_PIN 0
#define KEY_PIN 18

#define DEBOUNCE_TIME_US (30 * 1000)

typedef enum
{
    BTN_BOOT = 0,
    BTN_KEY,
    BTN_COUNT
} btn_id_t;

typedef enum
{
    PRESS_SHORT,
    PRESS_LONG,
    PRESS_DOUBLE
} btn_press_t;

typedef struct
{
    bool stable;
    int64_t deadline_us;
    enum
    {
        IDLE,
        DOWN,
        WAIT_2ND
    } state;
    bool long_fired;
    bool debouncing;
} btn_state_t;

typedef struct
{
    btn_id_t id;
    btn_press_t press;
} btn_event_t;

void button_init(void);
btn_event_t button_get_event(void);
