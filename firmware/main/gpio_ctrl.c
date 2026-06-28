#include "gpio_ctrl.h"
#include "mqtt.h"
#include "ui.h"

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"

#include <string.h>

static const char *TAG = "gpio_ctrl";

/* ── Pin configuration ──────────────────────────────────────────────────────
 *
 * VERIFY these against the Waveshare BSP before wiring!
 * Reserved by the BSP (approximate):
 *   GPIO 1–8:  SPI display (MOSI/CLK/CS/DC/RST/BL) + I2C SDA/SCL
 *   GPIO 7–11: I2C SCL/SDA, touch INT, RGB LED
 *
 * Pins chosen here (12, 13, 16, 17) are in the safe upper range.
 * ───────────────────────────────────────────────────────────────────────── */
#define PIN_TOGGLE_A    GPIO_NUM_12   /* High when toggle UP   (SIG GEN) */
#define PIN_TOGGLE_B    GPIO_NUM_13   /* High when toggle DOWN (MOTOR)   */
#define PIN_RELAY_SIG   GPIO_NUM_16   /* High = signal gen selected; NC = motor sensor */
#define PIN_RELAY_PWR   GPIO_NUM_17   /* High = DUT powered (only when SIG relay HIGH) */

/* Debounce: 5 consecutive 10 ms reads = 50 ms stable */
#define DEBOUNCE_COUNT  5
#define POLL_MS         10

/* ── Internal toggle state ─────────────────────────────────────────────── */
typedef enum { POS_SIG_GEN, POS_CENTER, POS_MOTOR } toggle_pos_t;

static toggle_pos_t read_toggle_raw(void)
{
    int a = gpio_get_level(PIN_TOGGLE_A);
    int b = gpio_get_level(PIN_TOGGLE_B);
    if      (a == 0 && b == 1) return POS_SIG_GEN;
    else if (a == 1 && b == 0) return POS_MOTOR;
    else                        return POS_CENTER;   /* 0,0 or invalid */
}

static void apply_position(toggle_pos_t pos)
{
    switch (pos) {
    case POS_SIG_GEN:
        gpio_set_level(PIN_RELAY_SIG, 1);
        gpio_set_level(PIN_RELAY_PWR, 1);
        if (lvgl_port_lock(0)) {
            ui_set_source(SRC_SIG_GEN);
            ui_set_power(true);
            lvgl_port_unlock();
        }
        mqtt_publish("speedo-bench/cmd/source", "sig_gen", false);
        mqtt_publish("speedo-bench/cmd/power",  "on",      false);
        ESP_LOGI(TAG, "SIG GEN selected, DUT powered");
        break;

    case POS_MOTOR:
        gpio_set_level(PIN_RELAY_SIG, 1);
        gpio_set_level(PIN_RELAY_PWR, 1);
        if (lvgl_port_lock(0)) {
            ui_set_source(SRC_MOTOR);
            ui_set_power(true);
            lvgl_port_unlock();
        }
        mqtt_publish("speedo-bench/cmd/source", "motor", false);
        mqtt_publish("speedo-bench/cmd/power",  "on",    false);
        ESP_LOGI(TAG, "MOTOR selected, DUT powered");
        break;

    case POS_CENTER:
        /* Power relay must go LOW before (or simultaneously with) signal relay.
         * Write power first to enforce interlock. */
        gpio_set_level(PIN_RELAY_PWR, 0);
        gpio_set_level(PIN_RELAY_SIG, 0);
        /* Relay connects to NC terminal (transmission sensor) when LOW */
        if (lvgl_port_lock(0)) {
            ui_set_source(SRC_STANDBY);
            ui_set_power(false);
            ui_set_speed(0);
            lvgl_port_unlock();
        }
        mqtt_publish("speedo-bench/cmd/source", "standby", false);
        mqtt_publish("speedo-bench/cmd/power",  "off",     false);
        mqtt_publish("speedo/target",            "0",       false);
        ESP_LOGI(TAG, "STANDBY — motor zeroed, DUT unpowered");
        break;
    }
}

/* ── Poll task ──────────────────────────────────────────────────────────── */

static void gpio_poll_task(void *arg)
{
    toggle_pos_t confirmed  = POS_CENTER;
    toggle_pos_t candidate  = POS_CENTER;
    uint8_t      stable_cnt = DEBOUNCE_COUNT; /* start "stable" at boot state */

    /* Apply boot state immediately */
    apply_position(POS_CENTER);

    for (;;) {
        toggle_pos_t raw = read_toggle_raw();

        if (raw == candidate) {
            if (stable_cnt < DEBOUNCE_COUNT) {
                stable_cnt++;
            }
        } else {
            candidate  = raw;
            stable_cnt = 0;
        }

        if (stable_cnt == DEBOUNCE_COUNT && candidate != confirmed) {
            confirmed = candidate;
            apply_position(confirmed);
        }

        vTaskDelay(pdMS_TO_TICKS(POLL_MS));
    }
}

/* ── Public API ─────────────────────────────────────────────────────────── */

void gpio_ctrl_init(void)
{
    /* Toggle inputs — active-high (external switch pulls to 3.3 V) */
    gpio_config_t in_cfg = {
        .pin_bit_mask = (1ULL << PIN_TOGGLE_A) | (1ULL << PIN_TOGGLE_B),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&in_cfg));

    /* Relay outputs — start LOW (safe/off) */
    gpio_config_t out_cfg = {
        .pin_bit_mask = (1ULL << PIN_RELAY_SIG) | (1ULL << PIN_RELAY_PWR),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&out_cfg));
    gpio_set_level(PIN_RELAY_SIG, 0);
    gpio_set_level(PIN_RELAY_PWR, 0);

    xTaskCreate(gpio_poll_task, "gpio_poll", 3072, NULL, 5, NULL);
    ESP_LOGI(TAG, "GPIO control started (toggle A=GPIO%d B=GPIO%d, "
                  "relay_sig=GPIO%d relay_pwr=GPIO%d)",
             PIN_TOGGLE_A, PIN_TOGGLE_B, PIN_RELAY_SIG, PIN_RELAY_PWR);
}
