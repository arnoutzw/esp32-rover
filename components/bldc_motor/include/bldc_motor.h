#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/gpio.h"
#include "as5600.h"

#ifdef __cplusplus
extern "C" {
#endif

// Motor control modes
typedef enum {
    MOTOR_MODE_DISABLED = 0,
    MOTOR_MODE_OPEN_LOOP,
    MOTOR_MODE_VELOCITY,
    MOTOR_MODE_ANGLE,
} motor_mode_t;

// Motor direction
typedef enum {
    MOTOR_DIR_CW = 1,
    MOTOR_DIR_CCW = -1,
} motor_direction_t;

// PID controller configuration
typedef struct {
    float kp;           // Proportional gain
    float ki;           // Integral gain
    float kd;           // Derivative gain
    float output_ramp;  // Maximum rate of change
    float limit;        // Output limit
} pid_config_t;

// Low pass filter configuration
typedef struct {
    float tf;           // Time constant
} lpf_config_t;

// Motor configuration
typedef struct {
    // PWM pins
    gpio_num_t pin_in1;
    gpio_num_t pin_in2;
    gpio_num_t pin_in3;
    gpio_num_t pin_en;

    // Motor parameters
    uint8_t pole_pairs;
    float voltage_limit;
    float velocity_limit;
    motor_direction_t direction;

    // PWM configuration
    uint32_t pwm_frequency;

    // Control parameters
    pid_config_t velocity_pid;
    pid_config_t angle_pid;
    lpf_config_t velocity_lpf;

    // Encoder handle
    as5600_handle_t encoder;
} bldc_motor_config_t;

// Motor handle type
typedef struct bldc_motor_dev_t* bldc_motor_handle_t;

/**
 * @brief Initialize BLDC motor
 * @param config Motor configuration
 * @param handle Pointer to store motor handle
 * @return ESP_OK on success
 */
esp_err_t bldc_motor_init(const bldc_motor_config_t *config, bldc_motor_handle_t *handle);

/**
 * @brief Deinitialize BLDC motor
 * @param handle Motor handle
 * @return ESP_OK on success
 */
esp_err_t bldc_motor_deinit(bldc_motor_handle_t handle);

/**
 * @brief Enable motor driver
 * @param handle Motor handle
 * @return ESP_OK on success
 */
esp_err_t bldc_motor_enable(bldc_motor_handle_t handle);

/**
 * @brief Disable motor driver
 * @param handle Motor handle
 * @return ESP_OK on success
 */
esp_err_t bldc_motor_disable(bldc_motor_handle_t handle);

/**
 * @brief Set motor control mode
 * @param handle Motor handle
 * @param mode Control mode
 * @return ESP_OK on success
 */
esp_err_t bldc_motor_set_mode(bldc_motor_handle_t handle, motor_mode_t mode);

/**
 * @brief Set target velocity (in velocity mode)
 * @param handle Motor handle
 * @param velocity Target velocity in rad/s
 * @return ESP_OK on success
 */
esp_err_t bldc_motor_set_velocity(bldc_motor_handle_t handle, float velocity);

/**
 * @brief Set target angle (in angle mode)
 * @param handle Motor handle
 * @param angle Target angle in radians
 * @return ESP_OK on success
 */
esp_err_t bldc_motor_set_angle(bldc_motor_handle_t handle, float angle);

/**
 * @brief Set voltage directly (open loop mode)
 * @param handle Motor handle
 * @param voltage Voltage amplitude
 * @param angle_el Electrical angle
 * @return ESP_OK on success
 */
esp_err_t bldc_motor_set_voltage(bldc_motor_handle_t handle, float voltage, float angle_el);

/**
 * @brief Run motor control loop (call at fixed frequency)
 * @param handle Motor handle
 * @return ESP_OK on success
 */
esp_err_t bldc_motor_loop(bldc_motor_handle_t handle);

/**
 * @brief Get current velocity
 * @param handle Motor handle
 * @param velocity Pointer to store velocity in rad/s
 * @return ESP_OK on success
 */
esp_err_t bldc_motor_get_velocity(bldc_motor_handle_t handle, float *velocity);

/**
 * @brief Get current electrical angle
 * @param handle Motor handle
 * @param angle Pointer to store angle in radians
 * @return ESP_OK on success
 */
esp_err_t bldc_motor_get_angle(bldc_motor_handle_t handle, float *angle);

/**
 * @brief Emergency stop
 * @param handle Motor handle
 * @return ESP_OK on success
 */
esp_err_t bldc_motor_emergency_stop(bldc_motor_handle_t handle);

/**
 * @brief Calibrate encoder zero position
 * @param handle Motor handle
 * @return ESP_OK on success
 */
esp_err_t bldc_motor_calibrate(bldc_motor_handle_t handle);

/**
 * @brief Update PID parameters
 * @param handle Motor handle
 * @param pid New PID configuration
 * @return ESP_OK on success
 */
esp_err_t bldc_motor_set_pid(bldc_motor_handle_t handle, const pid_config_t *pid);

#ifdef __cplusplus
}
#endif
