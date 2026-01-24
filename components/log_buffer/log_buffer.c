// =============================================================================
// Log Buffer Component - Captures ESP_LOG output for web display
// =============================================================================
#include "log_buffer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

static const char *TAG = "LOG_BUFFER";

// Use a simple circular array instead of ring buffer for read-without-consume
#define MAX_LOG_ENTRIES   300  // ~300 entries, ~42KB heap usage
#define MAX_MSG_LEN       128
#define MAX_TAG_LEN       16

// Compact log entry structure
typedef struct {
    uint32_t timestamp_ms;
    char tag[MAX_TAG_LEN];
    char msg[MAX_MSG_LEN];
    uint8_t level;
} log_entry_storage_t;

// Internal state
typedef struct {
    log_entry_storage_t *entries;   // Circular array
    size_t head;                    // Next write position
    size_t count;                   // Number of valid entries
    SemaphoreHandle_t mutex;
    vprintf_like_t original_vprintf;
    uint32_t dropped_count;
    uint32_t oldest_valid_timestamp;  // For 2-hour rotation
    bool initialized;
} log_buffer_state_t;

static log_buffer_state_t s_state = {0};

// Forward declaration
static int log_vprintf_hook(const char *fmt, va_list args);

// -----------------------------------------------------------------------------
// Helper Functions
// -----------------------------------------------------------------------------

char log_buffer_level_to_char(uint8_t level)
{
    switch (level) {
        case 1: return 'E';  // ESP_LOG_ERROR
        case 2: return 'W';  // ESP_LOG_WARN
        case 3: return 'I';  // ESP_LOG_INFO
        case 4: return 'D';  // ESP_LOG_DEBUG
        case 5: return 'V';  // ESP_LOG_VERBOSE
        default: return '?';
    }
}

log_filter_t log_buffer_parse_level_filter(const char *level_str)
{
    if (!level_str || strcmp(level_str, "all") == 0) {
        return LOG_FILTER_ALL;
    }
    if (strcmp(level_str, "error") == 0) {
        return LOG_FILTER_ERROR;
    }
    if (strcmp(level_str, "warn") == 0) {
        return LOG_FILTER_ERROR | LOG_FILTER_WARN;
    }
    if (strcmp(level_str, "info") == 0) {
        return LOG_FILTER_ERROR | LOG_FILTER_WARN | LOG_FILTER_INFO;
    }
    if (strcmp(level_str, "debug") == 0) {
        return LOG_FILTER_ERROR | LOG_FILTER_WARN | LOG_FILTER_INFO | LOG_FILTER_DEBUG;
    }
    if (strcmp(level_str, "verbose") == 0) {
        return LOG_FILTER_ALL;
    }
    return LOG_FILTER_ALL;
}

// Parse ESP-IDF log format: "X (timestamp) TAG: message\n"
// where X is the log level letter (E/W/I/D/V)
static bool parse_log_line(const char *line, uint8_t *out_level,
                           char *out_tag, size_t tag_size,
                           char *out_msg, size_t msg_size)
{
    if (!line || strlen(line) < 5) return false;

    // Parse level from first character
    switch (line[0]) {
        case 'E': *out_level = 1; break;
        case 'W': *out_level = 2; break;
        case 'I': *out_level = 3; break;
        case 'D': *out_level = 4; break;
        case 'V': *out_level = 5; break;
        default: *out_level = 3; break;  // Default to INFO
    }

    // Find tag - after ") " and before ": "
    const char *tag_start = strstr(line, ") ");
    if (!tag_start) {
        // Fallback: just use "LOG" as tag
        strncpy(out_tag, "LOG", tag_size - 1);
        out_tag[tag_size - 1] = '\0';
        strncpy(out_msg, line, msg_size - 1);
        out_msg[msg_size - 1] = '\0';
        return true;
    }
    tag_start += 2;

    const char *tag_end = strstr(tag_start, ": ");
    if (!tag_end) {
        strncpy(out_tag, "LOG", tag_size - 1);
        out_tag[tag_size - 1] = '\0';
        strncpy(out_msg, line, msg_size - 1);
        out_msg[msg_size - 1] = '\0';
        return true;
    }

    // Copy tag
    size_t tag_len = tag_end - tag_start;
    if (tag_len >= tag_size) tag_len = tag_size - 1;
    strncpy(out_tag, tag_start, tag_len);
    out_tag[tag_len] = '\0';

    // Copy message (after ": ")
    const char *msg_start = tag_end + 2;
    size_t msg_len = strlen(msg_start);
    // Remove trailing newline
    while (msg_len > 0 && (msg_start[msg_len - 1] == '\n' || msg_start[msg_len - 1] == '\r')) {
        msg_len--;
    }
    if (msg_len >= msg_size) msg_len = msg_size - 1;
    strncpy(out_msg, msg_start, msg_len);
    out_msg[msg_len] = '\0';

    return true;
}

// Store a log entry in the circular buffer
static void store_log_entry(uint8_t level, const char *tag, const char *msg)
{
    if (!s_state.initialized || !s_state.entries) return;

    if (xSemaphoreTake(s_state.mutex, pdMS_TO_TICKS(5)) != pdTRUE) {
        s_state.dropped_count++;
        return;
    }

    // Write to current head position
    log_entry_storage_t *entry = &s_state.entries[s_state.head];
    entry->timestamp_ms = esp_log_timestamp();
    entry->level = level;
    strncpy(entry->tag, tag, MAX_TAG_LEN - 1);
    entry->tag[MAX_TAG_LEN - 1] = '\0';
    strncpy(entry->msg, msg, MAX_MSG_LEN - 1);
    entry->msg[MAX_MSG_LEN - 1] = '\0';

    // Advance head
    s_state.head = (s_state.head + 1) % MAX_LOG_ENTRIES;
    if (s_state.count < MAX_LOG_ENTRIES) {
        s_state.count++;
    }

    xSemaphoreGive(s_state.mutex);
}

// Custom vprintf hook that captures logs
static int log_vprintf_hook(const char *fmt, va_list args)
{
    // Format the message first
    char temp[256];
    int len = vsnprintf(temp, sizeof(temp), fmt, args);

    // Parse and store if valid
    if (len > 0 && s_state.initialized) {
        uint8_t level;
        char tag[MAX_TAG_LEN];
        char msg[MAX_MSG_LEN];

        if (parse_log_line(temp, &level, tag, sizeof(tag), msg, sizeof(msg))) {
            store_log_entry(level, tag, msg);
        }
    }

    // Pass through to UART
    return printf("%s", temp);
}

// -----------------------------------------------------------------------------
// Public API
// -----------------------------------------------------------------------------

esp_err_t log_buffer_init(void)
{
    if (s_state.initialized) {
        return ESP_OK;
    }

    // Create mutex
    s_state.mutex = xSemaphoreCreateMutex();
    if (!s_state.mutex) {
        return ESP_ERR_NO_MEM;
    }

    // Allocate entry array
    s_state.entries = calloc(MAX_LOG_ENTRIES, sizeof(log_entry_storage_t));
    if (!s_state.entries) {
        vSemaphoreDelete(s_state.mutex);
        s_state.mutex = NULL;
        return ESP_ERR_NO_MEM;
    }

    // Initialize state
    s_state.head = 0;
    s_state.count = 0;
    s_state.dropped_count = 0;
    s_state.oldest_valid_timestamp = 0;
    s_state.initialized = true;

    // Install custom vprintf handler
    s_state.original_vprintf = esp_log_set_vprintf(log_vprintf_hook);

    // Log that we're initialized (this will be captured!)
    ESP_LOGI(TAG, "Log buffer initialized (%d entries, %d KB)",
             MAX_LOG_ENTRIES, (int)(MAX_LOG_ENTRIES * sizeof(log_entry_storage_t) / 1024));

    return ESP_OK;
}

void log_buffer_deinit(void)
{
    if (!s_state.initialized) return;

    // Restore original vprintf
    if (s_state.original_vprintf) {
        esp_log_set_vprintf(s_state.original_vprintf);
        s_state.original_vprintf = NULL;
    }

    // Free resources
    if (s_state.entries) {
        free(s_state.entries);
        s_state.entries = NULL;
    }

    if (s_state.mutex) {
        vSemaphoreDelete(s_state.mutex);
        s_state.mutex = NULL;
    }

    s_state.initialized = false;
}

bool log_buffer_is_initialized(void)
{
    return s_state.initialized;
}

esp_err_t log_buffer_get_stats(log_buffer_stats_t *stats)
{
    if (!stats) return ESP_ERR_INVALID_ARG;
    if (!s_state.initialized) return ESP_ERR_INVALID_STATE;

    if (xSemaphoreTake(s_state.mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    stats->entry_count = s_state.count;
    stats->dropped_count = s_state.dropped_count;
    stats->buffer_size = MAX_LOG_ENTRIES * sizeof(log_entry_storage_t);
    stats->buffer_used = s_state.count * sizeof(log_entry_storage_t);
    stats->oldest_timestamp = s_state.oldest_valid_timestamp;

    xSemaphoreGive(s_state.mutex);
    return ESP_OK;
}

void log_buffer_clear(void)
{
    if (!s_state.initialized) return;

    if (xSemaphoreTake(s_state.mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        s_state.head = 0;
        s_state.count = 0;
        xSemaphoreGive(s_state.mutex);
    }

    ESP_LOGI(TAG, "Log buffer cleared");
}

void log_buffer_rotate(void)
{
    if (!s_state.initialized) return;

    uint32_t now = esp_log_timestamp();
    uint32_t cutoff = (now > LOG_ROTATION_PERIOD_MS) ? (now - LOG_ROTATION_PERIOD_MS) : 0;

    if (xSemaphoreTake(s_state.mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        s_state.oldest_valid_timestamp = cutoff;
        xSemaphoreGive(s_state.mutex);
    }
}

// Escape special characters for JSON
static void json_escape_string(const char *src, char *dst, size_t dst_size)
{
    size_t j = 0;
    for (size_t i = 0; src[i] && j < dst_size - 1; i++) {
        char c = src[i];
        if (c == '"' || c == '\\') {
            if (j + 2 >= dst_size) break;
            dst[j++] = '\\';
            dst[j++] = c;
        } else if (c == '\n') {
            if (j + 2 >= dst_size) break;
            dst[j++] = '\\';
            dst[j++] = 'n';
        } else if (c == '\r') {
            if (j + 2 >= dst_size) break;
            dst[j++] = '\\';
            dst[j++] = 'r';
        } else if (c == '\t') {
            if (j + 2 >= dst_size) break;
            dst[j++] = '\\';
            dst[j++] = 't';
        } else if ((unsigned char)c < 0x20) {
            // Skip other control characters
            continue;
        } else {
            dst[j++] = c;
        }
    }
    dst[j] = '\0';
}

esp_err_t log_buffer_get_json(
    log_filter_t filter,
    const char *tag_filter,
    uint32_t since_timestamp,
    char **out_json,
    size_t max_entries)
{
    if (!out_json) return ESP_ERR_INVALID_ARG;
    if (!s_state.initialized) return ESP_ERR_INVALID_STATE;

    *out_json = NULL;

    if (xSemaphoreTake(s_state.mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    // Get rotation cutoff
    uint32_t rotation_cutoff = s_state.oldest_valid_timestamp;

    // Calculate starting index (oldest entry)
    size_t start_idx;
    if (s_state.count < MAX_LOG_ENTRIES) {
        start_idx = 0;
    } else {
        start_idx = s_state.head;  // head points to oldest when buffer is full
    }

    // Count matching entries first
    size_t match_count = 0;
    for (size_t i = 0; i < s_state.count; i++) {
        size_t idx = (start_idx + i) % MAX_LOG_ENTRIES;
        log_entry_storage_t *entry = &s_state.entries[idx];

        // Apply filters
        if (entry->timestamp_ms < rotation_cutoff) continue;
        if (entry->timestamp_ms <= since_timestamp) continue;
        if (!(filter & (1 << entry->level))) continue;
        if (tag_filter && *tag_filter && strstr(entry->tag, tag_filter) == NULL) continue;

        match_count++;
    }

    // Allocate JSON buffer (estimate ~150 bytes per entry + overhead)
    size_t output_count = (max_entries > 0 && max_entries < match_count) ? max_entries : match_count;
    size_t json_capacity = 256 + output_count * 300;
    char *json = malloc(json_capacity);
    if (!json) {
        xSemaphoreGive(s_state.mutex);
        return ESP_ERR_NO_MEM;
    }

    size_t json_len = 0;
    json_len += snprintf(json + json_len, json_capacity - json_len, "{\"logs\":[");

    // If we need to limit, skip older entries
    size_t skip_count = (match_count > output_count) ? (match_count - output_count) : 0;
    size_t skipped = 0;
    bool first = true;

    char escaped_tag[MAX_TAG_LEN * 2];
    char escaped_msg[MAX_MSG_LEN * 2];

    for (size_t i = 0; i < s_state.count; i++) {
        size_t idx = (start_idx + i) % MAX_LOG_ENTRIES;
        log_entry_storage_t *entry = &s_state.entries[idx];

        // Apply filters
        if (entry->timestamp_ms < rotation_cutoff) continue;
        if (entry->timestamp_ms <= since_timestamp) continue;
        if (!(filter & (1 << entry->level))) continue;
        if (tag_filter && *tag_filter && strstr(entry->tag, tag_filter) == NULL) continue;

        // Skip older entries if limiting
        if (skipped < skip_count) {
            skipped++;
            continue;
        }

        json_escape_string(entry->tag, escaped_tag, sizeof(escaped_tag));
        json_escape_string(entry->msg, escaped_msg, sizeof(escaped_msg));

        if (!first) {
            json_len += snprintf(json + json_len, json_capacity - json_len, ",");
        }
        first = false;

        json_len += snprintf(json + json_len, json_capacity - json_len,
            "{\"t\":%lu,\"l\":\"%c\",\"tag\":\"%s\",\"msg\":\"%s\"}",
            (unsigned long)entry->timestamp_ms,
            log_buffer_level_to_char(entry->level),
            escaped_tag,
            escaped_msg);
    }

    // Add stats
    json_len += snprintf(json + json_len, json_capacity - json_len,
        "],\"stats\":{\"count\":%lu,\"dropped\":%lu,\"bufferUsed\":%lu,\"bufferSize\":%lu}}",
        (unsigned long)s_state.count,
        (unsigned long)s_state.dropped_count,
        (unsigned long)(s_state.count * sizeof(log_entry_storage_t)),
        (unsigned long)(MAX_LOG_ENTRIES * sizeof(log_entry_storage_t)));

    xSemaphoreGive(s_state.mutex);

    *out_json = json;
    return ESP_OK;
}

esp_err_t log_buffer_get_new_logs(
    log_filter_t filter,
    uint32_t since_timestamp,
    char **out_json,
    uint32_t *out_last_timestamp)
{
    // Just use get_json with the since filter
    esp_err_t err = log_buffer_get_json(filter, NULL, since_timestamp, out_json, 100);

    if (err == ESP_OK && out_last_timestamp) {
        *out_last_timestamp = esp_log_timestamp();
    }

    return err;
}
