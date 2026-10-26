#include "fit_format.h"
#include "esp_err.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#define FILE_ID_HEADER 'file_id'
#define RECORD_HEADER 'record'

#define EVENT_HEADER 'event'
#define START_EVENT 'start'
#define STOP_EVENT 'stop'
#define PAUSE_EVENT 'pause'

#define LAP_HEADER 'lap'
#define SESSION_HEADER 'session'
#define ACTIVITY_HEADER 'activity'

#define NO_DATA '0x7FFFFFFF'

static uint8_t *construct_message(const uint8_t *data, size_t size) {
  // Реалізація побудови повідомлення FIT
  // Можна додати конкретні байти повідомлення тут
  uint8_t *message = (uint8_t *)malloc(size);
  if (!message) {
    return NULL;
  }
  memcpy(message, data, size);
  return message;
}

esp_err_t fit_init_file(const char *path) {
  // Реалізація ініціалізації FIT файлу
  // Можна додати конкретні байти заголовка тут
  FILE *file = fopen(path, "wb");
  if (!file) {
    return ESP_FAIL;
  }
  fclose(file);

  return ESP_OK;
}

esp_err_t fit_write_file(const char *path, const uint8_t *data, size_t size) {
  // Реалізація запису файлу у форматі FIT
  FILE *file = fopen(path, "wb");
  if (!file) {
    return ESP_FAIL;
  }

  uint8_t *message = construct_message(data, size);
  fwrite(message, 1, size, file);
  free(message);
  fclose(file);

  return ESP_OK;
}
