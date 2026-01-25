#include "log_buffer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

static const char *TAG = "LOG_BUFFER";

// =============================================================================
// Stub implementations when log buffer is disabled
// =============================================================================
#if !defined(ENABLE_LOG_BUFFER) || !ENABLE_LOG_BUFFER

esp_err_t log_buffer_init(void) {
    ESP_LOGI(TAG, "Log buffer disabled (ENABLE_LOG_BUFFER=0)");
    return ESP_OK;
}

void log_buffer_deinit(void) {}

bool log_buffer_is_initialized(void) {
    return false;
}

esp_err_t log_buffer_get_text(char **out_text, size_t *out_len, uint8_t min_level) {
    (void)min_level;
    if (out_text) *out_text = NULL;
    if (out_len) *out_len = 0;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t log_buffer_get_stats(log_buffer_stats_t *stats) {
    if (stats) memset(stats, 0, sizeof(*stats));
    return ESP_OK;
}

void log_buffer_clear(void) {}

size_t log_buffer_get_read_position(void) {
    return 0;
}

bool log_buffer_read_next(size_t *position, char *json_out, size_t max_len, uint8_t min_level) {
    (void)position;
    (void)json_out;
    (void)max_len;
    (void)min_level;
    return false;
}

#else // ENABLE_LOG_BUFFER is enabled

// Log entry header stored in ring buffer
typedef struct __attribute__((packed)) {
    uint32_t timestamp_ms;
    uint8_t  level;
    uint8_t  tag_len;
    uint16_t msg_len;
    // Followed by: tag bytes (tag_len) + message bytes (msg_len)
} log_entry_header_t;

// Ring buffer state
typedef struct {
    uint8_t *buffer;
    size_t size;
    size_t head;           // Write position
    size_t tail;           // Read position (oldest entry)
    size_t count;          // Number of entries
    size_t bytes_used;
    size_t bytes_dropped;
    uint32_t write_seq;    // Monotonic sequence number for stream tracking
    bool initialized;
    SemaphoreHandle_t mutex;
    vprintf_like_t original_vprintf;
} log_buffer_state_t;

static log_buffer_state_t state = {0};

// Forward declaration
static int log_vprintf_hook(const char *fmt, va_list args);

// Level character to level number
static uint8_t char_to_level(char c) {
    switch (c) {
        case 'E': return LOG_LEVEL_ERROR;
        case 'W': return LOG_LEVEL_WARN;
        case 'I': return LOG_LEVEL_INFO;
        case 'D': return LOG_LEVEL_DEBUG;
        case 'V': return LOG_LEVEL_VERBOSE;
        default:  return LOG_LEVEL_INFO;
    }
}

// Level number to character
static char level_to_char(uint8_t level) {
    switch (level) {
        case LOG_LEVEL_ERROR:   return 'E';
        case LOG_LEVEL_WARN:    return 'W';
        case LOG_LEVEL_INFO:    return 'I';
        case LOG_LEVEL_DEBUG:   return 'D';
        case LOG_LEVEL_VERBOSE: return 'V';
        default:                return '?';
    }
}

// Remove oldest entry to make room
static void discard_oldest_entry(void) {
    if (state.count == 0) return;

    log_entry_header_t header;
    memcpy(&header, state.buffer + state.tail, sizeof(header));

    size_t entry_size = sizeof(header) + header.tag_len + header.msg_len;
    state.tail = (state.tail + entry_size) % state.size;
    state.count--;
    state.bytes_used -= entry_size;
}

// Write data to ring buffer (handles wrap-around)
static void ring_write(size_t pos, const void *data, size_t len) {
    const uint8_t *src = (const uint8_t *)data;
    size_t first_part = state.size - pos;

    if (first_part >= len) {
        memcpy(state.buffer + pos, src, len);
    } else {
        memcpy(state.buffer + pos, src, first_part);
        memcpy(state.buffer, src + first_part, len - first_part);
    }
}

// Read data from ring buffer (handles wrap-around)
static void ring_read(size_t pos, void *data, size_t len) {
    uint8_t *dst = (uint8_t *)data;
    size_t first_part = state.size - pos;

    if (first_part >= len) {
        memcpy(dst, state.buffer + pos, len);
    } else {
        memcpy(dst, state.buffer + pos, first_part);
        memcpy(dst + first_part, state.buffer, len - first_part);
    }
}

// Add log entry to buffer
static void add_log_entry(uint32_t timestamp, uint8_t level,
                          const char *tag, size_t tag_len,
                          const char *msg, size_t msg_len) {
    if (!state.initialized) return;

    // Limit sizes
    if (tag_len > 31) tag_len = 31;
    if (msg_len > LOG_ENTRY_MAX_SIZE - sizeof(log_entry_header_t) - 32) {
        msg_len = LOG_ENTRY_MAX_SIZE - sizeof(log_entry_header_t) - 32;
    }

    size_t entry_size = sizeof(log_entry_header_t) + tag_len + msg_len;

    if (xSemaphoreTake(state.mutex, pdMS_TO_TICKS(10)) != pdTRUE) {
        return;  // Skip if can't get mutex quickly
    }

    // Make room if needed
    while (state.bytes_used + entry_size > state.size - 64) {
        if (state.count == 0) {
            state.bytes_dropped += entry_size;
            xSemaphoreGive(state.mutex);
            return;  // Buffer too small for this entry
        }
        discard_oldest_entry();
        state.bytes_dropped += entry_size;
    }

    // Write header
    log_entry_header_t header = {
        .timestamp_ms = timestamp,
        .level = level,
        .tag_len = (uint8_t)tag_len,
        .msg_len = (uint16_t)msg_len
    };

    ring_write(state.head, &header, sizeof(header));
    size_t pos = (state.head + sizeof(header)) % state.size;

    // Write tag
    ring_write(pos, tag, tag_len);
    pos = (pos + tag_len) % state.size;

    // Write message
    ring_write(pos, msg, msg_len);

    state.head = (state.head + entry_size) % state.size;
    state.count++;
    state.bytes_used += entry_size;
    state.write_seq++;  // Increment sequence number for each new entry

    xSemaphoreGive(state.mutex);
}

// Parse ESP-IDF log format: "[color]X (timestamp) tag: message[/color]\n"
static int log_vprintf_hook(const char *fmt, va_list args) {
    // First, pass through to original vprintf for UART output
    int ret = 0;
    if (state.original_vprintf) {
        va_list args_copy;
        va_copy(args_copy, args);
        ret = state.original_vprintf(fmt, args_copy);
        va_end(args_copy);
    }

    // Now capture the log
    if (!state.initialized) return ret;

    // Format the log message
    char buf[LOG_ENTRY_MAX_SIZE];
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    if (len <= 0 || len >= (int)sizeof(buf)) return ret;

    // Parse ESP-IDF log format, which may include ANSI color codes
    // Examples:
    //   "I (12345) MAIN: Hello world\n"  (no color)
    //   "\033[0;32mI (12345) MAIN: Hello world\033[0m\n" (with color)
    if (len < 10) return ret;  // Too short

    // Skip ANSI escape sequences at the start (e.g., \033[0;32m)
    char *p = buf;
    while (*p == '\033') {
        // Skip escape sequence: ESC [ ... m
        p++;
        if (*p == '[') {
            p++;
            while (*p && *p != 'm') p++;
            if (*p == 'm') p++;
        }
    }

    // Check for level character
    char level_char = *p;
    if (level_char != 'E' && level_char != 'W' && level_char != 'I' &&
        level_char != 'D' && level_char != 'V') {
        return ret;  // Not a standard ESP-IDF log
    }

    // Find the timestamp in parentheses
    if (*(p+1) != ' ' || *(p+2) != '(') return ret;

    char *ts_end = strchr(p + 3, ')');
    if (!ts_end) return ret;

    // Parse timestamp
    uint32_t timestamp = 0;
    char *ts_ptr = p + 3;
    while (ts_ptr < ts_end && *ts_ptr >= '0' && *ts_ptr <= '9') {
        timestamp = timestamp * 10 + (*ts_ptr - '0');
        ts_ptr++;
    }

    // Find tag (after ") " and before ": ")
    char *tag_start = ts_end + 2;  // Skip ") "
    char *tag_end = strstr(tag_start, ": ");
    if (!tag_end) return ret;

    size_t tag_len = tag_end - tag_start;

    // Message is after ": "
    char *msg_start = tag_end + 2;

    // Find end of message (before trailing ANSI codes or newlines)
    char *msg_end = msg_start;
    while (*msg_end && *msg_end != '\033' && *msg_end != '\n' && *msg_end != '\r') {
        msg_end++;
    }
    size_t msg_len = msg_end - msg_start;

    // Add to buffer
    add_log_entry(timestamp, char_to_level(level_char), tag_start, tag_len, msg_start, msg_len);

    return ret;
}

esp_err_t log_buffer_init(void) {
    if (state.initialized) {
        return ESP_OK;
    }

    // Allocate buffer
    state.buffer = malloc(LOG_BUFFER_SIZE);
    if (!state.buffer) {
        ESP_LOGE(TAG, "Failed to allocate log buffer");
        return ESP_ERR_NO_MEM;
    }

    state.size = LOG_BUFFER_SIZE;
    state.head = 0;
    state.tail = 0;
    state.count = 0;
    state.bytes_used = 0;
    state.bytes_dropped = 0;
    state.write_seq = 0;

    // Create mutex
    state.mutex = xSemaphoreCreateMutex();
    if (!state.mutex) {
        free(state.buffer);
        state.buffer = NULL;
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }

    state.initialized = true;

    // Install vprintf hook
    state.original_vprintf = esp_log_set_vprintf(log_vprintf_hook);

    ESP_LOGI(TAG, "Log buffer initialized (%d KB)", LOG_BUFFER_SIZE / 1024);
    return ESP_OK;
}

void log_buffer_deinit(void) {
    if (!state.initialized) return;

    // Restore original vprintf
    if (state.original_vprintf) {
        esp_log_set_vprintf(state.original_vprintf);
        state.original_vprintf = NULL;
    }

    state.initialized = false;

    if (state.mutex) {
        vSemaphoreDelete(state.mutex);
        state.mutex = NULL;
    }

    if (state.buffer) {
        free(state.buffer);
        state.buffer = NULL;
    }
}

bool log_buffer_is_initialized(void) {
    return state.initialized;
}

esp_err_t log_buffer_get_text(char **out_text, size_t *out_len, uint8_t min_level) {
    if (!state.initialized || !out_text || !out_len) {
        return ESP_ERR_INVALID_ARG;
    }

    // Estimate output size (generous estimate)
    size_t estimated_size = state.bytes_used * 2 + 1024;
    char *output = malloc(estimated_size);
    if (!output) {
        return ESP_ERR_NO_MEM;
    }

    size_t output_pos = 0;
    output[0] = '\0';

    if (xSemaphoreTake(state.mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        free(output);
        return ESP_ERR_TIMEOUT;
    }

    size_t pos = state.tail;
    size_t entries_read = 0;
    char tag_buf[64];
    char msg_buf[LOG_ENTRY_MAX_SIZE];

    while (entries_read < state.count) {
        // Read header
        log_entry_header_t header;
        ring_read(pos, &header, sizeof(header));
        pos = (pos + sizeof(header)) % state.size;

        // Read tag
        size_t tag_len = header.tag_len;
        if (tag_len >= sizeof(tag_buf)) tag_len = sizeof(tag_buf) - 1;
        ring_read(pos, tag_buf, header.tag_len);
        tag_buf[tag_len] = '\0';
        pos = (pos + header.tag_len) % state.size;

        // Read message
        size_t msg_len = header.msg_len;
        if (msg_len >= sizeof(msg_buf)) msg_len = sizeof(msg_buf) - 1;
        ring_read(pos, msg_buf, header.msg_len);
        msg_buf[msg_len] = '\0';
        pos = (pos + header.msg_len) % state.size;

        entries_read++;

        // Filter by level
        if (header.level > min_level) continue;

        // Format: "[timestamp] L TAG: message\n"
        int written = snprintf(output + output_pos, estimated_size - output_pos,
                               "[%8lu] %c %s: %s\n",
                               (unsigned long)header.timestamp_ms,
                               level_to_char(header.level),
                               tag_buf, msg_buf);

        if (written > 0 && output_pos + written < estimated_size) {
            output_pos += written;
        }
    }

    xSemaphoreGive(state.mutex);

    *out_text = output;
    *out_len = output_pos;

    return ESP_OK;
}

esp_err_t log_buffer_get_stats(log_buffer_stats_t *stats) {
    if (!stats) return ESP_ERR_INVALID_ARG;

    if (!state.initialized) {
        memset(stats, 0, sizeof(*stats));
        return ESP_OK;
    }

    if (xSemaphoreTake(state.mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    stats->entry_count = state.count;
    stats->bytes_used = state.bytes_used;
    stats->bytes_dropped = state.bytes_dropped;
    stats->buffer_size = state.size;

    xSemaphoreGive(state.mutex);

    return ESP_OK;
}

void log_buffer_clear(void) {
    if (!state.initialized) return;

    if (xSemaphoreTake(state.mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        state.head = 0;
        state.tail = 0;
        state.count = 0;
        state.bytes_used = 0;
        xSemaphoreGive(state.mutex);
    }
}

size_t log_buffer_get_read_position(void) {
    if (!state.initialized) return 0;

    size_t seq = 0;
    if (xSemaphoreTake(state.mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        // Return the current write sequence - caller will start from next entry
        seq = state.write_seq;
        xSemaphoreGive(state.mutex);
    }
    return seq;
}

bool log_buffer_read_next(size_t *position, char *json_out, size_t max_len, uint8_t min_level) {
    if (!state.initialized || !position || !json_out || max_len < 64) {
        return false;
    }

    if (xSemaphoreTake(state.mutex, pdMS_TO_TICKS(50)) != pdTRUE) {
        return false;
    }

    // Position is a sequence number - check if there are newer entries
    uint32_t read_seq = (uint32_t)*position;

    // Check if there are any new entries
    if (read_seq >= state.write_seq) {
        xSemaphoreGive(state.mutex);
        return false;  // No new entries
    }

    // Calculate which entry to read (0 = oldest in buffer)
    // The oldest entry has sequence: write_seq - count
    uint32_t oldest_seq = state.write_seq - state.count;

    // If our position is too old (entries have been discarded), start from oldest
    if (read_seq < oldest_seq) {
        read_seq = oldest_seq;
    }

    // Calculate offset from tail (how many entries to skip)
    uint32_t skip_count = read_seq - oldest_seq;

    // Navigate to the entry we want to read
    size_t pos = state.tail;
    char tag_buf[64];
    char msg_buf[LOG_ENTRY_MAX_SIZE];
    log_entry_header_t header;
    bool found = false;

    for (uint32_t i = 0; i < state.count; i++) {
        // Read header
        ring_read(pos, &header, sizeof(header));
        pos = (pos + sizeof(header)) % state.size;

        // Read tag
        size_t tag_len = header.tag_len;
        if (tag_len >= sizeof(tag_buf)) tag_len = sizeof(tag_buf) - 1;
        ring_read(pos, tag_buf, header.tag_len);
        tag_buf[tag_len] = '\0';
        pos = (pos + header.tag_len) % state.size;

        // Read message
        size_t msg_len = header.msg_len;
        if (msg_len >= sizeof(msg_buf)) msg_len = sizeof(msg_buf) - 1;
        ring_read(pos, msg_buf, header.msg_len);
        msg_buf[msg_len] = '\0';
        pos = (pos + header.msg_len) % state.size;

        // Skip entries we've already read
        if (i < skip_count) {
            continue;
        }

        // Update position to next entry's sequence
        *position = oldest_seq + i + 1;

        // Check level filter
        if (header.level > min_level) {
            continue;
        }

        // Found a matching entry - format as JSON
        found = true;

        // Escape JSON strings
        char escaped_tag[128];
        char escaped_msg[LOG_ENTRY_MAX_SIZE];
        size_t ti = 0, mi = 0;

        for (size_t j = 0; tag_buf[j] && ti < sizeof(escaped_tag) - 2; j++) {
            char c = tag_buf[j];
            if (c == '"' || c == '\\') {
                escaped_tag[ti++] = '\\';
            }
            escaped_tag[ti++] = c;
        }
        escaped_tag[ti] = '\0';

        for (size_t j = 0; msg_buf[j] && mi < sizeof(escaped_msg) - 2; j++) {
            char c = msg_buf[j];
            if (c == '"' || c == '\\') {
                escaped_msg[mi++] = '\\';
            } else if (c == '\n') {
                escaped_msg[mi++] = '\\';
                escaped_msg[mi++] = 'n';
                continue;
            } else if (c == '\r') {
                continue;
            }
            escaped_msg[mi++] = c;
        }
        escaped_msg[mi] = '\0';

        snprintf(json_out, max_len,
                 "{\"t\":%lu,\"l\":\"%c\",\"tag\":\"%s\",\"msg\":\"%s\"}",
                 (unsigned long)header.timestamp_ms,
                 level_to_char(header.level),
                 escaped_tag, escaped_msg);

        break;
    }

    xSemaphoreGive(state.mutex);

    return found;
}

#endif // ENABLE_LOG_BUFFER
