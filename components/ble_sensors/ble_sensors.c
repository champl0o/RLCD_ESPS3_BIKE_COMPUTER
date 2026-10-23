#include "ble_sensors.h"
#include "nvs_flash.h"

#include "esp_log.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "services/gap/ble_svc_gap.h"
#include "host/util/util.h"

#define TAG "BLE"

static ble_sensors_event_t ble_event[6] = {
    {.state = IDLE, .next_state = SCANNING, .callback = NULL},
    {.state = SCANNING, .next_state = CONNECTING, .callback = NULL},
    {.state = CONNECTING, .next_state = DISCOVERING, .callback = NULL},
    {.state = DISCOVERING, .next_state = SUBSCRIBING, .callback = NULL},
    {.state = SUBSCRIBING, .next_state = SUBSCRIBED, .callback = NULL},
    {.state = SUBSCRIBED, .next_state = IDLE, .callback = NULL}};

void ble_store_config_init(void);

static void ble_sensors_on_reset(int reason)
{
    ESP_LOGI(TAG, "Reset reason: %d", reason);
}

static void ble_sensors_on_sync(void)
{
    ble_addr_t addr;
    uint8_t own_addr_type;

    ble_hs_util_ensure_addr(0);
    ble_hs_id_infer_auto(0, &own_addr_type);

    struct ble_hs_adv_fields fields;
    memset(&fields, 0, sizeof(fields));
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name = ble_svc_gap_device_name();
    fields.name_len = strlen(fields.name);
    fields.name_is_complete = 1;
    ble_gap_adv_set_fields(&fields);

    uint8_t uuid[16];
    memset(uuid, 0, sizeof(uuid));

    ble_ibeacon_set_adv_data(uuid, 2, 10, -50);

    struct ble_gap_adv_params adv_params = (struct ble_gap_adv_params){0};
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER, &adv_params, NULL, NULL);
}

static void host_task(void *param)
{
    ESP_LOGI(TAG, "BLE Host Task Started");
    nimble_port_run();
}

void ble_sensors_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(nimble_port_init());

    ble_hs_cfg.reset_cb = ble_sensors_on_reset;
    ble_hs_cfg.sync_cb = ble_sensors_on_sync;
    ble_svc_gap_device_name_set("bike-computer");

    ble_store_config_init();

    nimble_port_freertos_init(host_task);
}
