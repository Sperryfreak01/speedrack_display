#include "mqtt.h"
#include "ui.h"

#include "mqtt_client.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <string.h>
#include <stdlib.h>

static const char *TAG = "mqtt";

/* ── Configuration ─────────────────────────────────────────────────────── */
#define MQTT_BROKER_URI     "mqtt://192.168.2.9:1883"
#define MQTT_CLIENT_ID      "speedo-bench"
#define MQTT_FW_VERSION     "v0.1.0"

/* Topics published by this device */
#define TOPIC_ONLINE        "speedo-bench/state/online"
#define TOPIC_CMD_SOURCE    "speedo-bench/cmd/source"
#define TOPIC_CMD_POWER     "speedo-bench/cmd/power"
#define TOPIC_SPEED_ZERO    "speedo/target"   /* existing controller input */

/* Topics subscribed by this device */
#define TOPIC_STATE_SPEED   "speedo/speed"    /* existing controller output */
#define TOPIC_CFG_UNITS     "speedo-bench/cfg/units"

/* HA auto-discovery */
#define TOPIC_HA_SPEED_CFG      "homeassistant/sensor/speedo_bench_speed/config"
#define TOPIC_HA_SOURCE_CFG     "homeassistant/sensor/speedo_bench_source/config"
#define TOPIC_HA_POWER_CFG      "homeassistant/binary_sensor/speedo_bench_power/config"
#define TOPIC_HA_ONLINE_CFG     "homeassistant/binary_sensor/speedo_bench_online/config"

/* ── Shared state ───────────────────────────────────────────────────────── */
static esp_mqtt_client_handle_t s_client = NULL;
static volatile bool s_connected = false;

/* ── HA discovery payloads ──────────────────────────────────────────────── */

static const char *HA_DEVICE_BLOCK =
    "\"device\":{"
        "\"identifiers\":[\"speedo_bench\"],"
        "\"name\":\"Speedometer Test Bench\","
        "\"model\":\"ESP32-C6-Touch-LCD-1.47\","
        "\"manufacturer\":\"Waveshare / Custom\","
        "\"sw_version\":\"" MQTT_FW_VERSION "\""
    "},"
    "\"availability_topic\":\"" TOPIC_ONLINE "\","
    "\"payload_available\":\"online\","
    "\"payload_not_available\":\"offline\"";

static void publish_discovery(void)
{
    char buf[512];

    /* Speed sensor */
    snprintf(buf, sizeof(buf),
        "{"
            "\"name\":\"Speed\","
            "\"unique_id\":\"speedo_bench_speed\","
            "\"state_topic\":\"" TOPIC_STATE_SPEED "\","
            "\"unit_of_measurement\":\"MPH\","
            "\"icon\":\"mdi:speedometer\","
            "%s"
        "}",
        HA_DEVICE_BLOCK);
    esp_mqtt_client_publish(s_client, TOPIC_HA_SPEED_CFG, buf, 0, 1, 1);

    /* Source sensor */
    snprintf(buf, sizeof(buf),
        "{"
            "\"name\":\"Source\","
            "\"unique_id\":\"speedo_bench_source\","
            "\"state_topic\":\"" TOPIC_CMD_SOURCE "\","
            "\"icon\":\"mdi:source-branch\","
            "%s"
        "}",
        HA_DEVICE_BLOCK);
    esp_mqtt_client_publish(s_client, TOPIC_HA_SOURCE_CFG, buf, 0, 1, 1);

    /* Power binary sensor */
    snprintf(buf, sizeof(buf),
        "{"
            "\"name\":\"Power\","
            "\"unique_id\":\"speedo_bench_power\","
            "\"state_topic\":\"" TOPIC_CMD_POWER "\","
            "\"payload_on\":\"on\","
            "\"payload_off\":\"off\","
            "\"icon\":\"mdi:power\","
            "%s"
        "}",
        HA_DEVICE_BLOCK);
    esp_mqtt_client_publish(s_client, TOPIC_HA_POWER_CFG, buf, 0, 1, 1);

    /* Online binary sensor */
    snprintf(buf, sizeof(buf),
        "{"
            "\"name\":\"Online\","
            "\"unique_id\":\"speedo_bench_online\","
            "\"state_topic\":\"" TOPIC_ONLINE "\","
            "\"payload_on\":\"online\","
            "\"payload_off\":\"offline\","
            "\"device_class\":\"connectivity\","
            "%s"
        "}",
        HA_DEVICE_BLOCK);
    esp_mqtt_client_publish(s_client, TOPIC_HA_ONLINE_CFG, buf, 0, 1, 1);

    ESP_LOGI(TAG, "HA discovery published");
}

/* ── MQTT event handler ─────────────────────────────────────────────────── */

static void mqtt_event_handler(void *arg, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

    switch ((esp_mqtt_event_id_t)event_id) {

    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "Connected to broker");
        s_connected = true;

        /* 1. Announce online (retained) */
        esp_mqtt_client_publish(s_client, TOPIC_ONLINE, "online", 0, 1, 1);

        /* 2. HA discovery (retained) */
        publish_discovery();

        /* 3. Subscribe to state topics */
        esp_mqtt_client_subscribe(s_client, TOPIC_STATE_SPEED, 1);
        esp_mqtt_client_subscribe(s_client, TOPIC_CFG_UNITS, 1);

        /* 4. Update UI and diagnostics */
        if (lvgl_port_lock(0)) {
            ui_set_mqtt_connected(true);
            ui_diag_set_mqtt_broker("192.168.2.9:1883");
            ui_diag_set_fw(MQTT_FW_VERSION);
            lvgl_port_unlock();
        }
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "Disconnected from broker");
        s_connected = false;
        if (lvgl_port_lock(0)) {
            ui_set_mqtt_connected(false);
            lvgl_port_unlock();
        }
        break;

    case MQTT_EVENT_DATA: {
        /* Make null-terminated copies of topic and payload */
        char topic[128] = {0};
        char payload[64] = {0};
        int tlen = event->topic_len < (int)sizeof(topic) - 1
                   ? event->topic_len : (int)sizeof(topic) - 1;
        int plen = event->data_len < (int)sizeof(payload) - 1
                   ? event->data_len : (int)sizeof(payload) - 1;
        memcpy(topic, event->topic, tlen);
        memcpy(payload, event->data, plen);

        ESP_LOGD(TAG, "RX %s = %s", topic, payload);

        if (strcmp(topic, TOPIC_STATE_SPEED) == 0) {
            int spd = atoi(payload);
            if (lvgl_port_lock(0)) {
                ui_set_speed(spd);
                lvgl_port_unlock();
            }
        } else if (strcmp(topic, TOPIC_CFG_UNITS) == 0) {
            units_t u = (strcmp(payload, "kph") == 0) ? UNITS_KPH : UNITS_MPH;
            if (lvgl_port_lock(0)) {
                ui_set_units(u);
                lvgl_port_unlock();
            }
        }
        break;
    }

    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "MQTT error");
        break;

    default:
        break;
    }
}

/* ── Public API ─────────────────────────────────────────────────────────── */

void mqtt_start(void)
{
    esp_mqtt_client_config_t cfg = {
        .broker.address.uri    = MQTT_BROKER_URI,
        .credentials.client_id = MQTT_CLIENT_ID,
        .session.last_will = {
            .topic   = TOPIC_ONLINE,
            .msg     = "offline",
            .msg_len = 7,
            .qos     = 1,
            .retain  = 1,
        },
    };

    s_client = esp_mqtt_client_init(&cfg);
    esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID,
                                   mqtt_event_handler, NULL);
    esp_mqtt_client_start(s_client);
}

void mqtt_publish(const char *topic, const char *payload, bool retain)
{
    if (!s_client || !s_connected) return;
    esp_mqtt_client_publish(s_client, topic, payload, 0, 1, retain ? 1 : 0);
}

bool mqtt_is_connected(void)
{
    return s_connected;
}
