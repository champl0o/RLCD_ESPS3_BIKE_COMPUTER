#pragma once

#include "driver/uart.h"
#include "esp_log.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "nmea.h"

#define RX_PIN 44
#define TX_PIN 43

#define UART_NUM UART_NUM_0
#define UART_BUFFER_SIZE 1024

typedef struct
{
    bool valid;
    int satellites;
    float hdop;
    float speed_kmh;
    double lat, lon;
    uint8_t hh, mm, ss;
    uint8_t day, month, year;
} gps_data_t;

esp_err_t gps_init(void);
int gps_poll(void);
gps_data_t gps_get(void);
