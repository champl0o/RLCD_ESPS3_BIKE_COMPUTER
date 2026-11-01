#pragma once

#include "button_types.h"
#include <stdbool.h>
#include <stdint.h>

typedef void (*button_callback_t)(btn_event_t event);
void button_init(button_callback_t callback);
