#include "gpio_ctrl.h"
#include "mqtt.h"
#include "ui.h"

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"

#include <string.h>
#include <stdbool.h>

static const char *TAG = "gpio_ctrl";

/* ── Pin configuration ──────────────────────────────────────────────────────
 *
 * Confirmed against docs.waveshare.com/ESP32-C6-LCD-1.47 (non-touch board):
 * display SPI uses GPIO6/7/14/15/21/22, RGB LED is GPIO8, BOOT button is
 * GPIO9. GPIO12/13 are this chip's native USB Serial/JTAG D-/D+ pins (used
 * by the console/flash link) — do NOT use them for anything else. GPIO18/19
 * are safe general-purpose pins on the SiP-flash variant of this chip.
 * ───────────────────────────────────────────────────────────────────────── */
#define PIN_TOGGLE_A    GPIO_NUM_18   /* High when toggle UP   (SIG GEN) */
#define PIN_TOGGLE_B    GPIO_NUM_19   /* High when toggle DOWN (MOTOR)   */
#define PIN_RELAY_SIG   GPIO_NUM_16   /* High = signal gen selected; NC = motor sensor */
#define PIN_RELAY_PWR   GPIO_NUM_17   /* High = DUT powered (only when SIG relay HIGH) */
#define PIN_BOOT_BTN    GPIO_NUM_9    /* Onboard BOOT button, active-low, internal pull-up */

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

    /* BOOT button: pulled up, so "not pressed" (1) is the stable boot state */
    bool    boot_confirmed  = true;
    bool    boot_candidate  = true;
    uint8_t boot_stable_cnt = DEBOUNCE_COUNT;

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

        bool boot_raw = gpio_get_level(PIN_BOOT_BTN) != 0;

        if (boot_raw == boot_candidate) {
            if (boot_stable_cnt < DEBOUNCE_COUNT) {
                boot_stable_cnt++;
            }
        } else {
            boot_candidate  = boot_raw;
            boot_stable_cnt = 0;
        }

        if (boot_stable_cnt == DEBOUNCE_COUNT && boot_candidate != boot_confirmed) {
            boot_confirmed = boot_candidate;
            if (!boot_confirmed) { /* falling edge: button pressed */
                if (lvgl_port_lock(0)) {
                    ui_toggle_screen();
                    lvgl_port_unlock();
                }
                ESP_LOGI(TAG, "BOOT button pressed, screen toggled");
            }
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

    /* BOOT button — active-low, uses its own internal pull-up */
    gpio_config_t boot_cfg = {
        .pin_bit_mask = 1ULL << PIN_BOOT_BTN,
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&boot_cfg));

    /* 3072 wasn't enough: ui_toggle_screen() -> lv_screen_load_anim()'s call
     * depth overflowed it (confirmed via Guru Meditation stack protection
     * fault on physical hardware, task name corrupted in the panic dump). */
    xTaskCreate(gpio_poll_task, "gpio_poll", 6144, NULL, 5, NULL);
    ESP_LOGI(TAG, "GPIO control started (toggle A=GPIO%d B=GPIO%d, "
                  "relay_sig=GPIO%d relay_pwr=GPIO%d, boot_btn=GPIO%d)",
             PIN_TOGGLE_A, PIN_TOGGLE_B, PIN_RELAY_SIG, PIN_RELAY_PWR, PIN_BOOT_BTN);
}
