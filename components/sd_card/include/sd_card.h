/**
 * @file sd_card.h
 * @brief SD Card component for ESP32-CAM
 *
 * REQ-40: SD Card Debug Mode
 * Provides SD card access in 1-bit SDMMC mode on ESP32-CAM.
 * Note: SD card shares GPIO pins with motor control, so they are mutually exclusive.
 *
 * 1-bit Mode Pin Mapping:
 *   CLK  -> GPIO 14
 *   CMD  -> GPIO 15
 *   DATA0 -> GPIO 2
 *   (DATA1-DATA3 freed: GPIO 4, 12, 13)
 */

#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief SD card status structure
 */
typedef struct {
    bool mounted;           ///< True if SD card is mounted
    uint64_t total_bytes;   ///< Total capacity in bytes
    uint64_t used_bytes;    ///< Used space in bytes
    uint64_t free_bytes;    ///< Free space in bytes
} sd_card_status_t;

/**
 * @brief Initialize and mount SD card in 1-bit SDMMC mode
 *
 * Only available on ESP32-CAM when SD_CARD_DEBUG build flag is set.
 * Motor control must be disabled when using SD card.
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_NOT_SUPPORTED if not available on current target
 *      - ESP_ERR_NOT_FOUND if no SD card is inserted
 *      - ESP_FAIL on mount failure
 */
esp_err_t sd_card_init(void);

/**
 * @brief Unmount and deinitialize SD card
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_STATE if not mounted
 */
esp_err_t sd_card_deinit(void);

/**
 * @brief Check if SD card is mounted
 *
 * @return true if mounted, false otherwise
 */
bool sd_card_is_mounted(void);

/**
 * @brief Get the mount point path
 *
 * @return Mount point string (e.g., "/sdcard") or NULL if not mounted
 */
const char* sd_card_get_mount_point(void);

/**
 * @brief Get SD card status information
 *
 * @param[out] status Pointer to status structure to fill
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_STATE if not mounted
 *      - ESP_ERR_INVALID_ARG if status is NULL
 */
esp_err_t sd_card_get_status(sd_card_status_t *status);

/**
 * @brief Write data to a file on SD card
 *
 * @param path File path (relative to mount point)
 * @param data Data to write
 * @param len Length of data
 * @param append If true, append to file; if false, overwrite
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_STATE if not mounted
 *      - ESP_FAIL on write failure
 */
esp_err_t sd_card_write_file(const char *path, const void *data, size_t len, bool append);

/**
 * @brief Read data from a file on SD card
 *
 * @param path File path (relative to mount point)
 * @param buffer Buffer to read into
 * @param max_len Maximum bytes to read
 * @param[out] bytes_read Actual bytes read (can be NULL)
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_STATE if not mounted
 *      - ESP_ERR_NOT_FOUND if file doesn't exist
 *      - ESP_FAIL on read failure
 */
esp_err_t sd_card_read_file(const char *path, void *buffer, size_t max_len, size_t *bytes_read);

/**
 * @brief Check if a file exists on SD card
 *
 * @param path File path (relative to mount point)
 * @return true if file exists, false otherwise
 */
bool sd_card_file_exists(const char *path);

/**
 * @brief Delete a file from SD card
 *
 * @param path File path (relative to mount point)
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_STATE if not mounted
 *      - ESP_ERR_NOT_FOUND if file doesn't exist
 */
esp_err_t sd_card_delete_file(const char *path);

#ifdef __cplusplus
}
#endif
