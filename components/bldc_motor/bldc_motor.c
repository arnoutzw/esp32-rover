#include "bldc_motor.h"
#include "foc.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/ledc.h"

static const char *TAG = "BLDC_MOTOR";

#define PI 3.14159265358979323846f
#define TWO_PI (2.0f * PI)

// PID controller state
typedef struct {
    float kp, ki, kd;
    float output_ramp;
    float limit;
    float integral;
    float prev_error;
    float prev_output;
    int64_t prev_time;
} pid_state_t;

// Low pass filter state
typedef struct {
    float tf;
    float prev_output;
    int64_t prev_time;
} lpf_state_t;

// Internal motor structure
struct bldc_motor_dev_t {
    // Configuration
    gpio_num_t pin_in1, pin_in2, pin_in3, pin_en;
    uint8_t pole_pairs;
    float voltage_limit;
    float velocity_limit;
    motor_direction_t direction;

    // State
    motor_mode_t mode;
    bool enabled;

    // Encoder
    as5600_handle_t encoder;

    // Current state
    float current_angle;
    float current_velocity;
    float electrical_angle;

    // Targets
    float target_velocity;
    float target_angle;

    // Control
    pid_state_t velocity_pid;
    pid_state_t angle_pid;
    lpf_state_t velocity_lpf;

    // PWM channels
    ledc_channel_t pwm_channel_a;
    ledc_channel_t pwm_channel_b;
    ledc_channel_t pwm_channel_c;

    // Timing
    int64_t last_loop_time;

    // Calibration
    float zero_electrical_angle;
};

// PID controller
static float pid_compute(pid_state_t *pid, float error)
{
    int64_t now = esp_timer_get_time();
    float dt = (float)(now - pid->prev_time) / 1000000.0f;

    if (dt <= 0 || dt > 0.5f) {
        dt = 0.001f;  // Default to 1ms if invalid
    }

    // Proportional
    float p_term = pid->kp * error;

    // Integral with anti-windup
    pid->integral += pid->ki * error * dt;
    if (pid->integral > pid->limit) pid->integral = pid->limit;
    if (pid->integral < -pid->limit) pid->integral = -pid->limit;

    // Derivative
    float d_term = pid->kd * (error - pid->prev_error) / dt;

    // Output
    float output = p_term + pid->integral + d_term;

    // Output ramping
    if (pid->output_ramp > 0) {
        float max_change = pid->output_ramp * dt;
        float delta = output - pid->prev_output;
        if (delta > max_change) output = pid->prev_output + max_change;
        if (delta < -max_change) output = pid->prev_output - max_change;
    }

    // Limit output
    if (output > pid->limit) output = pid->limit;
    if (output < -pid->limit) output = -pid->limit;

    pid->prev_error = error;
    pid->prev_output = output;
    pid->prev_time = now;

    return output;
}

// Low pass filter
static float lpf_compute(lpf_state_t *lpf, float input)
{
    int64_t now = esp_timer_get_time();
    float dt = (float)(now - lpf->prev_time) / 1000000.0f;

    if (dt <= 0 || dt > 0.5f) {
        lpf->prev_output = input;
        lpf->prev_time = now;
        return input;
    }

    float alpha = lpf->tf / (lpf->tf + dt);
    float output = alpha * lpf->prev_output + (1.0f - alpha) * input;

    lpf->prev_output = output;
    lpf->prev_time = now;

    return output;
}

// Set PWM duty cycles
static void set_pwm_duty(bldc_motor_handle_t handle, float duty_a, float duty_b, float duty_c)
{
    uint32_t max_duty = (1 << LEDC_TIMER_10_BIT) - 1;

    ledc_set_duty(LEDC_LOW_SPEED_MODE, handle->pwm_channel_a, (uint32_t)(duty_a * max_duty));
    ledc_set_duty(LEDC_LOW_SPEED_MODE, handle->pwm_channel_b, (uint32_t)(duty_b * max_duty));
    ledc_set_duty(LEDC_LOW_SPEED_MODE, handle->pwm_channel_c, (uint32_t)(duty_c * max_duty));

    ledc_update_duty(LEDC_LOW_SPEED_MODE, handle->pwm_channel_a);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, handle->pwm_channel_b);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, handle->pwm_channel_c);
}

esp_err_t bldc_motor_init(const bldc_motor_config_t *config, bldc_motor_handle_t *handle)
{
    if (!config || !handle) {
        return ESP_ERR_INVALID_ARG;
    }

    // Allocate device structure
    struct bldc_motor_dev_t *dev = calloc(1, sizeof(struct bldc_motor_dev_t));
    if (!dev) {
        return ESP_ERR_NO_MEM;
    }

    // Store configuration
    dev->pin_in1 = config->pin_in1;
    dev->pin_in2 = config->pin_in2;
    dev->pin_in3 = config->pin_in3;
    dev->pin_en = config->pin_en;
    dev->pole_pairs = config->pole_pairs;
    dev->voltage_limit = config->voltage_limit;
    dev->velocity_limit = config->velocity_limit;
    dev->direction = config->direction;
    dev->encoder = config->encoder;
    dev->mode = MOTOR_MODE_DISABLED;

    // Initialize FOC
    foc_init();

    // Configure LEDC timer
    ledc_timer_config_t timer_conf = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .freq_hz = config->pwm_frequency,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    esp_err_t ret = ledc_timer_config(&timer_conf);
    if (ret != ESP_OK) {
        free(dev);
        return ret;
    }

    // Configure PWM channels
    dev->pwm_channel_a = LEDC_CHANNEL_0;
    dev->pwm_channel_b = LEDC_CHANNEL_1;
    dev->pwm_channel_c = LEDC_CHANNEL_2;

    ledc_channel_config_t channel_conf = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_sel = LEDC_TIMER_0,
        .intr_type = LEDC_INTR_DISABLE,
        .duty = 0,
        .hpoint = 0,
    };

    channel_conf.channel = dev->pwm_channel_a;
    channel_conf.gpio_num = config->pin_in1;
    ledc_channel_config(&channel_conf);

    channel_conf.channel = dev->pwm_channel_b;
    channel_conf.gpio_num = config->pin_in2;
    ledc_channel_config(&channel_conf);

    channel_conf.channel = dev->pwm_channel_c;
    channel_conf.gpio_num = config->pin_in3;
    ledc_channel_config(&channel_conf);

    // Configure enable pin
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << config->pin_en),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    gpio_set_level(config->pin_en, 0);  // Disabled initially

    // Initialize PID controllers
    dev->velocity_pid.kp = config->velocity_pid.kp;
    dev->velocity_pid.ki = config->velocity_pid.ki;
    dev->velocity_pid.kd = config->velocity_pid.kd;
    dev->velocity_pid.output_ramp = config->velocity_pid.output_ramp;
    dev->velocity_pid.limit = config->velocity_pid.limit;

    dev->angle_pid.kp = config->angle_pid.kp;
    dev->angle_pid.ki = config->angle_pid.ki;
    dev->angle_pid.kd = config->angle_pid.kd;
    dev->angle_pid.output_ramp = config->angle_pid.output_ramp;
    dev->angle_pid.limit = config->angle_pid.limit;

    // Initialize low pass filter
    dev->velocity_lpf.tf = config->velocity_lpf.tf;

    dev->last_loop_time = esp_timer_get_time();

    *handle = dev;
    ESP_LOGI(TAG, "BLDC motor initialized");
    return ESP_OK;
}

esp_err_t bldc_motor_deinit(bldc_motor_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }

    bldc_motor_disable(handle);
    free(handle);
    return ESP_OK;
}

esp_err_t bldc_motor_enable(bldc_motor_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }

    gpio_set_level(handle->pin_en, 1);
    handle->enabled = true;
    ESP_LOGI(TAG, "Motor enabled");
    return ESP_OK;
}

esp_err_t bldc_motor_disable(bldc_motor_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }

    gpio_set_level(handle->pin_en, 0);
    set_pwm_duty(handle, 0, 0, 0);
    handle->enabled = false;
    handle->mode = MOTOR_MODE_DISABLED;
    ESP_LOGI(TAG, "Motor disabled");
    return ESP_OK;
}

esp_err_t bldc_motor_set_mode(bldc_motor_handle_t handle, motor_mode_t mode)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }

    // Reset PID states when changing mode
    handle->velocity_pid.integral = 0;
    handle->velocity_pid.prev_error = 0;
    handle->angle_pid.integral = 0;
    handle->angle_pid.prev_error = 0;

    handle->mode = mode;
    return ESP_OK;
}

esp_err_t bldc_motor_set_velocity(bldc_motor_handle_t handle, float velocity)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }

    // Limit velocity
    if (velocity > handle->velocity_limit) velocity = handle->velocity_limit;
    if (velocity < -handle->velocity_limit) velocity = -handle->velocity_limit;

    handle->target_velocity = velocity * handle->direction;
    return ESP_OK;
}

esp_err_t bldc_motor_set_angle(bldc_motor_handle_t handle, float angle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }

    handle->target_angle = angle;
    return ESP_OK;
}

esp_err_t bldc_motor_set_voltage(bldc_motor_handle_t handle, float voltage, float angle_el)
{
    if (!handle || !handle->enabled) {
        return ESP_ERR_INVALID_STATE;
    }

    // Limit voltage
    if (voltage > handle->voltage_limit) voltage = handle->voltage_limit;
    if (voltage < -handle->voltage_limit) voltage = -handle->voltage_limit;

    // Generate PWM using sinusoidal modulation
    svpwm_output_t pwm;
    foc_sinusoidal_pwm(voltage, 0, angle_el, handle->voltage_limit, &pwm);
    set_pwm_duty(handle, pwm.duty_a, pwm.duty_b, pwm.duty_c);

    return ESP_OK;
}

esp_err_t bldc_motor_loop(bldc_motor_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }

    // Update encoder
    if (handle->encoder) {
        as5600_update(handle->encoder);

        float mechanical_angle;
        as5600_get_cumulative_angle(handle->encoder, &mechanical_angle);
        handle->current_angle = mechanical_angle;

        float raw_velocity;
        as5600_get_velocity(handle->encoder, &raw_velocity);
        handle->current_velocity = lpf_compute(&handle->velocity_lpf, raw_velocity);

        // Calculate electrical angle
        handle->electrical_angle = foc_electrical_angle(
            mechanical_angle + handle->zero_electrical_angle,
            handle->pole_pairs
        );
    }

    if (!handle->enabled || handle->mode == MOTOR_MODE_DISABLED) {
        set_pwm_duty(handle, 0, 0, 0);
        return ESP_OK;
    }

    float voltage_q = 0;

    switch (handle->mode) {
        case MOTOR_MODE_OPEN_LOOP: {
            // Open loop: increment electrical angle based on target velocity
            int64_t now = esp_timer_get_time();
            float dt = (float)(now - handle->last_loop_time) / 1000000.0f;
            handle->last_loop_time = now;

            handle->electrical_angle += handle->target_velocity * handle->pole_pairs * dt;
            handle->electrical_angle = foc_normalize_angle(handle->electrical_angle);

            voltage_q = handle->voltage_limit * 0.3f;  // 30% voltage for open loop
            break;
        }

        case MOTOR_MODE_VELOCITY: {
            // Velocity closed loop control
            float velocity_error = handle->target_velocity - handle->current_velocity;
            voltage_q = pid_compute(&handle->velocity_pid, velocity_error);
            break;
        }

        case MOTOR_MODE_ANGLE: {
            // Angle control with cascaded PID
            float angle_error = handle->target_angle - handle->current_angle;
            float velocity_target = pid_compute(&handle->angle_pid, angle_error);

            // Limit velocity target
            if (velocity_target > handle->velocity_limit) velocity_target = handle->velocity_limit;
            if (velocity_target < -handle->velocity_limit) velocity_target = -handle->velocity_limit;

            float velocity_error = velocity_target - handle->current_velocity;
            voltage_q = pid_compute(&handle->velocity_pid, velocity_error);
            break;
        }

        default:
            voltage_q = 0;
            break;
    }

    // Apply voltage
    svpwm_output_t pwm;
    foc_sinusoidal_pwm(voltage_q, 0, handle->electrical_angle, handle->voltage_limit, &pwm);
    set_pwm_duty(handle, pwm.duty_a, pwm.duty_b, pwm.duty_c);

    handle->last_loop_time = esp_timer_get_time();
    return ESP_OK;
}

esp_err_t bldc_motor_get_velocity(bldc_motor_handle_t handle, float *velocity)
{
    if (!handle || !velocity) {
        return ESP_ERR_INVALID_ARG;
    }
    *velocity = handle->current_velocity;
    return ESP_OK;
}

esp_err_t bldc_motor_get_angle(bldc_motor_handle_t handle, float *angle)
{
    if (!handle || !angle) {
        return ESP_ERR_INVALID_ARG;
    }
    *angle = handle->current_angle;
    return ESP_OK;
}

esp_err_t bldc_motor_emergency_stop(bldc_motor_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }

    handle->target_velocity = 0;
    handle->target_angle = handle->current_angle;
    set_pwm_duty(handle, 0, 0, 0);
    handle->mode = MOTOR_MODE_DISABLED;
    ESP_LOGW(TAG, "Emergency stop activated");
    return ESP_OK;
}

esp_err_t bldc_motor_calibrate(bldc_motor_handle_t handle)
{
    if (!handle || !handle->encoder) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Starting motor calibration...");

    // Enable motor temporarily
    bool was_enabled = handle->enabled;
    bldc_motor_enable(handle);

    // Apply voltage at electrical angle 0
    svpwm_output_t pwm;
    foc_sinusoidal_pwm(handle->voltage_limit * 0.4f, 0, 0, handle->voltage_limit, &pwm);
    set_pwm_duty(handle, pwm.duty_a, pwm.duty_b, pwm.duty_c);

    // Wait for motor to settle
    vTaskDelay(pdMS_TO_TICKS(1000));

    // Read encoder angle
    float mechanical_angle;
    as5600_update(handle->encoder);
    as5600_get_cumulative_angle(handle->encoder, &mechanical_angle);

    // Calculate zero electrical angle offset
    handle->zero_electrical_angle = -mechanical_angle * handle->pole_pairs;

    // Disable PWM
    set_pwm_duty(handle, 0, 0, 0);

    if (!was_enabled) {
        bldc_motor_disable(handle);
    }

    ESP_LOGI(TAG, "Calibration complete. Zero offset: %.2f rad", handle->zero_electrical_angle);
    return ESP_OK;
}

esp_err_t bldc_motor_set_pid(bldc_motor_handle_t handle, const pid_config_t *pid)
{
    if (!handle || !pid) {
        return ESP_ERR_INVALID_ARG;
    }

    handle->velocity_pid.kp = pid->kp;
    handle->velocity_pid.ki = pid->ki;
    handle->velocity_pid.kd = pid->kd;
    handle->velocity_pid.output_ramp = pid->output_ramp;
    handle->velocity_pid.limit = pid->limit;

    return ESP_OK;
}
