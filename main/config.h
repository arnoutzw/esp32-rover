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

// Default to TTGO T-Display if no target specified
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
// WiFi Configuration
// -----------------------------------------------------------------------------
#define ROVER_WIFI_MODE_AP  1   // 1 = Access Point, 0 = Station
#define WIFI_SSID           "ESP32-Rover"
#define WIFI_PASSWORD       "rover1234"
#define WIFI_CHANNEL        1
#define WIFI_MAX_CONN       4

// Station mode settings (when ROVER_WIFI_MODE_AP = 0)
#define WIFI_STA_SSID       "YourHomeNetwork"
#define WIFI_STA_PASSWORD   "YourPassword"

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
#define CAM_FRAME_SIZE      FRAMESIZE_VGA  // 640x480
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

// Camera pins not defined for TTGO (no camera)

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
