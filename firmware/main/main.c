#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"

/* Waveshare BSP ── bring up display, touch, and backlight.
 * If the component is not yet in your managed_components/, see
 * firmware/main/idf_component.yml for setup instructions. */
#include "bsp/esp-bsp.h"

#include "ui.h"
#include "wifi.h"
#include "mqtt.h"
#include "gpio_ctrl.h"

static const char *TAG = "main";

void app_main(void)
{
    /* NVS is required by Wi-Fi */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* 1. BSP: bring up display and touch.
     *    bsp_display_start() internally initialises esp_lvgl_port and
     *    registers the display + indev with LVGL. */
    ESP_LOGI(TAG, "Starting BSP");
    bsp_display_start();
    bsp_display_set_brightness(100);
    bsp_touch_init(NULL);

    /* 2. UI: build both screens, wire touch events, apply boot state.
     *    Must be called after BSP (display and LVGL are live). */
    ESP_LOGI(TAG, "Initialising UI");
    lvgl_port_lock(0);
    ui_init();
    lvgl_port_unlock();

    /* 3. GPIO: configure toggle inputs and relay outputs, start poll task. */
    gpio_ctrl_init();

    /* 4. Networking */
    ESP_LOGI(TAG, "Starting Wi-Fi");
    wifi_start();

    ESP_LOGI(TAG, "Starting MQTT");
    mqtt_start();

    /* app_main returns; all work happens in FreeRTOS tasks started above
     * and in the LVGL task managed by esp_lvgl_port. */
}
