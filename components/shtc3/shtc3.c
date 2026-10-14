/*
 * SHTC3 за даташитом Sensirion (v4, 2021).
 *
 * Датчик говорить 16-бітними командами, старший байт першим.
 * Між вимірами він спить (~0.3 мкА), тож кожне читання — це
 * wakeup → measure → read → sleep.
 */

#include "shtc3.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_rom_sys.h"
#include "esp_log.h"
#include "esp_check.h"

static const char *TAG = "shtc3";

#define SHTC3_ADDR 0x70
#define SHTC3_TIMEOUT_MS 50

#define CMD_WAKEUP 0x3517
#define CMD_SLEEP 0xB098
#define CMD_READ_ID 0xEFC8
/* Normal mode, температура першою, БЕЗ clock stretching.
 * Зі stretching датчик тримав би SCL низько ~12 мс — а шина спільна. */
#define CMD_MEASURE_T_FIRST 0x7866

static esp_err_t send_cmd(shtc3_t *s, uint16_t cmd)
{
    uint8_t buf[2] = {cmd >> 8, cmd & 0xFF};
    return i2c_master_transmit(s->dev, buf, sizeof(buf), SHTC3_TIMEOUT_MS);
}

/* CRC-8: поліном 0x31, старт 0xFF. Кожне 16-бітне слово від датчика
 * йде з власним CRC-байтом. Перевірка з даташиту: crc8({0xBE, 0xEF}) == 0x92. */
static uint8_t crc8(const uint8_t *data, int len)
{
    uint8_t crc = 0xFF;
    for (int i = 0; i < len; i++)
    {
        crc ^= data[i];
        for (int bit = 0; bit < 8; bit++)
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x31) : (uint8_t)(crc << 1);
    }
    return crc;
}

static esp_err_t wakeup(shtc3_t *s)
{
    esp_err_t err = send_cmd(s, CMD_WAKEUP);
    /* Даташит: 240 мкс до готовності. vTaskDelay тут не підходить —
     * тік 10 мс, це в 40 разів більше, ніж треба. */
    esp_rom_delay_us(300);
    return err;
}

esp_err_t shtc3_init(shtc3_t *s, i2c_master_bus_handle_t bus)
{
    const i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = SHTC3_ADDR,
        .scl_speed_hz = 400000,
    };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(bus, &cfg, &s->dev), TAG, "add device");

    /* Після подачі живлення датчик уже прокинувся, але якщо ESP перезавантажився
     * без вимкнення живлення — він міг лишитися спати. Wakeup безпечний в обох випадках. */
    ESP_RETURN_ON_ERROR(wakeup(s), TAG, "wakeup");

    uint8_t cmd[2] = {CMD_READ_ID >> 8, CMD_READ_ID & 0xFF};
    uint8_t id[3];
    ESP_RETURN_ON_ERROR(i2c_master_transmit_receive(s->dev, cmd, sizeof(cmd), id, sizeof(id),
                                                    SHTC3_TIMEOUT_MS),
                        TAG, "read id");
    if (crc8(id, 2) != id[2])
        return ESP_ERR_INVALID_CRC;

    /* У слові ID значущі лише біти 11 і 5..0 — решта різна між екземплярами. */
    uint16_t word = (id[0] << 8) | id[1];
    if ((word & 0x083F) != 0x0807)
    {
        ESP_LOGE(TAG, "unexpected id 0x%04x", word);
        return ESP_ERR_NOT_FOUND;
    }
    ESP_LOGI(TAG, "found, id 0x%04x", word);

    return send_cmd(s, CMD_SLEEP);
}

esp_err_t shtc3_read(shtc3_t *s, float *temp_c, float *humidity_pct)
{
    ESP_RETURN_ON_ERROR(wakeup(s), TAG, "wakeup");
    ESP_RETURN_ON_ERROR(send_cmd(s, CMD_MEASURE_T_FIRST), TAG, "measure");

    /* Вимір триває до 12.1 мс. Поки він іде, датчик NACK-ає свою адресу,
     * тому читати раніше — отримати помилку. Тік 10 мс, а vTaskDelay(n)
     * гарантує лише (n-1) повних тіків, тож 3 тіки = щонайменше 20 мс. */
    vTaskDelay(pdMS_TO_TICKS(30));

    uint8_t raw[6]; /* T_hi T_lo T_crc  RH_hi RH_lo RH_crc */
    esp_err_t err = i2c_master_receive(s->dev, raw, sizeof(raw), SHTC3_TIMEOUT_MS);
    send_cmd(s, CMD_SLEEP); /* приспати навіть якщо читання впало */
    ESP_RETURN_ON_ERROR(err, TAG, "receive");

    if (crc8(&raw[0], 2) != raw[2] || crc8(&raw[3], 2) != raw[5])
        return ESP_ERR_INVALID_CRC;

    uint16_t t_raw = (raw[0] << 8) | raw[1];
    uint16_t rh_raw = (raw[3] << 8) | raw[4];

    *temp_c = -45.0f + 175.0f * (float)t_raw / 65536.0f;
    *humidity_pct = 100.0f * (float)rh_raw / 65536.0f;
    return ESP_OK;
}
