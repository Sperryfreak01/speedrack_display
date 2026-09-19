#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"

#include "ui.h"
#include "wifi.h"
#include "mqtt.h"
#include "gpio_ctrl.h"

static const char *TAG = "main";

/* Confirmed pins for Waveshare ESP32-C6-LCD-1.47 (non-touch), per
 * docs.waveshare.com/ESP32-C6-LCD-1.47 and
 * firmware/reference/st7789_confirmed_init.c (verified working on hardware).
 * No touch controller exists on this board — see gpio_ctrl.c for BOOT
 * button (GPIO9) screen navigation instead. */
#define PIN_MOSI GPIO_NUM_6
#define PIN_SCLK GPIO_NUM_7
#define PIN_CS   GPIO_NUM_14
#define PIN_DC   GPIO_NUM_15
#define PIN_RST  GPIO_NUM_21
#define PIN_BL   GPIO_NUM_22

/* LVGL-logical (landscape) resolution the UI is laid out against. The panel
 * is physically portrait (172 wide x 320 tall) — corrected to landscape via
 * rotation.swap_xy in lvgl_display_init() plus a matching gap-axis swap
 * below (esp_lcd_panel_set_gap). */
#define LCD_H_RES             320
#define LCD_V_RES             172
#define SPI_HOST_USED         SPI2_HOST
#define LCD_DRAW_BUFF_HEIGHT  40

/* Backlight is PWM-driven via LEDC and hard-capped at 50% duty — full
 * brightness is not to be used on this display, so there is no path to
 * exceed the cap even if brightness ever becomes runtime-configurable. */
#define BACKLIGHT_MAX_PERCENT    50
#define BACKLIGHT_LEDC_TIMER     LEDC_TIMER_0
#define BACKLIGHT_LEDC_MODE      LEDC_LOW_SPEED_MODE
#define BACKLIGHT_LEDC_CHANNEL   LEDC_CHANNEL_0
#define BACKLIGHT_LEDC_DUTY_RES  LEDC_TIMER_10_BIT
#define BACKLIGHT_LEDC_FREQ_HZ   5000

static esp_lcd_panel_io_handle_t io_handle    = NULL;
static esp_lcd_panel_handle_t    panel_handle = NULL;

static void backlight_init(void)
{
    const ledc_timer_config_t timer_cfg = {
        .speed_mode      = BACKLIGHT_LEDC_MODE,
        .duty_resolution = BACKLIGHT_LEDC_DUTY_RES,
        .timer_num       = BACKLIGHT_LEDC_TIMER,
        .freq_hz         = BACKLIGHT_LEDC_FREQ_HZ,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_cfg));

    const ledc_channel_config_t channel_cfg = {
        .gpio_num   = PIN_BL,
        .speed_mode = BACKLIGHT_LEDC_MODE,
        .channel    = BACKLIGHT_LEDC_CHANNEL,
        .timer_sel  = BACKLIGHT_LEDC_TIMER,
        .duty       = 0,
        .hpoint     = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&channel_cfg));

    uint32_t max_duty = (1U << BACKLIGHT_LEDC_DUTY_RES) - 1;
    uint32_t duty = (max_duty * BACKLIGHT_MAX_PERCENT) / 100;
    ESP_ERROR_CHECK(ledc_set_duty(BACKLIGHT_LEDC_MODE, BACKLIGHT_LEDC_CHANNEL, duty));
    ESP_ERROR_CHECK(ledc_update_duty(BACKLIGHT_LEDC_MODE, BACKLIGHT_LEDC_CHANNEL));
    ESP_LOGI(TAG, "Backlight GPIO%d PWM at %d%% duty (hard cap)", PIN_BL, BACKLIGHT_MAX_PERCENT);
}

static void display_init(void)
{
    backlight_init();

    ESP_LOGI(TAG, "Initializing SPI bus: MOSI=%d SCLK=%d", PIN_MOSI, PIN_SCLK);
    spi_bus_config_t buscfg = {
        .mosi_io_num     = PIN_MOSI,
        .miso_io_num     = -1,
        .sclk_io_num     = PIN_SCLK,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = LCD_H_RES * LCD_DRAW_BUFF_HEIGHT * 2,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI_HOST_USED, &buscfg, SPI_DMA_CH_AUTO));

    ESP_LOGI(TAG, "Installing panel IO: CS=%d DC=%d", PIN_CS, PIN_DC);
    esp_lcd_panel_io_spi_config_t io_config = {
        .cs_gpio_num     = PIN_CS,
        .dc_gpio_num     = PIN_DC,
        .spi_mode        = 0,
        .pclk_hz         = 20 * 1000 * 1000,
        .trans_queue_depth = 10,
        .lcd_cmd_bits    = 8,
        .lcd_param_bits  = 8,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI_HOST_USED, &io_config, &io_handle));

    ESP_LOGI(TAG, "Creating ST7789 panel: RST=%d", PIN_RST);
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = PIN_RST,
        .rgb_ele_order  = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true));
    /* The 34px silicon gap sits on the panel's physical short axis. With
     * swap_xy applied below, that axis is addressed via RASET (y), not
     * CASET (x) as in the no-rotation smoke test — so the gap moves from
     * x_gap to y_gap here. */
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel_handle, 0, 34));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, false, false));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));
    ESP_LOGI(TAG, "ST7789 panel initialized");
}

static void lvgl_display_init(void)
{
    const lvgl_port_cfg_t lvgl_cfg = {
        .task_priority   = 4,
        .task_stack      = 4096,
        .task_affinity   = -1,
        .task_max_sleep_ms = 500,
        .timer_period_ms = 5,
    };
    ESP_ERROR_CHECK(lvgl_port_init(&lvgl_cfg));

    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle    = io_handle,
        .panel_handle = panel_handle,
        .buffer_size  = LCD_H_RES * LCD_DRAW_BUFF_HEIGHT,
        .double_buffer = true,
        .hres         = LCD_H_RES,
        .vres         = LCD_V_RES,
        .monochrome   = false,
        .color_format = LV_COLOR_FORMAT_RGB565,
        .rotation = {
            .swap_xy  = true,
            .mirror_x = false,
            .mirror_y = false,
        },
        .flags = {
            .buff_dma   = true,
            .swap_bytes = true,
        },
    };
    if (lvgl_port_add_disp(&disp_cfg) == NULL) {
        ESP_LOGE(TAG, "lvgl_port_add_disp failed");
    }
}

void app_main(void)
{
    /* NVS is required by Wi-Fi */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* 1. Display: bring up the ST7789 panel directly (built-in ESP-IDF
     *    panel driver — no BSP component needed), then wire it into LVGL.
     *    No touch init: this board has no touch controller. */
    ESP_LOGI(TAG, "Bringing up ST7789 display");
    display_init();

    ESP_LOGI(TAG, "Starting LVGL port");
    lvgl_display_init();

    /* 2. UI: build both screens, wire fallback click events, apply boot
     *    state. Must be called after the display and LVGL are live. */
    ESP_LOGI(TAG, "Initialising UI");
    lvgl_port_lock(0);
    ui_init();
    lvgl_port_unlock();

    /* 3. GPIO: configure toggle inputs, relay outputs, and the BOOT button
     *    (screen navigation), then start the poll task. */
    gpio_ctrl_init();

    /* 4. Networking */
    ESP_LOGI(TAG, "Starting Wi-Fi");
    wifi_start();

    ESP_LOGI(TAG, "Starting MQTT");
    mqtt_start();

    /* app_main returns; all work happens in FreeRTOS tasks started above
     * and in the LVGL task managed by esp_lvgl_port. */
}
