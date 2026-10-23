idf_component_register(SRCS "sdcard.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES fatfs esp_driver_sdmmc)
