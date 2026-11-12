cmake_minimum_required(VERSION 3.16.0)
# Explicit version: otherwise IDF asks git describe, which fails in a repo with no commits.
set(PROJECT_VER "0.1.0")
list(APPEND EXTRA_COMPONENT_DIRS "${CMAKE_SOURCE_DIR}/src")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(RLCD_ESPS3_BIKE_COMPUTER)
