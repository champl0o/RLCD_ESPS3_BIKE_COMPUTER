#include "recorder.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "portmacro.h"
#include <string.h>

static const char *TAG = "recorder";
static QueueHandle_t recorder_queue = NULL;
static uint32_t dropped;

static void recorder_task(void *arg) {
  ESP_LOGI(TAG, "Recorder task started");
  fit_record_t record;

  while (1) {
    if (xQueueReceive(recorder_queue, &record, portMAX_DELAY) == pdTRUE) {
      // Process the record here
      ESP_LOGI(TAG, "Processing record: timestamp=%u", record.timestamp);
    }
  }
}

esp_err_t recorder_init(void) {
  ESP_LOGI(TAG, "Initializing recorder");

  if (recorder_queue != NULL) {
    ESP_LOGW(TAG, "Recorder already initialized");
    return ESP_OK;
  }

  recorder_queue = xQueueCreate(16, sizeof(fit_record_t));
  if (recorder_queue == NULL) {
    ESP_LOGE(TAG, "Failed to create recorder queue");
    return ESP_FAIL;
  }

  BaseType_t ret =
      xTaskCreate(recorder_task, "recorder_task", 4096, NULL, 3, NULL);
  if (ret != pdPASS) {
    ESP_LOGE(TAG, "Failed to create recorder task");
    vQueueDelete(recorder_queue);
    recorder_queue = NULL;
    return ESP_FAIL;
  }

  return ESP_OK;
}

// non blocking, returns ESP_OK if record was pushed, ESP_ERR_TIMEOUT if the
// queue is full
esp_err_t recorder_push(const fit_record_t *record) {
  if (recorder_queue == NULL) {
    ESP_LOGE(TAG, "Recorder not initialized");
    return ESP_ERR_INVALID_STATE;
  }

  if (xQueueSend(recorder_queue, record, 0) != pdTRUE) {
    ESP_LOGW(TAG, "Recorder queue full, dropping record");
    dropped++;
    ESP_LOGW(TAG, "Dropped records count: %u", dropped);
    return ESP_ERR_TIMEOUT;
  }

  return ESP_OK;
}

esp_err_t recorder_start(void) {
  ESP_LOGI(TAG, "Starting recorder");
  // Start code here
  return ESP_OK;
}

esp_err_t recorder_stop(void) {
  ESP_LOGI(TAG, "Stopping recorder");
  // Stop code here
  return ESP_OK;
}
