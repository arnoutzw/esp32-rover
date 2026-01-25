#ifndef LOG_BUFFER_H
#define LOG_BUFFER_H

#include "esp_err.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Log buffer size (32KB default)
#ifndef LOG_BUFFER_SIZE
#define LOG_BUFFER_SIZE (32 * 1024)
#endif

// Maximum size of a single log entry (tag + message + header)
#define LOG_ENTRY_MAX_SIZE 512

// Log levels matching ESP-IDF (esp_log.h)
#define LOG_LEVEL_NONE    0
#define LOG_LEVEL_ERROR   1
#define LOG_LEVEL_WARN    2
#define LOG_LEVEL_INFO    3
#define LOG_LEVEL_DEBUG   4
#define LOG_LEVEL_VERBOSE 5

/**
 * @brief Log buffer statistics
 */
typedef struct {
    size_t entry_count;       // Number of log entries in buffer
    size_t bytes_used;        // Bytes currently used in buffer
    size_t bytes_dropped;     // Total bytes dropped due to buffer full
    size_t buffer_size;       // Total buffer size
} log_buffer_stats_t;

/**
 * @brief Initialize log buffer and install vprintf hook
 *
 * This should be called early in app_main() to capture boot logs.
 * Logs will continue to be output via UART as normal.
 *
 * @return ESP_OK on success, error code otherwise
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
 * @brief Get all logs as formatted text
 *
 * @param out_text      Pointer to receive allocated string (caller must free)
 * @param out_len       Pointer to receive string length
 * @param min_level     Minimum log level to include (1=Error, 2=Warn, 3=Info, etc.)
 * @return ESP_OK on success
 */
esp_err_t log_buffer_get_text(char **out_text, size_t *out_len, uint8_t min_level);

/**
 * @brief Get log buffer statistics
 *
 * @param stats         Pointer to receive statistics
 * @return ESP_OK on success
 */
esp_err_t log_buffer_get_stats(log_buffer_stats_t *stats);

/**
 * @brief Clear all buffered logs
 */
void log_buffer_clear(void);

/**
 * @brief Get the read position for SSE streaming
 *
 * This returns an opaque position marker that can be used with
 * log_buffer_read_next() for streaming new logs.
 *
 * @return Current read position
 */
size_t log_buffer_get_read_position(void);

/**
 * @brief Read the next log entry as JSON for SSE streaming
 *
 * @param position      Pointer to read position (updated on success)
 * @param json_out      Buffer to receive JSON string
 * @param max_len       Maximum length of json_out buffer
 * @param min_level     Minimum log level to include
 * @return true if an entry was read, false if no more entries
 */
bool log_buffer_read_next(size_t *position, char *json_out, size_t max_len, uint8_t min_level);

#ifdef __cplusplus
}
#endif

#endif // LOG_BUFFER_H
