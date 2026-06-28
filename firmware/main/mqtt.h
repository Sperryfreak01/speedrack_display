#pragma once

#include <stdbool.h>
#include "ui.h"

/**
 * Initialize and start the MQTT client.
 * Connects to the configured broker, sets LWT, publishes HA discovery,
 * and subscribes to state topics. Handles reconnection automatically.
 */
void mqtt_start(void);

/**
 * Publish a retained message. Safe to call from any task.
 * @param topic    Full topic string
 * @param payload  Null-terminated payload string
 * @param retain   Whether to retain the message on the broker
 */
void mqtt_publish(const char *topic, const char *payload, bool retain);

/** True when the MQTT client is connected. */
bool mqtt_is_connected(void);
