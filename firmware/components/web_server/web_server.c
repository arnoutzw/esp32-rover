#include "web_server.h"
#include "config.h"

#ifdef ROVER_TARGET_ESP32CAM
#include "camera.h"
#endif
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

// =============================================================================
// REQ-SW-032: Steering/Velocity History for Live Telemetry Charts
// =============================================================================
// Ring buffer for steering history (used as placeholder until velocity sensors implemented)
// At 10Hz status updates, 600 samples = 60 seconds of history
#define STEERING_HISTORY_SIZE 600
static float steering_history[STEERING_HISTORY_SIZE] = {0};
static uint32_t steering_history_timestamps[STEERING_HISTORY_SIZE] = {0};
static size_t steering_history_head = 0;  // Next write position
static size_t steering_history_count = 0; // Number of valid samples

// Add steering sample to history buffer
static void steering_history_add(float steering, uint32_t timestamp) {
    steering_history[steering_history_head] = steering;
    steering_history_timestamps[steering_history_head] = timestamp;
    steering_history_head = (steering_history_head + 1) % STEERING_HISTORY_SIZE;
    if (steering_history_count < STEERING_HISTORY_SIZE) {
        steering_history_count++;
    }
}

#ifdef ROVER_TARGET_ESP32CAM
// Async stream task handle
static TaskHandle_t stream_task_handle = NULL;
#endif

// Forward declarations
static esp_err_t root_handler(httpd_req_t *req);
static esp_err_t control_handler(httpd_req_t *req);
#if ENABLE_REST_API
static esp_err_t status_handler(httpd_req_t *req);
#endif
#ifdef ROVER_TARGET_ESP32CAM
static esp_err_t stream_handler(httpd_req_t *req);
static esp_err_t led_get_handler(httpd_req_t *req);
static esp_err_t led_post_handler(httpd_req_t *req);
static esp_err_t camera_get_handler(httpd_req_t *req);
static esp_err_t camera_post_handler(httpd_req_t *req);
#endif

// URI handlers
static const httpd_uri_t uri_root = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = root_handler,
    .user_ctx = NULL
};

#ifdef ROVER_TARGET_ESP32CAM
static const httpd_uri_t uri_stream = {
    .uri = "/stream",
    .method = HTTP_GET,
    .handler = stream_handler,
    .user_ctx = NULL
};
#endif

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

#ifdef ROVER_TARGET_ESP32CAM
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
#endif // ROVER_TARGET_ESP32CAM

// REQ-34: Camera stream control endpoints
#ifdef ROVER_TARGET_ESP32CAM
static const httpd_uri_t uri_camera_get = {
    .uri = "/camera",
    .method = HTTP_GET,
    .handler = camera_get_handler,
    .user_ctx = NULL
};

static const httpd_uri_t uri_camera_post = {
    .uri = "/camera",
    .method = HTTP_POST,
    .handler = camera_post_handler,
    .user_ctx = NULL
};
#endif // ROVER_TARGET_ESP32CAM

// Root handler - serve HTML UI
static esp_err_t root_handler(httpd_req_t *req)
{
    const char *html = web_ui_get_html();
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, html, strlen(html));
}

#ifdef ROVER_TARGET_ESP32CAM
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
    int paused_counter = 0;

    ESP_LOGI(TAG, "Stream task started on socket %d", data->socket_fd);

    while (true) {
        // REQ-34: Check if streaming is enabled
        if (!camera_stream_is_enabled()) {
            // Wait while disabled, checking periodically
            vTaskDelay(pdMS_TO_TICKS(500));
            paused_counter++;
            // Log occasionally when paused
            if (paused_counter == 1) {
                ESP_LOGI(TAG, "Camera stream paused by user");
            }
            continue;
        }

        if (paused_counter > 0) {
            ESP_LOGI(TAG, "Camera stream resumed");
            paused_counter = 0;
        }

        camera_fb_t *fb = camera_capture_frame();
        if (!fb) {
            // Stream is disabled or capture failed, wait and retry
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
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
#endif // ROVER_TARGET_ESP32CAM

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

        // REQ-SW-032: Record steering for live telemetry chart
        uint32_t uptime_ms = (uint32_t)(esp_timer_get_time() / 1000);
        steering_history_add(cmd.steering, uptime_ms);

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
// Helper: Get WiFi mode string from numeric mode
static const char* get_wifi_mode_str(uint8_t wifi_mode)
{
    switch (wifi_mode) {
        case 1:  return "sta";
        case 2:  return "ap";
        case 3:  return "apsta";
        default: return "unknown";
    }
}

// Helper: Build status JSON response
// Uses cJSON for cleaner, safer JSON construction
static cJSON* build_status_json(const rover_status_t *status)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) return NULL;

    // Determine target string
#ifdef ROVER_TARGET_ESP32CAM
    const char *target_str = "esp32cam";
#else
    const char *target_str = "ttgo";
#endif

    // Root level fields
    cJSON_AddStringToObject(root, "target", target_str);
    cJSON_AddNumberToObject(root, "velocity", 0.0);  // Placeholder for UI compatibility
    cJSON_AddNumberToObject(root, "battery", status->battery_voltage);
    cJSON_AddBoolToObject(root, "camera", status->camera_active);
    cJSON_AddNumberToObject(root, "rssi", status->wifi_rssi);
    cJSON_AddBoolToObject(root, "btnL", status->button_left);
    cJSON_AddBoolToObject(root, "btnR", status->button_right);

    // REQ-SW-032: Add steering history for live telemetry chart
    // Return last 60 samples (6 seconds at 10Hz polling) to keep payload reasonable
    {
        const size_t MAX_SAMPLES = 60;
        size_t samples_to_return = steering_history_count < MAX_SAMPLES ? steering_history_count : MAX_SAMPLES;

        cJSON *history = cJSON_CreateArray();
        if (history) {
            // Read from oldest to newest (circular buffer)
            if (samples_to_return > 0) {
                size_t start_idx;
                if (steering_history_count < STEERING_HISTORY_SIZE) {
                    // Buffer not full yet, start from beginning
                    start_idx = (steering_history_count >= MAX_SAMPLES) ?
                                (steering_history_count - MAX_SAMPLES) : 0;
                } else {
                    // Buffer is full, calculate start position
                    start_idx = (steering_history_head + STEERING_HISTORY_SIZE - MAX_SAMPLES) % STEERING_HISTORY_SIZE;
                }

                for (size_t i = 0; i < samples_to_return; i++) {
                    size_t idx = (start_idx + i) % STEERING_HISTORY_SIZE;
                    cJSON *sample = cJSON_CreateObject();
                    if (sample) {
                        cJSON_AddNumberToObject(sample, "t", steering_history_timestamps[idx]);
                        cJSON_AddNumberToObject(sample, "v", steering_history[idx]);
                        cJSON_AddItemToArray(history, sample);
                    }
                }
            }
            cJSON_AddItemToObject(root, "steeringHistory", history);
        }
    }

    // Diagnostic sub-object
    cJSON *diag = cJSON_CreateObject();
    if (!diag) {
        cJSON_Delete(root);
        return NULL;
    }

    cJSON_AddStringToObject(diag, "ssid", status->wifi_ssid ? status->wifi_ssid : "");
    cJSON_AddStringToObject(diag, "ip", status->wifi_ip ? status->wifi_ip : "");
    cJSON_AddStringToObject(diag, "wifiMode", get_wifi_mode_str(status->wifi_mode));
    cJSON_AddNumberToObject(diag, "channel", status->wifi_channel);

    // Only include clients count in AP or APSTA mode
    if (status->wifi_mode >= 2) {
        cJSON_AddNumberToObject(diag, "clients", status->connected_clients);
    }

    cJSON_AddNumberToObject(diag, "txPower", status->wifi_tx_power);
    cJSON_AddNumberToObject(diag, "freeHeap", status->free_heap);
    cJSON_AddNumberToObject(diag, "minHeap", status->min_free_heap);
    cJSON_AddNumberToObject(diag, "totalHeap", status->total_heap);
    cJSON_AddNumberToObject(diag, "freeInternal", status->free_internal);
    cJSON_AddNumberToObject(diag, "uptime", status->uptime_secs);
    cJSON_AddBoolToObject(diag, "restApi", status->rest_api_enabled);
    cJSON_AddBoolToObject(diag, "mqttEnabled", status->mqtt_enabled);
    cJSON_AddBoolToObject(diag, "mqttConnected", status->mqtt_connected);
    cJSON_AddStringToObject(diag, "localTime", status->local_time ? status->local_time : "--:--:--");
    cJSON_AddBoolToObject(diag, "ntpSynced", status->ntp_synced);
    cJSON_AddStringToObject(diag, "buildVersion", status->build_version ? status->build_version : "dev");
    cJSON_AddStringToObject(diag, "buildFingerprint", status->build_fingerprint ? status->build_fingerprint : "unknown");
    cJSON_AddStringToObject(diag, "buildTime", status->build_time ? status->build_time : "unknown");
    cJSON_AddStringToObject(diag, "buildBranch", status->build_branch ? status->build_branch : "unknown");
    cJSON_AddBoolToObject(diag, "buildDirty", status->build_dirty);

    cJSON_AddItemToObject(root, "diag", diag);

    return root;
}

// Status handler - return current status with full diagnostics
static esp_err_t status_handler(httpd_req_t *req)
{
    rover_status_t status = {0};
    if (state_mutex && xSemaphoreTake(state_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        status = current_status;
        xSemaphoreGive(state_mutex);
    }

    // Build JSON response using cJSON helper
    cJSON *json = build_status_json(&status);
    if (!json) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to build response");
        return ESP_FAIL;
    }

    // Print JSON to string (cJSON handles allocation)
    char *response = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);

    if (!response) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to serialize JSON");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    esp_err_t ret = httpd_resp_sendstr(req, response);

    free(response);  // Free cJSON allocated string
    return ret;
}
#endif // ENABLE_REST_API

// =============================================================================
// LED Control Handlers (ESP32-CAM Flash LED)
// =============================================================================

#ifdef ROVER_TARGET_ESP32CAM
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
#endif // ROVER_TARGET_ESP32CAM

// =============================================================================
// REQ-34: Camera Stream Control Handlers
// =============================================================================

#ifdef ROVER_TARGET_ESP32CAM
// GET /camera - Return camera stream state
static esp_err_t camera_get_handler(httpd_req_t *req)
{
    bool enabled = camera_stream_is_enabled();
    char response[48];
    snprintf(response, sizeof(response), "{\"enabled\":%s}", enabled ? "true" : "false");

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_sendstr(req, response);
}

// POST /camera - Set camera stream state
static esp_err_t camera_post_handler(httpd_req_t *req)
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

    cJSON *enabled = cJSON_GetObjectItem(root, "enabled");
    if (enabled && cJSON_IsBool(enabled)) {
        camera_stream_set_enabled(cJSON_IsTrue(enabled));
        ESP_LOGI(TAG, "Camera stream set to %s", cJSON_IsTrue(enabled) ? "enabled" : "disabled");
    }

    cJSON_Delete(root);

    // Return current state
    bool state = camera_stream_is_enabled();
    char response[48];
    snprintf(response, sizeof(response), "{\"enabled\":%s}", state ? "true" : "false");

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_sendstr(req, response);
}
#endif // ROVER_TARGET_ESP32CAM

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
    // Configure max sockets based on target's LWIP_MAX_SOCKETS setting
    // TTGO has LWIP_MAX_SOCKETS=8, ESP32-CAM has 16
    // HTTP server uses 3 sockets internally, so max_open_sockets can be at most (LWIP_MAX_SOCKETS - 3)
#ifdef ROVER_TARGET_ESP32CAM
    http_config.max_open_sockets = 10;  // ESP32-CAM: 10 + 3 = 13 (safe with LWIP_MAX_SOCKETS=16)
#elif defined(ROVER_TARGET_TTGO)
    http_config.max_open_sockets = 4;   // TTGO: 4 + 3 = 7 (safe with LWIP_MAX_SOCKETS=8)
#else
    http_config.max_open_sockets = 4;   // Conservative default
#endif

    ESP_LOGI(TAG, "Starting server on port %d", config->port);

    esp_err_t ret = httpd_start(&server, &http_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start server: %s", esp_err_to_name(ret));
        return ret;
    }

    // Register URI handlers
    httpd_register_uri_handler(server, &uri_root);
#ifdef ROVER_TARGET_ESP32CAM
    httpd_register_uri_handler(server, &uri_stream);
#endif
    httpd_register_uri_handler(server, &uri_control);
#if ENABLE_REST_API
    httpd_register_uri_handler(server, &uri_status);
    ESP_LOGI(TAG, "REST API enabled (/status endpoint)");
#endif

#ifdef ROVER_TARGET_ESP32CAM
    // Register LED endpoints (Flash LED control)
    httpd_register_uri_handler(server, &uri_led_get);
    httpd_register_uri_handler(server, &uri_led_post);
    ESP_LOGI(TAG, "LED endpoint enabled (/led)");
#endif

#ifdef ROVER_TARGET_ESP32CAM
    // REQ-34: Register camera stream control endpoints
    httpd_register_uri_handler(server, &uri_camera_get);
    httpd_register_uri_handler(server, &uri_camera_post);
    ESP_LOGI(TAG, "Camera control endpoint enabled (/camera)");
#endif

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
