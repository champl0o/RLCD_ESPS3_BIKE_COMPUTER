#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
  uint16_t bpm;
  bool contact_supported;
  bool contact_detected;
} hr_data_t;

bool hr_parse(const uint8_t *data, size_t len, hr_data_t *out);
