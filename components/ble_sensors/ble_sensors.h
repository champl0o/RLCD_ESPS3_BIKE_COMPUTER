#pragma once
#include "esp_err.h"

typedef enum
{
    IDLE = 1,
    SCANNING,
    CONNECTING,
    DISCOVERING,
    SUBSCRIBING,
    SUBSCRIBED
} ble_sensors_state_t;

typedef struct
{
    ble_sensors_state_t state;
    ble_sensors_state_t next_state;
    void (*callback)(void);
} ble_sensors_event_t;

void ble_sensors_init(void);
