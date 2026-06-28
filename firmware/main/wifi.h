#pragma once

/**
 * Initialize Wi-Fi station and begin connecting.
 * Non-blocking. Results are delivered via the default ESP event loop:
 *   WIFI_EVENT_STA_CONNECTED / IP_EVENT_STA_GOT_IP / WIFI_EVENT_STA_DISCONNECTED
 */
void wifi_start(void);

/** Current RSSI in dBm, or 0 if not connected. */
int wifi_get_rssi(void);
