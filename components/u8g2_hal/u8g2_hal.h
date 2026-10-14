#pragma once

#include "u8g2.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Піни дисплея ST7305 — з HARDWARE.md (Waveshare ESP32-S3-RLCD-4.2). */
#define RLCD_PIN_SCK   11
#define RLCD_PIN_MOSI  12
#define RLCD_PIN_DC     5
#define RLCD_PIN_CS    40
#define RLCD_PIN_RST   41
#define RLCD_PIN_TE     6   /* поки не використовуємо */

#define RLCD_SPI_HOST  SPI2_HOST

/* Драйвер u8g2 підказує 2 МГц (display_info.sck_clock_hz), але це орієнтир
 * для Arduino-HAL. Кадр на дроті ~30 КБ, тож на 2 МГц він займав би ~120 мс.
 * Починаємо з 4 МГц; якщо картинка чиста — піднімай і дивись на лог часу кадру. */
#define RLCD_SPI_CLOCK_HZ  (4 * 1000 * 1000)

/* Два callback'и, через які u8g2 говорить із залізом. */
uint8_t u8g2_esp32_byte_spi(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);
uint8_t u8g2_esp32_gpio_and_delay(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);

/* Зібрати все докупи: setup + init + вимкнути power save. */
void rlcd_init(u8g2_t *u8g2);

#ifdef __cplusplus
}
#endif
