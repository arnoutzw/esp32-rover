/**
 * @file lcd_display.h
 * @brief LCD Display driver for TTGO T-Display (ST7789 135x240)
 *
 * Provides a condensed status UI showing rover telemetry on the built-in LCD.
 */

#ifndef LCD_DISPLAY_H
#define LCD_DISPLAY_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief LCD display configuration
 */
typedef struct {
    int pin_sclk;       /**< SPI clock pin */
    int pin_mosi;       /**< SPI MOSI pin */
    int pin_dc;         /**< Data/Command pin */
    int pin_cs;         /**< Chip select pin */
    int pin_rst;        /**< Reset pin */
    int pin_backlight;  /**< Backlight control pin */
} lcd_display_config_t;

/**
 * @brief Rover status for display
 */
typedef struct {
    int speed_percent;      /**< Speed -100 to +100 */
    int steering_degrees;   /**< Steering angle in degrees */
    float battery_volts;    /**< Battery voltage */
    bool connected;         /**< WiFi client connected */
    bool estop;             /**< Emergency stop active */
    bool button_left;       /**< Left button pressed */
    bool button_right;      /**< Right button pressed */
    const char* wifi_ssid;  /**< WiFi SSID */
    const char* wifi_ip;    /**< IP address */
    const char* mac_addr;   /**< MAC address string */
    const char* mdns_hostname; /**< mDNS hostname (without .local) */
    uint32_t uptime_secs;   /**< Uptime in seconds */
} lcd_rover_status_t;

/**
 * @brief WiFi diagnostics for diagnostic screen
 */
typedef struct {
    const char* ssid;           /**< AP SSID */
    const char* ip_addr;        /**< IP address */
    const char* mac_addr;       /**< MAC address */
    const char* mdns_hostname;  /**< mDNS hostname (without .local) */
    uint8_t channel;            /**< WiFi channel */
    uint8_t connected_stations; /**< Number of connected stations */
    int8_t tx_power;            /**< TX power in dBm */
    uint32_t free_heap;         /**< Free heap memory */
    uint32_t min_free_heap;     /**< Minimum free heap since boot */
    uint32_t total_heap;        /**< Total heap memory */
    uint32_t free_internal;     /**< Free internal RAM */
    uint32_t free_psram;        /**< Free PSRAM (if available) */
    uint32_t uptime_secs;       /**< Uptime in seconds */
    float battery_volts;        /**< Battery voltage */
    float cpu_freq_mhz;         /**< CPU frequency */
    uint8_t task_count;         /**< Total number of running tasks */
    uint8_t tasks_core0;        /**< Tasks running on core 0 */
    uint8_t tasks_core1;        /**< Tasks running on core 1 */
    uint8_t tasks_no_affinity;  /**< Tasks with no core affinity */
    bool rest_api_enabled;      /**< REST API enabled */
    bool mqtt_enabled;          /**< MQTT enabled */
    bool mqtt_connected;        /**< MQTT broker connected */
    bool internet_connected;    /**< Internet connectivity (REQ-10) */
    const char* local_time;     /**< Local time string (REQ-13) */
    bool ntp_synced;            /**< NTP time synchronized (REQ-13) */
} lcd_wifi_diag_t;

/**
 * @brief Initialize the LCD display
 *
 * @param config Display pin configuration
 * @return ESP_OK on success
 */
esp_err_t lcd_display_init(const lcd_display_config_t *config);

/**
 * @brief Update the display with rover status
 *
 * @param status Current rover status
 * @return ESP_OK on success
 */
esp_err_t lcd_display_update(const lcd_rover_status_t *status);

/**
 * @brief Show startup splash screen
 *
 * @return ESP_OK on success
 */
esp_err_t lcd_display_splash(void);

/**
 * @brief Set display backlight brightness
 *
 * @param brightness 0-100 percent
 * @return ESP_OK on success
 */
esp_err_t lcd_display_set_backlight(uint8_t brightness);

/**
 * @brief Clear the display
 *
 * @return ESP_OK on success
 */
esp_err_t lcd_display_clear(void);

/**
 * @brief Show diagnostic screen with WiFi info
 *
 * @param diag WiFi diagnostic data
 * @return ESP_OK on success
 */
esp_err_t lcd_display_diagnostics(const lcd_wifi_diag_t *diag);

/**
 * @brief Reset display state (call when exiting diagnostic mode)
 */
void lcd_display_reset_state(void);

/**
 * @brief Show sleep screen with animated Snorlax
 *
 * Displays a pixelated Snorlax sprite with "Zzz..." animation
 * for entering deep sleep mode.
 *
 * @return ESP_OK on success
 */
esp_err_t lcd_display_sleep_screen(void);

#ifdef __cplusplus
}
#endif

#endif /* LCD_DISPLAY_H */
