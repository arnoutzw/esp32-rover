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
    bool camera_active;
    int wifi_rssi;
    bool button_left;
    bool button_right;

    // Diagnostic data (mirrors LCD diagnostics)
    const char* wifi_ssid;          // AP SSID
    const char* wifi_ip;            // IP address
    const char* mac_addr;           // MAC address
    uint8_t wifi_channel;           // WiFi channel
    uint8_t wifi_mode;              // WiFi mode: 1=STA, 2=AP, 3=APSTA
    uint8_t connected_clients;      // Number of connected AP stations (only valid in AP mode)
    int8_t wifi_tx_power;           // TX power in dBm
    uint32_t free_heap;             // Free heap memory
    uint32_t min_free_heap;         // Minimum free heap since boot
    uint32_t total_heap;            // Total heap memory
    uint32_t free_internal;         // Free internal RAM
    uint32_t uptime_secs;           // Uptime in seconds
    float cpu_freq_mhz;             // CPU frequency
    uint8_t task_count;             // Number of running tasks
    uint8_t tasks_core0;            // Tasks running on core 0
    uint8_t tasks_core1;            // Tasks running on core 1
    uint8_t tasks_no_affinity;      // Tasks with no core affinity

    // Service status
    bool rest_api_enabled;          // REST API enabled at compile time
    bool mqtt_enabled;              // MQTT enabled at compile time
    bool mqtt_connected;            // MQTT broker connected
    bool internet_connected;        // Internet connectivity (REQ-10)

    // Time (REQ-13)
    const char* local_time;         // Local time string (HH:MM:SS)
    bool ntp_synced;                // NTP time synchronized

    // Build information
    const char* build_version;      // Project version (from git tags, e.g., "2.0.0")
    const char* build_fingerprint;  // Git commit hash (short)
    const char* build_time;         // Build timestamp (ISO 8601)
    const char* build_branch;       // Git branch name
    bool build_dirty;               // Working directory had uncommitted changes
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

/**
 * @brief Get HTTP server handle for adding custom endpoints
 * @return Server handle or NULL if not running
 */
httpd_handle_t web_server_get_handle(void);

// HTML content (implemented in web_ui.c)
extern const char* web_ui_get_html(void);

#ifdef __cplusplus
}
#endif
