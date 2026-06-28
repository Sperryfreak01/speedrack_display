#include "scr_diag.h"
#include "theme.h"
#include "../fonts/fonts.h"
#include "../images/images.h"

/* §6 — Widget handles exposed to ui.c */
lv_obj_t *scr_diag;

lv_obj_t *icon_wifi_diag;
lv_obj_t *icon_mqtt_diag;

lv_obj_t *val_ip;
lv_obj_t *val_host;
lv_obj_t *val_mqtt;
lv_obj_t *val_rssi;
lv_obj_t *val_fw;

static void reset_container(lv_obj_t *o)
{
    lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_pad_all(o, 0, 0);
    lv_obj_set_style_radius(o, 0, 0);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
}

static void setup_icon(lv_obj_t *img, const lv_image_dsc_t *src, lv_color_t color)
{
    lv_image_set_src(img, src);
    lv_obj_set_style_image_recolor(img, color, 0);
    lv_obj_set_style_image_recolor_opa(img, LV_OPA_COVER, 0);
    lv_obj_remove_flag(img, LV_OBJ_FLAG_CLICKABLE);
}

/* Create one SPACE_BETWEEN row in the diagnostic table.
 * Returns the value label so the caller can store it. */
static lv_obj_t *make_row(lv_obj_t *parent, const char *label_text,
                           const char *value_text)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    reset_container(row);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *lbl = lv_label_create(row);
    lv_obj_set_style_text_font(lbl, &font_mono_14, 0);
    lv_obj_set_style_text_color(lbl, COL_LABEL, 0);
    lv_label_set_text(lbl, label_text);

    lv_obj_t *val = lv_label_create(row);
    lv_obj_set_style_text_font(val, &font_mono_14, 0);
    lv_obj_set_style_text_color(val, COL_PRIMARY, 0);
    lv_label_set_text(val, value_text);

    return val;
}

void scr_diag_build(void)
{
    /* ── Root screen (320 × 172) ──────────────────────────────────────── */
    scr_diag = lv_obj_create(NULL);
    lv_obj_set_size(scr_diag, 320, 172);
    lv_obj_set_style_bg_color(scr_diag, COL_BG, 0);
    lv_obj_set_style_bg_opa(scr_diag, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(scr_diag, 0, 0);
    lv_obj_set_style_pad_all(scr_diag, 0, 0);
    lv_obj_set_style_radius(scr_diag, 0, 0);
    lv_obj_remove_flag(scr_diag, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(scr_diag, LV_OBJ_FLAG_CLICKABLE); /* §8 */

    /* ── Status bar (0, 0, 320 × 22) — bottom hairline ──────────────── */
    lv_obj_t *status_bar = lv_obj_create(scr_diag);
    lv_obj_set_pos(status_bar, 0, 0);
    lv_obj_set_size(status_bar, 320, 22);
    reset_container(status_bar);
    lv_obj_set_style_border_color(status_bar, COL_SURFACE_HAIR, 0);
    lv_obj_set_style_border_width(status_bar, 1, 0);
    lv_obj_set_style_border_side(status_bar, LV_BORDER_SIDE_BOTTOM, 0);

    /* Back chevron (10, 6) — §4 pipeline has no chevron SVG; use symbol.
     * Gap 1: rendered as text label rather than a third image asset. */
    lv_obj_t *icon_back = lv_label_create(status_bar);
    lv_obj_set_pos(icon_back, 10, 6);
    lv_obj_set_style_text_font(icon_back, &font_mono_12, 0);
    lv_obj_set_style_text_color(icon_back, COL_LABEL, 0);
    lv_label_set_text(icon_back, LV_SYMBOL_LEFT);

    /* "DIAGNOSTICS" title (28, 6) */
    lv_obj_t *lbl_diag_title = lv_label_create(status_bar);
    lv_obj_set_pos(lbl_diag_title, 28, 6);
    lv_obj_set_style_text_font(lbl_diag_title, &font_mono_12, 0);
    lv_obj_set_style_text_color(lbl_diag_title, COL_LABEL, 0);
    lv_label_set_text(lbl_diag_title, "DIAGNOSTICS");

    /* Wi-Fi icon (RIGHT_MID, x-offset -32) — recolor matches home screen state.
     * Gap 2: §6 referenced font_icons_14 (undefined); using A8 image instead. */
    icon_wifi_diag = lv_image_create(status_bar);
    lv_obj_set_size(icon_wifi_diag, 14, 14);
    setup_icon(icon_wifi_diag, &img_wifi, COL_DIM_ICON); /* disconnected until updated */
    lv_obj_align(icon_wifi_diag, LV_ALIGN_RIGHT_MID, -32, 0);

    /* MQTT icon (RIGHT_MID, x-offset -10) */
    icon_mqtt_diag = lv_image_create(status_bar);
    lv_obj_set_size(icon_mqtt_diag, 14, 14);
    setup_icon(icon_mqtt_diag, &img_broadcast, COL_DIM_ICON);
    lv_obj_align(icon_mqtt_diag, LV_ALIGN_RIGHT_MID, -10, 0);

    /* ── Diagnostic table (14, 30, 292 × 100) — flex column ─────────── */
    lv_obj_t *diag_table = lv_obj_create(scr_diag);
    lv_obj_set_pos(diag_table, 14, 30);
    lv_obj_set_size(diag_table, 292, 100);
    reset_container(diag_table);
    lv_obj_set_flex_flow(diag_table, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(diag_table, 4, 0);

    /* Gap 3: RSSI default uses ASCII hyphen 0x2D; spec mockup shows U+2212
     * which is outside the 0x20-0x7A font range. snprintf always emits 0x2D. */
    val_ip   = make_row(diag_table, "IP",   "192.168.2.144");
    val_host = make_row(diag_table, "HOST", "speedo-bench");
    val_mqtt = make_row(diag_table, "MQTT", "192.168.2.9:1883");
    val_rssi = make_row(diag_table, "RSSI", "-52 dBm");
    val_fw   = make_row(diag_table, "FW",   "v0.1.0");

    /* ── Footer (0, 152, 320 × 20) — top hairline ───────────────────── */
    lv_obj_t *footer = lv_obj_create(scr_diag);
    lv_obj_set_pos(footer, 0, 152);
    lv_obj_set_size(footer, 320, 20);
    reset_container(footer);
    lv_obj_set_style_border_color(footer, COL_SURFACE_HAIR, 0);
    lv_obj_set_style_border_width(footer, 1, 0);
    lv_obj_set_style_border_side(footer, LV_BORDER_SIDE_TOP, 0);

    lv_obj_t *lbl_return = lv_label_create(footer);
    lv_obj_set_style_text_font(lbl_return, &font_mono_12, 0);
    lv_obj_set_style_text_color(lbl_return, COL_DIM_TEXT, 0);
    lv_obj_set_style_text_letter_space(lbl_return, 2, 0);
    lv_label_set_text(lbl_return, "TAP TO RETURN");
    lv_obj_align(lbl_return, LV_ALIGN_CENTER, 0, 0);
}
