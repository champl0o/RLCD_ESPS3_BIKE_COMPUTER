#include "gesture.h"
#include "stddef.h"

typedef struct
{
    btn_event_type_t event;
    btn_state_enum_t current_state;
    btn_state_enum_t next_state;
    btn_gesture_t gesture;
    void (*action)(btn_state_t *b, int64_t now);
} btn_event_transition_t;

static void idle_press(btn_state_t *b, int64_t now)
{
    b->gesture_deadline_us = now + LONG_PRESS_TIME_US;
}

static void down_release(btn_state_t *b, int64_t now)
{
    b->gesture_deadline_us = now + SECOND_CLICK_TIME_US;
}

static void clean_deadline(btn_state_t *b, int64_t now)
{
    b->gesture_deadline_us = NO_DEADLINE;
}

static const btn_event_transition_t state_transitions[] = {
    {
        .event = PRESS,
        .current_state = IDLE,
        .next_state = DOWN,
        .gesture = GESTURE_NONE,
        .action = idle_press,
    },
    {
        .event = PRESS,
        .current_state = WAIT_2ND,
        .next_state = DOUBLE_DOWN,
        .gesture = PRESS_DOUBLE,
        .action = clean_deadline,
    },
    {
        .event = RELEASE,
        .current_state = DOWN,
        .next_state = WAIT_2ND,
        .gesture = GESTURE_NONE,
        .action = down_release,
    },
    {
        .event = RELEASE,
        .current_state = LONG_DOWN,
        .next_state = IDLE,
        .gesture = GESTURE_NONE,
        .action = clean_deadline,
    },
    {
        .event = RELEASE,
        .current_state = DOUBLE_DOWN,
        .next_state = IDLE,
        .gesture = GESTURE_NONE,
        .action = clean_deadline,
    },
    {
        .event = TIMEOUT,
        .current_state = DOWN,
        .next_state = LONG_DOWN,
        .gesture = PRESS_LONG,
        .action = clean_deadline,
    },
    {
        .event = TIMEOUT,
        .current_state = WAIT_2ND,
        .next_state = IDLE,
        .gesture = PRESS_SHORT,
        .action = clean_deadline,
    }};

btn_gesture_t handle_event(btn_state_t *b, btn_event_type_t event, int64_t now)
{
    int8_t state = b->state;
    btn_gesture_t gesture = GESTURE_NONE;

    for (size_t i = 0; i < sizeof(state_transitions) / sizeof(state_transitions[0]); i++)
    {
        const btn_event_transition_t *t = &state_transitions[i];
        if (t->event == event && t->current_state == state)
        {
            b->state = t->next_state;
            if (t->action)
            {
                t->action(b, now);
            }
            gesture = t->gesture;
        }
    }

    return gesture;
}
