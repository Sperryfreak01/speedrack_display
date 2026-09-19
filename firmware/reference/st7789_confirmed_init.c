#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "esp_err.h"

static const char *TAG = "st7789_smoketest";

/* Confirmed pins for Waveshare ESP32-C6-LCD-1.47 (non-touch),
 * per docs.waveshare.com/ESP32-C6-LCD-1.47 */
#define PIN_MOSI GPIO_NUM_6
#define PIN_SCLK GPIO_NUM_7
#define PIN_CS   GPIO_NUM_14
#define PIN_DC   GPIO_NUM_15
#define PIN_RST  GPIO_NUM_21
#define PIN_BL   GPIO_NUM_22

#define LCD_H_RES 172
#define LCD_V_RES 320
#define SPI_HOST_USED SPI2_HOST

#define CHUNK_ROWS 40

void app_main(void)
{
    ESP_LOGI(TAG, "Boot: ST7789 diagnostic for ESP32-C6-LCD-1.47 (non-touch)");

    /* Backlight: plain GPIO high (method not documented, simplest first try) */
    gpio_config_t bl_conf = {
        .pin_bit_mask = 1ULL << PIN_BL,
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&bl_conf));
    gpio_set_level(PIN_BL, 1);
    ESP_LOGI(TAG, "Backlight GPIO%d driven high", PIN_BL);

    ESP_LOGI(TAG, "Initializing SPI bus: MOSI=%d SCLK=%d", PIN_MOSI, PIN_SCLK);
    spi_bus_config_t buscfg = {
        .mosi_io_num = PIN_MOSI,
        .miso_io_num = -1,
        .sclk_io_num = PIN_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_H_RES * CHUNK_ROWS * 2,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI_HOST_USED, &buscfg, SPI_DMA_CH_AUTO));

    ESP_LOGI(TAG, "Installing panel IO: CS=%d DC=%d", PIN_CS, PIN_DC);
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .cs_gpio_num = PIN_CS,
        .dc_gpio_num = PIN_DC,
        .spi_mode = 0,
        .pclk_hz = 20 * 1000 * 1000, /* conservative clock for the first bring-up test */
        .trans_queue_depth = 10,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI_HOST_USED, &io_config, &io_handle));

    ESP_LOGI(TAG, "Creating ST7789 panel: RST=%d", PIN_RST);
    esp_lcd_panel_handle_t panel_handle = NULL;
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = PIN_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_LOGI(TAG, "Panel reset OK");
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_LOGI(TAG, "Panel init OK");
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true));
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel_handle, 34, 0));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, false, false));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));
    ESP_LOGI(TAG, "Panel on. Filling screen red via raw draw_bitmap (bypassing LVGL entirely)");

    uint8_t *line_buf = malloc(LCD_H_RES * CHUNK_ROWS * 2);
    if (!line_buf) {
        ESP_LOGE(TAG, "malloc failed");
        return;
    }
    /* RGB565 red (0xF800), big-endian byte order as expected over the wire */
    for (int i = 0; i < LCD_H_RES * CHUNK_ROWS; i++) {
        line_buf[2 * i] = 0xF8;
        line_buf[2 * i + 1] = 0x00;
    }

    for (int y = 0; y < LCD_V_RES; y += CHUNK_ROWS) {
        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel_handle, 0, y, LCD_H_RES, y + CHUNK_ROWS, line_buf));
    }
    ESP_LOGI(TAG, "Fill complete. If the screen is showing solid red, the pin/driver fix worked.");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
