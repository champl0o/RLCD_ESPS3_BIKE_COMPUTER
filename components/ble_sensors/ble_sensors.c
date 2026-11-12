#include "ble_sensors.h"
#include "ble_parse.h"
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

static ble_hr_cb_t hr_callback = NULL;
static ble_lost_cb_t lost_callback = NULL;

#define HR_SERVICE_UUID16 0x180D
#define HR_MEASUREMENT_UUID16 0x2A37
#define CCCD_UUID16 0x2902
#define CONNECT_TIMEOUT_MS 30000
#define HR_PACKET_MAX 20 /* flags + bpm come first; the rest is not used */

static uint16_t conn_handle = BLE_HS_CONN_HANDLE_NONE;
static bool connecting = false;

/* Handles found by GATT discovery; 0 means "not found yet" (0 is not a valid
 * ATT handle). */
static uint16_t hr_svc_end;
static uint16_t hr_val_handle;
static uint16_t hr_cccd_handle;

void ble_store_config_init(void);
static void ble_sensors_on_reset(int reason);
static void ble_sensors_on_sync(void);
static void start_scan(void);
static void host_task(void *param);

void ble_sensors_init(ble_hr_cb_t on_hr, ble_lost_cb_t on_lost) {
  hr_callback = on_hr;
  lost_callback = on_lost;
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

static void drop_connection(uint16_t conn, const char *why) {
  ESP_LOGW(TAG, "%s, terminating connection", why);
  ble_gap_terminate(conn, BLE_ERR_REM_USER_CONN_TERM);
}

static int on_cccd_written(uint16_t conn, const struct ble_gatt_error *error,
                           struct ble_gatt_attr *attr, void *arg) {
  if (error->status != 0) {
    drop_connection(conn, "CCCD write failed");
    return 0;
  }
  ESP_LOGI(TAG, "Subscribed to heart rate notifications");
  return 0;
}

static int on_dsc_disc(uint16_t conn, const struct ble_gatt_error *error,
                       uint16_t chr_val_handle, const struct ble_gatt_dsc *dsc,
                       void *arg) {
  if (error->status == 0) {
    if (hr_cccd_handle == 0 && ble_uuid_u16(&dsc->uuid.u) == CCCD_UUID16)
      hr_cccd_handle = dsc->handle;
    return 0;
  }

  if (error->status != BLE_HS_EDONE) {
    drop_connection(conn, "Descriptor discovery failed");
    return 0;
  }

  if (hr_cccd_handle == 0) {
    drop_connection(conn, "No CCCD on heart rate measurement");
    return 0;
  }

  const uint8_t enable_notify[2] = {0x01, 0x00};
  int rc = ble_gattc_write_flat(conn, hr_cccd_handle, enable_notify,
                                sizeof(enable_notify), on_cccd_written, NULL);
  if (rc != 0)
    drop_connection(conn, "Could not write CCCD");
  return 0;
}

static int on_chr_disc(uint16_t conn, const struct ble_gatt_error *error,
                       const struct ble_gatt_chr *chr, void *arg) {
  if (error->status == 0) {
    hr_val_handle = chr->val_handle;
    return 0;
  }

  if (error->status != BLE_HS_EDONE) {
    drop_connection(conn, "Characteristic discovery failed");
    return 0;
  }

  if (hr_val_handle == 0 || hr_val_handle >= hr_svc_end) {
    drop_connection(conn, "No usable heart rate measurement characteristic");
    return 0;
  }

  /* The CCCD sits right after the value handle, inside the same service. */
  int rc = ble_gattc_disc_all_dscs(conn, hr_val_handle + 1, hr_svc_end,
                                   on_dsc_disc, NULL);
  if (rc != 0)
    drop_connection(conn, "Could not start descriptor discovery");
  return 0;
}

static int on_svc_disc(uint16_t conn, const struct ble_gatt_error *error,
                       const struct ble_gatt_svc *service, void *arg) {
  static uint16_t svc_start;

  if (error->status == 0) {
    svc_start = service->start_handle;
    hr_svc_end = service->end_handle;
    return 0;
  }

  if (error->status != BLE_HS_EDONE) {
    drop_connection(conn, "Service discovery failed");
    return 0;
  }

  if (hr_svc_end == 0) {
    drop_connection(conn, "Heart rate service not found");
    return 0;
  }

  int rc = ble_gattc_disc_chrs_by_uuid(conn, svc_start, hr_svc_end,
                                       BLE_UUID16_DECLARE(HR_MEASUREMENT_UUID16),
                                       on_chr_disc, NULL);
  if (rc != 0)
    drop_connection(conn, "Could not start characteristic discovery");
  return 0;
}

static void start_hr_discovery(uint16_t conn) {
  hr_svc_end = 0;
  hr_val_handle = 0;
  hr_cccd_handle = 0;

  int rc = ble_gattc_disc_svc_by_uuid(
      conn, BLE_UUID16_DECLARE(HR_SERVICE_UUID16), on_svc_disc, NULL);
  if (rc != 0)
    drop_connection(conn, "Could not start service discovery");
}

static void handle_hr_notification(const struct ble_gap_event *event) {
  uint8_t buf[HR_PACKET_MAX];
  uint16_t len = OS_MBUF_PKTLEN(event->notify_rx.om);
  if (len > sizeof(buf))
    len = sizeof(buf);

  /* The mbuf is only valid inside this callback, so copy it out. */
  if (os_mbuf_copydata(event->notify_rx.om, 0, len, buf) != 0)
    return;

  hr_data_t hr;
  if (!hr_parse(buf, len, &hr)) {
    ESP_LOGW(TAG, "Malformed heart rate packet (%u bytes)", (unsigned)len);
    return;
  }
  if (hr_callback)
    hr_callback(hr);
}

static bool is_hr_sensor(const struct ble_hs_adv_fields *fields) {
  for (int i = 0; i < fields->num_uuids16; i++) {
    if (fields->uuids16[i].value == HR_SERVICE_UUID16)
      return true;
  }
  return false;
}

static int ble_sensors_gap_event(struct ble_gap_event *event, void *arg) {
  struct ble_hs_adv_fields fields;
  int rc;

  switch (event->type) {
  case BLE_GAP_EVENT_DISC:
    rc = ble_hs_adv_parse_fields(&fields, event->disc.data,
                                 event->disc.length_data);
    if (rc != 0) {
      ESP_LOGW(TAG, "Error parsing advertising fields; rc=%d", rc);
      break;
    }

    if (!is_hr_sensor(&fields) || connecting ||
        conn_handle != BLE_HS_CONN_HANDLE_NONE)
      break;

    ESP_LOGI(TAG, "Found HR sensor %02x:%02x:%02x:%02x:%02x:%02x (type %d) "
                  "RSSI %d name '%.*s'",
             event->disc.addr.val[5], event->disc.addr.val[4],
             event->disc.addr.val[3], event->disc.addr.val[2],
             event->disc.addr.val[1], event->disc.addr.val[0],
             event->disc.addr.type, event->disc.rssi, fields.name_len,
             fields.name ? (const char *)fields.name : "");

    rc = ble_gap_disc_cancel();
    if (rc != 0) {
      ESP_LOGE(TAG, "Error cancelling discovery; rc=%d", rc);
      break;
    }

    connecting = true;
    rc = ble_gap_connect(own_addr_type, &event->disc.addr, CONNECT_TIMEOUT_MS,
                         NULL, ble_sensors_gap_event, NULL);
    if (rc != 0) {
      ESP_LOGE(TAG, "Error initiating connection; rc=%d", rc);
      connecting = false;
      start_scan();
    }
    break;

  case BLE_GAP_EVENT_CONNECT:
    connecting = false;
    if (event->connect.status == 0) {
      conn_handle = event->connect.conn_handle;
      ESP_LOGI(TAG, "Connection established; handle=%d", conn_handle);
      start_hr_discovery(conn_handle);
    } else {
      ESP_LOGW(TAG, "Connection failed; status=%d", event->connect.status);
      start_scan();
    }
    break;

  case BLE_GAP_EVENT_DISCONNECT:
    ESP_LOGI(TAG, "Disconnected; reason=%d", event->disconnect.reason);
    conn_handle = BLE_HS_CONN_HANDLE_NONE;
    connecting = false;
    hr_val_handle = 0;
    hr_cccd_handle = 0;
    hr_svc_end = 0;
    if (lost_callback)
      lost_callback();
    start_scan();
    break;

  case BLE_GAP_EVENT_NOTIFY_RX:
    if (event->notify_rx.conn_handle == conn_handle &&
        event->notify_rx.attr_handle == hr_val_handle)
      handle_hr_notification(event);
    break;

  default:
    break;
  }
  return 0;
}

static void ble_sensors_on_reset(int reason) {
  ESP_LOGI(TAG, "Reset reason: %d", reason);
}

static void start_scan(void) {
  struct ble_gap_disc_params disc_params = {0};

  disc_params.filter_duplicates = 1;

  int rc = ble_gap_disc(own_addr_type, BLE_HS_FOREVER, &disc_params,
                        ble_sensors_gap_event, NULL);
  if (rc != 0)
    ESP_LOGE(TAG, "Error initiating GAP discovery procedure; rc=%d", rc);
}

static void ble_sensors_on_sync(void) {
  int rc = ble_hs_util_ensure_addr(0);
  if (rc != 0) {
    ESP_LOGE(TAG, "error ensuring address; rc=%d", rc);
    return;
  }

  rc = ble_hs_id_infer_auto(0, &own_addr_type);
  if (rc != 0) {
    ESP_LOGE(TAG, "error determining address type; rc=%d", rc);
    return;
  }

  ESP_LOGI(TAG, "BLE ready, scanning");
  start_scan();
}

static void host_task(void *param) {
  ESP_LOGI(TAG, "BLE Host Task Started");
  nimble_port_run();
}
