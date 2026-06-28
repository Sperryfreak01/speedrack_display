#pragma once

#include "lvgl.h"

/* §5 — Home screen handle; set by scr_home_build() */
extern lv_obj_t *scr_home;

/* Widget pointers mutated by ui_set_*() in ui.c */
extern lv_obj_t *icon_wifi;
extern lv_obj_t *icon_mqtt;

extern lv_obj_t *lbl_speed_caption;
extern lv_obj_t *lbl_speed_value;
extern lv_obj_t *lbl_speed_unit;

extern lv_obj_t *dot_source;
extern lv_obj_t *lbl_src_value;

extern lv_obj_t *dot_power;
extern lv_obj_t *lbl_pwr_value;

/* Build the home screen. Call once from ui_init(). */
void scr_home_build(void);
