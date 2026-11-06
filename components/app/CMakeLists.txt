idf_component_register(
    SRCS "model.c" "ui.c" "sensors.c" "handlers.c"
    INCLUDE_DIRS "."
    REQUIRES screen event_orchestrator trip gps button freertos
    PRIV_REQUIRES esp_timer log esp_driver_i2c fit recorder sdcard
                  battery shtc3 monitor_rtc
)
