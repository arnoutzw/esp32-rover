#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

// AS5600 Register addresses
#define AS5600_REG_ZMCO         0x00
#define AS5600_REG_ZPOS_H       0x01
#define AS5600_REG_ZPOS_L       0x02
#define AS5600_REG_MPOS_H       0x03
#define AS5600_REG_MPOS_L       0x04
#define AS5600_REG_MANG_H       0x05
#define AS5600_REG_MANG_L       0x06
#define AS5600_REG_CONF_H       0x07
#define AS5600_REG_CONF_L       0x08
#define AS5600_REG_RAW_ANGLE_H  0x0C
#define AS5600_REG_RAW_ANGLE_L  0x0D
#define AS5600_REG_ANGLE_H      0x0E
#define AS5600_REG_ANGLE_L      0x0F
#define AS5600_REG_STATUS       0x0B
#define AS5600_REG_AGC          0x1A
#define AS5600_REG_MAGNITUDE_H  0x1B
#define AS5600_REG_MAGNITUDE_L  0x1C

// Status bits
#define AS5600_STATUS_MH        0x08    // Magnet too strong
#define AS5600_STATUS_ML        0x10    // Magnet too weak
#define AS5600_STATUS_MD        0x20    // Magnet detected

// Configuration structure
typedef struct {
    i2c_port_t i2c_port;
    gpio_num_t sda_pin;
    gpio_num_t scl_pin;
    uint32_t i2c_freq;
    uint8_t i2c_addr;
} as5600_config_t;

// Handle type
typedef struct as5600_dev_t* as5600_handle_t;

/**
 * @brief Initialize AS5600 encoder
 * @param config Configuration parameters
 * @param handle Pointer to store device handle
 * @return ESP_OK on success
 */
esp_err_t as5600_init(const as5600_config_t *config, as5600_handle_t *handle);

/**
 * @brief Deinitialize AS5600 encoder
 * @param handle Device handle
 * @return ESP_OK on success
 */
esp_err_t as5600_deinit(as5600_handle_t handle);

/**
 * @brief Get raw angle (0-4095)
 * @param handle Device handle
 * @param angle Pointer to store raw angle
 * @return ESP_OK on success
 */
esp_err_t as5600_get_raw_angle(as5600_handle_t handle, uint16_t *angle);

/**
 * @brief Get angle in radians (0 to 2*PI)
 * @param handle Device handle
 * @param angle_rad Pointer to store angle in radians
 * @return ESP_OK on success
 */
esp_err_t as5600_get_angle_rad(as5600_handle_t handle, float *angle_rad);

/**
 * @brief Get angle in degrees (0 to 360)
 * @param handle Device handle
 * @param angle_deg Pointer to store angle in degrees
 * @return ESP_OK on success
 */
esp_err_t as5600_get_angle_deg(as5600_handle_t handle, float *angle_deg);

/**
 * @brief Get cumulative angle (tracks full rotations)
 * @param handle Device handle
 * @param angle_rad Pointer to store cumulative angle in radians
 * @return ESP_OK on success
 */
esp_err_t as5600_get_cumulative_angle(as5600_handle_t handle, float *angle_rad);

/**
 * @brief Get velocity in radians per second
 * @param handle Device handle
 * @param velocity Pointer to store velocity
 * @return ESP_OK on success
 */
esp_err_t as5600_get_velocity(as5600_handle_t handle, float *velocity);

/**
 * @brief Reset cumulative angle counter
 * @param handle Device handle
 * @return ESP_OK on success
 */
esp_err_t as5600_reset_position(as5600_handle_t handle);

/**
 * @brief Check magnet status
 * @param handle Device handle
 * @param detected Pointer to store magnet detected flag
 * @param too_strong Pointer to store magnet too strong flag
 * @param too_weak Pointer to store magnet too weak flag
 * @return ESP_OK on success
 */
esp_err_t as5600_get_magnet_status(as5600_handle_t handle, bool *detected,
                                    bool *too_strong, bool *too_weak);

/**
 * @brief Update encoder state (call periodically for velocity calculation)
 * @param handle Device handle
 * @return ESP_OK on success
 */
esp_err_t as5600_update(as5600_handle_t handle);

#ifdef __cplusplus
}
#endif
