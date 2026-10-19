idf_component_register(SRCS "ble_sensors.c"
                       INCLUDE_DIRS "."
                       REQUIRES bt nvs_flash ble_parse)
