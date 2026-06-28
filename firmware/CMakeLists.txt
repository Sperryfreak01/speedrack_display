cmake_minimum_required(VERSION 3.16)

# Point LVGL at our lv_conf.h before any component is included.
set(LV_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h" CACHE STRING "" FORCE)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(speedrack_display)
