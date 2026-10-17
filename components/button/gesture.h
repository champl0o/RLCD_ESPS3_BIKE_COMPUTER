#pragma once
#include "button_types.h"

#define NO_DEADLINE (-1)

#define SECOND_CLICK_TIME_US (300 * 1000)
#define LONG_PRESS_TIME_US (700 * 1000)

typedef enum
{
    PRESS = 0,
    RELEASE,
    TIMEOUT
} btn_event_type_t;

typedef enum
{
    IDLE = 0,
    DOWN,
    LONG_DOWN,
    WAIT_2ND,
    DOUBLE_DOWN,
} btn_state_enum_t;

typedef struct
{
    bool stable;
    uint8_t btn_id;

    int64_t debounce_deadline_us;
    int64_t gesture_deadline_us;

    btn_state_enum_t state;
} btn_state_t;

btn_gesture_t handle_event(btn_state_t *b, btn_event_type_t event, int64_t now);
