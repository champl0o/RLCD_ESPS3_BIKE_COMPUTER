#include "ble_sensors.h"
#include "host/ble_gatt.h"
#include "host/ble_hs_adv.h"
#include "nvs_flash.h"

#include "esp_log.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"

#define TAG "BLE"

static uint8_t own_addr_type;

void ble_store_config_init(void);
static void ble_sensors_on_reset(int reason);
static void ble_sensors_on_sync(void);
static void host_task(void *param);

void ble_sensors_init(void) {
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  ret = nimble_port_init();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to init nimble %d", ret);
    return;
  }

  // Initialize the NimBLE host configuration
  ble_hs_cfg.reset_cb = ble_sensors_on_reset;
  ble_hs_cfg.sync_cb = ble_sensors_on_sync;

  int rc;

  rc = ble_svc_gap_device_name_set("bike-computer");
  if (rc != 0) {
    ESP_LOGE(TAG, "Error setting device name; code=%d", rc);
    return;
  }
  nimble_port_freertos_init(host_task);
}

// BLE event callbacks and host task implementation

static int ble_sensors_gap_event(struct ble_gap_event *event, void *arg) {
  struct ble_hs_adv_fields fields;
  int rc;

  switch (event->type) {
  case BLE_GAP_EVENT_DISC:
    ESP_LOGI(TAG, "Discovered device: %02x:%02x:%02x:%02x:%02x:%02x",
             event->disc.addr.val[5], event->disc.addr.val[4],
             event->disc.addr.val[3], event->disc.addr.val[2],
             event->disc.addr.val[1], event->disc.addr.val[0]);
    ESP_LOGI(TAG, "RSSI: %d", event->disc.rssi);
    ESP_LOGI(TAG, "Event disc addr type: %d", event->disc.addr.type);

    rc = ble_hs_adv_parse_fields(&fields, event->disc.data,
                                 event->disc.length_data);
    if (rc != 0) {
      ESP_LOGE(TAG, "Error parsing advertising fields; rc=%d", rc);
    }

    ESP_LOGI(TAG, "Advertising fields parsed successfully");
    ESP_LOGI(TAG, "Advertising fields length: %.*s", fields.name_len);
    ESP_LOGI(TAG, "Advertising fields type: %d", fields.name);
    for (int i = 0; i < fields.num_uuids16; i++) {
      ESP_LOGI(TAG, "UUID16: %04x", fields.uuids16[i].value);
    }
    break;
  default:
    break;
  }
  return 0;
}
static void ble_sensors_on_reset(int reason) {
  ESP_LOGI(TAG, "Reset reason: %d", reason);
}

static void ble_sensors_on_sync(void) {
  int rc;
  struct ble_gap_disc_params disc_params = {0};

  rc = ble_hs_util_ensure_addr(0);
  if (rc != 0) {
    ESP_LOGE(TAG, "error ensuring address; rc=%d", rc);
    return;
  }

  rc = ble_hs_id_infer_auto(0, &own_addr_type);
  if (rc != 0) {
    ESP_LOGE(TAG, "error determining address type; rc=%d", rc);
    return;
  }

  disc_params.filter_duplicates = 1;

  rc = ble_gap_disc(own_addr_type, BLE_HS_FOREVER, &disc_params,
                    ble_sensors_gap_event, NULL);
  if (rc != 0) {
    ESP_LOGE(TAG, "Error initiating GAP discovery procedure; rc=%d", rc);
  }

  ESP_LOGI(TAG, "BLE on sync");
}

static void host_task(void *param) {
  ESP_LOGI(TAG, "BLE Host Task Started");
  nimble_port_run();
}
