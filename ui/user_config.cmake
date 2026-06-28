# =============================================================================
# user_config.cmake - Add your custom source files here
# =============================================================================
#
# This file is included by the generated CMakeLists.txt and allows you to
# add extra source files to the project without modifying generated files
# (which may be overwritten).
#
# To add your own sources, append them to LV_EDITOR_PROJECT_SOURCES:
#
#   list(APPEND LV_EDITOR_PROJECT_SOURCES
#       ${CMAKE_CURRENT_LIST_DIR}/src/my_widget.c
#       ${CMAKE_CURRENT_LIST_DIR}/src/my_screen.c
#   )
#
# Tip:
#   - Use ${CMAKE_CURRENT_LIST_DIR} to get paths relative to this file
#
# =============================================================================

list(APPEND LV_EDITOR_PROJECT_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/src/ui.c
    ${CMAKE_CURRENT_LIST_DIR}/src/scr_home.c
    ${CMAKE_CURRENT_LIST_DIR}/src/scr_diag.c
    ${CMAKE_CURRENT_LIST_DIR}/images/images.c
)

# Expose hand-written headers to the build system
target_include_directories(${LIB_NAME} PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/src
    ${CMAKE_CURRENT_LIST_DIR}/fonts
    ${CMAKE_CURRENT_LIST_DIR}/images
)
