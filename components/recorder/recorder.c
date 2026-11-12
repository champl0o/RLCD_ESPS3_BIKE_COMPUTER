#include "recorder.h"
#include "esp_log.h"
#include "fit_writer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>

#define RECORDER_QUEUE_LENGTH 16
#define RECORDER_TASK_STACK 6144
#define RECORDER_TASK_PRIORITY 3
#define RECORDER_FSYNC_INTERVAL_MS 5000 /* start value, to be measured */
#define RECORDER_POLL_MS 1000           /* wake up to sync with no messages */
#define PATH_SIZE 64
#define TMP_SUFFIX ".tmp"
#define FIT_EPOCH_UNIX_OFFSET 631065600 /* 1989-12-31 у Unix-секундах */

typedef enum {
  MSG_START = 0,
  MSG_STOP,
  MSG_RECORD,
} recorder_msg_type_t;

typedef struct {
  recorder_msg_type_t type;
  uint32_t start_time;
  fit_record_t record;
} recorder_msg_t;

typedef enum {
  REC_IDLE = 0,
  REC_RECORDING,
  REC_FAILED, /* write or sync failed: partial .tmp kept, records dropped */
} recorder_mode_t;

typedef struct {
  fit_writer_t writer;
  recorder_mode_t mode;
  char path[PATH_SIZE];
  char tmp_path[PATH_SIZE + sizeof(TMP_SUFFIX)];
  TickType_t last_sync;
  uint32_t lost_records;
} recorder_state_t;

static const char *TAG = "recorder";
static QueueHandle_t recorder_queue = NULL;
static uint32_t dropped;

static esp_err_t enqueue(const recorder_msg_t *msg) {
  if (recorder_queue == NULL) {
    ESP_LOGE(TAG, "Recorder not initialized");
    return ESP_ERR_INVALID_STATE;
  }

  if (xQueueSend(recorder_queue, msg, 0) != pdTRUE) {
    dropped++;
    ESP_LOGW(TAG,
             "Queue full, message type %d dropped (total dropped: %" PRIu32 ")",
             (int)msg->type, dropped);
    return ESP_ERR_TIMEOUT;
  }
  return ESP_OK;
}

static void build_path(char *path, size_t size, uint32_t fit_time) {
  time_t unix_time = (time_t)fit_time + FIT_EPOCH_UNIX_OFFSET;
  struct tm tm_utc;
  gmtime_r(&unix_time, &tm_utc);
  snprintf(path, size, "/sdcard/ride_%04d%02d%02d_%02d%02d%02d.fit",
           tm_utc.tm_year + 1900, tm_utc.tm_mon + 1, tm_utc.tm_mday,
           tm_utc.tm_hour, tm_utc.tm_min, tm_utc.tm_sec);
}

static void fail_recording(recorder_state_t *st, const char *why) {
  ESP_LOGE(TAG, "%s: card lost? Stopping, partial data kept in %s", why,
           st->tmp_path);
  fit_writer_abort(&st->writer);
  st->mode = REC_FAILED;
}

static void handle_start(recorder_state_t *st, uint32_t start_time) {
  if (st->mode == REC_RECORDING) {
    ESP_LOGW(TAG, "Start ignored: already recording");
    return;
  }
  if (st->mode == REC_FAILED) {
    ESP_LOGI(TAG, "Retrying after a failed recording");
    st->mode = REC_IDLE;
  }

  build_path(st->path, sizeof(st->path), start_time);
  snprintf(st->tmp_path, sizeof(st->tmp_path), "%s" TMP_SUFFIX, st->path);
  if (fit_writer_open(&st->writer, st->tmp_path, start_time) != 0) {
    ESP_LOGE(TAG, "Failed to open %s", st->tmp_path);
    return;
  }

  st->mode = REC_RECORDING;
  st->last_sync = xTaskGetTickCount();
  st->lost_records = 0;
  ESP_LOGI(TAG, "Recording to %s", st->tmp_path);
}

static void handle_record(recorder_state_t *st, const fit_record_t *record) {
  if (st->mode == REC_FAILED) {
    st->lost_records++;
    return;
  }
  if (st->mode != REC_RECORDING)
    return;

  if (fit_writer_add_record(&st->writer, record) != 0)
    fail_recording(st, "Write failed");
}

/* Flush to the card on a timer: a power cut then loses a few seconds of
 * records, not the whole file. */
static void sync_if_due(recorder_state_t *st) {
  if (st->mode != REC_RECORDING)
    return;
  if (xTaskGetTickCount() - st->last_sync <
      pdMS_TO_TICKS(RECORDER_FSYNC_INTERVAL_MS))
    return;

  st->last_sync = xTaskGetTickCount();
  if (fit_writer_sync(&st->writer) != 0)
    fail_recording(st, "Sync failed");
}

static void handle_stop(recorder_state_t *st) {
  if (st->mode == REC_FAILED) {
    ESP_LOGW(TAG, "Stop: recording had failed (%" PRIu32 " records lost), %s "
                  "kept for repair",
             st->lost_records, st->tmp_path);
    st->mode = REC_IDLE;
    return;
  }
  if (st->mode != REC_RECORDING) {
    ESP_LOGW(TAG, "Stop ignored: not recording");
    return;
  }

  st->mode = REC_IDLE;
  if (fit_writer_close(&st->writer) != 0) {
    ESP_LOGE(TAG, "Failed to close file cleanly, %s kept for repair",
             st->tmp_path);
    return;
  }

  /* Only a cleanly closed file gets the real name. */
  if (rename(st->tmp_path, st->path) != 0) {
    ESP_LOGE(TAG, "Failed to rename %s to %s", st->tmp_path, st->path);
    return;
  }

  ESP_LOGI(TAG, "Recording saved to %s (%" PRIu32 " bytes of data)", st->path,
           st->writer.data_size);
  ESP_LOGI(TAG, "Stack high water mark: %u bytes",
           (unsigned)uxTaskGetStackHighWaterMark(NULL));
}

static void recorder_task(void *arg) {
  recorder_state_t state = {.mode = REC_IDLE};
  recorder_msg_t msg;

  ESP_LOGI(TAG, "Recorder task started");

  for (;;) {
    if (xQueueReceive(recorder_queue, &msg, pdMS_TO_TICKS(RECORDER_POLL_MS)) ==
        pdTRUE) {
      switch (msg.type) {
      case MSG_START:
        handle_start(&state, msg.start_time);
        break;
      case MSG_RECORD:
        handle_record(&state, &msg.record);
        break;
      case MSG_STOP:
        handle_stop(&state);
        break;
      }
    }

    sync_if_due(&state);
  }
}

esp_err_t recorder_init(void) {
  ESP_LOGI(TAG, "Initializing recorder");

  if (recorder_queue != NULL) {
    ESP_LOGW(TAG, "Recorder already initialized");
    return ESP_OK;
  }

  recorder_queue = xQueueCreate(RECORDER_QUEUE_LENGTH, sizeof(recorder_msg_t));
  if (recorder_queue == NULL) {
    ESP_LOGE(TAG, "Failed to create recorder queue");
    return ESP_FAIL;
  }

  BaseType_t ret =
      xTaskCreate(recorder_task, "recorder_task", RECORDER_TASK_STACK, NULL,
                  RECORDER_TASK_PRIORITY, NULL);
  if (ret != pdPASS) {
    ESP_LOGE(TAG, "Failed to create recorder task");
    vQueueDelete(recorder_queue);
    recorder_queue = NULL;
    return ESP_FAIL;
  }

  return ESP_OK;
}

esp_err_t recorder_push(const fit_record_t *record) {
  recorder_msg_t msg = {.type = MSG_RECORD, .record = *record};
  return enqueue(&msg);
}

esp_err_t recorder_start(uint32_t fit_time) {
  recorder_msg_t msg = {.type = MSG_START, .start_time = fit_time};
  return enqueue(&msg);
}

esp_err_t recorder_stop(void) {
  recorder_msg_t msg = {.type = MSG_STOP};
  return enqueue(&msg);
}
