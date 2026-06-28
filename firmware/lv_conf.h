/**
 * LVGL v9 configuration for Waveshare ESP32-C6-Touch-LCD-1.47
 * 320 × 172 px landscape, JD9853 RGB565, 250 PPI
 *
 * This file is referenced by the root CMakeLists.txt via LV_CONF_PATH.
 */

#if 1 /* Set to 1 to enable content */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/*====================
   COLOR SETTINGS
 *====================*/
#define LV_COLOR_DEPTH 16   /* RGB565 */

/*====================
   MEMORY SETTINGS
 *====================*/
#define LV_MEM_CUSTOM 0
#define LV_MEM_SIZE (64 * 1024U)  /* 64 KB — plenty for this simple UI */

/*====================
   HAL SETTINGS
 *====================*/
#define LV_DPI_DEF 250   /* 250 PPI at 1.47" */

/*====================
   DRAWING BACKEND
 *====================*/
#define LV_USE_DRAW_SW 1
#define LV_USE_DRAW_SW_COMPLEX_GRADIENTS 0

/*====================
   FONTS
 *====================*/
/* Built-in fonts NOT needed — real IBM Plex Mono assets compiled in */
#define LV_FONT_MONTSERRAT_12 0
#define LV_FONT_MONTSERRAT_14 0
#define LV_FONT_MONTSERRAT_48 0
#define LV_FONT_DEFAULT (&lv_font_montserrat_14)

/*====================
   WIDGET FEATURES
 *====================*/
#define LV_USE_LABEL   1
#define LV_USE_IMAGE   1
#define LV_USE_OBJ     1
#define LV_USE_FLEX    1

/* Disable unused widgets to save flash */
#define LV_USE_ARC     0
#define LV_USE_BAR     0
#define LV_USE_BTN     0
#define LV_USE_BTNMATRIX 0
#define LV_USE_CALENDAR 0
#define LV_USE_CANVAS  0
#define LV_USE_CHART   0
#define LV_USE_CHECKBOX 0
#define LV_USE_DROPDOWN 0
#define LV_USE_KEYBOARD 0
#define LV_USE_LED     0
#define LV_USE_LINE    0
#define LV_USE_LIST    0
#define LV_USE_MENU    0
#define LV_USE_MSGBOX  0
#define LV_USE_ROLLER  0
#define LV_USE_SCALE   0
#define LV_USE_SLIDER  0
#define LV_USE_SPAN    0
#define LV_USE_SPINBOX 0
#define LV_USE_SPINNER 0
#define LV_USE_SWITCH  0
#define LV_USE_TABLE   0
#define LV_USE_TABVIEW 0
#define LV_USE_TEXTAREA 0
#define LV_USE_TILEVIEW 0
#define LV_USE_WIN     0

/*====================
   ANIMATIONS
 *====================*/
#define LV_USE_ANIM 1  /* Required for screen FADE_IN transition */

/*====================
   LOGGING
 *====================*/
#define LV_USE_LOG 0

/*====================
   THEME
 *====================*/
/* Do NOT initialize default theme — ui_init() handles all styling explicitly */
#define LV_USE_THEME_DEFAULT 0
#define LV_USE_THEME_SIMPLE  0
#define LV_USE_THEME_MONO    0

#endif /* LV_CONF_H */
#endif /* End of "Content enable" */
