#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool sd_card_is_mounted(void);
esp_err_t sd_card_mount(void);
esp_err_t sd_card_unmount(void);

/* Reads exactly buffer_size bytes; a shorter file is an error. */
esp_err_t sd_card_read_file(const char *path, uint8_t *buffer,
                            size_t buffer_size);
esp_err_t sd_card_write_file(const char *path, const uint8_t *buffer,
                             size_t buffer_size);
