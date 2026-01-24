#pragma once

#include "driver/gpio.h"

// =============================================================================
// ESP32 WiFi Rover Configuration
// =============================================================================
// Build Target Selection:
//   - Define ROVER_TARGET_ESP32CAM for ESP32-CAM AI-Thinker (with camera)
//   - Define ROVER_TARGET_TTGO for TTGO T-Display (no camera)
//
// Set the target here or via compiler flag: -DROVER_TARGET_ESP32CAM=1
// =============================================================================

// Include auto-generated configuration from rover_config.yaml
// Run: python generate_config.py to regenerate after YAML changes
#include "config_generated.h"

// Default to TTGO T-Display if no target specified (override from config_generated.h)
#if !defined(ROVER_TARGET_ESP32CAM) && !defined(ROVER_TARGET_TTGO)
    #define ROVER_TARGET_TTGO       1
#endif

// Validate only one target is selected
#if defined(ROVER_TARGET_ESP32CAM) && defined(ROVER_TARGET_TTGO)
    #error "Only one target can be defined: ROVER_TARGET_ESP32CAM or ROVER_TARGET_TTGO"
#endif

// Set camera enable/disable based on target
#ifdef ROVER_TARGET_ESP32CAM
    #define DISABLE_CAMERA      0   // Camera enabled for ESP32-CAM
#else
    #define DISABLE_CAMERA      1   // Camera disabled for TTGO T-Display
#endif

// -----------------------------------------------------------------------------
// REQ-38: JTAG Debug Mode
// -----------------------------------------------------------------------------
// When enabled, motor control is disabled to free GPIO 12-15 for JTAG debugging.
// Set via CMake: JTAG_DEBUG=1 ROVER_TARGET=esp32cam idf.py build
// Only useful for ESP32-CAM target (TTGO uses different motor pins).
#ifndef ENABLE_JTAG_DEBUG
#define ENABLE_JTAG_DEBUG   0
#endif

// -----------------------------------------------------------------------------
// WiFi Configuration (defaults - overridden by config_generated.h if present)
// -----------------------------------------------------------------------------
#ifndef WIFI_MODE_AP_ONLY
#define WIFI_MODE_AP_ONLY   1   // Default to AP mode
#endif
#ifndef WIFI_MODE_STA_ONLY
#define WIFI_MODE_STA_ONLY  0
#endif
#ifndef WIFI_MODE_STA_FIRST
#define WIFI_MODE_STA_FIRST 0
#endif

// AP settings (defaults)
#ifndef WIFI_AP_SSID
#define WIFI_AP_SSID        "ESP32-Rover"
#endif
#ifndef WIFI_AP_PASSWORD
#define WIFI_AP_PASSWORD    "rover1234"
#endif
#ifndef WIFI_AP_CHANNEL
#define WIFI_AP_CHANNEL     1
#endif
#ifndef WIFI_AP_MAX_CONN
#define WIFI_AP_MAX_CONN    4
#endif

// STA settings (defaults)
#ifndef WIFI_STA_SSID
#define WIFI_STA_SSID       "YourHomeNetwork"
#endif
#ifndef WIFI_STA_PASSWORD
#define WIFI_STA_PASSWORD   "YourPassword"
#endif
#ifndef WIFI_STA_CONNECT_TIMEOUT_S
#define WIFI_STA_CONNECT_TIMEOUT_S 10
#endif

// Legacy compatibility defines
#define WIFI_SSID           WIFI_AP_SSID
#define WIFI_PASSWORD       WIFI_AP_PASSWORD
#define WIFI_CHANNEL        WIFI_AP_CHANNEL
#define WIFI_MAX_CONN       WIFI_AP_MAX_CONN

// =============================================================================
// Target-Specific Pin Configurations
// =============================================================================

#ifdef ROVER_TARGET_ESP32CAM
// -----------------------------------------------------------------------------
// ESP32-CAM AI-Thinker Pin Configuration
// Note: Many pins are used by camera, limited GPIO available
// Available pins: GPIO 12, 13, 14, 15 (some have boot restrictions)
// -----------------------------------------------------------------------------

// Motor Control Pins - using available GPIO on ESP32-CAM
// WARNING: GPIO 12 affects flash voltage at boot - keep LOW during boot
#define MOTOR_PIN_IN1       GPIO_NUM_12
#define MOTOR_PIN_IN2       GPIO_NUM_13
#define MOTOR_PIN_IN3       GPIO_NUM_14
#define MOTOR_PIN_EN        GPIO_NUM_15

// Encoder Configuration (AS5600) - shared I2C with camera SCCB
// Note: Camera uses GPIO 26 (SIOD) and 27 (SIOC) for SCCB
// We need separate I2C pins for encoder
#define ENCODER_I2C_SDA     GPIO_NUM_14  // Shared with motor, configure carefully
#define ENCODER_I2C_SCL     GPIO_NUM_15  // Shared with motor, configure carefully
#define ENCODER_I2C_PORT    I2C_NUM_1    // Use I2C port 1 (camera uses port 0)
#define ENCODER_I2C_FREQ    400000       // 400kHz I2C clock
#define AS5600_I2C_ADDR     0x36         // AS5600 default address

// Servo Configuration - using GPIO 2 (has LED, usable after boot)
#define SERVO_PIN           GPIO_NUM_2
#define SERVO_PWM_FREQ      50           // 50Hz for servo
#define SERVO_PWM_CHANNEL   LEDC_CHANNEL_3
#define SERVO_PWM_TIMER     LEDC_TIMER_1

// Camera Configuration (ESP32-CAM AI Thinker)
#define CAM_PIN_PWDN        GPIO_NUM_32
#define CAM_PIN_RESET       -1           // Not connected
#define CAM_PIN_XCLK        GPIO_NUM_0
#define CAM_PIN_SIOD        GPIO_NUM_26
#define CAM_PIN_SIOC        GPIO_NUM_27
#define CAM_PIN_D7          GPIO_NUM_35
#define CAM_PIN_D6          GPIO_NUM_34
#define CAM_PIN_D5          GPIO_NUM_39
#define CAM_PIN_D4          GPIO_NUM_36
#define CAM_PIN_D3          GPIO_NUM_21
#define CAM_PIN_D2          GPIO_NUM_19
#define CAM_PIN_D1          GPIO_NUM_18
#define CAM_PIN_D0          GPIO_NUM_5
#define CAM_PIN_VSYNC       GPIO_NUM_25
#define CAM_PIN_HREF        GPIO_NUM_23
#define CAM_PIN_PCLK        GPIO_NUM_22

#define CAM_XCLK_FREQ       20000000     // 20MHz XCLK
#define CAM_FRAME_SIZE      FRAMESIZE_QVGA // 320x240 (lower resolution, less noise)
#define CAM_JPEG_QUALITY    12           // 0-63, lower is better quality

#else // ROVER_TARGET_TTGO
// -----------------------------------------------------------------------------
// TTGO T-Display Pin Configuration
// More GPIO available since no camera
// Avoid: GPIO 0,2,5,12,15 (boot), 34-39 (input only), 16-17 (PSRAM on some)
// -----------------------------------------------------------------------------

// Motor Control Pins (SimpleFOC)
#define MOTOR_PIN_IN1       GPIO_NUM_25
#define MOTOR_PIN_IN2       GPIO_NUM_26
#define MOTOR_PIN_IN3       GPIO_NUM_27
#define MOTOR_PIN_EN        GPIO_NUM_33

// Encoder Configuration (AS5600)
#define ENCODER_I2C_SDA     GPIO_NUM_21
#define ENCODER_I2C_SCL     GPIO_NUM_22
#define ENCODER_I2C_PORT    I2C_NUM_0
#define ENCODER_I2C_FREQ    400000       // 400kHz I2C clock
#define AS5600_I2C_ADDR     0x36         // AS5600 default address

// Servo Configuration (MG90S)
#define SERVO_PIN           GPIO_NUM_32
#define SERVO_PWM_FREQ      50           // 50Hz for servo
#define SERVO_PWM_CHANNEL   LEDC_CHANNEL_3
#define SERVO_PWM_TIMER     LEDC_TIMER_1

// LCD Display Configuration (ST7789 135x240)
// TTGO T-Display built-in LCD pins
#define LCD_PIN_SCLK        GPIO_NUM_18
#define LCD_PIN_MOSI        GPIO_NUM_19
#define LCD_PIN_DC          GPIO_NUM_16
#define LCD_PIN_CS          GPIO_NUM_5
#define LCD_PIN_RST         GPIO_NUM_23
#define LCD_PIN_BACKLIGHT   GPIO_NUM_4
#define ENABLE_LCD_DISPLAY  1            // Enable LCD on TTGO

// Front Button Configuration (TTGO T-Display built-in buttons)
#define BUTTON_LEFT_PIN     GPIO_NUM_0   // Left button (active LOW)
#define BUTTON_RIGHT_PIN    GPIO_NUM_35  // Right button (active LOW)
#define ENABLE_BUTTONS      1            // Enable button support

// Battery ADC Configuration (TTGO T-Display internal battery)
// GPIO 34 is connected to battery through a 100K/100K voltage divider (2:1 ratio)
#define BATTERY_ADC_PIN     GPIO_NUM_34
#define BATTERY_ADC_CHANNEL ADC_CHANNEL_6   // GPIO 34 = ADC1 channel 6
#define BATTERY_DIVIDER_RATIO 2.0f          // Voltage divider ratio
#define ENABLE_BATTERY_ADC  1               // Enable battery ADC reading

// Camera pins not defined for TTGO (no camera)

// Deep Sleep Power Save Configuration (TTGO only - has accessible buttons)
#define ENABLE_DEEP_SLEEP           1
#define SLEEP_BUTTON_PIN            GPIO_NUM_0      // Left button triggers sleep
#define SLEEP_BUTTON_HOLD_TIME_MS   5000            // 5 second hold to enter sleep

#endif // Target selection

// =============================================================================
// Common Configuration (shared between targets)
// =============================================================================

// -----------------------------------------------------------------------------
// Motor PWM Configuration
// -----------------------------------------------------------------------------
#define MOTOR_PWM_FREQ      20000        // 20kHz PWM frequency
#define MOTOR_PWM_RES       LEDC_TIMER_10_BIT  // 10-bit resolution (0-1023)
#define MOTOR_PWM_MAX       1023

// Motor Control Parameters
#define MOTOR_POLE_PAIRS    7            // Number of pole pairs (adjust for your motor)
#define MOTOR_VOLTAGE_LIMIT 6.0f         // Voltage limit for motor
#define MOTOR_VELOCITY_LIMIT 20.0f       // Maximum velocity in rad/s

// PID Parameters for velocity control
#define MOTOR_PID_P         0.2f
#define MOTOR_PID_I         2.0f
#define MOTOR_PID_D         0.0f
#define MOTOR_PID_RAMP      1000.0f
#define MOTOR_LPF_TF        0.01f        // Low pass filter time constant

// -----------------------------------------------------------------------------
// Servo Configuration (common)
// -----------------------------------------------------------------------------
// Servo pulse width (microseconds)
#define SERVO_MIN_PULSE_US  500          // Full left
#define SERVO_MAX_PULSE_US  2500         // Full right
#define SERVO_CENTER_PULSE_US 1500       // Center position

// Steering angles
#define STEERING_MAX_ANGLE  45           // Maximum steering angle in degrees
#define STEERING_TRIM       0            // Steering trim offset

// -----------------------------------------------------------------------------
// Web Server Configuration
// -----------------------------------------------------------------------------
#define WEB_SERVER_PORT     80
#define STREAM_BOUNDARY     "frame"
#define MAX_CLIENTS         4

// -----------------------------------------------------------------------------
// Control Parameters
// -----------------------------------------------------------------------------
#define CONTROL_LOOP_FREQ   100          // Control loop frequency in Hz
#define WATCHDOG_TIMEOUT_MS 500          // Stop motors if no command received

// Speed limits
#define MAX_SPEED           100          // Maximum speed percentage (0-100)
#define SPEED_RAMP_RATE     5            // Speed change per control loop iteration

// -----------------------------------------------------------------------------
// Safety Features
// -----------------------------------------------------------------------------
#define ENABLE_WATCHDOG     1            // Enable command timeout watchdog
#define ENABLE_ESTOP        1            // Enable emergency stop feature

// -----------------------------------------------------------------------------
// Debug Options
// -----------------------------------------------------------------------------
#define DEBUG_MOTOR         0            // Enable motor debug output
#define DEBUG_ENCODER       0            // Enable encoder debug output
#define DEBUG_SERVO         0            // Enable servo debug output
#define DEBUG_CAMERA        0            // Enable camera debug output
#define DEBUG_WEBSERVER     0            // Enable web server debug output
