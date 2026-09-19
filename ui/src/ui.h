#pragma once

#include <stdbool.h>

/* §7 — State update API types */
typedef enum { SRC_SIG_GEN, SRC_MOTOR, SRC_STANDBY } src_t;
typedef enum { UNITS_MPH, UNITS_KPH }                units_t;

/*
 * Call once after LVGL and BSP are initialized.
 * Builds both screens, wires touch events, and applies the
 * boot-time state (STANDBY / power OFF / disconnected).
 */
void ui_init(void);

/*
 * Switch to the other screen (home <-> diagnostics). Safe to call from
 * any task, but the caller must hold the LVGL lock (lvgl_port_lock()/
 * lvgl_port_unlock()) — this board has no touch hardware, so screen
 * switching is driven externally (BOOT button), not by LVGL click events.
 */
void ui_toggle_screen(void);

/* §7 — State update functions called by the application layer */
void ui_set_speed(int speed);
void ui_set_units(units_t u);
void ui_set_source(src_t s);
void ui_set_power(bool on);
void ui_set_mqtt_connected(bool conn);
void ui_set_wifi_connected(bool conn);

/* Diagnostic screen value setters */
void ui_diag_set_ip(const char *ip);
void ui_diag_set_rssi(int dbm);
void ui_diag_set_mqtt_broker(const char *brokerport);
void ui_diag_set_hostname(const char *hn);
void ui_diag_set_fw(const char *ver);
