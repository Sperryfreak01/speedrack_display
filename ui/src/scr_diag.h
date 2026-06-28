#pragma once

#include "lvgl.h"

/* §6 — Diagnostic screen handle; set by scr_diag_build() */
extern lv_obj_t *scr_diag;

/* Status icons — recolored by ui_set_wifi/mqtt_connected() alongside home icons */
extern lv_obj_t *icon_wifi_diag;
extern lv_obj_t *icon_mqtt_diag;

/* Diagnostic value labels updated by ui_diag_set_*() */
extern lv_obj_t *val_ip;
extern lv_obj_t *val_host;
extern lv_obj_t *val_mqtt;
extern lv_obj_t *val_rssi;
extern lv_obj_t *val_fw;

/* Build the diagnostic screen. Call once from ui_init(). */
void scr_diag_build(void);
