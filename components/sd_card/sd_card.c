/**
 * @file sd_card.c
 * @brief SD Card component implementation for ESP32-CAM
 *
 * REQ-40: SD Card Debug Mode
 * Implements SD card access using 1-bit SDMMC mode on ESP32-CAM.
 *
 * Pin Mapping (1-bit SDMMC mode):
 *   CLK   -> GPIO 14
 *   CMD   -> GPIO 15
 *   DATA0 -> GPIO 2
 *
 * Note: GPIO 12 and 13 (DATA2, DATA3) are freed in 1-bit mode.
 */

#include "sd_card.h"
#include "config.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include <string.h>
#include <sys/stat.h>
#include <sys/unistd.h>

static const char *TAG = "SD_CARD";

// Only compile full implementation when SD card is enabled
#if defined(ENABLE_SD_CARD) && ENABLE_SD_CARD && defined(ROVER_TARGET_ESP32CAM)

// Mount point for SD card
#define SD_MOUNT_POINT "/sdcard"

// SD card pin definitions (1-bit SDMMC mode)
#define SD_PIN_CLK      GPIO_NUM_14
#define SD_PIN_CMD      GPIO_NUM_15
#define SD_PIN_D0       GPIO_NUM_2

// Static state
static bool s_mounted = false;
static sdmmc_card_t *s_card = NULL;

esp_err_t sd_card_init(void)
{
    if (s_mounted) {
        ESP_LOGW(TAG, "SD card already mounted");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing SD card in 1-bit SDMMC mode...");

    // Configure SDMMC host
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.flags = SDMMC_HOST_FLAG_1BIT;  // Use 1-bit mode
    host.max_freq_khz = SDMMC_FREQ_DEFAULT;

    // Configure slot
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = 1;  // 1-bit mode

    // Set GPIO pins for 1-bit mode
    slot_config.clk = SD_PIN_CLK;
    slot_config.cmd = SD_PIN_CMD;
    slot_config.d0 = SD_PIN_D0;
    // d1, d2, d3 not used in 1-bit mode
    slot_config.d1 = GPIO_NUM_NC;
    slot_config.d2 = GPIO_NUM_NC;
    slot_config.d3 = GPIO_NUM_NC;

    // Enable internal pullups
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    ESP_LOGI(TAG, "SD card pins: CLK=%d, CMD=%d, D0=%d", SD_PIN_CLK, SD_PIN_CMD, SD_PIN_D0);

    // Mount filesystem
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    esp_err_t ret = esp_vfs_fat_sdmmc_mount(SD_MOUNT_POINT, &host, &slot_config, &mount_config, &s_card);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem. Make sure SD card is formatted with FAT.");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "No SD card found. Please insert a card.");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SD card: %s", esp_err_to_name(ret));
        }
        return ret;
    }

    s_mounted = true;

    // Log card info
    ESP_LOGI(TAG, "SD card mounted at %s", SD_MOUNT_POINT);
    ESP_LOGI(TAG, "  Name: %s", s_card->cid.name);
    ESP_LOGI(TAG, "  Type: %s", (s_card->ocr & SD_OCR_SDHC_CAP) ? "SDHC/SDXC" : "SDSC");
    ESP_LOGI(TAG, "  Speed: %s", (s_card->csd.tr_speed > 25000000) ? "high speed" : "default");
    ESP_LOGI(TAG, "  Size: %lluMB", ((uint64_t)s_card->csd.capacity) * s_card->csd.sector_size / (1024 * 1024));

    return ESP_OK;
}

esp_err_t sd_card_deinit(void)
{
    if (!s_mounted) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = esp_vfs_fat_sdcard_unmount(SD_MOUNT_POINT, s_card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to unmount SD card: %s", esp_err_to_name(ret));
        return ret;
    }

    s_mounted = false;
    s_card = NULL;
    ESP_LOGI(TAG, "SD card unmounted");

    return ESP_OK;
}

bool sd_card_is_mounted(void)
{
    return s_mounted;
}

const char* sd_card_get_mount_point(void)
{
    return s_mounted ? SD_MOUNT_POINT : NULL;
}

esp_err_t sd_card_get_status(sd_card_status_t *status)
{
    if (!status) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_mounted) {
        status->mounted = false;
        status->total_bytes = 0;
        status->used_bytes = 0;
        status->free_bytes = 0;
        return ESP_ERR_INVALID_STATE;
    }

    status->mounted = true;

    // Get filesystem info
    FATFS *fs;
    DWORD fre_clust;
    if (f_getfree("0:", &fre_clust, &fs) == FR_OK) {
        uint64_t total_sectors = (fs->n_fatent - 2) * fs->csize;
        uint64_t free_sectors = fre_clust * fs->csize;
        uint64_t sector_size = FF_MIN_SS;  // Typically 512 bytes

        status->total_bytes = total_sectors * sector_size;
        status->free_bytes = free_sectors * sector_size;
        status->used_bytes = status->total_bytes - status->free_bytes;
    } else {
        // Fallback to card info
        status->total_bytes = (uint64_t)s_card->csd.capacity * s_card->csd.sector_size;
        status->free_bytes = 0;
        status->used_bytes = 0;
    }

    return ESP_OK;
}

esp_err_t sd_card_write_file(const char *path, const void *data, size_t len, bool append)
{
    if (!s_mounted) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!path || !data) {
        return ESP_ERR_INVALID_ARG;
    }

    // Build full path
    char full_path[128];
    snprintf(full_path, sizeof(full_path), "%s/%s", SD_MOUNT_POINT, path);

    FILE *f = fopen(full_path, append ? "a" : "w");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open file for writing: %s", full_path);
        return ESP_FAIL;
    }

    size_t written = fwrite(data, 1, len, f);
    fclose(f);

    if (written != len) {
        ESP_LOGE(TAG, "Write incomplete: %zu of %zu bytes", written, len);
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t sd_card_read_file(const char *path, void *buffer, size_t max_len, size_t *bytes_read)
{
    if (!s_mounted) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!path || !buffer) {
        return ESP_ERR_INVALID_ARG;
    }

    // Build full path
    char full_path[128];
    snprintf(full_path, sizeof(full_path), "%s/%s", SD_MOUNT_POINT, path);

    FILE *f = fopen(full_path, "r");
    if (!f) {
        return ESP_ERR_NOT_FOUND;
    }

    size_t read_bytes = fread(buffer, 1, max_len, f);
    fclose(f);

    if (bytes_read) {
        *bytes_read = read_bytes;
    }

    return ESP_OK;
}

bool sd_card_file_exists(const char *path)
{
    if (!s_mounted || !path) {
        return false;
    }

    char full_path[128];
    snprintf(full_path, sizeof(full_path), "%s/%s", SD_MOUNT_POINT, path);

    struct stat st;
    return (stat(full_path, &st) == 0);
}

esp_err_t sd_card_delete_file(const char *path)
{
    if (!s_mounted) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!path) {
        return ESP_ERR_INVALID_ARG;
    }

    char full_path[128];
    snprintf(full_path, sizeof(full_path), "%s/%s", SD_MOUNT_POINT, path);

    if (unlink(full_path) != 0) {
        return ESP_ERR_NOT_FOUND;
    }

    return ESP_OK;
}

#else
// Stub implementations when SD card is not enabled

esp_err_t sd_card_init(void)
{
    ESP_LOGW(TAG, "SD card not available (SD_CARD_DEBUG not enabled or wrong target)");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t sd_card_deinit(void)
{
    return ESP_ERR_NOT_SUPPORTED;
}

bool sd_card_is_mounted(void)
{
    return false;
}

const char* sd_card_get_mount_point(void)
{
    return NULL;
}

esp_err_t sd_card_get_status(sd_card_status_t *status)
{
    if (status) {
        status->mounted = false;
        status->total_bytes = 0;
        status->used_bytes = 0;
        status->free_bytes = 0;
    }
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t sd_card_write_file(const char *path, const void *data, size_t len, bool append)
{
    (void)path;
    (void)data;
    (void)len;
    (void)append;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t sd_card_read_file(const char *path, void *buffer, size_t max_len, size_t *bytes_read)
{
    (void)path;
    (void)buffer;
    (void)max_len;
    (void)bytes_read;
    return ESP_ERR_NOT_SUPPORTED;
}

bool sd_card_file_exists(const char *path)
{
    (void)path;
    return false;
}

esp_err_t sd_card_delete_file(const char *path)
{
    (void)path;
    return ESP_ERR_NOT_SUPPORTED;
}

#endif // ENABLE_SD_CARD && ROVER_TARGET_ESP32CAM
