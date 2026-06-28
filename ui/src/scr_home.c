#include "scr_home.h"
#include "theme.h"
#include "../fonts/fonts.h"
#include "../images/images.h"

/* §5 — Widget handles exposed to ui.c via scr_home.h */
lv_obj_t *scr_home;

lv_obj_t *icon_wifi;
lv_obj_t *icon_mqtt;

lv_obj_t *lbl_speed_caption;
lv_obj_t *lbl_speed_value;
lv_obj_t *lbl_speed_unit;

lv_obj_t *dot_source;
lv_obj_t *lbl_src_value;

lv_obj_t *dot_power;
lv_obj_t *lbl_pwr_value;

/* Strip a container of all LVGL defaults that would interfere with layout.
 * Hairline containers re-add their border after this call. */
static void reset_container(lv_obj_t *o)
{
    lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_pad_all(o, 0, 0);
    lv_obj_set_style_radius(o, 0, 0);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE); /* §8: pass taps to screen */
}

/* Create an 8×8 circle indicator dot (§5 widget notes). */
static lv_obj_t *make_dot(lv_obj_t *parent, lv_color_t color)
{
    lv_obj_t *dot = lv_obj_create(parent);
    lv_obj_set_size(dot, 8, 8);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot, color, 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(dot, 0, 0);
    lv_obj_set_style_pad_all(dot, 0, 0);
    lv_obj_remove_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(dot, LV_OBJ_FLAG_CLICKABLE);
    return dot;
}

/* Apply A8 image source and set full-opacity recolor (§4). */
static void setup_icon(lv_obj_t *img, const lv_image_dsc_t *src, lv_color_t color)
{
    lv_image_set_src(img, src);
    lv_obj_set_style_image_recolor(img, color, 0);
    lv_obj_set_style_image_recolor_opa(img, LV_OPA_COVER, 0);
    lv_obj_remove_flag(img, LV_OBJ_FLAG_CLICKABLE);
}

void scr_home_build(void)
{
    /* ── Root screen (320 × 172) ──────────────────────────────────────── */
    scr_home = lv_obj_create(NULL);
    lv_obj_set_size(scr_home, 320, 172);
    lv_obj_set_style_bg_color(scr_home, COL_BG, 0);
    lv_obj_set_style_bg_opa(scr_home, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(scr_home, 0, 0);
    lv_obj_set_style_pad_all(scr_home, 0, 0);
    lv_obj_set_style_radius(scr_home, 0, 0);
    lv_obj_remove_flag(scr_home, LV_OBJ_FLAG_SCROLLABLE);
    /* §8: screen must be clickable to receive taps (base obj default is
     * clickable, but we add the flag explicitly for clarity). */
    lv_obj_add_flag(scr_home, LV_OBJ_FLAG_CLICKABLE);

    /* ── Status bar (0, 0, 320 × 22) — bottom hairline ──────────────── */
    lv_obj_t *status_bar = lv_obj_create(scr_home);
    lv_obj_set_pos(status_bar, 0, 0);
    lv_obj_set_size(status_bar, 320, 22);
    reset_container(status_bar);
    lv_obj_set_style_border_color(status_bar, COL_SURFACE_HAIR, 0);
    lv_obj_set_style_border_width(status_bar, 1, 0);
    lv_obj_set_style_border_side(status_bar, LV_BORDER_SIDE_BOTTOM, 0);

    /* Wi-Fi icon (10, 4, 14 × 14) */
    icon_wifi = lv_image_create(status_bar);
    lv_obj_set_pos(icon_wifi, 10, 4);
    lv_obj_set_size(icon_wifi, 14, 14);
    setup_icon(icon_wifi, &img_wifi, COL_PRIMARY);

    /* MQTT/broadcast icon (32, 4, 14 × 14) */
    icon_mqtt = lv_image_create(status_bar);
    lv_obj_set_pos(icon_mqtt, 32, 4);
    lv_obj_set_size(icon_mqtt, 14, 14);
    setup_icon(icon_mqtt, &img_broadcast, COL_PRIMARY);

    /* ── Speed area (0, 22, 320 × 108) ───────────────────────────────── */
    lv_obj_t *speed_area = lv_obj_create(scr_home);
    lv_obj_set_pos(speed_area, 0, 22);
    lv_obj_set_size(speed_area, 320, 108);
    reset_container(speed_area);

    /* "COMMANDED SPEED" caption at (16, 12) */
    lbl_speed_caption = lv_label_create(speed_area);
    lv_obj_set_pos(lbl_speed_caption, 16, 12);
    lv_obj_set_style_text_font(lbl_speed_caption, &font_mono_12, 0);
    lv_obj_set_style_text_color(lbl_speed_caption, COL_LABEL, 0);
    lv_obj_set_style_text_letter_space(lbl_speed_caption, 2, 0);
    lv_label_set_text(lbl_speed_caption, "COMMANDED SPEED");

    /* Speed numeral — right-aligned, right of unit label */
    lbl_speed_value = lv_label_create(speed_area);
    lv_obj_set_style_text_font(lbl_speed_value, &font_mono_68, 0);
    lv_obj_set_style_text_color(lbl_speed_value, COL_PRIMARY, 0);
    lv_label_set_long_mode(lbl_speed_value, LV_LABEL_LONG_CLIP);
    lv_label_set_text(lbl_speed_value, "0");
    lv_obj_align(lbl_speed_value, LV_ALIGN_RIGHT_MID, -64, 6);

    /* "MPH" unit label */
    lbl_speed_unit = lv_label_create(speed_area);
    lv_obj_set_style_text_font(lbl_speed_unit, &font_mono_14, 0);
    lv_obj_set_style_text_color(lbl_speed_unit, COL_LABEL, 0);
    lv_label_set_text(lbl_speed_unit, "MPH");
    lv_obj_align(lbl_speed_unit, LV_ALIGN_RIGHT_MID, -16, 24);

    /* ── Indicator row (0, 130, 320 × 42) — top hairline ─────────────── */
    lv_obj_t *indicator_row = lv_obj_create(scr_home);
    lv_obj_set_pos(indicator_row, 0, 130);
    lv_obj_set_size(indicator_row, 320, 42);
    reset_container(indicator_row);
    lv_obj_set_style_border_color(indicator_row, COL_SURFACE_HAIR, 0);
    lv_obj_set_style_border_width(indicator_row, 1, 0);
    lv_obj_set_style_border_side(indicator_row, LV_BORDER_SIDE_TOP, 0);

    /* Source column (14, 134, 146 × 38) */
    lv_obj_t *source_col = lv_obj_create(scr_home);
    lv_obj_set_pos(source_col, 14, 134);
    lv_obj_set_size(source_col, 146, 38);
    reset_container(source_col);

    lv_obj_t *lbl_src_caption = lv_label_create(source_col);
    lv_obj_set_pos(lbl_src_caption, 0, 0);
    lv_obj_set_style_text_font(lbl_src_caption, &font_mono_12, 0);
    lv_obj_set_style_text_color(lbl_src_caption, COL_LABEL, 0);
    lv_label_set_text(lbl_src_caption, "SOURCE");

    dot_source = make_dot(source_col, COL_AMBER);
    lv_obj_set_pos(dot_source, 0, 22);

    lbl_src_value = lv_label_create(source_col);
    lv_obj_set_pos(lbl_src_value, 14, 18);
    lv_obj_set_style_text_font(lbl_src_value, &font_mono_14, 0);
    lv_obj_set_style_text_color(lbl_src_value, COL_AMBER, 0);
    lv_label_set_text(lbl_src_value, "STANDBY");

    /* Power column (160, 134, 146 × 38) */
    lv_obj_t *power_col = lv_obj_create(scr_home);
    lv_obj_set_pos(power_col, 160, 134);
    lv_obj_set_size(power_col, 146, 38);
    reset_container(power_col);

    lv_obj_t *lbl_pwr_caption = lv_label_create(power_col);
    lv_obj_set_pos(lbl_pwr_caption, 0, 0);
    lv_obj_set_style_text_font(lbl_pwr_caption, &font_mono_12, 0);
    lv_obj_set_style_text_color(lbl_pwr_caption, COL_LABEL, 0);
    lv_label_set_text(lbl_pwr_caption, "POWER");

    dot_power = make_dot(power_col, COL_RED);
    lv_obj_set_pos(dot_power, 0, 22);

    lbl_pwr_value = lv_label_create(power_col);
    lv_obj_set_pos(lbl_pwr_value, 14, 18);
    lv_obj_set_style_text_font(lbl_pwr_value, &font_mono_14, 0);
    lv_obj_set_style_text_color(lbl_pwr_value, COL_RED, 0);
    lv_label_set_text(lbl_pwr_value, "OFF");
}
