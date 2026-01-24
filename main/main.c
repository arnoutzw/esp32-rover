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
#include "nvs_flash.h"

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
// WiFi Configuration
// =============================================================================

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
                ESP_LOGI(TAG, "Disconnected, reconnecting...");
                esp_wifi_connect();
                break;
            default:
                break;
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
    }
}

static esp_err_t wifi_init_ap(void)
{
    ESP_LOGI(TAG, "Initializing WiFi AP mode");

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Use RAM storage to avoid NVS overriding our config
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = WIFI_SSID,
            .ssid_len = strlen(WIFI_SSID),
            .channel = WIFI_CHANNEL,
            .password = WIFI_PASSWORD,
            .max_connection = WIFI_MAX_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
        },
    };

    if (strlen(WIFI_PASSWORD) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi AP started. SSID: %s, Password: %s", WIFI_SSID, WIFI_PASSWORD);
    return ESP_OK;
}

static esp_err_t wifi_init_sta(void)
{
    ESP_LOGI(TAG, "Initializing WiFi Station mode");

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_STA_SSID,
            .password = WIFI_STA_PASSWORD,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi Station connecting to: %s", WIFI_STA_SSID);
    return ESP_OK;
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

        // Battery voltage (placeholder - implement ADC reading if needed)
        status.battery_voltage = 7.4f;

        // WiFi RSSI
        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            status.wifi_rssi = ap_info.rssi;
        }

        // Update web server status
        web_server_update_status(&status);

        vTaskDelay(pdMS_TO_TICKS(200));  // Update at 5Hz
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

#if defined(ENABLE_LCD_DISPLAY) && ENABLE_LCD_DISPLAY
// =============================================================================
// LCD Display Task
// =============================================================================

static esp_err_t init_lcd_display(void)
{
    ESP_LOGI(TAG, "Initializing LCD display");

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

static void lcd_update_task(void *pvParameters)
{
    ESP_LOGI(TAG, "LCD update task started");

    lcd_rover_status_t lcd_status = {
        .wifi_ssid = WIFI_SSID,
        .wifi_ip = "192.168.4.1",
    };

    while (1) {
        // Get current command
        rover_command_t cmd = {0};
        if (command_mutex && xSemaphoreTake(command_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            cmd = current_command;
            xSemaphoreGive(command_mutex);
        }

        // Update LCD status struct
        lcd_status.speed_percent = (int)cmd.speed;
        lcd_status.steering_degrees = (int)((cmd.steering / 100.0f) * STEERING_MAX_ANGLE);
        lcd_status.estop = cmd.emergency_stop;

        // Get motor velocity
        if (motor_handle) {
            bldc_motor_get_velocity(motor_handle, &lcd_status.velocity_rads);
        }

        // Check if client connected (command age < 1 second means active)
        lcd_status.connected = (web_server_get_command_age_ms() < 1000);

        // Battery voltage (placeholder)
        lcd_status.battery_volts = 7.4f;

        // Update display
        lcd_display_update(&lcd_status);

        vTaskDelay(pdMS_TO_TICKS(100));  // Update at 10Hz
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

    // Initialize WiFi
#if ROVER_WIFI_MODE_AP
    ESP_ERROR_CHECK(wifi_init_ap());
#else
    ESP_ERROR_CHECK(wifi_init_sta());
#endif

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
        2048,
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
    ESP_LOGI(TAG, "Connect to WiFi '%s' and open http://192.168.4.1", WIFI_SSID);
    ESP_LOGI(TAG, "Free heap: %lu bytes", esp_get_free_heap_size());
}
