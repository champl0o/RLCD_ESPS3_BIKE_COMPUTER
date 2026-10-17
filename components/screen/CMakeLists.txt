idf_component_register(SRCS "screen.c" "detailed_screen.c" "minimal_screen.c" "map_screen.c" "helper.c"
                       INCLUDE_DIRS "."
                       REQUIRES u8g2 u8g2_hal esp_timer)
