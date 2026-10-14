#include "u8g2_hal.h"

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_rom_sys.h"
#include "esp_log.h"

static const char *TAG = "rlcd";

static spi_device_handle_t s_spi;

/* ---------------------------------------------------------------------------
 * Навіщо власний буфер передачі.
 *
 * Драйвер ST7305 віддає кадр шматочками по 6 байтів (u8x8_cad_SendData(...,6,...)).
 * На повний кадр це понад 5000 викликів. Кожен spi_device_polling_transmit()
 * коштує кілька мікросекунд накладних витрат — разом це більше, ніж сама
 * передача даних. Тому накопичуємо байти тут і віддаємо SPI великими шматками.
 *
 * Критично: буфер треба скидати ПЕРЕД кожною зміною DC і в кінці транзакції.
 * Накопичені байти належать тому рівню DC, що був під час їх накопичення.
 * Забудеш скинути — частина команд поїде як дані, і навпаки.
 * --------------------------------------------------------------------------- */
#define RLCD_TX_BUF_SZ 512
static uint8_t s_tx_buf[RLCD_TX_BUF_SZ];
static size_t  s_tx_len;

static void tx_flush(void)
{
    if (s_tx_len == 0) {
        return;
    }
    spi_transaction_t t = {
        .length    = s_tx_len * 8,   /* УВАГА: довжина в БІТАХ, не в байтах */
        .tx_buffer = s_tx_buf,
    };
    ESP_ERROR_CHECK(spi_device_polling_transmit(s_spi, &t));
    s_tx_len = 0;
}

uint8_t u8g2_esp32_byte_spi(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
    switch (msg) {

    case U8X8_MSG_BYTE_INIT: {
        spi_bus_config_t bus = {
            .sclk_io_num     = RLCD_PIN_SCK,
            .mosi_io_num     = RLCD_PIN_MOSI,
            .miso_io_num     = -1,   /* у ST7305 немає MISO: читати з панелі нізвідки */
            .quadwp_io_num   = -1,
            .quadhd_io_num   = -1,
            .max_transfer_sz = RLCD_TX_BUF_SZ,
        };
        ESP_ERROR_CHECK(spi_bus_initialize(RLCD_SPI_HOST, &bus, SPI_DMA_CH_AUTO));

        spi_device_interface_config_t dev = {
            .clock_speed_hz = RLCD_SPI_CLOCK_HZ,
            .mode           = 0,     /* display_info.spi_mode = 0 */
            /* CS тримаємо самі через GPIO, а не апаратним SPI.
             * Одна транзакція u8g2 (START_TRANSFER..END_TRANSFER) складається
             * з багатьох окремих spi_device_polling_transmit() із перемиканням DC
             * між ними. Апаратний CS знімався б після КОЖНОГО transmit, і
             * контролер сприймав би це як десятки обірваних транзакцій. */
            .spics_io_num   = -1,
            .queue_size     = 1,
        };
        ESP_ERROR_CHECK(spi_bus_add_device(RLCD_SPI_HOST, &dev, &s_spi));
        break;
    }

    case U8X8_MSG_BYTE_SEND: {
        const uint8_t *data = (const uint8_t *)arg_ptr;
        while (arg_int > 0) {
            if (s_tx_len == RLCD_TX_BUF_SZ) {
                tx_flush();
            }
            s_tx_buf[s_tx_len++] = *data++;
            arg_int--;
        }
        break;
    }

    case U8X8_MSG_BYTE_SET_DC:
        tx_flush();                      /* накопичене належить СТАРОМУ рівню DC */
        u8x8_gpio_SetDC(u8x8, arg_int);
        break;

    case U8X8_MSG_BYTE_START_TRANSFER:
        s_tx_len = 0;
        u8x8_gpio_SetCS(u8x8, u8x8->display_info->chip_enable_level);
        u8x8_gpio_Delay(u8x8, U8X8_MSG_DELAY_NANO,
                        u8x8->display_info->post_chip_enable_wait_ns);
        break;

    case U8X8_MSG_BYTE_END_TRANSFER:
        tx_flush();                      /* дотиснути хвіст ДО зняття CS */
        u8x8_gpio_Delay(u8x8, U8X8_MSG_DELAY_NANO,
                        u8x8->display_info->pre_chip_disable_wait_ns);
        u8x8_gpio_SetCS(u8x8, u8x8->display_info->chip_disable_level);
        break;

    default:
        return 0;
    }
    return 1;
}

uint8_t u8g2_esp32_gpio_and_delay(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
    (void)u8x8;
    (void)arg_ptr;

    switch (msg) {

    case U8X8_MSG_GPIO_AND_DELAY_INIT: {
        gpio_config_t io = {
            .pin_bit_mask = (1ULL << RLCD_PIN_DC)
                          | (1ULL << RLCD_PIN_CS)
                          | (1ULL << RLCD_PIN_RST),
            .mode         = GPIO_MODE_OUTPUT,
            .pull_up_en   = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type    = GPIO_INTR_DISABLE,
        };
        ESP_ERROR_CHECK(gpio_config(&io));
        gpio_set_level(RLCD_PIN_CS, 1);   /* CS активний у нулі → 1 = неактивний */
        break;
    }

    case U8X8_MSG_DELAY_MILLI:
        /* Викликається лише під час reset/init, тому busy-wait тут прийнятний.
         * vTaskDelay(pdMS_TO_TICKS(1)) при тику 100 Гц дав би 0 тиків = без затримки. */
        esp_rom_delay_us((uint32_t)arg_int * 1000);
        break;

    case U8X8_MSG_DELAY_10MICRO:
        esp_rom_delay_us(10);
        break;

    case U8X8_MSG_DELAY_100NANO:
    case U8X8_MSG_DELAY_NANO:
        /* На 240 МГц один такт ≈ 4 нс. Тайминги SCK/MOSI тримає апаратний SPI;
         * ці затримки u8g2 просить лише навколо CS/DC, там вистачає кількох тактів. */
        __asm__ __volatile__("nop; nop; nop; nop;");
        break;

    case U8X8_MSG_GPIO_DC:
        gpio_set_level(RLCD_PIN_DC, arg_int);
        break;

    case U8X8_MSG_GPIO_CS:
        gpio_set_level(RLCD_PIN_CS, arg_int);
        break;

    case U8X8_MSG_GPIO_RESET:
        gpio_set_level(RLCD_PIN_RST, arg_int);
        break;

    default:
        /* Повернути 1 тут — збрехати бібліотеці, що повідомлення оброблено. */
        return 0;
    }
    return 1;
}

void rlcd_init(u8g2_t *u8g2)
{
    /* _f = full frame buffer (38*8 x 50*8 тайлів = 15200 байт у .bss).
     * U8G2_R0 = без повороту: панель фізично 300x400, тобто портрет, як у телефона.
     * U8G2_R2 — те саме догори дриґом, якщо так буде зручніше кріпити на кермо. */
    u8g2_Setup_st7305_300x400_f(u8g2, U8G2_R0,
                                u8g2_esp32_byte_spi,
                                u8g2_esp32_gpio_and_delay);

    u8g2_InitDisplay(u8g2);        /* сюди входить апаратний reset і init-послідовність */
    u8g2_SetPowerSave(u8g2, 0);    /* увага: для 300x400 апстрім бере послідовність
                                    * від 122x250 — поводиться, можливо, не за очікуванням */

    ESP_LOGI(TAG, "display ready: %dx%d",
             (int)u8g2_GetDisplayWidth(u8g2), (int)u8g2_GetDisplayHeight(u8g2));
}
