#pragma once

#include "button_types.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <stdbool.h>
#include <stdint.h>

QueueHandle_t button_init(void);
