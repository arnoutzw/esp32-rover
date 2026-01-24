#include <stdio.h>
#include <string.h>
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

#include "config.h"
#include "as5600.h"
#include "bldc_motor.h"
#include "servo_control.h"
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

static const char *TAG = "ROVER_MAIN";

// Global handles
static as5600_handle_t encoder_handle = NULL;
static bldc_motor_handle_t motor_handle = NULL;
static servo_handle_t servo_handle = NULL;

// Control state
static rover_command_t current_command = {0};
static SemaphoreHandle_t command_mutex = NULL;

// Task handles
static TaskHandle_t motor_task_handle = NULL;
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
    ESP_LOGI(TAG, "WiFi AP started. SSID: %s, Password: %s", WIFI_AP_SSID, WIFI_AP_PASSWORD);
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

// =============================================================================
// Command Callback
// =============================================================================

static void on_rover_command(const rover_command_t *cmd)
{
    if (command_mutex && xSemaphoreTake(command_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        current_command = *cmd;
        xSemaphoreGive(command_mutex);
    }

    if (cmd->emergency_stop) {
        ESP_LOGW(TAG, "Emergency stop activated!");
        if (motor_handle) {
            bldc_motor_emergency_stop(motor_handle);
        }
    }

#if DEBUG_WEBSERVER
    ESP_LOGI(TAG, "Command: speed=%.1f, steering=%.1f, estop=%d",
             cmd->speed, cmd->steering, cmd->emergency_stop);
#endif
}

// =============================================================================
// Motor Control Task (runs on Core 1)
// =============================================================================

static void motor_control_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Motor control task started on core %d", xPortGetCoreID());

    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t loop_period = pdMS_TO_TICKS(1000 / CONTROL_LOOP_FREQ);

    rover_command_t cmd = {0};

    while (1) {
        // Get latest command
        if (command_mutex && xSemaphoreTake(command_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
            cmd = current_command;
            xSemaphoreGive(command_mutex);
        }

        // Check for command timeout (watchdog)
#if ENABLE_WATCHDOG
        uint32_t cmd_age = web_server_get_command_age_ms();
        if (cmd_age > WATCHDOG_TIMEOUT_MS) {
            // No recent commands, stop motors
            cmd.speed = 0;
            cmd.steering = 0;
        }
#endif

        // Update motor velocity
        if (motor_handle) {
            // Convert speed percentage to velocity (rad/s)
            float target_velocity = (cmd.speed / 100.0f) * MOTOR_VELOCITY_LIMIT;
            bldc_motor_set_velocity(motor_handle, target_velocity);
            bldc_motor_loop(motor_handle);
        }

        // Update servo steering
        if (servo_handle) {
            // Convert steering percentage to angle
            float steering_angle = (cmd.steering / 100.0f) * STEERING_MAX_ANGLE;
            servo_set_angle(servo_handle, steering_angle);
        }

        // Maintain loop timing
        vTaskDelayUntil(&last_wake_time, loop_period);
    }
}

// =============================================================================
// Status Update Task
// =============================================================================

static void status_update_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Status update task started");

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

        // Get motor velocity
        if (motor_handle) {
            bldc_motor_get_velocity(motor_handle, &status.motor_velocity);
            status.motor_enabled = true;  // TODO: track actual state
        }

        // Get steering angle
        if (servo_handle) {
            servo_get_angle(servo_handle, &status.steering_angle);
        }

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

        // Update web server status
        web_server_update_status(&status);

#if defined(ENABLE_MQTT) && ENABLE_MQTT
        // Update MQTT service status
        mqtt_service_update_status(&status);
#endif

        vTaskDelay(pdMS_TO_TICKS(50));  // Update at 20Hz for responsive button feedback
    }
}

// =============================================================================
// Initialization
// =============================================================================

static esp_err_t init_encoder(void)
{
    ESP_LOGI(TAG, "Initializing AS5600 encoder");

    as5600_config_t config = {
        .i2c_port = ENCODER_I2C_PORT,
        .sda_pin = ENCODER_I2C_SDA,
        .scl_pin = ENCODER_I2C_SCL,
        .i2c_freq = ENCODER_I2C_FREQ,
        .i2c_addr = AS5600_I2C_ADDR,
    };

    return as5600_init(&config, &encoder_handle);
}

static esp_err_t init_motor(void)
{
    ESP_LOGI(TAG, "Initializing BLDC motor");

    bldc_motor_config_t config = {
        .pin_in1 = MOTOR_PIN_IN1,
        .pin_in2 = MOTOR_PIN_IN2,
        .pin_in3 = MOTOR_PIN_IN3,
        .pin_en = MOTOR_PIN_EN,
        .pole_pairs = MOTOR_POLE_PAIRS,
        .voltage_limit = MOTOR_VOLTAGE_LIMIT,
        .velocity_limit = MOTOR_VELOCITY_LIMIT,
        .direction = MOTOR_DIR_CW,
        .pwm_frequency = MOTOR_PWM_FREQ,
        .velocity_pid = {
            .kp = MOTOR_PID_P,
            .ki = MOTOR_PID_I,
            .kd = MOTOR_PID_D,
            .output_ramp = MOTOR_PID_RAMP,
            .limit = MOTOR_VOLTAGE_LIMIT,
        },
        .angle_pid = {
            .kp = 10.0f,
            .ki = 0.0f,
            .kd = 0.0f,
            .output_ramp = 0,
            .limit = MOTOR_VELOCITY_LIMIT,
        },
        .velocity_lpf = {
            .tf = MOTOR_LPF_TF,
        },
        .encoder = encoder_handle,
    };

    esp_err_t ret = bldc_motor_init(&config, &motor_handle);
    if (ret != ESP_OK) {
        return ret;
    }

    // Calibrate motor
    ESP_LOGI(TAG, "Calibrating motor...");
    bldc_motor_calibrate(motor_handle);

    // Enable motor and set to velocity mode
    bldc_motor_enable(motor_handle);
    bldc_motor_set_mode(motor_handle, MOTOR_MODE_VELOCITY);
    bldc_motor_set_velocity(motor_handle, 0);

    return ESP_OK;
}

static esp_err_t init_servo(void)
{
    ESP_LOGI(TAG, "Initializing steering servo");

    servo_config_t config = {
        .gpio_pin = SERVO_PIN,
        .pwm_channel = SERVO_PWM_CHANNEL,
        .pwm_timer = SERVO_PWM_TIMER,
        .pwm_freq = SERVO_PWM_FREQ,
        .min_pulse_us = SERVO_MIN_PULSE_US,
        .max_pulse_us = SERVO_MAX_PULSE_US,
        .center_pulse_us = SERVO_CENTER_PULSE_US,
        .max_angle = STEERING_MAX_ANGLE,
        .trim_offset = STEERING_TRIM,
    };

    esp_err_t ret = servo_init(&config, &servo_handle);
    if (ret != ESP_OK) {
        return ret;
    }

    // Center servo
    servo_center(servo_handle);

    return ESP_OK;
}

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
    DIAG_MODE_ENTERING,  // Both buttons held, counting down
    DIAG_MODE_ON,        // Diagnostic screen shown
    DIAG_MODE_EXITING    // Buttons released, returning to normal
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
    const TickType_t DIAG_HOLD_TIME = pdMS_TO_TICKS(3000);  // 3 seconds

    lcd_rover_status_t lcd_status = {
        .wifi_ssid = wifi_get_current_ssid(),
        .wifi_ip = wifi_get_ip_str(),
        .mac_addr = mac_str,
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
        bool both_pressed = btn_left && btn_right;

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
                } else if ((xTaskGetTickCount() - both_buttons_start) >= DIAG_HOLD_TIME) {
                    // Held long enough, enter diagnostic mode
                    diag_mode = DIAG_MODE_ON;
                    lcd_display_clear();
                    ESP_LOGI(TAG, "Entering diagnostic mode");
                }
                break;

            case DIAG_MODE_ON:
                if (!both_pressed) {
                    // Buttons released, exit diagnostic mode
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

        if (diag_mode == DIAG_MODE_ON) {
            // Show diagnostic screen
            lcd_wifi_diag_t diag = {
                .ssid = wifi_get_current_ssid(),
                .ip_addr = wifi_get_ip_str(),
                .mac_addr = mac_str,
                .channel = WIFI_AP_CHANNEL,
                .connected_stations = get_connected_station_count(),
                .tx_power = get_wifi_tx_power(),
                .free_heap = esp_get_free_heap_size(),
                .min_free_heap = esp_get_minimum_free_heap_size(),
                .total_heap = heap_caps_get_total_size(MALLOC_CAP_DEFAULT),
                .free_internal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                .free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
                .uptime_secs = uptime_secs,
                .cpu_freq_mhz = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ,
                .task_count = uxTaskGetNumberOfTasks(),
#if defined(ENABLE_BATTERY_ADC) && ENABLE_BATTERY_ADC
                .battery_volts = read_battery_voltage(),
#else
                .battery_volts = 0.0f,
#endif
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
            lcd_status.steering_degrees = (int)((cmd.steering / 100.0f) * STEERING_MAX_ANGLE);
            lcd_status.estop = cmd.emergency_stop;

            // Get motor velocity
            if (motor_handle) {
                bldc_motor_get_velocity(motor_handle, &lcd_status.velocity_rads);
            }

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
    ESP_LOGI(TAG, "ESP32-CAM Rover starting...");
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

    // Initialize hardware components
    ret = init_encoder();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Encoder init failed: %s (continuing without encoder)", esp_err_to_name(ret));
        // Continue without encoder - will use open-loop control
    }

    ret = init_motor();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Motor init failed: %s", esp_err_to_name(ret));
        // Continue - motor control will be disabled
    }

    ret = init_servo();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Servo init failed: %s", esp_err_to_name(ret));
        // Continue - steering will be disabled
    }

#if !DISABLE_CAMERA
    ret = init_camera();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Camera init failed: %s", esp_err_to_name(ret));
        // Continue - camera will be disabled
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

    // Create motor control task on Core 1
    xTaskCreatePinnedToCore(
        motor_control_task,
        "motor_ctrl",
        4096,
        NULL,
        5,  // High priority
        &motor_task_handle,
        1   // Core 1
    );

    // Create status update task on Core 0
    xTaskCreatePinnedToCore(
        status_update_task,
        "status",
        4096,  // Increased from 2048 for diagnostic data collection
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
        4096,
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
