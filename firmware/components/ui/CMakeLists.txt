# ESP-IDF component wrapper for the speedrack_display UI module.
# Sources live in the repo's top-level ui/ directory; this wrapper
# references them by relative path so the ui/ tree stays unpolluted.

cmake_minimum_required(VERSION 3.16)

set(UI_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../ui")

idf_component_register(
    SRCS
        "${UI_ROOT}/src/ui.c"
        "${UI_ROOT}/src/scr_home.c"
        "${UI_ROOT}/src/scr_diag.c"
        "${UI_ROOT}/fonts/font_mono_12.c"
        "${UI_ROOT}/fonts/font_mono_14.c"
        "${UI_ROOT}/fonts/font_mono_68.c"
        "${UI_ROOT}/images/img_wifi.c"
        "${UI_ROOT}/images/img_broadcast.c"
    INCLUDE_DIRS
        "${UI_ROOT}/src"
        "${UI_ROOT}/fonts"
        "${UI_ROOT}/images"
    REQUIRES
        lvgl
        esp_lcd
)
