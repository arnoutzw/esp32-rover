#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/gpio.h"
#include "driver/ledc.h"

#ifdef __cplusplus
extern "C" {
#endif

// Servo configuration
typedef struct {
    gpio_num_t gpio_pin;
    ledc_channel_t pwm_channel;
    ledc_timer_t pwm_timer;
    uint32_t pwm_freq;          // Typically 50Hz for servos
    uint16_t min_pulse_us;      // Minimum pulse width in microseconds
    uint16_t max_pulse_us;      // Maximum pulse width in microseconds
    uint16_t center_pulse_us;   // Center pulse width in microseconds
    int16_t max_angle;          // Maximum angle in degrees (from center)
    int16_t trim_offset;        // Trim offset in degrees
} servo_config_t;

// Servo handle type
typedef struct servo_dev_t* servo_handle_t;

/**
 * @brief Initialize servo
 * @param config Servo configuration
 * @param handle Pointer to store servo handle
 * @return ESP_OK on success
 */
esp_err_t servo_init(const servo_config_t *config, servo_handle_t *handle);

/**
 * @brief Deinitialize servo
 * @param handle Servo handle
 * @return ESP_OK on success
 */
esp_err_t servo_deinit(servo_handle_t handle);

/**
 * @brief Set servo angle in degrees
 * @param handle Servo handle
 * @param angle Angle in degrees (negative = left, positive = right)
 * @return ESP_OK on success
 */
esp_err_t servo_set_angle(servo_handle_t handle, float angle);

/**
 * @brief Set servo to center position
 * @param handle Servo handle
 * @return ESP_OK on success
 */
esp_err_t servo_center(servo_handle_t handle);

/**
 * @brief Set servo pulse width directly
 * @param handle Servo handle
 * @param pulse_us Pulse width in microseconds
 * @return ESP_OK on success
 */
esp_err_t servo_set_pulse(servo_handle_t handle, uint16_t pulse_us);

/**
 * @brief Set trim offset
 * @param handle Servo handle
 * @param trim_degrees Trim offset in degrees
 * @return ESP_OK on success
 */
esp_err_t servo_set_trim(servo_handle_t handle, int16_t trim_degrees);

/**
 * @brief Get current servo angle
 * @param handle Servo handle
 * @param angle Pointer to store current angle
 * @return ESP_OK on success
 */
esp_err_t servo_get_angle(servo_handle_t handle, float *angle);

/**
 * @brief Enable/disable servo (sets PWM output)
 * @param handle Servo handle
 * @param enable true to enable, false to disable
 * @return ESP_OK on success
 */
esp_err_t servo_enable(servo_handle_t handle, bool enable);

#ifdef __cplusplus
}
#endif
