#pragma once

#include "lvgl.h"

/* §2 — Color palette. All colors referenced by these macros; no inline hex. */
#define COL_BG              lv_color_hex(0x0B100D)  /* screen background */
#define COL_SURFACE_HAIR    lv_color_hex(0x1F2A22)  /* hairline dividers */
#define COL_PRIMARY         lv_color_hex(0x4ADE80)  /* phosphor green — active text */
#define COL_PRIMARY_DOT     lv_color_hex(0x22C55E)  /* active indicator LED */
#define COL_LABEL           lv_color_hex(0x6C8074)  /* muted slate-green — labels */
#define COL_DIM_TEXT        lv_color_hex(0x4B5650)  /* standby labels, footer hints */
#define COL_DIM_VALUE       lv_color_hex(0x3A4A40)  /* standby speed numeral */
#define COL_DIM_ICON        lv_color_hex(0x2D3530)  /* disconnected status icon */
#define COL_AMBER           lv_color_hex(0xFBBF24)  /* STANDBY indicator */
#define COL_RED             lv_color_hex(0xEF4444)  /* power OFF / fault */
