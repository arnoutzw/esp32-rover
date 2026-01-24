// =============================================================================
// AUTO-GENERATED FILE - DO NOT EDIT MANUALLY
// Generated from rover_config.yaml by generate_config.py
// =============================================================================
#pragma once

#define ROVER_TARGET_TTGO 1

// WiFi Mode Configuration
#define WIFI_MODE_AP_ONLY 0
#define WIFI_MODE_STA_ONLY 0
#define WIFI_MODE_STA_FIRST 1

// WiFi AP Settings
#define WIFI_AP_SSID "ESP32-Rover"
#define WIFI_AP_PASSWORD "rover1234"
#define WIFI_AP_CHANNEL 1
#define WIFI_AP_MAX_CONN 4

// WiFi STA Settings
#define WIFI_STA_SSID "KPNCCD9C6"
#define WIFI_STA_PASSWORD "Xtvd6fh3FncT2cd8"
#define WIFI_STA_CONNECT_TIMEOUT_S 30

// REST API Configuration
#define ENABLE_REST_API 1
#define REST_API_CACHE_INTERVAL_MS 1000

// MQTT Configuration
#define ENABLE_MQTT 1
#define MQTT_BROKER_HOST "192.168.1.100"
#define MQTT_BROKER_PORT 1883
#define MQTT_USERNAME ""
#define MQTT_PASSWORD ""
#define MQTT_CLIENT_ID "esp32-rover"
#define MQTT_TOPIC_PREFIX "esp32-rover"
#define MQTT_PUBLISH_INTERVAL_MS 5000
#define MQTT_QOS 0

// Motor Configuration
#define CFG_MOTOR_POLE_PAIRS 7
#define CFG_MOTOR_VOLTAGE_LIMIT 6.0f
#define CFG_MOTOR_VELOCITY_LIMIT 20.0f
#define CFG_MOTOR_PWM_FREQ 20000
#define CFG_MOTOR_PID_P 0.2f
#define CFG_MOTOR_PID_I 2.0f
#define CFG_MOTOR_PID_D 0.0f
#define CFG_MOTOR_PID_RAMP 1000.0f
#define CFG_MOTOR_LPF_TF 0.01f

// Servo Configuration
#define CFG_SERVO_MIN_PULSE_US 500
#define CFG_SERVO_MAX_PULSE_US 2500
#define CFG_SERVO_CENTER_PULSE_US 1500
#define CFG_SERVO_MAX_ANGLE 45
#define CFG_SERVO_TRIM 0

// Control Parameters
#define CFG_CONTROL_LOOP_FREQ 100
#define CFG_WATCHDOG_TIMEOUT_MS 500
#define CFG_MAX_SPEED 100
#define CFG_SPEED_RAMP_RATE 5

// Safety Features
#define CFG_ENABLE_WATCHDOG 1
#define CFG_ENABLE_ESTOP 1

// Debug Options
#define CFG_DEBUG_MOTOR 0
#define CFG_DEBUG_ENCODER 0
#define CFG_DEBUG_SERVO 0
#define CFG_DEBUG_CAMERA 0
#define CFG_DEBUG_WEBSERVER 0
#define CFG_DEBUG_MQTT 0
