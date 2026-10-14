idf_component_register(SRCS "button.c"
                       INCLUDE_DIRS "."
                       REQUIRES esp_driver_gpio freertos esp_timer)
