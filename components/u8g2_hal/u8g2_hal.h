#pragma once

#include "u8g2.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ST7305 display pins — from HARDWARE.md (Waveshare ESP32-S3-RLCD-4.2). */
#define RLCD_PIN_SCK   11
#define RLCD_PIN_MOSI  12
#define RLCD_PIN_DC     5
#define RLCD_PIN_CS    40
#define RLCD_PIN_RST   41
#define RLCD_PIN_TE     6   /* not used yet */

#define RLCD_SPI_HOST  SPI2_HOST

/* The u8g2 driver suggests 2 MHz (display_info.sck_clock_hz), but that is only a guideline
 * for the Arduino HAL. A frame on the wire is ~30 KB, so at 2 MHz it would take ~120 ms.
 * We start at 4 MHz; if the picture is clean, raise it and watch the frame-time log. */
#define RLCD_SPI_CLOCK_HZ  (4 * 1000 * 1000)

/* Two callbacks through which u8g2 talks to the hardware. */
uint8_t u8g2_esp32_byte_spi(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);
uint8_t u8g2_esp32_gpio_and_delay(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);

/* Put it all together: setup + init + disable power save. */
void rlcd_init(u8g2_t *u8g2);

#ifdef __cplusplus
}
#endif
