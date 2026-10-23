# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.*)

# idf.py auto-requires every component only for a component named "main";
# src/ is not one, so list what main.c includes.
idf_component_register(SRCS ${app_sources}
                       REQUIRES freertos esp_timer log esp_driver_i2c
                                screen shtc3 monitor_rtc battery gps button trip ble_sensors)
