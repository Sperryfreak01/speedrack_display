#pragma once

/**
 * Configure GPIO inputs (toggle switch, BOOT button) and outputs (relays),
 * then start the polling task. The task reads every 10 ms, applies 50 ms
 * debounce, drives relays with power interlock, updates the UI, publishes
 * MQTT on toggle changes, and calls ui_toggle_screen() on a BOOT button
 * press (this board has no touch hardware for screen navigation).
 */
void gpio_ctrl_init(void);
