#include "u8g2_hal.h"

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_rom_sys.h"
#include "esp_log.h"

static const char *TAG = "rlcd";

static spi_device_handle_t s_spi;

/* ---------------------------------------------------------------------------
 * Why a custom transmit buffer.
 *
 * The ST7305 driver sends the frame in chunks of 6 bytes (u8x8_cad_SendData(...,6,...)).
 * For a full frame that is over 5000 calls. Each spi_device_polling_transmit()
 * costs a few microseconds of overhead — together that is more than the
 * data transfer itself. So we accumulate bytes here and hand them to SPI in big chunks.
 *
 * Critical: the buffer must be flushed BEFORE every DC change and at the end of the transaction.
 * Accumulated bytes belong to the DC level that was active while they were accumulated.
 * Forget to flush — and some commands go out as data, and vice versa.
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
        .length    = s_tx_len * 8,   /* WARNING: the length is in BITS, not bytes */
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
            .miso_io_num     = -1,   /* the ST7305 has no MISO: there is nothing to read from the panel */
            .quadwp_io_num   = -1,
            .quadhd_io_num   = -1,
            .max_transfer_sz = RLCD_TX_BUF_SZ,
        };
        ESP_ERROR_CHECK(spi_bus_initialize(RLCD_SPI_HOST, &bus, SPI_DMA_CH_AUTO));

        spi_device_interface_config_t dev = {
            .clock_speed_hz = RLCD_SPI_CLOCK_HZ,
            .mode           = 0,     /* display_info.spi_mode = 0 */
            /* CS is held by us via GPIO, not by the hardware SPI.
             * One u8g2 transaction (START_TRANSFER..END_TRANSFER) consists
             * of many separate spi_device_polling_transmit() calls with DC switching DC
             * between them. Hardware CS would be released after EVERY transmit, and
             * the controller would take that for dozens of aborted transactions. */
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
        tx_flush();                      /* what is accumulated belongs to the OLD DC level DC */
        u8x8_gpio_SetDC(u8x8, arg_int);
        break;

    case U8X8_MSG_BYTE_START_TRANSFER:
        s_tx_len = 0;
        u8x8_gpio_SetCS(u8x8, u8x8->display_info->chip_enable_level);
        u8x8_gpio_Delay(u8x8, U8X8_MSG_DELAY_NANO,
                        u8x8->display_info->post_chip_enable_wait_ns);
        break;

    case U8X8_MSG_BYTE_END_TRANSFER:
        tx_flush();                      /* push out the tail BEFORE releasing CS */
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
        gpio_set_level(RLCD_PIN_CS, 1);   /* CS is active low → 1 = inactive */
        break;
    }

    case U8X8_MSG_DELAY_MILLI:
        /* Called only during reset/init, so a busy-wait is acceptable here.
         * vTaskDelay(pdMS_TO_TICKS(1)) with a 100 Hz tick would give 0 ticks = no delay. */
        esp_rom_delay_us((uint32_t)arg_int * 1000);
        break;

    case U8X8_MSG_DELAY_10MICRO:
        esp_rom_delay_us(10);
        break;

    case U8X8_MSG_DELAY_100NANO:
    case U8X8_MSG_DELAY_NANO:
        /* At 240 MHz one cycle ≈ 4 ns. The SCK/MOSI timing is held by the hardware SPI;
         * u8g2 asks for these delays only around CS/DC, where a few cycles are enough. */
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
        /* Returning 1 here would lie to the library that the message was handled. */
        return 0;
    }
    return 1;
}

void rlcd_init(u8g2_t *u8g2)
{
    /* _f = full frame buffer (38*8 x 50*8 tiles = 15200 bytes in .bss).
     * U8G2_R0 = no rotation: the panel is physically 300x400, i.e. portrait, like a phone.
     * U8G2_R2 — the same upside down, if that is easier to mount on the handlebar. */
    u8g2_Setup_st7305_300x400_f(u8g2, U8G2_R0,
                                u8g2_esp32_byte_spi,
                                u8g2_esp32_gpio_and_delay);

    u8g2_InitDisplay(u8g2);        /* this includes the hardware reset and the init sequence */
    u8g2_SetPowerSave(u8g2, 0);    /* note: for 300x400 upstream takes the sequence
                                    * from 122x250 — it may not behave as expected */

    ESP_LOGI(TAG, "display ready: %dx%d",
             (int)u8g2_GetDisplayWidth(u8g2), (int)u8g2_GetDisplayHeight(u8g2));
}
