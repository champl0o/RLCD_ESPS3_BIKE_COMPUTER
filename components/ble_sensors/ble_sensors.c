#include "ble_sensors.h"
#include "nvs_flash.h"
#include "nimble/nimble_port.h"
#include "host/ble_hs.h"

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
}
