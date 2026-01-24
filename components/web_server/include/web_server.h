#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_http_server.h"

#ifdef __cplusplus
extern "C" {
#endif

// Control command structure
typedef struct {
    float speed;        // -100 to 100 (percentage)
    float steering;     // -100 to 100 (percentage, negative = left, positive = right)
    bool emergency_stop;
} rover_command_t;

// Status structure
typedef struct {
    float battery_voltage;
    float motor_velocity;
    float steering_angle;
    bool motor_enabled;
    bool camera_active;
    int wifi_rssi;
    bool button_left;
    bool button_right;
} rover_status_t;

// Command callback type
typedef void (*command_callback_t)(const rover_command_t *cmd);

// Web server configuration
typedef struct {
    uint16_t port;
    command_callback_t on_command;
} web_server_config_t;

/**
 * @brief Initialize web server
 * @param config Server configuration
 * @return ESP_OK on success
 */
esp_err_t web_server_init(const web_server_config_t *config);

/**
 * @brief Stop web server
 * @return ESP_OK on success
 */
esp_err_t web_server_stop(void);

/**
 * @brief Update rover status (for status endpoint)
 * @param status Current status
 * @return ESP_OK on success
 */
esp_err_t web_server_update_status(const rover_status_t *status);

/**
 * @brief Get last received command
 * @param cmd Pointer to store command
 * @return ESP_OK if command available
 */
esp_err_t web_server_get_last_command(rover_command_t *cmd);

/**
 * @brief Check if server is running
 * @return true if running
 */
bool web_server_is_running(void);

/**
 * @brief Get time since last command in milliseconds
 * @return Time in milliseconds
 */
uint32_t web_server_get_command_age_ms(void);

// HTML content (implemented in web_ui.c)
extern const char* web_ui_get_html(void);

#ifdef __cplusplus
}
#endif
