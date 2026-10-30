#pragma once

#include "fit.h"
#include <stdint.h>
#include <stdio.h>

typedef struct {
  FILE *file;
  uint32_t data_size; // байти повідомлень, без заголовка і кінцевого CRC
  uint32_t start_time;
  uint32_t last_time;
  uint32_t last_distance_cm;
  uint16_t max_speed_mm_s;
} fit_writer_t;

int fit_writer_open(fit_writer_t *writer, const char *path,
                    uint32_t time_created);
int fit_writer_add_record(fit_writer_t *writer, const fit_record_t *record);
int fit_writer_close(fit_writer_t *writer);
