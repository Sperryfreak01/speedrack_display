#include "ui.h"
#include "theme.h"
#include "scr_home.h"
#include "scr_diag.h"
#include "../fonts/fonts.h"

#include "lvgl.h"
#include <stdio.h>
#include <stdbool.h>

/* ── Touch event handlers (§8) ─────────────────────────────────────────── */

static void on_home_click(lv_event_t *e)
{
    (void)e;
    lv_screen_load_anim(scr_diag, LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, false);
}

static void on_diag_click(lv_event_t *e)
{
    (void)e;
    lv_screen_load_anim(scr_home, LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, false);
}

/* ── UI initialisation ──────────────────────────────────────────────────── */

void ui_init(void)
{
    /* §10: skip the default LVGL theme so built-in color choices do not
     * override our explicit per-widget styles. If the Waveshare BSP calls
     * lv_theme_default_init() internally, this no-ops harmlessly. */
    lv_display_set_theme(lv_display_get_default(), NULL);

    /* Build both screens */
    scr_home_build();
    scr_diag_build();

    /* Wire tap-to-switch events (§8) */
    lv_obj_add_event_cb(scr_home, on_home_click, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(scr_diag, on_diag_click, LV_EVENT_CLICKED, NULL);

    /* Apply boot-time state via setters (gap 4: build bright, drive to true
     * state here rather than baking STANDBY colors into the build functions). */
    ui_set_units(UNITS_MPH);
    ui_set_speed(0);
    ui_set_source(SRC_STANDBY);
    ui_set_power(false);
    ui_set_wifi_connected(false);
    ui_set_mqtt_connected(false);

    lv_screen_load(scr_home);
}

/* ── State update implementations (§7) ─────────────────────────────────── */

void ui_set_speed(int speed)
{
    char buf[8];
    snprintf(buf, sizeof(buf), "%d", speed);
    lv_label_set_text(lbl_speed_value, buf);
}

void ui_set_units(units_t u)
{
    lv_label_set_text(lbl_speed_unit, (u == UNITS_KPH) ? "KPH" : "MPH");
}

void ui_set_source(src_t s)
{
    if (s == SRC_STANDBY) {
        lv_obj_set_style_bg_color(dot_source, COL_AMBER, 0);
        lv_obj_set_style_text_color(lbl_src_value, COL_AMBER, 0);
        lv_label_set_text(lbl_src_value, "STANDBY");

        /* Dim the speed area */
        lv_obj_set_style_text_color(lbl_speed_caption, COL_DIM_TEXT,  0);
        lv_obj_set_style_text_color(lbl_speed_value,   COL_DIM_VALUE, 0);
        lv_obj_set_style_text_color(lbl_speed_unit,    COL_DIM_TEXT,  0);
    } else {
        lv_obj_set_style_bg_color(dot_source, COL_PRIMARY_DOT, 0);
        lv_obj_set_style_text_color(lbl_src_value, COL_PRIMARY, 0);
        lv_label_set_text(lbl_src_value, (s == SRC_SIG_GEN) ? "SIG GEN" : "MOTOR");

        /* Restore bright speed area */
        lv_obj_set_style_text_color(lbl_speed_caption, COL_LABEL,   0);
        lv_obj_set_style_text_color(lbl_speed_value,   COL_PRIMARY, 0);
        lv_obj_set_style_text_color(lbl_speed_unit,    COL_LABEL,   0);
    }
}

void ui_set_power(bool on)
{
    if (on) {
        lv_obj_set_style_bg_color(dot_power, COL_PRIMARY_DOT, 0);
        lv_obj_set_style_text_color(lbl_pwr_value, COL_PRIMARY, 0);
        lv_label_set_text(lbl_pwr_value, "ENABLED");
    } else {
        lv_obj_set_style_bg_color(dot_power, COL_RED, 0);
        lv_obj_set_style_text_color(lbl_pwr_value, COL_RED, 0);
        lv_label_set_text(lbl_pwr_value, "OFF");
    }
}

void ui_set_wifi_connected(bool conn)
{
    lv_color_t color = conn ? COL_PRIMARY : COL_DIM_ICON;
    lv_obj_set_style_image_recolor(icon_wifi,      color, 0);
    lv_obj_set_style_image_recolor(icon_wifi_diag, color, 0);
}

void ui_set_mqtt_connected(bool conn)
{
    lv_color_t color = conn ? COL_PRIMARY : COL_DIM_ICON;
    lv_obj_set_style_image_recolor(icon_mqtt,      color, 0);
    lv_obj_set_style_image_recolor(icon_mqtt_diag, color, 0);
}

/* ── Diagnostic value setters ───────────────────────────────────────────── */

void ui_diag_set_ip(const char *ip)
{
    lv_label_set_text(val_ip, ip);
}

void ui_diag_set_hostname(const char *hn)
{
    lv_label_set_text(val_host, hn);
}

void ui_diag_set_mqtt_broker(const char *brokerport)
{
    lv_label_set_text(val_mqtt, brokerport);
}

void ui_diag_set_rssi(int dbm)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "%d dBm", dbm);
    lv_label_set_text(val_rssi, buf);
}

void ui_diag_set_fw(const char *ver)
{
    lv_label_set_text(val_fw, ver);
}
