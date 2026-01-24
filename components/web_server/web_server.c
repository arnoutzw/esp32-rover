#include "web_server.h"
#include "camera.h"
#include "config.h"
#include "log_buffer.h"
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "cJSON.h"

// Default REST API enable if not defined in config
#ifndef ENABLE_REST_API
#define ENABLE_REST_API 1
#endif

static const char *TAG = "WEB_SERVER";

#define STREAM_BOUNDARY "frame"
#define STREAM_CONTENT_TYPE "multipart/x-mixed-replace;boundary=" STREAM_BOUNDARY
#define STREAM_PART "--" STREAM_BOUNDARY "\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n"

// Server state
static httpd_handle_t server = NULL;
static command_callback_t command_callback = NULL;
static rover_command_t last_command = {0};
static rover_status_t current_status = {0};
static int64_t last_command_time = 0;
static SemaphoreHandle_t state_mutex = NULL;

// Async stream task handle
static TaskHandle_t stream_task_handle = NULL;

// Log stream task handle
static TaskHandle_t log_stream_task_handle = NULL;

// Forward declarations
static esp_err_t root_handler(httpd_req_t *req);
static esp_err_t stream_handler(httpd_req_t *req);
static esp_err_t control_handler(httpd_req_t *req);
#if ENABLE_REST_API
static esp_err_t status_handler(httpd_req_t *req);
#endif
static esp_err_t led_get_handler(httpd_req_t *req);
static esp_err_t led_post_handler(httpd_req_t *req);
static esp_err_t logs_get_handler(httpd_req_t *req);
static esp_err_t logs_stream_handler(httpd_req_t *req);
static esp_err_t logs_delete_handler(httpd_req_t *req);

// URI handlers
static const httpd_uri_t uri_root = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = root_handler,
    .user_ctx = NULL
};

static const httpd_uri_t uri_stream = {
    .uri = "/stream",
    .method = HTTP_GET,
    .handler = stream_handler,
    .user_ctx = NULL
};

static const httpd_uri_t uri_control = {
    .uri = "/control",
    .method = HTTP_POST,
    .handler = control_handler,
    .user_ctx = NULL
};

#if ENABLE_REST_API
static const httpd_uri_t uri_status = {
    .uri = "/status",
    .method = HTTP_GET,
    .handler = status_handler,
    .user_ctx = NULL
};
#endif

static const httpd_uri_t uri_led_get = {
    .uri = "/led",
    .method = HTTP_GET,
    .handler = led_get_handler,
    .user_ctx = NULL
};

static const httpd_uri_t uri_led_post = {
    .uri = "/led",
    .method = HTTP_POST,
    .handler = led_post_handler,
    .user_ctx = NULL
};

// REQ-31: Log buffer endpoints
static const httpd_uri_t uri_logs_get = {
    .uri = "/logs",
    .method = HTTP_GET,
    .handler = logs_get_handler,
    .user_ctx = NULL
};

static const httpd_uri_t uri_logs_stream = {
    .uri = "/logs/stream",
    .method = HTTP_GET,
    .handler = logs_stream_handler,
    .user_ctx = NULL
};

static const httpd_uri_t uri_logs_delete = {
    .uri = "/logs",
    .method = HTTP_DELETE,
    .handler = logs_delete_handler,
    .user_ctx = NULL
};

// Root handler - serve HTML UI
static esp_err_t root_handler(httpd_req_t *req)
{
    const char *html = web_ui_get_html();
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, html, strlen(html));
}

// Async stream task data
typedef struct {
    httpd_req_t *req;
    int socket_fd;
} stream_task_data_t;

// Stream task that runs independently
static void stream_task(void *pvParameters)
{
    stream_task_data_t *data = (stream_task_data_t *)pvParameters;
    httpd_req_t *req = data->req;
    char part_buf[128];
    esp_err_t res = ESP_OK;

    ESP_LOGI(TAG, "Stream task started on socket %d", data->socket_fd);

    while (true) {
        camera_fb_t *fb = camera_capture_frame();
        if (!fb) {
            ESP_LOGE(TAG, "Camera capture failed");
            break;
        }

        size_t hlen = snprintf(part_buf, sizeof(part_buf), STREAM_PART, fb->len);

        res = httpd_resp_send_chunk(req, part_buf, hlen);
        if (res != ESP_OK) {
            camera_return_frame(fb);
            ESP_LOGI(TAG, "Stream client disconnected (header)");
            break;
        }

        res = httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);
        if (res != ESP_OK) {
            camera_return_frame(fb);
            ESP_LOGI(TAG, "Stream client disconnected (data)");
            break;
        }

        res = httpd_resp_send_chunk(req, "\r\n", 2);
        if (res != ESP_OK) {
            camera_return_frame(fb);
            ESP_LOGI(TAG, "Stream client disconnected (boundary)");
            break;
        }

        camera_return_frame(fb);

        // Small delay to limit frame rate and reduce CPU load
        vTaskDelay(pdMS_TO_TICKS(66));  // ~15 FPS
    }

    // Complete the async request
    httpd_req_async_handler_complete(req);
    free(data);
    stream_task_handle = NULL;
    ESP_LOGI(TAG, "Stream task ended");
    vTaskDelete(NULL);
}

// Stream handler - MJPEG stream (async version)
static esp_err_t stream_handler(httpd_req_t *req)
{
    if (!camera_is_initialized()) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Camera not initialized");
        return ESP_FAIL;
    }

    // Only allow one stream at a time
    if (stream_task_handle != NULL) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Stream already active");
        return ESP_FAIL;
    }

    esp_err_t res = httpd_resp_set_type(req, STREAM_CONTENT_TYPE);
    if (res != ESP_OK) {
        return res;
    }

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "X-Framerate", "15");

    // Start async handling - this allows other requests to be processed
    httpd_req_t *async_req = NULL;
    res = httpd_req_async_handler_begin(req, &async_req);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start async handler: %s", esp_err_to_name(res));
        return res;
    }

    // Allocate task data
    stream_task_data_t *task_data = malloc(sizeof(stream_task_data_t));
    if (!task_data) {
        httpd_req_async_handler_complete(async_req);
        return ESP_ERR_NO_MEM;
    }
    task_data->req = async_req;
    task_data->socket_fd = httpd_req_to_sockfd(req);

    // Create stream task on Core 0
    BaseType_t xReturned = xTaskCreatePinnedToCore(
        stream_task,
        "mjpeg_stream",
        4096,
        task_data,
        3,  // Medium priority
        &stream_task_handle,
        0   // Core 0
    );

    if (xReturned != pdPASS) {
        ESP_LOGE(TAG, "Failed to create stream task");
        httpd_req_async_handler_complete(async_req);
        free(task_data);
        return ESP_FAIL;
    }

    return ESP_OK;
}

// Control handler - receive commands
static esp_err_t control_handler(httpd_req_t *req)
{
    char buf[256];
    int ret, remaining = req->content_len;

    if (remaining > sizeof(buf) - 1) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Content too large");
        return ESP_FAIL;
    }

    ret = httpd_req_recv(req, buf, remaining);
    if (ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    // Parse JSON
    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    rover_command_t cmd = {0};

    cJSON *speed = cJSON_GetObjectItem(root, "speed");
    if (speed && cJSON_IsNumber(speed)) {
        cmd.speed = (float)speed->valuedouble;
        // Clamp to valid range
        if (cmd.speed > 100) cmd.speed = 100;
        if (cmd.speed < -100) cmd.speed = -100;
    }

    cJSON *steering = cJSON_GetObjectItem(root, "steering");
    if (steering && cJSON_IsNumber(steering)) {
        cmd.steering = (float)steering->valuedouble;
        // Clamp to valid range
        if (cmd.steering > 100) cmd.steering = 100;
        if (cmd.steering < -100) cmd.steering = -100;
    }

    cJSON *estop = cJSON_GetObjectItem(root, "estop");
    if (estop && cJSON_IsBool(estop)) {
        cmd.emergency_stop = cJSON_IsTrue(estop);
    }

    cJSON_Delete(root);

    // Update state
    if (state_mutex && xSemaphoreTake(state_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        last_command = cmd;
        last_command_time = esp_timer_get_time();
        xSemaphoreGive(state_mutex);
    }

    // Call callback
    if (command_callback) {
        command_callback(&cmd);
    }

    // Send response
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");

    return ESP_OK;
}

#if ENABLE_REST_API
// Status handler - return current status with full diagnostics
static esp_err_t status_handler(httpd_req_t *req)
{
    char response[896];

    rover_status_t status = {0};
    if (state_mutex && xSemaphoreTake(state_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        status = current_status;
        xSemaphoreGive(state_mutex);
    }

    // Determine target string
#ifdef ROVER_TARGET_ESP32CAM
    const char *target_str = "esp32cam";
#else
    const char *target_str = "ttgo";
#endif

    // Build JSON with all diagnostic data
    snprintf(response, sizeof(response),
        "{"
        "\"target\":\"%s\","
        "\"velocity\":%.2f,"
        "\"battery\":%.2f,"
        "\"steering\":%.1f,"
        "\"motor\":%s,"
        "\"camera\":%s,"
        "\"rssi\":%d,"
        "\"btnL\":%s,"
        "\"btnR\":%s,"
        "\"diag\":{"
            "\"ssid\":\"%s\","
            "\"ip\":\"%s\","
            "\"mac\":\"%s\","
            "\"channel\":%d,"
            "\"clients\":%d,"
            "\"txPower\":%d,"
            "\"freeHeap\":%lu,"
            "\"minHeap\":%lu,"
            "\"totalHeap\":%lu,"
            "\"freeInternal\":%lu,"
            "\"uptime\":%lu,"
            "\"cpuFreq\":%.0f,"
            "\"tasks\":%d,"
            "\"tasksCore0\":%d,"
            "\"tasksCore1\":%d,"
            "\"tasksNoAffinity\":%d,"
            "\"restApi\":%s,"
            "\"mqttEnabled\":%s,"
            "\"mqttConnected\":%s,"
            "\"internet\":%s,"
            "\"localTime\":\"%s\","
            "\"ntpSynced\":%s"
        "}"
        "}",
        target_str,
        status.motor_velocity,
        status.battery_voltage,
        status.steering_angle,
        status.motor_enabled ? "true" : "false",
        status.camera_active ? "true" : "false",
        status.wifi_rssi,
        status.button_left ? "true" : "false",
        status.button_right ? "true" : "false",
        status.wifi_ssid ? status.wifi_ssid : "",
        status.wifi_ip ? status.wifi_ip : "",
        status.mac_addr ? status.mac_addr : "",
        status.wifi_channel,
        status.connected_clients,
        status.wifi_tx_power,
        (unsigned long)status.free_heap,
        (unsigned long)status.min_free_heap,
        (unsigned long)status.total_heap,
        (unsigned long)status.free_internal,
        (unsigned long)status.uptime_secs,
        status.cpu_freq_mhz,
        status.task_count,
        status.tasks_core0,
        status.tasks_core1,
        status.tasks_no_affinity,
        status.rest_api_enabled ? "true" : "false",
        status.mqtt_enabled ? "true" : "false",
        status.mqtt_connected ? "true" : "false",
        status.internet_connected ? "true" : "false",
        status.local_time ? status.local_time : "--:--:--",
        status.ntp_synced ? "true" : "false"
    );

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_sendstr(req, response);
}
#endif // ENABLE_REST_API

// =============================================================================
// LED Control Handlers
// =============================================================================

// GET /led - Return LED state
static esp_err_t led_get_handler(httpd_req_t *req)
{
    bool state = flash_led_get_state();
    char response[32];
    snprintf(response, sizeof(response), "{\"on\":%s}", state ? "true" : "false");

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_sendstr(req, response);
}

// POST /led - Set LED state
static esp_err_t led_post_handler(httpd_req_t *req)
{
    char buf[64];
    int ret, remaining = req->content_len;

    if (remaining > sizeof(buf) - 1) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Content too large");
        return ESP_FAIL;
    }

    ret = httpd_req_recv(req, buf, remaining);
    if (ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    // Parse JSON
    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    cJSON *on = cJSON_GetObjectItem(root, "on");
    if (on && cJSON_IsBool(on)) {
        flash_led_set(cJSON_IsTrue(on));
        ESP_LOGI(TAG, "Flash LED set to %s", cJSON_IsTrue(on) ? "ON" : "OFF");
    }

    cJSON_Delete(root);

    // Return current state
    bool state = flash_led_get_state();
    char response[32];
    snprintf(response, sizeof(response), "{\"on\":%s}", state ? "true" : "false");

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_sendstr(req, response);
}

// =============================================================================
// REQ-31: Log Buffer Handlers
// =============================================================================

// Helper to parse level query parameter
static uint8_t get_level_from_query(httpd_req_t *req) {
    char query[64] = {0};
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        char level_str[8] = {0};
        if (httpd_query_key_value(query, "level", level_str, sizeof(level_str)) == ESP_OK) {
            int level = atoi(level_str);
            if (level >= 1 && level <= 5) {
                return (uint8_t)level;
            }
        }
    }
    return LOG_LEVEL_INFO;  // Default: Info level and above
}

// Helper to check if download mode
static bool is_download_request(httpd_req_t *req) {
    char query[64] = {0};
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        char download_str[8] = {0};
        if (httpd_query_key_value(query, "download", download_str, sizeof(download_str)) == ESP_OK) {
            return download_str[0] == '1' || download_str[0] == 't';
        }
    }
    return false;
}

// GET /logs - Return all buffered logs as text
static esp_err_t logs_get_handler(httpd_req_t *req)
{
    if (!log_buffer_is_initialized()) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Log buffer not initialized");
        return ESP_FAIL;
    }

    uint8_t min_level = get_level_from_query(req);
    bool download = is_download_request(req);

    char *log_text = NULL;
    size_t log_len = 0;

    esp_err_t ret = log_buffer_get_text(&log_text, &log_len, min_level);
    if (ret != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to get logs");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "text/plain");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    // REQ-35: Add download header if requested
    if (download) {
        httpd_resp_set_hdr(req, "Content-Disposition", "attachment; filename=\"esp32_logs.txt\"");
    }

    esp_err_t send_ret = httpd_resp_send(req, log_text ? log_text : "", log_len);
    free(log_text);

    return send_ret;
}

// DELETE /logs - Clear log buffer
static esp_err_t logs_delete_handler(httpd_req_t *req)
{
    log_buffer_clear();

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_sendstr(req, "{\"status\":\"cleared\"}");
}

// Log stream task data
typedef struct {
    httpd_req_t *req;
    uint8_t min_level;
} log_stream_task_data_t;

// SSE stream task for logs
static void log_stream_task(void *pvParameters)
{
    log_stream_task_data_t *data = (log_stream_task_data_t *)pvParameters;
    httpd_req_t *req = data->req;
    uint8_t min_level = data->min_level;
    char json_buf[512];
    esp_err_t res;

    ESP_LOGI(TAG, "Log stream task started");

    // Get current position (start from end to only get new logs)
    size_t position = log_buffer_get_read_position();

    while (true) {
        // Try to read next log entry
        if (log_buffer_read_next(&position, json_buf, sizeof(json_buf), min_level)) {
            // Send SSE event
            char sse_buf[600];
            int len = snprintf(sse_buf, sizeof(sse_buf), "event: log\ndata: %s\n\n", json_buf);

            res = httpd_resp_send_chunk(req, sse_buf, len);
            if (res != ESP_OK) {
                ESP_LOGI(TAG, "Log stream client disconnected");
                break;
            }
        } else {
            // No new logs, wait a bit
            vTaskDelay(pdMS_TO_TICKS(100));

            // Send keepalive comment every ~3 seconds (30 iterations)
            static int keepalive_counter = 0;
            if (++keepalive_counter >= 30) {
                keepalive_counter = 0;
                res = httpd_resp_send_chunk(req, ": keepalive\n\n", 13);
                if (res != ESP_OK) {
                    ESP_LOGI(TAG, "Log stream client disconnected (keepalive)");
                    break;
                }
            }
        }
    }

    httpd_req_async_handler_complete(req);
    free(data);
    log_stream_task_handle = NULL;
    ESP_LOGI(TAG, "Log stream task ended");
    vTaskDelete(NULL);
}

// GET /logs/stream - SSE stream for live logs
static esp_err_t logs_stream_handler(httpd_req_t *req)
{
    if (!log_buffer_is_initialized()) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Log buffer not initialized");
        return ESP_FAIL;
    }

    // Only allow one log stream at a time
    if (log_stream_task_handle != NULL) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Log stream already active");
        return ESP_FAIL;
    }

    uint8_t min_level = get_level_from_query(req);

    // Set SSE headers
    httpd_resp_set_type(req, "text/event-stream");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_set_hdr(req, "Connection", "keep-alive");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    // Start async handling
    httpd_req_t *async_req = NULL;
    esp_err_t res = httpd_req_async_handler_begin(req, &async_req);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start async handler for log stream: %s", esp_err_to_name(res));
        return res;
    }

    // Allocate task data
    log_stream_task_data_t *task_data = malloc(sizeof(log_stream_task_data_t));
    if (!task_data) {
        httpd_req_async_handler_complete(async_req);
        return ESP_ERR_NO_MEM;
    }
    task_data->req = async_req;
    task_data->min_level = min_level;

    // Create log stream task on Core 0
    BaseType_t xReturned = xTaskCreatePinnedToCore(
        log_stream_task,
        "log_stream",
        4096,
        task_data,
        2,  // Lower priority than MJPEG stream
        &log_stream_task_handle,
        0   // Core 0
    );

    if (xReturned != pdPASS) {
        ESP_LOGE(TAG, "Failed to create log stream task");
        httpd_req_async_handler_complete(async_req);
        free(task_data);
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t web_server_init(const web_server_config_t *config)
{
    if (server) {
        ESP_LOGW(TAG, "Server already running");
        return ESP_OK;
    }

    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    // Create mutex
    if (!state_mutex) {
        state_mutex = xSemaphoreCreateMutex();
        if (!state_mutex) {
            return ESP_ERR_NO_MEM;
        }
    }

    command_callback = config->on_command;

    httpd_config_t http_config = HTTPD_DEFAULT_CONFIG();
    http_config.server_port = config->port;
    http_config.stack_size = 8192;
    http_config.max_uri_handlers = 16;  // Increased for log endpoints
    http_config.lru_purge_enable = true;
    // Allow multiple concurrent connections (stream + status/control requests)
    http_config.max_open_sockets = 10;

    ESP_LOGI(TAG, "Starting server on port %d", config->port);

    esp_err_t ret = httpd_start(&server, &http_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start server: %s", esp_err_to_name(ret));
        return ret;
    }

    // Register URI handlers
    httpd_register_uri_handler(server, &uri_root);
    httpd_register_uri_handler(server, &uri_stream);
    httpd_register_uri_handler(server, &uri_control);
#if ENABLE_REST_API
    httpd_register_uri_handler(server, &uri_status);
    ESP_LOGI(TAG, "REST API enabled (/status endpoint)");
#endif

    // Register LED endpoints
    httpd_register_uri_handler(server, &uri_led_get);
    httpd_register_uri_handler(server, &uri_led_post);
    ESP_LOGI(TAG, "LED endpoint enabled (/led)");

    // REQ-31: Register log buffer endpoints
    httpd_register_uri_handler(server, &uri_logs_get);
    httpd_register_uri_handler(server, &uri_logs_stream);
    httpd_register_uri_handler(server, &uri_logs_delete);
    ESP_LOGI(TAG, "Log endpoints enabled (/logs, /logs/stream)");

    ESP_LOGI(TAG, "Web server started");
    return ESP_OK;
}

esp_err_t web_server_stop(void)
{
    if (!server) {
        return ESP_OK;
    }

    esp_err_t ret = httpd_stop(server);
    server = NULL;
    return ret;
}

esp_err_t web_server_update_status(const rover_status_t *status)
{
    if (!status) {
        return ESP_ERR_INVALID_ARG;
    }

    if (state_mutex && xSemaphoreTake(state_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        current_status = *status;
        xSemaphoreGive(state_mutex);
    }

    return ESP_OK;
}

esp_err_t web_server_get_last_command(rover_command_t *cmd)
{
    if (!cmd) {
        return ESP_ERR_INVALID_ARG;
    }

    if (state_mutex && xSemaphoreTake(state_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        *cmd = last_command;
        xSemaphoreGive(state_mutex);
        return ESP_OK;
    }

    return ESP_ERR_TIMEOUT;
}

bool web_server_is_running(void)
{
    return server != NULL;
}

uint32_t web_server_get_command_age_ms(void)
{
    int64_t now = esp_timer_get_time();
    int64_t cmd_time = 0;

    if (state_mutex && xSemaphoreTake(state_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        cmd_time = last_command_time;
        xSemaphoreGive(state_mutex);
    }

    if (cmd_time == 0) {
        return UINT32_MAX;  // No command received yet
    }

    return (uint32_t)((now - cmd_time) / 1000);  // Convert to ms
}

httpd_handle_t web_server_get_handle(void)
{
    return server;
}
