#include "mqtt_service.h"
#include "config.h"
#include <string.h>
#include <stdio.h>
#include "esp_log.h"
#include "mqtt_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "cJSON.h"

static const char *TAG = "MQTT_SERVICE";

// Default enable if not defined in config
#ifndef ENABLE_MQTT
#define ENABLE_MQTT 0
#endif

#if ENABLE_MQTT

// MQTT client handle
static esp_mqtt_client_handle_t s_mqtt_client = NULL;
static bool s_mqtt_connected = false;
static rover_status_t s_current_status = {0};
static SemaphoreHandle_t s_status_mutex = NULL;
static TaskHandle_t s_publish_task_handle = NULL;
static char s_topic_prefix[64] = "esp32-rover";
static uint32_t s_publish_interval_ms = 5000;
static int s_qos = 0;

// Forward declarations
static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                                int32_t event_id, void *event_data);
static void mqtt_publish_task(void *pvParameters);

static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                                int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT connected to broker");
            s_mqtt_connected = true;
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "MQTT disconnected from broker");
            s_mqtt_connected = false;
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT error occurred");
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                ESP_LOGE(TAG, "Transport error: %s",
                         strerror(event->error_handle->esp_transport_sock_errno));
            }
            break;

        case MQTT_EVENT_PUBLISHED:
#if CFG_DEBUG_MQTT
            ESP_LOGI(TAG, "MQTT message published, msg_id=%d", event->msg_id);
#endif
            break;

        default:
            break;
    }
}

static void mqtt_publish_task(void *pvParameters)
{
    char topic[128];

    ESP_LOGI(TAG, "MQTT publish task started (interval: %lu ms)", s_publish_interval_ms);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(s_publish_interval_ms));

        if (!s_mqtt_connected) {
            continue;
        }

        // Get current status
        rover_status_t status = {0};
        if (s_status_mutex && xSemaphoreTake(s_status_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            status = s_current_status;
            xSemaphoreGive(s_status_mutex);
        }

        // Build JSON payload
        cJSON *root = cJSON_CreateObject();
        if (!root) {
            ESP_LOGE(TAG, "Failed to create JSON object");
            continue;
        }

        // Basic status
        cJSON_AddNumberToObject(root, "battery", status.battery_voltage);
        cJSON_AddBoolToObject(root, "camera", status.camera_active);
        cJSON_AddNumberToObject(root, "rssi", status.wifi_rssi);

        // Diagnostic data
        cJSON *diag = cJSON_CreateObject();
        if (diag) {
            if (status.wifi_ssid) cJSON_AddStringToObject(diag, "ssid", status.wifi_ssid);
            if (status.wifi_ip) cJSON_AddStringToObject(diag, "ip", status.wifi_ip);
            if (status.mac_addr) cJSON_AddStringToObject(diag, "mac", status.mac_addr);
            cJSON_AddNumberToObject(diag, "channel", status.wifi_channel);
            cJSON_AddNumberToObject(diag, "clients", status.connected_clients);
            cJSON_AddNumberToObject(diag, "txPower", status.wifi_tx_power);
            cJSON_AddNumberToObject(diag, "freeHeap", status.free_heap);
            cJSON_AddNumberToObject(diag, "minHeap", status.min_free_heap);
            cJSON_AddNumberToObject(diag, "totalHeap", status.total_heap);
            cJSON_AddNumberToObject(diag, "freeInternal", status.free_internal);
            cJSON_AddNumberToObject(diag, "uptime", status.uptime_secs);
            cJSON_AddNumberToObject(diag, "cpuFreq", status.cpu_freq_mhz);
            cJSON_AddNumberToObject(diag, "tasks", status.task_count);
            cJSON_AddItemToObject(root, "diag", diag);
        }

        // Convert to string
        char *json_str = cJSON_PrintUnformatted(root);
        cJSON_Delete(root);

        if (!json_str) {
            ESP_LOGE(TAG, "Failed to print JSON");
            continue;
        }

        // Publish to status topic
        snprintf(topic, sizeof(topic), "%s/status", s_topic_prefix);
        int msg_id = esp_mqtt_client_publish(s_mqtt_client, topic, json_str,
                                              strlen(json_str), s_qos, 0);

        if (msg_id < 0) {
            ESP_LOGW(TAG, "Failed to publish MQTT message");
        }
#if CFG_DEBUG_MQTT
        else {
            ESP_LOGI(TAG, "Published to %s: %s", topic, json_str);
        }
#endif

        free(json_str);
    }
}

esp_err_t mqtt_service_init(const mqtt_service_config_t *config)
{
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_mqtt_client) {
        ESP_LOGW(TAG, "MQTT service already initialized");
        return ESP_OK;
    }

    // Create mutex
    if (!s_status_mutex) {
        s_status_mutex = xSemaphoreCreateMutex();
        if (!s_status_mutex) {
            return ESP_ERR_NO_MEM;
        }
    }

    // Store configuration
    strncpy(s_topic_prefix, config->topic_prefix ? config->topic_prefix : "esp32-rover",
            sizeof(s_topic_prefix) - 1);
    s_publish_interval_ms = config->publish_interval_ms > 0 ? config->publish_interval_ms : 5000;
    s_qos = config->qos;

    // Build broker URI
    char broker_uri[128];
    snprintf(broker_uri, sizeof(broker_uri), "mqtt://%s:%u",
             config->broker_host, config->broker_port);

    ESP_LOGI(TAG, "Connecting to MQTT broker: %s", broker_uri);

    // Configure MQTT client
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = broker_uri,
        .credentials.client_id = config->client_id ? config->client_id : "esp32-rover",
    };

    if (config->username && strlen(config->username) > 0) {
        mqtt_cfg.credentials.username = config->username;
    }
    if (config->password && strlen(config->password) > 0) {
        mqtt_cfg.credentials.authentication.password = config->password;
    }

    // Create client
    s_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (!s_mqtt_client) {
        ESP_LOGE(TAG, "Failed to initialize MQTT client");
        return ESP_FAIL;
    }

    // Register event handler
    esp_mqtt_client_register_event(s_mqtt_client, ESP_EVENT_ANY_ID,
                                    mqtt_event_handler, NULL);

    // Start client
    esp_err_t ret = esp_mqtt_client_start(s_mqtt_client);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start MQTT client: %s", esp_err_to_name(ret));
        esp_mqtt_client_destroy(s_mqtt_client);
        s_mqtt_client = NULL;
        return ret;
    }

    // Create publish task
    xTaskCreatePinnedToCore(
        mqtt_publish_task,
        "mqtt_pub",
        4096,
        NULL,
        2,
        &s_publish_task_handle,
        0  // Core 0
    );

    ESP_LOGI(TAG, "MQTT service started (topic prefix: %s)", s_topic_prefix);
    return ESP_OK;
}

esp_err_t mqtt_service_stop(void)
{
    if (s_publish_task_handle) {
        vTaskDelete(s_publish_task_handle);
        s_publish_task_handle = NULL;
    }

    if (s_mqtt_client) {
        esp_mqtt_client_stop(s_mqtt_client);
        esp_mqtt_client_destroy(s_mqtt_client);
        s_mqtt_client = NULL;
    }

    s_mqtt_connected = false;
    ESP_LOGI(TAG, "MQTT service stopped");
    return ESP_OK;
}

esp_err_t mqtt_service_update_status(const rover_status_t *status)
{
    if (!status) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_status_mutex && xSemaphoreTake(s_status_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        s_current_status = *status;
        xSemaphoreGive(s_status_mutex);
    }

    return ESP_OK;
}

bool mqtt_service_is_connected(void)
{
    return s_mqtt_connected;
}

#else // ENABLE_MQTT == 0

// Stub implementations when MQTT is disabled
esp_err_t mqtt_service_init(const mqtt_service_config_t *config)
{
    ESP_LOGI(TAG, "MQTT service disabled at compile time");
    return ESP_OK;
}

esp_err_t mqtt_service_stop(void)
{
    return ESP_OK;
}

esp_err_t mqtt_service_update_status(const rover_status_t *status)
{
    return ESP_OK;
}

bool mqtt_service_is_connected(void)
{
    return false;
}

#endif // ENABLE_MQTT
