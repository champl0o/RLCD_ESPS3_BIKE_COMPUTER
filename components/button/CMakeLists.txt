idf_component_register(SRCS "button.c" "gesture.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_driver_gpio freertos esp_timer)
