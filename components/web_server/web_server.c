#include "web_server.h"
#include "camera.h"
#include "config.h"
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

// Forward declarations
static esp_err_t root_handler(httpd_req_t *req);
static esp_err_t stream_handler(httpd_req_t *req);
static esp_err_t control_handler(httpd_req_t *req);
#if ENABLE_REST_API
static esp_err_t status_handler(httpd_req_t *req);
#endif

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

// Root handler - serve HTML UI
static esp_err_t root_handler(httpd_req_t *req)
{
    const char *html = web_ui_get_html();
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, html, strlen(html));
}

// Stream handler - MJPEG stream
static esp_err_t stream_handler(httpd_req_t *req)
{
    esp_err_t res = ESP_OK;
    char part_buf[128];

    if (!camera_is_initialized()) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Camera not initialized");
        return ESP_FAIL;
    }

    res = httpd_resp_set_type(req, STREAM_CONTENT_TYPE);
    if (res != ESP_OK) {
        return res;
    }

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "X-Framerate", "15");

    while (true) {
        camera_fb_t *fb = camera_capture_frame();
        if (!fb) {
            ESP_LOGE(TAG, "Camera capture failed");
            res = ESP_FAIL;
            break;
        }

        size_t hlen = snprintf(part_buf, sizeof(part_buf), STREAM_PART, fb->len);

        res = httpd_resp_send_chunk(req, part_buf, hlen);
        if (res != ESP_OK) {
            camera_return_frame(fb);
            break;
        }

        res = httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);
        if (res != ESP_OK) {
            camera_return_frame(fb);
            break;
        }

        res = httpd_resp_send_chunk(req, "\r\n", 2);
        if (res != ESP_OK) {
            camera_return_frame(fb);
            break;
        }

        camera_return_frame(fb);

        // Small delay to limit frame rate and reduce CPU load
        vTaskDelay(pdMS_TO_TICKS(66));  // ~15 FPS
    }

    return res;
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

    // Build JSON with all diagnostic data
    snprintf(response, sizeof(response),
        "{"
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
            "\"restApi\":%s,"
            "\"mqttEnabled\":%s,"
            "\"mqttConnected\":%s"
        "}"
        "}",
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
        status.rest_api_enabled ? "true" : "false",
        status.mqtt_enabled ? "true" : "false",
        status.mqtt_connected ? "true" : "false"
    );

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_sendstr(req, response);
}
#endif // ENABLE_REST_API

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
    http_config.max_uri_handlers = 8;
    http_config.lru_purge_enable = true;

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
