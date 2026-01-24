#include "servo_control.h"
#include <stdlib.h>
#include <math.h>
#include "esp_log.h"

static const char *TAG = "SERVO";

// Internal servo structure
struct servo_dev_t {
    gpio_num_t gpio_pin;
    ledc_channel_t pwm_channel;
    ledc_timer_t pwm_timer;
    uint16_t min_pulse_us;
    uint16_t max_pulse_us;
    uint16_t center_pulse_us;
    int16_t max_angle;
    int16_t trim_offset;
    float current_angle;
    uint32_t pwm_period_us;  // PWM period in microseconds
    bool enabled;
};

// Convert pulse width to duty cycle
static uint32_t pulse_to_duty(servo_handle_t handle, uint16_t pulse_us)
{
    // LEDC uses 14-bit resolution for better precision with servos
    // duty = (pulse_us / period_us) * max_duty
    uint32_t max_duty = (1 << 14) - 1;  // 14-bit resolution
    return (uint32_t)((float)pulse_us / handle->pwm_period_us * max_duty);
}

esp_err_t servo_init(const servo_config_t *config, servo_handle_t *handle)
{
    if (!config || !handle) {
        return ESP_ERR_INVALID_ARG;
    }

    // Allocate device structure
    struct servo_dev_t *dev = calloc(1, sizeof(struct servo_dev_t));
    if (!dev) {
        return ESP_ERR_NO_MEM;
    }

    // Store configuration
    dev->gpio_pin = config->gpio_pin;
    dev->pwm_channel = config->pwm_channel;
    dev->pwm_timer = config->pwm_timer;
    dev->min_pulse_us = config->min_pulse_us;
    dev->max_pulse_us = config->max_pulse_us;
    dev->center_pulse_us = config->center_pulse_us;
    dev->max_angle = config->max_angle;
    dev->trim_offset = config->trim_offset;
    dev->pwm_period_us = 1000000 / config->pwm_freq;  // Convert Hz to period in us

    // Configure LEDC timer for servo (50Hz typically)
    ledc_timer_config_t timer_conf = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = config->pwm_timer,
        .duty_resolution = LEDC_TIMER_14_BIT,  // 14-bit for better servo resolution
        .freq_hz = config->pwm_freq,
        .clk_cfg = LEDC_AUTO_CLK,
    };

    esp_err_t ret = ledc_timer_config(&timer_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure LEDC timer");
        free(dev);
        return ret;
    }

    // Configure LEDC channel
    ledc_channel_config_t channel_conf = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = config->pwm_channel,
        .timer_sel = config->pwm_timer,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = config->gpio_pin,
        .duty = pulse_to_duty(dev, config->center_pulse_us),
        .hpoint = 0,
    };

    ret = ledc_channel_config(&channel_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure LEDC channel");
        free(dev);
        return ret;
    }

    dev->enabled = true;
    dev->current_angle = 0;

    *handle = dev;
    ESP_LOGI(TAG, "Servo initialized on GPIO %d", config->gpio_pin);
    return ESP_OK;
}

esp_err_t servo_deinit(servo_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }

    ledc_stop(LEDC_LOW_SPEED_MODE, handle->pwm_channel, 0);
    free(handle);
    return ESP_OK;
}

esp_err_t servo_set_angle(servo_handle_t handle, float angle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }

    // Apply trim offset
    float adjusted_angle = angle + handle->trim_offset;

    // Clamp angle to max range
    if (adjusted_angle > handle->max_angle) adjusted_angle = handle->max_angle;
    if (adjusted_angle < -handle->max_angle) adjusted_angle = -handle->max_angle;

    // Calculate pulse width
    // Map angle (-max_angle to +max_angle) to pulse width (min_pulse to max_pulse)
    float angle_range = handle->max_angle * 2.0f;
    float pulse_range = handle->max_pulse_us - handle->min_pulse_us;

    float normalized = (adjusted_angle + handle->max_angle) / angle_range;  // 0 to 1
    uint16_t pulse_us = handle->min_pulse_us + (uint16_t)(normalized * pulse_range);

    // Set PWM duty
    uint32_t duty = pulse_to_duty(handle, pulse_us);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, handle->pwm_channel, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, handle->pwm_channel);

    handle->current_angle = angle;

    return ESP_OK;
}

esp_err_t servo_center(servo_handle_t handle)
{
    return servo_set_angle(handle, 0);
}

esp_err_t servo_set_pulse(servo_handle_t handle, uint16_t pulse_us)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }

    // Clamp pulse width
    if (pulse_us < handle->min_pulse_us) pulse_us = handle->min_pulse_us;
    if (pulse_us > handle->max_pulse_us) pulse_us = handle->max_pulse_us;

    // Set PWM duty
    uint32_t duty = pulse_to_duty(handle, pulse_us);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, handle->pwm_channel, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, handle->pwm_channel);

    // Calculate and store corresponding angle
    float pulse_range = handle->max_pulse_us - handle->min_pulse_us;
    float angle_range = handle->max_angle * 2.0f;
    float normalized = (float)(pulse_us - handle->min_pulse_us) / pulse_range;
    handle->current_angle = (normalized * angle_range) - handle->max_angle - handle->trim_offset;

    return ESP_OK;
}

esp_err_t servo_set_trim(servo_handle_t handle, int16_t trim_degrees)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }

    handle->trim_offset = trim_degrees;

    // Re-apply current angle with new trim
    return servo_set_angle(handle, handle->current_angle);
}

esp_err_t servo_get_angle(servo_handle_t handle, float *angle)
{
    if (!handle || !angle) {
        return ESP_ERR_INVALID_ARG;
    }

    *angle = handle->current_angle;
    return ESP_OK;
}

esp_err_t servo_enable(servo_handle_t handle, bool enable)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }

    if (enable && !handle->enabled) {
        // Re-enable by setting to current angle
        servo_set_angle(handle, handle->current_angle);
        handle->enabled = true;
    } else if (!enable && handle->enabled) {
        // Disable by setting duty to 0 (no pulse)
        ledc_set_duty(LEDC_LOW_SPEED_MODE, handle->pwm_channel, 0);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, handle->pwm_channel);
        handle->enabled = false;
    }

    return ESP_OK;
}
