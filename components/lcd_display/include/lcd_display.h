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
    float velocity_rads;    /**< Actual velocity in rad/s */
    float battery_volts;    /**< Battery voltage */
    bool connected;         /**< WiFi client connected */
    bool estop;             /**< Emergency stop active */
    const char* wifi_ssid;  /**< WiFi SSID */
    const char* wifi_ip;    /**< IP address */
} lcd_rover_status_t;

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

#ifdef __cplusplus
}
#endif

#endif /* LCD_DISPLAY_H */
