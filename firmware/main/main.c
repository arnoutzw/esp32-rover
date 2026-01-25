#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_heap_caps.h"
#include "nvs_flash.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "ping/ping_sock.h"
#include "esp_ota_ops.h"
#include "esp_http_server.h"
#include "esp_sntp.h"
#include "esp_sleep.h"
#include "driver/rtc_io.h"
#include "mdns.h"

#include "config.h"
#include "build_info.h"

#if defined(ENABLE_TASK_WATCHDOG) && ENABLE_TASK_WATCHDOG
#include "esp_task_wdt.h"
#endif
#if !DISABLE_CAMERA
#include "camera.h"
#endif
#include "web_server.h"
#if defined(ENABLE_LCD_DISPLAY) && ENABLE_LCD_DISPLAY
#include "lcd_display.h"
#endif
#if defined(ENABLE_MQTT) && ENABLE_MQTT
#include "mqtt_service.h"
#endif
#if defined(ENABLE_LOG_BUFFER) && ENABLE_LOG_BUFFER
#include "log_buffer.h"
#endif

static const char *TAG = "ROVER_MAIN";

// Control state
static rover_command_t current_command = {0};
static SemaphoreHandle_t command_mutex = NULL;

// Task handles
static TaskHandle_t status_task_handle = NULL;
#if defined(ENABLE_LCD_DISPLAY) && ENABLE_LCD_DISPLAY
static TaskHandle_t lcd_task_handle = NULL;
#endif

// =============================================================================
// Button Support
// =============================================================================

#if defined(ENABLE_BUTTONS) && ENABLE_BUTTONS
static bool s_buttons_initialized = false;
static volatile bool s_button_left_pressed = false;
static volatile bool s_button_right_pressed = false;

// GPIO interrupt handler for buttons - runs in ISR context
static void IRAM_ATTR button_isr_handler(void* arg)
{
    // Read current button states directly (active LOW)
    s_button_left_pressed = (gpio_get_level(BUTTON_LEFT_PIN) == 0);
    s_button_right_pressed = (gpio_get_level(BUTTON_RIGHT_PIN) == 0);
}

static void init_buttons(void)
{
    if (s_buttons_initialized) return;

    // Configure button pins as input with pull-up and interrupts on both edges
    gpio_config_t btn_conf = {
        .pin_bit_mask = (1ULL << BUTTON_LEFT_PIN) | (1ULL << BUTTON_RIGHT_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE,  // Trigger on press AND release
    };
    gpio_config(&btn_conf);

    // Install GPIO ISR service and add handlers
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BUTTON_LEFT_PIN, button_isr_handler, NULL);
    gpio_isr_handler_add(BUTTON_RIGHT_PIN, button_isr_handler, NULL);

    // Read initial state
    s_button_left_pressed = (gpio_get_level(BUTTON_LEFT_PIN) == 0);
    s_button_right_pressed = (gpio_get_level(BUTTON_RIGHT_PIN) == 0);

    s_buttons_initialized = true;
    ESP_LOGI(TAG, "Buttons initialized with interrupts (GPIO %d, %d)", BUTTON_LEFT_PIN, BUTTON_RIGHT_PIN);
}

static bool read_button_left(void)
{
    return s_buttons_initialized && s_button_left_pressed;
}

static bool read_button_right(void)
{
    return s_buttons_initialized && s_button_right_pressed;
}
#endif

// =============================================================================
// Battery ADC Support
// =============================================================================

#if defined(ENABLE_BATTERY_ADC) && ENABLE_BATTERY_ADC
static adc_oneshot_unit_handle_t s_adc_handle = NULL;
static adc_cali_handle_t s_adc_cali_handle = NULL;
static bool s_battery_adc_initialized = false;

// Low-pass filter for battery voltage (reduces display flicker)
// Alpha = 0.02 gives ~10 second time constant at 20Hz sampling
// This means it takes about 10 seconds to reach 63% of a step change
#define BATTERY_FILTER_ALPHA 0.02f
static float s_battery_voltage_filtered = 0.0f;
static bool s_battery_filter_initialized = false;

static void init_battery_adc(void)
{
    if (s_battery_adc_initialized) return;

    // Configure ADC unit
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &s_adc_handle));

    // Configure ADC channel
    adc_oneshot_chan_cfg_t chan_config = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten = ADC_ATTEN_DB_12,  // Full scale ~3.3V (with attenuation)
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc_handle, BATTERY_ADC_CHANNEL, &chan_config));

    // Create calibration handle for more accurate readings (ESP32 uses line fitting)
    adc_cali_line_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT_1,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    if (adc_cali_create_scheme_line_fitting(&cali_config, &s_adc_cali_handle) != ESP_OK) {
        ESP_LOGW(TAG, "ADC calibration scheme not available, using raw values");
        s_adc_cali_handle = NULL;
    }

    s_battery_adc_initialized = true;
    ESP_LOGI(TAG, "Battery ADC initialized (GPIO %d, channel %d)", BATTERY_ADC_PIN, BATTERY_ADC_CHANNEL);
}

static float read_battery_voltage(void)
{
    if (!s_battery_adc_initialized || !s_adc_handle) {
        return 0.0f;
    }

    int raw_value = 0;
    esp_err_t ret = adc_oneshot_read(s_adc_handle, BATTERY_ADC_CHANNEL, &raw_value);
    if (ret != ESP_OK) {
        // ADC read can timeout during WiFi activity - return last filtered value
        return s_battery_voltage_filtered;
    }

    float voltage_mv;
    if (s_adc_cali_handle) {
        int calibrated_mv = 0;
        adc_cali_raw_to_voltage(s_adc_cali_handle, raw_value, &calibrated_mv);
        voltage_mv = (float)calibrated_mv;
    } else {
        // Rough conversion without calibration (3.3V / 4095)
        voltage_mv = (raw_value / 4095.0f) * 3300.0f;
    }

    // Apply voltage divider ratio to get actual battery voltage
    float battery_voltage = (voltage_mv / 1000.0f) * BATTERY_DIVIDER_RATIO;

    // Apply exponential moving average low-pass filter
    // This smooths out ADC noise and prevents display flicker
    if (!s_battery_filter_initialized) {
        // Initialize filter with first reading
        s_battery_voltage_filtered = battery_voltage;
        s_battery_filter_initialized = true;
    } else {
        // EMA filter: filtered = alpha * new + (1-alpha) * old
        s_battery_voltage_filtered = BATTERY_FILTER_ALPHA * battery_voltage +
                                    (1.0f - BATTERY_FILTER_ALPHA) * s_battery_voltage_filtered;
    }

    return s_battery_voltage_filtered;
}
#endif

// =============================================================================
// WiFi Configuration
// =============================================================================

// WiFi state tracking
static bool s_wifi_is_sta_mode = false;
static bool s_wifi_sta_connected = false;
static char s_wifi_ip_str[16] = "192.168.4.1";
static EventGroupHandle_t s_wifi_event_group = NULL;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

// REQ-10: Internet connectivity tracking
static bool s_internet_connected = false;

// REQ-13: NTP time tracking
static bool s_ntp_synced = false;
static char s_local_time_str[32] = "--:--:--";

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_AP_STACONNECTED: {
                wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
                ESP_LOGI(TAG, "Station " MACSTR " joined, AID=%d",
                         MAC2STR(event->mac), event->aid);
                break;
            }
            case WIFI_EVENT_AP_STADISCONNECTED: {
                wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
                ESP_LOGI(TAG, "Station " MACSTR " left, AID=%d",
                         MAC2STR(event->mac), event->aid);
                break;
            }
            case WIFI_EVENT_STA_START:
                esp_wifi_connect();
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                if (s_wifi_event_group) {
                    xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
                }
                s_wifi_sta_connected = false;
                ESP_LOGI(TAG, "Disconnected from WiFi");
                break;
            default:
                break;
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        snprintf(s_wifi_ip_str, sizeof(s_wifi_ip_str), IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "Got IP: %s", s_wifi_ip_str);
        s_wifi_sta_connected = true;
        if (s_wifi_event_group) {
            xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        }
    }
}

#if WIFI_MODE_AP_ONLY
// Start WiFi in AP mode (AP-only configuration)
static esp_err_t wifi_start_ap(bool netif_already_init)
{
    ESP_LOGI(TAG, "Starting WiFi AP mode");

    if (!netif_already_init) {
        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());
    }
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    if (!netif_already_init) {
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));
        ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
        ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                            ESP_EVENT_ANY_ID,
                                                            &wifi_event_handler,
                                                            NULL, NULL));
    }

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = WIFI_AP_SSID,
            .ssid_len = strlen(WIFI_AP_SSID),
            .channel = WIFI_AP_CHANNEL,
            .password = WIFI_AP_PASSWORD,
            .max_connection = WIFI_AP_MAX_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
        },
    };

    if (strlen(WIFI_AP_PASSWORD) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    s_wifi_is_sta_mode = false;
    snprintf(s_wifi_ip_str, sizeof(s_wifi_ip_str), "192.168.4.1");

    // Verbose IP address output for easy visibility
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "============================================");
    ESP_LOGI(TAG, "  WiFi Access Point Started Successfully!");
    ESP_LOGI(TAG, "============================================");
    ESP_LOGI(TAG, "  SSID:      %s", WIFI_AP_SSID);
    ESP_LOGI(TAG, "  Password:  %s", WIFI_AP_PASSWORD);
    ESP_LOGI(TAG, "  Channel:   %d", WIFI_AP_CHANNEL);
    ESP_LOGI(TAG, "--------------------------------------------");
    ESP_LOGI(TAG, "  IP Address: http://%s", s_wifi_ip_str);
    ESP_LOGI(TAG, "============================================");
    ESP_LOGI(TAG, "");

    return ESP_OK;
}
#endif

#if WIFI_MODE_STA_FIRST || WIFI_MODE_STA_ONLY
// Try to connect to STA network with timeout, return true if successful
static bool wifi_try_sta_connect(void)
{
    ESP_LOGI(TAG, "Attempting to connect to WiFi network: %s", WIFI_STA_SSID);

    s_wifi_event_group = xEventGroupCreate();
    if (!s_wifi_event_group) {
        ESP_LOGE(TAG, "Failed to create event group");
        return false;
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL, NULL));

    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, WIFI_STA_SSID, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, WIFI_STA_PASSWORD, sizeof(wifi_config.sta.password) - 1);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    // Wait for connection with timeout
    ESP_LOGI(TAG, "Waiting for STA connection (timeout: %d seconds)...", WIFI_STA_CONNECT_TIMEOUT_S);
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                            pdFALSE, pdFALSE,
                                            pdMS_TO_TICKS(WIFI_STA_CONNECT_TIMEOUT_S * 1000));

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Successfully connected to WiFi network: %s", WIFI_STA_SSID);
        s_wifi_is_sta_mode = true;
        vEventGroupDelete(s_wifi_event_group);
        s_wifi_event_group = NULL;
        return true;
    }

    ESP_LOGW(TAG, "Failed to connect to WiFi network: %s", WIFI_STA_SSID);
    vEventGroupDelete(s_wifi_event_group);
    s_wifi_event_group = NULL;
    return false;
}
#endif // WIFI_MODE_STA_FIRST || WIFI_MODE_STA_ONLY

#if WIFI_MODE_STA_FIRST
// Switch from failed STA to AP mode (only needed in STA-first mode)
static esp_err_t wifi_switch_to_ap(void)
{
    ESP_LOGI(TAG, "Switching from STA to AP mode...");

    // Stop current WiFi but keep it initialized
    esp_wifi_stop();

    // Create AP netif
    esp_netif_create_default_wifi_ap();

    // Configure and start AP mode
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = WIFI_AP_SSID,
            .ssid_len = strlen(WIFI_AP_SSID),
            .channel = WIFI_AP_CHANNEL,
            .password = WIFI_AP_PASSWORD,
            .max_connection = WIFI_AP_MAX_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
        },
    };

    if (strlen(WIFI_AP_PASSWORD) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    s_wifi_is_sta_mode = false;
    snprintf(s_wifi_ip_str, sizeof(s_wifi_ip_str), "192.168.4.1");
    ESP_LOGI(TAG, "WiFi AP started. SSID: %s, Password: %s", WIFI_AP_SSID, WIFI_AP_PASSWORD);
    return ESP_OK;
}
#endif // WIFI_MODE_STA_FIRST

// Initialize WiFi based on configuration mode
static esp_err_t wifi_init(void)
{
#if WIFI_MODE_STA_FIRST
    // STA-first mode: Try STA, fall back to AP if connection fails
    ESP_LOGI(TAG, "WiFi mode: STA-first with AP fallback");
    if (wifi_try_sta_connect()) {
        return ESP_OK;  // Connected to STA network
    }
    // STA failed, switch to AP mode
    ESP_LOGW(TAG, "STA connection failed, falling back to AP mode");
    return wifi_switch_to_ap();

#elif WIFI_MODE_STA_ONLY
    // STA-only mode: Just connect to STA, no fallback
    ESP_LOGI(TAG, "WiFi mode: STA only");
    if (wifi_try_sta_connect()) {
        return ESP_OK;
    }
    ESP_LOGE(TAG, "STA connection failed and no fallback configured");
    return ESP_FAIL;

#else
    // AP-only mode (default): Just start AP
    ESP_LOGI(TAG, "WiFi mode: AP only");
    return wifi_start_ap(false);
#endif
}

// Helper functions for status reporting
bool wifi_is_sta_mode(void)
{
    return s_wifi_is_sta_mode;
}

bool wifi_is_sta_connected(void)
{
    return s_wifi_sta_connected;
}

const char* wifi_get_ip_str(void)
{
    return s_wifi_ip_str;
}

const char* wifi_get_current_ssid(void)
{
    return s_wifi_is_sta_mode ? WIFI_STA_SSID : WIFI_AP_SSID;
}

// REQ-10: Get internet connectivity status
bool wifi_is_internet_connected(void)
{
    return s_internet_connected;
}

// =============================================================================
// REQ-10: Internet Connectivity Check
// =============================================================================

static bool s_ping_success = false;

static void ping_success_callback(esp_ping_handle_t hdl, void *args)
{
    s_ping_success = true;
}

static void ping_end_callback(esp_ping_handle_t hdl, void *args)
{
    // Ping session ended
}

// Check internet connectivity by pinging 1.1.1.1
static bool check_internet_connectivity(void)
{
    ESP_LOGI(TAG, "Checking internet connectivity (ping 1.1.1.1)...");

    ip_addr_t target_addr;
    IP4_ADDR(&target_addr.u_addr.ip4, 1, 1, 1, 1);
    target_addr.type = IPADDR_TYPE_V4;

    esp_ping_config_t ping_config = ESP_PING_DEFAULT_CONFIG();
    ping_config.target_addr = target_addr;
    ping_config.count = 3;           // Send 3 pings
    ping_config.interval_ms = 1000;  // 1 second interval
    ping_config.timeout_ms = 2000;   // 2 second timeout per ping

    esp_ping_callbacks_t callbacks = {
        .on_ping_success = ping_success_callback,
        .on_ping_timeout = NULL,
        .on_ping_end = ping_end_callback,
        .cb_args = NULL,
    };

    s_ping_success = false;

    esp_ping_handle_t ping_handle;
    esp_err_t ret = esp_ping_new_session(&ping_config, &callbacks, &ping_handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to create ping session: %s", esp_err_to_name(ret));
        return false;
    }

    esp_ping_start(ping_handle);

    // Wait for ping to complete (3 pings * 2s timeout = 6s max, plus some buffer)
    vTaskDelay(pdMS_TO_TICKS(7000));

    esp_ping_stop(ping_handle);
    esp_ping_delete_session(ping_handle);

    if (s_ping_success) {
        ESP_LOGI(TAG, "Internet connectivity: CONNECTED");
    } else {
        ESP_LOGW(TAG, "Internet connectivity: NOT CONNECTED");
    }

    return s_ping_success;
}

// =============================================================================
// REQ-13: NTP Time Synchronization
// =============================================================================

static void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "NTP time synchronized!");
    s_ntp_synced = true;
}

static void init_sntp(void)
{
    ESP_LOGI(TAG, "Initializing SNTP...");

    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_setservername(1, "time.google.com");
    esp_sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    esp_sntp_init();

    // Set timezone for Netherlands (CET/CEST)
    // CET-1CEST,M3.5.0,M10.5.0/3 means:
    // - CET = Central European Time, UTC+1
    // - CEST = Central European Summer Time
    // - M3.5.0 = DST starts last Sunday of March
    // - M10.5.0/3 = DST ends last Sunday of October at 3:00
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
    tzset();

    ESP_LOGI(TAG, "SNTP initialized, waiting for time sync...");
}

static void update_local_time_string(void)
{
    time_t now;
    struct tm timeinfo;

    time(&now);
    localtime_r(&now, &timeinfo);

    if (timeinfo.tm_year > (2020 - 1900)) {
        // Time is valid (year > 2020)
        strftime(s_local_time_str, sizeof(s_local_time_str), "%H:%M:%S", &timeinfo);
    } else {
        snprintf(s_local_time_str, sizeof(s_local_time_str), "--:--:--");
    }
}

// Get current local time string
const char* get_local_time_str(void)
{
    update_local_time_string();
    return s_local_time_str;
}

// Check if NTP is synced
bool is_ntp_synced(void)
{
    return s_ntp_synced;
}

// =============================================================================
// REQ-11: OTA Update Support
// =============================================================================

#if defined(ENABLE_OTA) && ENABLE_OTA
static esp_err_t ota_update_handler(httpd_req_t *req)
{
    char buf[512];
    int received;
    int remaining = req->content_len;
    esp_ota_handle_t update_handle = 0;
    const esp_partition_t *update_partition = NULL;
    bool is_first_block = true;
    esp_err_t err;

    ESP_LOGI(TAG, "OTA update started, size: %d bytes", remaining);

    update_partition = esp_ota_get_next_update_partition(NULL);
    if (update_partition == NULL) {
        ESP_LOGE(TAG, "No OTA partition found");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No OTA partition");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Writing to partition: %s", update_partition->label);

    while (remaining > 0) {
        received = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf)));
        if (received <= 0) {
            if (received == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            ESP_LOGE(TAG, "OTA receive error");
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Receive error");
            return ESP_FAIL;
        }

        if (is_first_block) {
            err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(err));
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA begin failed");
                return ESP_FAIL;
            }
            is_first_block = false;
        }

        err = esp_ota_write(update_handle, buf, received);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_write failed: %s", esp_err_to_name(err));
            esp_ota_abort(update_handle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA write failed");
            return ESP_FAIL;
        }

        remaining -= received;
    }

    err = esp_ota_end(update_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end failed: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA end failed");
        return ESP_FAIL;
    }

    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Set boot partition failed");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "OTA update successful, restarting...");
    httpd_resp_sendstr(req, "OTA update successful. Rebooting...");

    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();

    return ESP_OK;
}

static esp_err_t init_ota_endpoint(httpd_handle_t server)
{
    httpd_uri_t ota_uri = {
        .uri = "/ota",
        .method = HTTP_POST,
        .handler = ota_update_handler,
        .user_ctx = NULL,
    };

    esp_err_t ret = httpd_register_uri_handler(server, &ota_uri);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "OTA endpoint registered at /ota");
    }
    return ret;
}
#endif // ENABLE_OTA

// =============================================================================
// Command Callback
// =============================================================================

static void on_rover_command(const rover_command_t *cmd)
{
    if (command_mutex && xSemaphoreTake(command_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        current_command = *cmd;
        xSemaphoreGive(command_mutex);
    }

#if DEBUG_WEBSERVER
    ESP_LOGI(TAG, "Command: speed=%.1f, steering=%.1f, estop=%d",
             cmd->speed, cmd->steering, cmd->emergency_stop);
#endif
}

// =============================================================================
// REQ-09: Get task counts per core
// =============================================================================

typedef struct {
    uint8_t core0;
    uint8_t core1;
    uint8_t no_affinity;
} task_core_counts_t;

static void get_task_core_counts(task_core_counts_t *counts)
{
    counts->core0 = 0;
    counts->core1 = 0;
    counts->no_affinity = 0;

    UBaseType_t num_tasks = uxTaskGetNumberOfTasks();
    if (num_tasks == 0) return;

    // Allocate buffer for task status array
    TaskStatus_t *task_array = pvPortMalloc(num_tasks * sizeof(TaskStatus_t));
    if (task_array == NULL) return;

    // Get task states
    UBaseType_t actual_count = uxTaskGetSystemState(task_array, num_tasks, NULL);

    for (UBaseType_t i = 0; i < actual_count; i++) {
        BaseType_t core = xTaskGetAffinity(task_array[i].xHandle);
        if (core == 0) {
            counts->core0++;
        } else if (core == 1) {
            counts->core1++;
        } else {
            // tskNO_AFFINITY means task can run on any core
            counts->no_affinity++;
        }
    }

    vPortFree(task_array);
}

// =============================================================================
// Status Update Task
// =============================================================================

static void status_update_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Status update task started");

#if defined(ENABLE_TASK_WATCHDOG) && ENABLE_TASK_WATCHDOG
    // REQ-37: Subscribe to task watchdog
    esp_err_t wdt_err = esp_task_wdt_add(NULL);
    if (wdt_err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to add status to watchdog: %s", esp_err_to_name(wdt_err));
    }
#endif

    // Get MAC address once at startup (static for lifetime of task)
    static char mac_str[18];
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    // Record start time for uptime calculation
    TickType_t start_ticks = xTaskGetTickCount();

    while (1) {
        rover_status_t status = {0};

        // Camera status
#if !DISABLE_CAMERA
        status.camera_active = camera_is_initialized();
#else
        status.camera_active = false;
#endif

        // Battery voltage
#if defined(ENABLE_BATTERY_ADC) && ENABLE_BATTERY_ADC
        status.battery_voltage = read_battery_voltage();
#else
        status.battery_voltage = 0.0f;  // No battery ADC available
#endif

        // WiFi RSSI
        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            status.wifi_rssi = ap_info.rssi;
        }

#if defined(ENABLE_BUTTONS) && ENABLE_BUTTONS
        // Button states
        status.button_left = read_button_left();
        status.button_right = read_button_right();
#endif

        // =================================================================
        // Diagnostic data (same as LCD diagnostics)
        // =================================================================
        status.wifi_ssid = wifi_get_current_ssid();
        status.wifi_ip = wifi_get_ip_str();
        status.mac_addr = mac_str;
        status.wifi_channel = WIFI_AP_CHANNEL;

        // Connected clients
        wifi_sta_list_t sta_list;
        if (esp_wifi_ap_get_sta_list(&sta_list) == ESP_OK) {
            status.connected_clients = sta_list.num;
        }

        // TX power
        int8_t tx_power = 0;
        esp_wifi_get_max_tx_power(&tx_power);
        status.wifi_tx_power = tx_power / 4;  // Convert from 0.25dBm units

        // Memory stats
        status.free_heap = esp_get_free_heap_size();
        status.min_free_heap = esp_get_minimum_free_heap_size();
        status.total_heap = heap_caps_get_total_size(MALLOC_CAP_DEFAULT);
        status.free_internal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);

        // System stats
        status.uptime_secs = (xTaskGetTickCount() - start_ticks) / configTICK_RATE_HZ;
        status.cpu_freq_mhz = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ;
        status.task_count = uxTaskGetNumberOfTasks();

        // REQ-09: Per-core task counts for web GUI
        task_core_counts_t task_counts;
        get_task_core_counts(&task_counts);
        status.tasks_core0 = task_counts.core0;
        status.tasks_core1 = task_counts.core1;
        status.tasks_no_affinity = task_counts.no_affinity;

        // Service status
#if defined(ENABLE_REST_API) && ENABLE_REST_API
        status.rest_api_enabled = true;
#else
        status.rest_api_enabled = false;
#endif
#if defined(ENABLE_MQTT) && ENABLE_MQTT
        status.mqtt_enabled = true;
        status.mqtt_connected = mqtt_service_is_connected();
#else
        status.mqtt_enabled = false;
        status.mqtt_connected = false;
#endif
        status.internet_connected = s_internet_connected;

        // REQ-13: Local time
        status.local_time = get_local_time_str();
        status.ntp_synced = s_ntp_synced;

        // Build information
        status.build_version = BUILD_VERSION;
        status.build_fingerprint = BUILD_FINGERPRINT;
        status.build_time = BUILD_TIME;
        status.build_branch = BUILD_GIT_BRANCH;
        status.build_dirty = BUILD_GIT_DIRTY;

        // Update web server status
        web_server_update_status(&status);

#if defined(ENABLE_MQTT) && ENABLE_MQTT
        // Update MQTT service status
        mqtt_service_update_status(&status);
#endif

#if defined(ENABLE_TASK_WATCHDOG) && ENABLE_TASK_WATCHDOG
        // REQ-37: Feed task watchdog
        esp_task_wdt_reset();
#endif

        vTaskDelay(pdMS_TO_TICKS(50));  // Update at 20Hz for responsive button feedback
    }
}

// =============================================================================
// Initialization
// =============================================================================

#if !DISABLE_CAMERA
static esp_err_t init_camera(void)
{
    ESP_LOGI(TAG, "Initializing camera");

    camera_config_params_t config = {
        .pin_pwdn = CAM_PIN_PWDN,
        .pin_reset = CAM_PIN_RESET,
        .pin_xclk = CAM_PIN_XCLK,
        .pin_sccb_sda = CAM_PIN_SIOD,
        .pin_sccb_scl = CAM_PIN_SIOC,
        .pin_d7 = CAM_PIN_D7,
        .pin_d6 = CAM_PIN_D6,
        .pin_d5 = CAM_PIN_D5,
        .pin_d4 = CAM_PIN_D4,
        .pin_d3 = CAM_PIN_D3,
        .pin_d2 = CAM_PIN_D2,
        .pin_d1 = CAM_PIN_D1,
        .pin_d0 = CAM_PIN_D0,
        .pin_vsync = CAM_PIN_VSYNC,
        .pin_href = CAM_PIN_HREF,
        .pin_pclk = CAM_PIN_PCLK,
        .xclk_freq = CAM_XCLK_FREQ,
        .pixel_format = PIXFORMAT_JPEG,
        .frame_size = CAM_FRAME_SIZE,
        .jpeg_quality = CAM_JPEG_QUALITY,
        .fb_count = 2,
    };

    return camera_module_init(&config);
}
#endif

static esp_err_t init_web_server(void)
{
    ESP_LOGI(TAG, "Initializing web server");

    web_server_config_t config = {
        .port = WEB_SERVER_PORT,
        .on_command = on_rover_command,
    };

    return web_server_init(&config);
}

#if defined(ENABLE_MQTT) && ENABLE_MQTT
static esp_err_t init_mqtt_service(void)
{
    ESP_LOGI(TAG, "Initializing MQTT service");

    mqtt_service_config_t config = {
        .broker_host = MQTT_BROKER_HOST,
        .broker_port = MQTT_BROKER_PORT,
        .username = MQTT_USERNAME,
        .password = MQTT_PASSWORD,
        .client_id = MQTT_CLIENT_ID,
        .topic_prefix = MQTT_TOPIC_PREFIX,
        .publish_interval_ms = MQTT_PUBLISH_INTERVAL_MS,
        .qos = MQTT_QOS,
    };

    return mqtt_service_init(&config);
}
#endif

#if defined(ENABLE_LCD_DISPLAY) && ENABLE_LCD_DISPLAY
// =============================================================================
// LCD Display Task
// =============================================================================

static esp_err_t init_lcd_display(void)
{
    ESP_LOGI(TAG, "Initializing LCD display");

#if defined(ENABLE_BUTTONS) && ENABLE_BUTTONS
    init_buttons();
#endif

#if defined(ENABLE_BATTERY_ADC) && ENABLE_BATTERY_ADC
    init_battery_adc();
#endif

    lcd_display_config_t config = {
        .pin_sclk = LCD_PIN_SCLK,
        .pin_mosi = LCD_PIN_MOSI,
        .pin_dc = LCD_PIN_DC,
        .pin_cs = LCD_PIN_CS,
        .pin_rst = LCD_PIN_RST,
        .pin_backlight = LCD_PIN_BACKLIGHT,
    };

    esp_err_t ret = lcd_display_init(&config);
    if (ret != ESP_OK) {
        return ret;
    }

    // Show splash screen
    lcd_display_splash();
    vTaskDelay(pdMS_TO_TICKS(1500));

    return ESP_OK;
}

// Diagnostic mode state
typedef enum {
    DIAG_MODE_OFF,
    DIAG_MODE_ENTERING,      // Both buttons held, counting down to enter
    DIAG_MODE_WAIT_RELEASE,  // Entered, waiting for buttons to be released
    DIAG_MODE_ON,            // Diagnostic screen active, buttons released
    DIAG_MODE_EXITING        // Confirmed exit, returning to normal
} diag_mode_t;

static uint8_t get_connected_station_count(void)
{
    wifi_sta_list_t sta_list;
    if (esp_wifi_ap_get_sta_list(&sta_list) == ESP_OK) {
        return sta_list.num;
    }
    return 0;
}

static int8_t get_wifi_tx_power(void)
{
    int8_t power = 0;
    esp_wifi_get_max_tx_power(&power);
    return power / 4;  // Convert from 0.25dBm units to dBm
}

// =============================================================================
// Deep Sleep Power Save Mode (REQ-30)
// =============================================================================
#if defined(ROVER_TARGET_TTGO) && defined(ENABLE_DEEP_SLEEP) && ENABLE_DEEP_SLEEP

// Sleep mode state machine
typedef enum {
    SLEEP_MODE_OFF,
    SLEEP_MODE_ENTERING,
    SLEEP_MODE_CONFIRMED
} sleep_mode_t;

static void enter_deep_sleep(void)
{
    ESP_LOGI(TAG, "Entering deep sleep mode...");

    // Show sleep screen with Snorlax sprite on LCD
    lcd_display_sleep_screen();
    vTaskDelay(pdMS_TO_TICKS(2000));  // Show sleep screen for 2 seconds

    // 3. Turn off LCD backlight
    lcd_display_set_backlight(0);
    vTaskDelay(pdMS_TO_TICKS(100));

    // 4. Stop WiFi
    esp_wifi_stop();
    esp_wifi_deinit();

    // 5. Wait for left button release before entering sleep
    // GPIO 0 is active LOW, so wait until it reads HIGH (released)
    ESP_LOGI(TAG, "Waiting for button release...");
    while (gpio_get_level(SLEEP_BUTTON_PIN) == 0) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    // Debounce - wait a bit more to ensure stable release
    vTaskDelay(pdMS_TO_TICKS(200));

    // 6. Configure GPIO 35 (right button) as EXT0 wake source (wake on LOW = button press)
    // Note: GPIO 35 is input-only but is an RTC GPIO, so it supports ext0 wakeup
    // Enable internal pull-up on RTC domain to prevent floating
    esp_sleep_enable_ext0_wakeup(BUTTON_RIGHT_PIN, 0);
    rtc_gpio_pullup_en(BUTTON_RIGHT_PIN);
    rtc_gpio_pulldown_dis(BUTTON_RIGHT_PIN);

    // 7. Power down unused domains for minimal power consumption
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_OFF);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_FAST_MEM, ESP_PD_OPTION_OFF);

    ESP_LOGI(TAG, "Good night! Press RIGHT button to wake up.");

    // 8. Enter deep sleep (never returns - chip resets on wake)
    esp_deep_sleep_start();
}

#endif // ROVER_TARGET_TTGO && ENABLE_DEEP_SLEEP

static void lcd_update_task(void *pvParameters)
{
    ESP_LOGI(TAG, "LCD update task started");

    // Get MAC address once at startup
    static char mac_str[18];
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    // Record start time for uptime calculation
    TickType_t start_ticks = xTaskGetTickCount();

    // Diagnostic mode tracking
    diag_mode_t diag_mode = DIAG_MODE_OFF;
    TickType_t both_buttons_start = 0;
    const TickType_t DIAG_ENTRY_HOLD_TIME = pdMS_TO_TICKS(3000);  // 3 seconds to enter
    bool prev_btn_left = false;
    bool prev_btn_right = false;

#if defined(ROVER_TARGET_TTGO) && defined(ENABLE_DEEP_SLEEP) && ENABLE_DEEP_SLEEP
    // Deep sleep mode tracking (REQ-30)
    sleep_mode_t sleep_mode = SLEEP_MODE_OFF;
    TickType_t left_button_hold_start = 0;
    const TickType_t SLEEP_ENTRY_HOLD_TIME = pdMS_TO_TICKS(SLEEP_BUTTON_HOLD_TIME_MS);
#endif

    lcd_rover_status_t lcd_status = {
        .wifi_ssid = wifi_get_current_ssid(),
        .wifi_ip = wifi_get_ip_str(),
        .mac_addr = mac_str,
        .mdns_hostname = MDNS_HOSTNAME,
    };

    while (1) {
        bool btn_left = false;
        bool btn_right = false;

#if defined(ENABLE_BUTTONS) && ENABLE_BUTTONS
        // Read button states
        btn_left = read_button_left();
        btn_right = read_button_right();
#endif

        // Calculate uptime
        uint32_t uptime_secs = (xTaskGetTickCount() - start_ticks) / configTICK_RATE_HZ;

        // Diagnostic mode state machine
        // REQ-06: Long press both buttons 3s to enter, short press any button to exit
        bool both_pressed = btn_left && btn_right;
        bool any_pressed = btn_left || btn_right;
        bool btn_left_pressed = btn_left && !prev_btn_left;   // Rising edge
        bool btn_right_pressed = btn_right && !prev_btn_right; // Rising edge

        switch (diag_mode) {
            case DIAG_MODE_OFF:
                if (both_pressed) {
                    // Start counting for diagnostic mode entry
                    both_buttons_start = xTaskGetTickCount();
                    diag_mode = DIAG_MODE_ENTERING;
                    ESP_LOGI(TAG, "Diagnostic mode: hold buttons for 3 seconds...");
                }
                break;

            case DIAG_MODE_ENTERING:
                if (!both_pressed) {
                    // Released too early
                    diag_mode = DIAG_MODE_OFF;
                } else if ((xTaskGetTickCount() - both_buttons_start) >= DIAG_ENTRY_HOLD_TIME) {
                    // Held long enough, enter diagnostic mode but wait for release first
                    diag_mode = DIAG_MODE_WAIT_RELEASE;
                    lcd_display_clear();
                    ESP_LOGI(TAG, "Diagnostic mode entered - release buttons to view");
                }
                break;

            case DIAG_MODE_WAIT_RELEASE:
                // Wait for user to release buttons after entering diagnostic mode
                if (!any_pressed) {
                    diag_mode = DIAG_MODE_ON;
                    ESP_LOGI(TAG, "Diagnostic mode active - press any button to exit");
                }
                break;

            case DIAG_MODE_ON:
                // Exit on any button press (short press)
                if (btn_left_pressed || btn_right_pressed) {
                    diag_mode = DIAG_MODE_EXITING;
                }
                break;

            case DIAG_MODE_EXITING:
                // Return to normal mode
                diag_mode = DIAG_MODE_OFF;
                lcd_display_reset_state();
                lcd_display_clear();
                ESP_LOGI(TAG, "Exiting diagnostic mode");
                break;
        }

        // Update previous button states for edge detection
        prev_btn_left = btn_left;
        prev_btn_right = btn_right;

#if defined(ROVER_TARGET_TTGO) && defined(ENABLE_DEEP_SLEEP) && ENABLE_DEEP_SLEEP
        // Deep sleep mode state machine (REQ-30)
        // Trigger: Hold LEFT button only (not both) for 5 seconds
        // This is separate from diagnostic mode (both buttons for 3s)
        bool left_only = btn_left && !btn_right;

        switch (sleep_mode) {
            case SLEEP_MODE_OFF:
                if (left_only && diag_mode == DIAG_MODE_OFF) {
                    // Start counting for sleep mode entry
                    left_button_hold_start = xTaskGetTickCount();
                    sleep_mode = SLEEP_MODE_ENTERING;
                    ESP_LOGI(TAG, "Sleep mode: hold left button for 5 seconds...");
                }
                break;

            case SLEEP_MODE_ENTERING:
                if (!left_only) {
                    // Released too early or right button pressed
                    sleep_mode = SLEEP_MODE_OFF;
                } else if ((xTaskGetTickCount() - left_button_hold_start) >= SLEEP_ENTRY_HOLD_TIME) {
                    // Held long enough - confirm sleep
                    sleep_mode = SLEEP_MODE_CONFIRMED;
                    ESP_LOGI(TAG, "Sleep mode confirmed - entering deep sleep");
                }
                break;

            case SLEEP_MODE_CONFIRMED:
                // Enter deep sleep (this function never returns)
                enter_deep_sleep();
                break;
        }
#endif

        if (diag_mode == DIAG_MODE_ON || diag_mode == DIAG_MODE_WAIT_RELEASE) {
            // Get per-core task counts (REQ-09)
            task_core_counts_t task_counts;
            get_task_core_counts(&task_counts);

            // Show diagnostic screen
            lcd_wifi_diag_t diag = {
                .ssid = wifi_get_current_ssid(),
                .ip_addr = wifi_get_ip_str(),
                .mdns_hostname = MDNS_HOSTNAME,
                .channel = WIFI_AP_CHANNEL,
                .connected_stations = get_connected_station_count(),
                .tx_power = get_wifi_tx_power(),
                .free_heap = esp_get_free_heap_size(),
                .min_free_heap = esp_get_minimum_free_heap_size(),
                .total_heap = heap_caps_get_total_size(MALLOC_CAP_DEFAULT),
                .free_internal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                .free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
                .uptime_secs = uptime_secs,
#if defined(ENABLE_BATTERY_ADC) && ENABLE_BATTERY_ADC
                .battery_volts = read_battery_voltage(),
#else
                .battery_volts = 0.0f,
#endif
#if defined(ENABLE_REST_API) && ENABLE_REST_API
                .rest_api_enabled = true,
#else
                .rest_api_enabled = false,
#endif
#if defined(ENABLE_MQTT) && ENABLE_MQTT
                .mqtt_enabled = true,
                .mqtt_connected = mqtt_service_is_connected(),
#else
                .mqtt_enabled = false,
                .mqtt_connected = false,
#endif
                .local_time = get_local_time_str(),
                .ntp_synced = s_ntp_synced,
                .build_version = BUILD_VERSION,
                .build_fingerprint = BUILD_FINGERPRINT,
            };
            lcd_display_diagnostics(&diag);
        } else if (diag_mode == DIAG_MODE_OFF) {
            // Normal operation - update rover status display
            rover_command_t cmd = {0};
            if (command_mutex && xSemaphoreTake(command_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                cmd = current_command;
                xSemaphoreGive(command_mutex);
            }

            // Update LCD status struct
            lcd_status.wifi_ssid = wifi_get_current_ssid();
            lcd_status.wifi_ip = wifi_get_ip_str();
            lcd_status.speed_percent = (int)cmd.speed;
            // Differential drive: steering shown as percentage (-100 to +100)
            lcd_status.steering_degrees = (int)cmd.steering;
            lcd_status.estop = cmd.emergency_stop;

            // Check if client connected (command age < 1 second means active)
            lcd_status.connected = (web_server_get_command_age_ms() < 1000);

            // Battery voltage
#if defined(ENABLE_BATTERY_ADC) && ENABLE_BATTERY_ADC
            lcd_status.battery_volts = read_battery_voltage();
#else
            lcd_status.battery_volts = 0.0f;
#endif

            lcd_status.button_left = btn_left;
            lcd_status.button_right = btn_right;
            lcd_status.uptime_secs = uptime_secs;

            // Update display
            lcd_display_update(&lcd_status);
        }

        vTaskDelay(pdMS_TO_TICKS(16));  // Update at ~60Hz for smooth display
    }
}
#endif

// =============================================================================
// Main Entry Point
// =============================================================================

void app_main(void)
{
#if defined(ENABLE_LOG_BUFFER) && ENABLE_LOG_BUFFER
    // REQ-31: Initialize log buffer FIRST to capture all boot logs
    log_buffer_init();
#endif

#if defined(ENABLE_TASK_WATCHDOG) && ENABLE_TASK_WATCHDOG
    // REQ-37: Initialize task watchdog timer
    esp_task_wdt_config_t wdt_config = {
        .timeout_ms = TASK_WDT_TIMEOUT_SEC * 1000,
        .idle_core_mask = 0,  // Don't monitor idle tasks
        .trigger_panic = TASK_WDT_PANIC_ON_TIMEOUT,
    };
    esp_err_t wdt_err = esp_task_wdt_init(&wdt_config);
    if (wdt_err == ESP_OK) {
        ESP_LOGI(TAG, "Task watchdog initialized (timeout: %ds, panic: %s)",
                 TASK_WDT_TIMEOUT_SEC, TASK_WDT_PANIC_ON_TIMEOUT ? "yes" : "no");
    } else {
        ESP_LOGW(TAG, "Task watchdog init failed: %s", esp_err_to_name(wdt_err));
    }
#endif

    ESP_LOGI(TAG, "ESP32-CAM Rover starting...");
    ESP_LOGI(TAG, "Build: %s (%s%s)", BUILD_FINGERPRINT, BUILD_GIT_BRANCH, BUILD_GIT_DIRTY ? "-dirty" : "");
    ESP_LOGI(TAG, "Build Time: %s", BUILD_TIME);
    ESP_LOGI(TAG, "ESP-IDF: %s", BUILD_IDF_VERSION);

#if defined(ROVER_TARGET_TTGO) && defined(ENABLE_DEEP_SLEEP) && ENABLE_DEEP_SLEEP
    // Check if we woke from deep sleep (REQ-30)
    esp_sleep_wakeup_cause_t wakeup_cause = esp_sleep_get_wakeup_cause();
    switch (wakeup_cause) {
        case ESP_SLEEP_WAKEUP_EXT0:
            ESP_LOGI(TAG, "Woke from deep sleep via button press");
            break;
        case ESP_SLEEP_WAKEUP_UNDEFINED:
            ESP_LOGI(TAG, "Normal boot (not from sleep)");
            break;
        default:
            ESP_LOGI(TAG, "Woke from deep sleep, cause: %d", wakeup_cause);
            break;
    }
#endif
    ESP_LOGI(TAG, "Free heap: %lu bytes", esp_get_free_heap_size());

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Create mutex
    command_mutex = xSemaphoreCreateMutex();
    if (!command_mutex) {
        ESP_LOGE(TAG, "Failed to create command mutex");
        return;
    }

    // Initialize WiFi (mode determined by config_generated.h)
    ESP_ERROR_CHECK(wifi_init());

    // REQ-32: Initialize mDNS for hostname resolution
    // Hostname is target-specific: esp32-rover.local (ESP32-CAM) or ttgo-rover.local (TTGO)
    {
        esp_err_t mdns_err = mdns_init();
        if (mdns_err == ESP_OK) {
            // Set hostname from config (target-specific)
            mdns_hostname_set(MDNS_HOSTNAME);
            // Set instance name for service browser
            mdns_instance_name_set(MDNS_INSTANCE_NAME);
            // Add HTTP service
            mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);
            ESP_LOGI(TAG, "mDNS initialized: http://%s.local", MDNS_HOSTNAME);
        } else {
            ESP_LOGW(TAG, "mDNS init failed: %s", esp_err_to_name(mdns_err));
        }
    }

#if defined(ENABLE_OTA) && ENABLE_OTA
    // Set hostname for OTA (uses same hostname as mDNS)
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif == NULL) {
        netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    }
    if (netif != NULL) {
        esp_netif_set_hostname(netif, OTA_HOSTNAME);
        ESP_LOGI(TAG, "OTA hostname set to: %s", OTA_HOSTNAME);
    }
#endif

    // REQ-10: Check internet connectivity if in STA mode
    if (wifi_is_sta_mode()) {
        s_internet_connected = check_internet_connectivity();

        // REQ-13: Initialize NTP if internet is available
        if (s_internet_connected) {
            init_sntp();
        }
    }

    // Initialize hardware components
#if !DISABLE_CAMERA
    ret = init_camera();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Camera init failed: %s", esp_err_to_name(ret));
        // Continue - camera will be disabled
    }

    // Initialize flash LED and blink to indicate startup
    ret = flash_led_init();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Flash LED startup blink");
        flash_led_blink(3, 100, 100);  // 3 quick blinks
    }
#else
    ESP_LOGI(TAG, "Camera disabled in config");
#endif

#if defined(ENABLE_LCD_DISPLAY) && ENABLE_LCD_DISPLAY
    ret = init_lcd_display();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LCD display init failed: %s", esp_err_to_name(ret));
        // Continue - LCD will be disabled
    }
#endif

    ret = init_web_server();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Web server init failed: %s", esp_err_to_name(ret));
        return;  // Can't continue without web server
    }

#if defined(ENABLE_OTA) && ENABLE_OTA
    // REQ-11: Register OTA update endpoint
    httpd_handle_t server_handle = web_server_get_handle();
    if (server_handle != NULL) {
        init_ota_endpoint(server_handle);
    }
#endif

#if defined(ENABLE_MQTT) && ENABLE_MQTT
    // MQTT requires WiFi connection - only start if in STA mode
    if (wifi_is_sta_mode()) {
        ret = init_mqtt_service();
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "MQTT service init failed: %s (continuing without MQTT)", esp_err_to_name(ret));
            // Continue - MQTT is optional
        }
    } else {
        ESP_LOGI(TAG, "MQTT disabled - not in STA mode");
    }
#endif

    // Create status update task on Core 0
    xTaskCreatePinnedToCore(
        status_update_task,
        "status",
        STACK_SIZE_STATUS_TASK,  // Target-specific stack size
        NULL,
        2,  // Lower priority
        &status_task_handle,
        0   // Core 0
    );

#if defined(ENABLE_LCD_DISPLAY) && ENABLE_LCD_DISPLAY
    // Create LCD update task on Core 0
    xTaskCreatePinnedToCore(
        lcd_update_task,
        "lcd",
        STACK_SIZE_LCD_TASK,  // Target-specific stack size
        NULL,
        1,  // Low priority
        &lcd_task_handle,
        0   // Core 0
    );
#endif

    ESP_LOGI(TAG, "Rover initialized successfully!");
    if (wifi_is_sta_mode()) {
        ESP_LOGI(TAG, "Connected to WiFi '%s', open http://%s", wifi_get_current_ssid(), wifi_get_ip_str());
    } else {
        ESP_LOGI(TAG, "Connect to WiFi '%s' and open http://%s", wifi_get_current_ssid(), wifi_get_ip_str());
    }
    ESP_LOGI(TAG, "Free heap: %lu bytes", esp_get_free_heap_size());
}
