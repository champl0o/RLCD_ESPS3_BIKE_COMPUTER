#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef enum
{
    GESTURE_NONE,
    PRESS_SHORT,
    PRESS_LONG,
    PRESS_DOUBLE
} btn_gesture_t;

typedef enum
{
    BTN_BOOT = 0,
    BTN_KEY,
    BTN_COUNT
} btn_id_t;

typedef struct
{
    btn_gesture_t gesture;
    btn_id_t btn_id;
} btn_event_t;
