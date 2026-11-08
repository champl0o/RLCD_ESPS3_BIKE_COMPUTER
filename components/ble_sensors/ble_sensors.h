#pragma once

#include "ble_parse.h"

typedef void (*ble_hr_cb_t)(hr_data_t hr_data);
typedef void (*ble_lost_cb_t)(void);

void ble_sensors_init(ble_hr_cb_t on_hr, ble_lost_cb_t on_lost);
