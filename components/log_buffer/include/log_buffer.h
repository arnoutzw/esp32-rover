// =============================================================================
// Log Buffer Component - Captures ESP_LOG output for web display
// =============================================================================
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Configuration
#define LOG_BUFFER_SIZE         (48 * 1024)   // 48KB ring buffer
#define LOG_ENTRY_MAX_SIZE      512           // Max single log entry
#define LOG_TAG_MAX_LEN         32            // Max tag length
#define LOG_ROTATION_PERIOD_MS  (2 * 60 * 60 * 1000)  // 2 hours

// Log level filter (bitmask)
typedef enum {
    LOG_FILTER_NONE    = 0,
    LOG_FILTER_ERROR   = (1 << 1),   // ESP_LOG_ERROR = 1
    LOG_FILTER_WARN    = (1 << 2),   // ESP_LOG_WARN = 2
    LOG_FILTER_INFO    = (1 << 3),   // ESP_LOG_INFO = 3
    LOG_FILTER_DEBUG   = (1 << 4),   // ESP_LOG_DEBUG = 4
    LOG_FILTER_VERBOSE = (1 << 5),   // ESP_LOG_VERBOSE = 5
    LOG_FILTER_ALL     = 0xFF
} log_filter_t;

// Statistics structure
typedef struct {
    uint32_t entry_count;       // Current entries in buffer
    uint32_t dropped_count;     // Entries dropped since init
    uint32_t buffer_size;       // Total buffer size
    uint32_t buffer_used;       // Bytes currently used
    uint32_t oldest_timestamp;  // Oldest entry timestamp (ms since boot)
} log_buffer_stats_t;

// Log entry for iteration
typedef struct {
    uint32_t timestamp_ms;      // Timestamp in ms since boot
    uint8_t level;              // Log level (1=E, 2=W, 3=I, 4=D, 5=V)
    const char *tag;            // Tag string (pointer to internal buffer)
    const char *message;        // Message string (pointer to internal buffer)
} log_entry_t;

/**
 * @brief Initialize log buffer and install vprintf hook
 *
 * Must be called early in app_main() before other logging occurs.
 * All subsequent ESP_LOGx calls will be captured to the ring buffer
 * while still being output to UART.
 *
 * @return ESP_OK on success, ESP_ERR_NO_MEM if buffer allocation fails
 */
esp_err_t log_buffer_init(void);

/**
 * @brief Deinitialize log buffer and restore original vprintf
 */
void log_buffer_deinit(void);

/**
 * @brief Check if log buffer is initialized
 * @return true if initialized
 */
bool log_buffer_is_initialized(void);

/**
 * @brief Get logs as JSON array string
 *
 * Returns a JSON object containing logs array and stats.
 * Format: {"logs":[{"t":123,"l":"I","tag":"X","msg":"Y"}],"stats":{...}}
 *
 * @param filter Bitmask of log levels to include (LOG_FILTER_ALL for all)
 * @param tag_filter Optional tag substring filter (NULL for all)
 * @param since_timestamp Only logs after this timestamp (0 for all)
 * @param out_json Output buffer for JSON string (caller must free with free())
 * @param max_entries Maximum entries to return (0 for all)
 * @return ESP_OK on success, ESP_ERR_NO_MEM if allocation fails
 */
esp_err_t log_buffer_get_json(
    log_filter_t filter,
    const char *tag_filter,
    uint32_t since_timestamp,
    char **out_json,
    size_t max_entries
);

/**
 * @brief Get new logs since last timestamp as JSON
 *
 * Efficient for polling - only returns logs newer than since_timestamp.
 * Updates out_last_timestamp to the newest log's timestamp for next call.
 *
 * @param filter Bitmask of log levels to include
 * @param since_timestamp Only logs after this timestamp
 * @param out_json Output buffer for JSON string (caller must free)
 * @param out_last_timestamp Updated with timestamp of newest log returned
 * @return ESP_OK on success
 */
esp_err_t log_buffer_get_new_logs(
    log_filter_t filter,
    uint32_t since_timestamp,
    char **out_json,
    uint32_t *out_last_timestamp
);

/**
 * @brief Get buffer statistics
 * @param stats Output structure
 * @return ESP_OK on success
 */
esp_err_t log_buffer_get_stats(log_buffer_stats_t *stats);

/**
 * @brief Clear all logs from buffer
 */
void log_buffer_clear(void);

/**
 * @brief Force rotation check (remove entries older than 2 hours)
 *
 * Called automatically, but can be forced manually.
 */
void log_buffer_rotate(void);

/**
 * @brief Convert log level number to character
 * @param level Log level (1-5)
 * @return Character: 'E', 'W', 'I', 'D', 'V', or '?'
 */
char log_buffer_level_to_char(uint8_t level);

/**
 * @brief Convert level string to filter bitmask
 * @param level_str "error", "warn", "info", "debug", "verbose", or "all"
 * @return Bitmask including requested level and all more severe levels
 */
log_filter_t log_buffer_parse_level_filter(const char *level_str);

#ifdef __cplusplus
}
#endif
