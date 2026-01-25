/**
 * @file status_led.h
 * @brief Status LED indicator component for ESP32 Rover
 *
 * REQ-39: Status LED Indicator
 * Provides visual WiFi status indication via on-board LED (GPIO 33 on ESP32-CAM)
 * - Solid ON: Powered but no WiFi connection
 * - 1Hz blink: STA WiFi connection active
 * - 2Hz blink: AP mode active
 *
 * Note: LED uses inverted logic (LOW = on, HIGH = off)
 */

#pragma once

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief LED blink mode enumeration
 */
typedef enum {
    STATUS_LED_MODE_OFF,           ///< LED completely off
    STATUS_LED_MODE_SOLID,         ///< LED solid on (no connection)
    STATUS_LED_MODE_BLINK_1HZ,     ///< 1Hz blink (STA connected)
    STATUS_LED_MODE_BLINK_2HZ,     ///< 2Hz blink (AP mode)
} status_led_mode_t;

/**
 * @brief Initialize the status LED
 *
 * Configures the GPIO pin as output and sets initial state (LED on).
 * Only works on ESP32-CAM target where GPIO 33 is the on-board red LED.
 *
 * @return ESP_OK on success, ESP_FAIL if not supported on current target
 */
esp_err_t status_led_init(void);

/**
 * @brief Update the LED state based on WiFi mode and connection status
 *
 * This function should be called periodically (recommended: 20Hz from status task).
 * It handles the blink timing internally.
 *
 * @param is_sta_mode True if WiFi is in STA mode, false if AP mode
 * @param is_connected True if WiFi is connected (only relevant in STA mode)
 */
void status_led_update(bool is_sta_mode, bool is_connected);

/**
 * @brief Set the LED mode directly
 *
 * @param mode The desired LED mode
 */
void status_led_set_mode(status_led_mode_t mode);

/**
 * @brief Get current LED mode
 *
 * @return Current LED mode
 */
status_led_mode_t status_led_get_mode(void);

/**
 * @brief Turn LED on (for direct control)
 */
void status_led_on(void);

/**
 * @brief Turn LED off (for direct control)
 */
void status_led_off(void);

/**
 * @brief Toggle LED state
 */
void status_led_toggle(void);

#ifdef __cplusplus
}
#endif
