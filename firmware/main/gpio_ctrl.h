#pragma once

/**
 * Configure GPIO inputs (toggle switch) and outputs (relays), then
 * start the polling task. The task reads the toggle every 10 ms,
 * applies 50 ms debounce, drives relays with power interlock, updates
 * the UI, and publishes MQTT on state changes.
 */
void gpio_ctrl_init(void);
