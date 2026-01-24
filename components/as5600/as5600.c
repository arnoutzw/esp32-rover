#include "as5600.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "AS5600";

#define PI 3.14159265358979323846f
#define TWO_PI (2.0f * PI)
#define AS5600_RESOLUTION 4096

// Internal device structure
struct as5600_dev_t {
    i2c_port_t i2c_port;
    uint8_t i2c_addr;

    // Position tracking
    uint16_t raw_angle;
    uint16_t prev_raw_angle;
    float cumulative_angle;
    int32_t full_rotations;

    // Velocity calculation
    float velocity;
    int64_t last_update_time;
    float angle_history[4];
    int history_index;
};

// I2C read helper
static esp_err_t as5600_read_reg(as5600_handle_t handle, uint8_t reg, uint8_t *data, size_t len)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (handle->i2c_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (handle->i2c_addr << 1) | I2C_MASTER_READ, true);
    if (len > 1) {
        i2c_master_read(cmd, data, len - 1, I2C_MASTER_ACK);
    }
    i2c_master_read_byte(cmd, data + len - 1, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(handle->i2c_port, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    return ret;
}

esp_err_t as5600_init(const as5600_config_t *config, as5600_handle_t *handle)
{
    if (!config || !handle) {
        return ESP_ERR_INVALID_ARG;
    }

    // Allocate device structure
    struct as5600_dev_t *dev = calloc(1, sizeof(struct as5600_dev_t));
    if (!dev) {
        return ESP_ERR_NO_MEM;
    }

    dev->i2c_port = config->i2c_port;
    dev->i2c_addr = config->i2c_addr;

    // Configure I2C
    i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = config->sda_pin,
        .scl_io_num = config->scl_pin,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = config->i2c_freq,
    };

    esp_err_t ret = i2c_param_config(config->i2c_port, &i2c_conf);
    if (ret != ESP_OK) {
        free(dev);
        return ret;
    }

    ret = i2c_driver_install(config->i2c_port, I2C_MODE_MASTER, 0, 0, 0);
    if (ret != ESP_OK) {
        free(dev);
        return ret;
    }

    // Verify device presence
    uint8_t status;
    ret = as5600_read_reg(dev, AS5600_REG_STATUS, &status, 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read AS5600 status");
        i2c_driver_delete(config->i2c_port);
        free(dev);
        return ret;
    }

    if (!(status & AS5600_STATUS_MD)) {
        ESP_LOGW(TAG, "Magnet not detected");
    }

    // Initialize position tracking
    uint16_t angle;
    ret = as5600_get_raw_angle(dev, &angle);
    if (ret == ESP_OK) {
        dev->raw_angle = angle;
        dev->prev_raw_angle = angle;
    }

    dev->last_update_time = esp_timer_get_time();

    *handle = dev;
    ESP_LOGI(TAG, "AS5600 initialized successfully");
    return ESP_OK;
}

esp_err_t as5600_deinit(as5600_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }

    i2c_driver_delete(handle->i2c_port);
    free(handle);
    return ESP_OK;
}

esp_err_t as5600_get_raw_angle(as5600_handle_t handle, uint16_t *angle)
{
    if (!handle || !angle) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t data[2];
    esp_err_t ret = as5600_read_reg(handle, AS5600_REG_RAW_ANGLE_H, data, 2);
    if (ret == ESP_OK) {
        *angle = ((uint16_t)data[0] << 8) | data[1];
        *angle &= 0x0FFF;  // 12-bit value
    }
    return ret;
}

esp_err_t as5600_get_angle_rad(as5600_handle_t handle, float *angle_rad)
{
    if (!handle || !angle_rad) {
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t raw;
    esp_err_t ret = as5600_get_raw_angle(handle, &raw);
    if (ret == ESP_OK) {
        *angle_rad = ((float)raw / AS5600_RESOLUTION) * TWO_PI;
    }
    return ret;
}

esp_err_t as5600_get_angle_deg(as5600_handle_t handle, float *angle_deg)
{
    if (!handle || !angle_deg) {
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t raw;
    esp_err_t ret = as5600_get_raw_angle(handle, &raw);
    if (ret == ESP_OK) {
        *angle_deg = ((float)raw / AS5600_RESOLUTION) * 360.0f;
    }
    return ret;
}

esp_err_t as5600_update(as5600_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t new_angle;
    esp_err_t ret = as5600_get_raw_angle(handle, &new_angle);
    if (ret != ESP_OK) {
        return ret;
    }

    // Calculate delta with wrap-around detection
    int32_t delta = (int32_t)new_angle - (int32_t)handle->prev_raw_angle;

    // Detect wrap-around (crosses 0/4095 boundary)
    if (delta > (AS5600_RESOLUTION / 2)) {
        delta -= AS5600_RESOLUTION;
        handle->full_rotations--;
    } else if (delta < -(AS5600_RESOLUTION / 2)) {
        delta += AS5600_RESOLUTION;
        handle->full_rotations++;
    }

    // Update cumulative angle
    handle->cumulative_angle += ((float)delta / AS5600_RESOLUTION) * TWO_PI;

    // Calculate velocity
    int64_t now = esp_timer_get_time();
    float dt = (float)(now - handle->last_update_time) / 1000000.0f;  // Convert to seconds

    if (dt > 0.0001f) {  // Avoid division by very small numbers
        float instant_velocity = ((float)delta / AS5600_RESOLUTION) * TWO_PI / dt;

        // Simple low-pass filter for velocity
        handle->angle_history[handle->history_index] = instant_velocity;
        handle->history_index = (handle->history_index + 1) % 4;

        // Average velocity from history
        handle->velocity = 0;
        for (int i = 0; i < 4; i++) {
            handle->velocity += handle->angle_history[i];
        }
        handle->velocity /= 4.0f;
    }

    handle->prev_raw_angle = new_angle;
    handle->raw_angle = new_angle;
    handle->last_update_time = now;

    return ESP_OK;
}

esp_err_t as5600_get_cumulative_angle(as5600_handle_t handle, float *angle_rad)
{
    if (!handle || !angle_rad) {
        return ESP_ERR_INVALID_ARG;
    }

    *angle_rad = handle->cumulative_angle;
    return ESP_OK;
}

esp_err_t as5600_get_velocity(as5600_handle_t handle, float *velocity)
{
    if (!handle || !velocity) {
        return ESP_ERR_INVALID_ARG;
    }

    *velocity = handle->velocity;
    return ESP_OK;
}

esp_err_t as5600_reset_position(as5600_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }

    handle->cumulative_angle = 0;
    handle->full_rotations = 0;
    handle->velocity = 0;

    for (int i = 0; i < 4; i++) {
        handle->angle_history[i] = 0;
    }

    return ESP_OK;
}

esp_err_t as5600_get_magnet_status(as5600_handle_t handle, bool *detected,
                                    bool *too_strong, bool *too_weak)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t status;
    esp_err_t ret = as5600_read_reg(handle, AS5600_REG_STATUS, &status, 1);
    if (ret == ESP_OK) {
        if (detected) *detected = (status & AS5600_STATUS_MD) != 0;
        if (too_strong) *too_strong = (status & AS5600_STATUS_MH) != 0;
        if (too_weak) *too_weak = (status & AS5600_STATUS_ML) != 0;
    }
    return ret;
}
