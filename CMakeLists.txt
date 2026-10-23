cmake_minimum_required(VERSION 3.16.0)
# Явна версія: інакше IDF питає git describe, а в репо без жодного коміту той падає.
set(PROJECT_VER "0.1.0")
list(APPEND EXTRA_COMPONENT_DIRS "${CMAKE_SOURCE_DIR}/src")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(RLCD_ESPS3_BIKE_COMPUTER)
