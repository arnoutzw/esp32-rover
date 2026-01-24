#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_camera.h"

#ifdef __cplusplus
extern "C" {
#endif

// Camera configuration for ESP32-CAM AI Thinker
typedef struct {
    // Pin configuration
    int pin_pwdn;
    int pin_reset;
    int pin_xclk;
    int pin_sccb_sda;
    int pin_sccb_scl;
    int pin_d7;
    int pin_d6;
    int pin_d5;
    int pin_d4;
    int pin_d3;
    int pin_d2;
    int pin_d1;
    int pin_d0;
    int pin_vsync;
    int pin_href;
    int pin_pclk;

    // Camera settings
    uint32_t xclk_freq;
    pixformat_t pixel_format;
    framesize_t frame_size;
    int jpeg_quality;
    int fb_count;
} camera_config_params_t;

/**
 * @brief Initialize camera
 * @param config Camera configuration
 * @return ESP_OK on success
 */
esp_err_t camera_module_init(const camera_config_params_t *config);

/**
 * @brief Deinitialize camera
 * @return ESP_OK on success
 */
esp_err_t camera_module_deinit(void);

/**
 * @brief Capture a single frame
 * @return Pointer to camera frame buffer, NULL on error
 */
camera_fb_t* camera_capture_frame(void);

/**
 * @brief Return frame buffer after use
 * @param fb Frame buffer to return
 */
void camera_return_frame(camera_fb_t *fb);

/**
 * @brief Set camera resolution
 * @param frame_size New frame size
 * @return ESP_OK on success
 */
esp_err_t camera_set_resolution(framesize_t frame_size);

/**
 * @brief Set JPEG quality
 * @param quality Quality value (0-63, lower is better)
 * @return ESP_OK on success
 */
esp_err_t camera_set_quality(int quality);

/**
 * @brief Set vertical flip
 * @param flip Enable/disable vertical flip
 * @return ESP_OK on success
 */
esp_err_t camera_set_vflip(bool flip);

/**
 * @brief Set horizontal mirror
 * @param mirror Enable/disable horizontal mirror
 * @return ESP_OK on success
 */
esp_err_t camera_set_hmirror(bool mirror);

/**
 * @brief Check if camera is initialized
 * @return true if initialized
 */
bool camera_is_initialized(void);

/**
 * @brief Get current frame size
 * @return Current frame size enum
 */
framesize_t camera_get_frame_size(void);

// =============================================================================
// Flash LED Control (ESP32-CAM GPIO 4)
// =============================================================================

/**
 * @brief Initialize the flash LED GPIO
 * @return ESP_OK on success
 */
esp_err_t flash_led_init(void);

/**
 * @brief Turn flash LED on
 */
void flash_led_on(void);

/**
 * @brief Turn flash LED off
 */
void flash_led_off(void);

/**
 * @brief Set flash LED state
 * @param on true to turn on, false to turn off
 */
void flash_led_set(bool on);

/**
 * @brief Get flash LED state
 * @return true if LED is on
 */
bool flash_led_get_state(void);

/**
 * @brief Blink flash LED
 * @param count Number of blinks
 * @param on_ms On time in milliseconds
 * @param off_ms Off time in milliseconds
 */
void flash_led_blink(int count, int on_ms, int off_ms);

#ifdef __cplusplus
}
#endif
