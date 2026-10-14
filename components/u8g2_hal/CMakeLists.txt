idf_component_register(SRCS "u8g2_hal.c"
                       INCLUDE_DIRS "."
                       REQUIRES u8g2 esp_driver_spi esp_driver_gpio esp_rom)
