#pragma once

#include "driver/uart.h"
#include "esp_log.h"
#include "nmea.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define RX_PIN 44
#define TX_PIN 43

#define UART_NUM UART_NUM_0
#define UART_BUFFER_SIZE 1024

typedef struct {
  bool valid;
  int satellites;
  float hdop;
  float speed_kmh;
  double lat, lon;

  double dist_km;
  double moving_time;
  double average_speed;
  double stopped_time;

  uint8_t hh, mm, ss;
  uint8_t day, month, year;

} gps_data_t;

esp_err_t gps_init(void);
gps_data_t gps_get(void);

typedef void (*gps_cb_t)(const gps_data_t *data);
esp_err_t gps_start(gps_cb_t cb);
