#include "sdcard.h"
#include "driver/sdmmc_host.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>

#define TAG "SDCARD"
#define CLK 38
#define CMD 21
#define D0 39
#define WIDTH 1

static sdmmc_card_t *sdcard_host = NULL;

typedef struct {
  int clk;
  int cmd;
  int d0;
  int width;
  bool initialized_correctly;
  const char *name;
} sd_card_config_t;

static sd_card_config_t sd_card_config = {.clk = CLK,
                                          .cmd = CMD,
                                          .d0 = D0,
                                          .width = WIDTH,
                                          .initialized_correctly = false,
                                          .name = "/sdcard"};

static esp_err_t require_mounted(void) {
  if (!sd_card_is_mounted()) {
    ESP_LOGE(TAG, "SD card not initialized correctly.");
    return ESP_ERR_INVALID_STATE;
  }
  return ESP_OK;
}

static esp_err_t check_file(FILE *file, const char *path) {
  if (!file) {
    ESP_LOGE(TAG, "Failed to open %s: %s", path, strerror(errno));
    return ESP_FAIL;
  }

  return ESP_OK;
}

static esp_err_t is_file_errored(FILE *file) {
  if (ferror(file)) {
    ESP_LOGE(TAG, "File error occurred.");
    return ESP_FAIL;
  }
  return ESP_OK;
}

bool sd_card_is_mounted(void) { return sd_card_config.initialized_correctly; }

esp_err_t sd_card_mount(sd_card_mounted_callback_t callback) {
  if (sd_card_is_mounted())
    return ESP_OK;

  esp_vfs_fat_mount_config_t mount_cfg = {
      .format_if_mount_failed = false,
      .max_files = 5,                   // check later
      .allocation_unit_size = 16 * 1024 // 16 KB, adjust as needed
  };

  sdmmc_host_t host = SDMMC_HOST_DEFAULT();
  sdmmc_slot_config_t slot_cfg = SDMMC_SLOT_CONFIG_DEFAULT();
  slot_cfg.clk = sd_card_config.clk;
  slot_cfg.cmd = sd_card_config.cmd;
  slot_cfg.d0 = sd_card_config.d0;
  slot_cfg.width = sd_card_config.width;

  esp_err_t ret = esp_vfs_fat_sdmmc_mount(sd_card_config.name, &host, &slot_cfg,
                                          &mount_cfg, &sdcard_host);
  ESP_ERROR_CHECK_WITHOUT_ABORT(ret);
  if (ret != ESP_OK) {
    if (callback) {
      callback(NULL);
    }
    return ret;
  }
  ESP_LOGI(TAG, "SD card mounted successfully.");

  sd_card_config.initialized_correctly = true;

  if (callback) {
    callback(NULL);
  }

  return ESP_OK;
}

esp_err_t sd_card_unmount(void) {
  esp_err_t err = require_mounted();
  if (err != ESP_OK) {
    return err;
  }

  esp_err_t ret = esp_vfs_fat_sdcard_unmount(sd_card_config.name, sdcard_host);
  if (ret == ESP_OK) {
    sd_card_config.initialized_correctly = false;
    sdcard_host = NULL;
    ESP_LOGI(TAG, "SD card unmounted.");
  }

  return ret;
}

esp_err_t sd_card_read_file(const char *path, uint8_t *buffer,
                            size_t buffer_size) {
  esp_err_t err = require_mounted();
  if (err != ESP_OK) {
    return err;
  }

  FILE *file = fopen(path, "rb");
  err = check_file(file, path);
  if (err != ESP_OK) {
    return err;
  }

  size_t read_bytes = fread(buffer, 1, buffer_size, file);
  err = is_file_errored(file);
  fclose(file);

  if (err != ESP_OK) {
    return err;
  }
  if (read_bytes != buffer_size) {
    ESP_LOGE(TAG, "Failed to read the complete buffer from the file.");
    return ESP_FAIL;
  }

  return ESP_OK;
}

esp_err_t sd_card_write_file(const char *path, const uint8_t *buffer,
                             size_t buffer_size) {
  esp_err_t err = require_mounted();
  if (err != ESP_OK) {
    return err;
  }

  FILE *file = fopen(path, "wb");
  err = check_file(file, path);
  if (err != ESP_OK) {
    return err;
  }

  size_t written_bytes = fwrite(buffer, 1, buffer_size, file);
  err = is_file_errored(file);
  // fwrite only fills the buffer; a full card shows up when fclose flushes it.
  if (fclose(file) != 0) {
    ESP_LOGE(TAG, "Failed to close %s: %s", path, strerror(errno));
    return ESP_FAIL;
  }
  if (err != ESP_OK) {
    return err;
  }
  if (written_bytes != buffer_size) {
    ESP_LOGE(TAG, "Failed to write the complete buffer to the file.");
    return ESP_FAIL;
  }

  return ESP_OK;
}
