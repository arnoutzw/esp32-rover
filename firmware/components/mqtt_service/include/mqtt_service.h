#pragma once

#include "esp_err.h"
#include "web_server.h"  // For rover_status_t

/**
 * @brief MQTT service configuration
 */
typedef struct {
    const char *broker_host;    // MQTT broker hostname or IP
    uint16_t broker_port;       // MQTT broker port (default 1883)
    const char *username;       // MQTT username (can be NULL)
    const char *password;       // MQTT password (can be NULL)
    const char *client_id;      // MQTT client ID
    const char *topic_prefix;   // Topic prefix for all messages
    uint32_t publish_interval_ms; // Interval between status publishes
    int qos;                    // QoS level (0, 1, or 2)
} mqtt_service_config_t;

/**
 * @brief Initialize and start the MQTT service
 *
 * @param config Pointer to configuration structure
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t mqtt_service_init(const mqtt_service_config_t *config);

/**
 * @brief Stop the MQTT service
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t mqtt_service_stop(void);

/**
 * @brief Update the rover status for MQTT publishing
 *
 * @param status Pointer to current rover status
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t mqtt_service_update_status(const rover_status_t *status);

/**
 * @brief Check if MQTT service is connected
 *
 * @return true if connected, false otherwise
 */
bool mqtt_service_is_connected(void);

/**
 * @brief Set MQTT publish interval at runtime
 *
 * @param interval_ms New publish interval in milliseconds (1000-60000 ms)
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if interval out of range
 */
esp_err_t mqtt_service_set_publish_interval(uint32_t interval_ms);
