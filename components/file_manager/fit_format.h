#pragma once

#include "esp_err.h"
#include <stdint.h>

esp_err_t fit_init_file(const char *path);
esp_err_t fit_write_file(const char *path, const uint8_t *data, size_t size);
