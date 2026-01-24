# ESP32 Rover Firmware - API Reference

This document provides detailed API documentation for all firmware components.

## Table of Contents

- [Overview](#overview)
- [Build Targets](#build-targets)
- [Component Architecture](#component-architecture)
- [AS5600 Magnetic Encoder](#as5600-magnetic-encoder)
- [BLDC Motor Control](#bldc-motor-control)
- [Servo Control](#servo-control)
- [Web Server](#web-server)
- [Camera Module](#camera-module)
- [LCD Display](#lcd-display)

---

## Overview

The ESP32 Rover firmware is built using ESP-IDF and follows a modular component architecture. Each component is designed to be reusable and has a clean C API.

### Supported Hardware

The firmware supports two hardware targets:

| Target | Board | Camera | LCD | Buttons | PSRAM | Notes |
|--------|-------|--------|-----|---------|-------|-------|
| **ESP32-CAM** | AI-Thinker ESP32-CAM | Yes | No | No | Required | Full features with live video |
| **TTGO T-Display** | LilyGO TTGO T-Display | No | Yes | Yes | Not available | LCD status display + buttons |

### Self-Contained Project

This project includes ESP-IDF v5.2.2 embedded in the `esp-idf/` directory. No external ESP-IDF installation required.

### Directory Structure

```
esp32-rover-firmware/
├── main/
│   ├── main.c           # Application entry point
│   └── config.h         # Hardware configuration (target-specific)
├── components/
│   ├── as5600/          # Magnetic encoder driver
│   ├── bldc_motor/      # BLDC motor controller (SimpleFOC-style)
│   ├── servo_control/   # PWM servo driver
│   ├── camera/          # Camera wrapper module (ESP32-CAM)
│   ├── lcd_display/     # ST7789 LCD driver (TTGO)
│   └── web_server/      # HTTP server with control UI
├── esp-idf/             # Embedded ESP-IDF v5.2.2
├── sdkconfig.defaults.esp32cam  # ESP32-CAM SDK configuration
├── sdkconfig.defaults.ttgo      # TTGO T-Display SDK configuration
├── build.sh             # Build script for target selection
├── setup.sh             # First-time toolchain installation
└── partitions.csv       # Flash partition table
```

---

## Build Targets

### First-Time Setup

Run once to install the ESP-IDF toolchain:

```bash
./setup.sh
```

### Quick Start

Use the build script for easy target selection:

```bash
# Build for ESP32-CAM (with camera)
./build.sh esp32cam

# Build for TTGO T-Display (with LCD + buttons)
./build.sh ttgo

# Build and flash
./build.sh esp32cam flash

# Build and flash to specific port
./build.sh ttgo flash -p /dev/cu.usbserial-0001
```

### Manual Build

You can also build manually using idf.py:

```bash
# Source the embedded ESP-IDF
source esp-idf/export.sh

# For ESP32-CAM
cp sdkconfig.defaults.esp32cam sdkconfig.defaults
ROVER_TARGET=esp32cam idf.py build

# For TTGO T-Display
cp sdkconfig.defaults.ttgo sdkconfig.defaults
ROVER_TARGET=ttgo idf.py build
```

### Target Selection in Code

The target is controlled by preprocessor defines in `config.h`:

```c
// Define one of these (or set via -D compiler flag):
#define ROVER_TARGET_ESP32CAM  1  // ESP32-CAM with camera
#define ROVER_TARGET_TTGO      1  // TTGO T-Display with LCD + buttons

// Camera is automatically enabled/disabled based on target:
#ifdef ROVER_TARGET_ESP32CAM
    #define DISABLE_CAMERA  0   // Camera enabled
#else
    #define DISABLE_CAMERA  1   // Camera disabled
#endif

// LCD and buttons enabled for TTGO:
#ifdef ROVER_TARGET_TTGO
    #define ENABLE_LCD_DISPLAY  1
    #define ENABLE_BUTTONS      1
#endif
```

### Pin Mapping by Target

#### ESP32-CAM Pins
| Function | GPIO | Notes |
|----------|------|-------|
| Motor IN1 | 12 | Boot-sensitive (keep LOW) |
| Motor IN2 | 13 | |
| Motor IN3 | 14 | |
| Motor EN | 15 | |
| Servo | 2 | Has onboard LED |
| I2C SDA | 14 | Encoder |
| I2C SCL | 15 | Encoder |
| Camera | Many | See config.h |

#### TTGO T-Display Pins
| Function | GPIO | Notes |
|----------|------|-------|
| Motor IN1 | 25 | |
| Motor IN2 | 26 | |
| Motor IN3 | 27 | |
| Motor EN | 33 | |
| Servo | 32 | |
| I2C SDA | 21 | Encoder |
| I2C SCL | 22 | Encoder |
| LCD SCLK | 18 | SPI clock |
| LCD MOSI | 19 | SPI data |
| LCD DC | 16 | Data/command |
| LCD CS | 5 | Chip select |
| LCD RST | 23 | Reset |
| LCD BL | 4 | Backlight PWM |
| Button L | 0 | Active LOW |
| Button R | 35 | Active LOW |

---

## Component Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                         main.c                                   │
│   ┌─────────────┐  ┌──────────────┐  ┌─────────────────────┐   │
│   │ WiFi Init   │  │ Hardware Init │  │ Task Creation       │   │
│   └─────────────┘  └──────────────┘  └─────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
        │                    │                    │
        ▼                    ▼                    ▼
┌───────────────┐  ┌─────────────────┐  ┌─────────────────────┐
│  web_server   │  │   bldc_motor    │  │   servo_control     │
│  (Core 0)     │  │   (Core 1)      │  │   (Core 1)          │
└───────────────┘  └─────────────────┘  └─────────────────────┘
        │                    │
        │                    ▼
        │          ┌─────────────────┐
        │          │     as5600      │
        │          │   (I2C Encoder) │
        │          └─────────────────┘
        ▼
┌───────────────┐  ┌─────────────────┐
│    camera     │  │   lcd_display   │
│  (if enabled) │  │  (TTGO only)    │
└───────────────┘  └─────────────────┘
```

### Core Allocation

- **Core 0**: WiFi stack, HTTP server, camera capture, LCD updates, status updates
- **Core 1**: Motor control loop (100Hz), servo updates

### Task Allocation

| Task | Core | Priority | Frequency | Stack |
|------|------|----------|-----------|-------|
| Motor control | 1 | 5 | 100 Hz | 4096 |
| Status update | 0 | 2 | 5 Hz | 2048 |
| LCD update | 0 | 1 | 10 Hz | 4096 |
| Web server | 0 | - | Event-driven | - |

---

## AS5600 Magnetic Encoder

The AS5600 component provides an interface to the AS5600 12-bit contactless magnetic rotary position sensor.

### Header File

```c
#include "as5600.h"
```

### Configuration

```c
typedef struct {
    i2c_port_t i2c_port;     // I2C port number (I2C_NUM_0 or I2C_NUM_1)
    gpio_num_t sda_pin;      // GPIO for I2C SDA
    gpio_num_t scl_pin;      // GPIO for I2C SCL
    uint32_t i2c_freq;       // I2C clock frequency (typically 400000)
    uint8_t i2c_addr;        // I2C address (default 0x36)
} as5600_config_t;
```

### Functions

#### `as5600_init`
```c
esp_err_t as5600_init(const as5600_config_t *config, as5600_handle_t *handle);
```
Initialize the AS5600 encoder. Configures I2C and validates communication.

**Parameters:**
- `config`: Pointer to configuration structure
- `handle`: Pointer to store the device handle

**Returns:** `ESP_OK` on success, error code otherwise

---

#### `as5600_deinit`
```c
esp_err_t as5600_deinit(as5600_handle_t handle);
```
Deinitialize the encoder and free resources.

---

#### `as5600_get_raw_angle`
```c
esp_err_t as5600_get_raw_angle(as5600_handle_t handle, uint16_t *angle);
```
Get the raw 12-bit angle value (0-4095).

---

#### `as5600_get_angle_rad`
```c
esp_err_t as5600_get_angle_rad(as5600_handle_t handle, float *angle_rad);
```
Get the angle in radians (0 to 2π).

---

#### `as5600_get_cumulative_angle`
```c
esp_err_t as5600_get_cumulative_angle(as5600_handle_t handle, float *angle_rad);
```
Get cumulative angle that tracks multiple rotations. Essential for velocity control.

---

#### `as5600_get_velocity`
```c
esp_err_t as5600_get_velocity(as5600_handle_t handle, float *velocity);
```
Get angular velocity in radians per second.

---

#### `as5600_update`
```c
esp_err_t as5600_update(as5600_handle_t handle);
```
Update internal state. Must be called periodically for accurate velocity calculation.

---

#### `as5600_get_magnet_status`
```c
esp_err_t as5600_get_magnet_status(as5600_handle_t handle,
                                    bool *detected,
                                    bool *too_strong,
                                    bool *too_weak);
```
Check magnet positioning status. Useful for debugging encoder issues.

---

## BLDC Motor Control

The BLDC motor component implements SimpleFOC-style field-oriented control for brushless DC motors.

### Header File

```c
#include "bldc_motor.h"
```

### Enumerations

```c
typedef enum {
    MOTOR_MODE_DISABLED = 0,  // Motor disabled, no output
    MOTOR_MODE_OPEN_LOOP,     // Open loop voltage control
    MOTOR_MODE_VELOCITY,      // Closed-loop velocity control
    MOTOR_MODE_ANGLE,         // Closed-loop position control
} motor_mode_t;

typedef enum {
    MOTOR_DIR_CW = 1,         // Clockwise rotation
    MOTOR_DIR_CCW = -1,       // Counter-clockwise rotation
} motor_direction_t;
```

### Configuration

```c
typedef struct {
    float kp;           // Proportional gain
    float ki;           // Integral gain
    float kd;           // Derivative gain
    float output_ramp;  // Maximum rate of change (limits acceleration)
    float limit;        // Output limit
} pid_config_t;

typedef struct {
    // PWM pins for 3-phase driver
    gpio_num_t pin_in1;
    gpio_num_t pin_in2;
    gpio_num_t pin_in3;
    gpio_num_t pin_en;       // Enable pin

    // Motor parameters
    uint8_t pole_pairs;      // Number of magnetic pole pairs
    float voltage_limit;     // Maximum voltage to apply
    float velocity_limit;    // Maximum velocity in rad/s
    motor_direction_t direction;

    // PWM configuration
    uint32_t pwm_frequency;  // Typically 20kHz

    // Control parameters
    pid_config_t velocity_pid;
    pid_config_t angle_pid;
    lpf_config_t velocity_lpf;

    // Encoder handle
    as5600_handle_t encoder;
} bldc_motor_config_t;
```

### Functions

#### `bldc_motor_init`
```c
esp_err_t bldc_motor_init(const bldc_motor_config_t *config,
                          bldc_motor_handle_t *handle);
```
Initialize the BLDC motor controller. Configures PWM channels and links to encoder.

---

#### `bldc_motor_enable` / `bldc_motor_disable`
```c
esp_err_t bldc_motor_enable(bldc_motor_handle_t handle);
esp_err_t bldc_motor_disable(bldc_motor_handle_t handle);
```
Enable or disable the motor driver output.

---

#### `bldc_motor_set_mode`
```c
esp_err_t bldc_motor_set_mode(bldc_motor_handle_t handle, motor_mode_t mode);
```
Set the motor control mode.

---

#### `bldc_motor_set_velocity`
```c
esp_err_t bldc_motor_set_velocity(bldc_motor_handle_t handle, float velocity);
```
Set target velocity in rad/s (velocity mode only).

---

#### `bldc_motor_get_velocity`
```c
esp_err_t bldc_motor_get_velocity(bldc_motor_handle_t handle, float *velocity);
```
Get current motor velocity in rad/s from encoder feedback.

---

#### `bldc_motor_loop`
```c
esp_err_t bldc_motor_loop(bldc_motor_handle_t handle);
```
Execute one iteration of the control loop. **Must be called at a fixed frequency** (typically 100Hz or higher).

---

#### `bldc_motor_emergency_stop`
```c
esp_err_t bldc_motor_emergency_stop(bldc_motor_handle_t handle);
```
Immediately stop the motor and disable output.

---

#### `bldc_motor_calibrate`
```c
esp_err_t bldc_motor_calibrate(bldc_motor_handle_t handle);
```
Calibrate the encoder zero position. The motor will briefly move during calibration.

---

## Servo Control

The servo control component provides PWM-based control for standard hobby servos.

### Header File

```c
#include "servo_control.h"
```

### Configuration

```c
typedef struct {
    gpio_num_t gpio_pin;        // PWM output GPIO
    ledc_channel_t pwm_channel; // LEDC channel
    ledc_timer_t pwm_timer;     // LEDC timer
    uint32_t pwm_freq;          // PWM frequency (typically 50Hz)
    uint16_t min_pulse_us;      // Minimum pulse width (e.g., 500µs)
    uint16_t max_pulse_us;      // Maximum pulse width (e.g., 2500µs)
    uint16_t center_pulse_us;   // Center pulse width (e.g., 1500µs)
    int16_t max_angle;          // Maximum angle in degrees from center
    int16_t trim_offset;        // Steering trim offset
} servo_config_t;
```

### Functions

#### `servo_init`
```c
esp_err_t servo_init(const servo_config_t *config, servo_handle_t *handle);
```
Initialize servo with specified configuration.

---

#### `servo_set_angle`
```c
esp_err_t servo_set_angle(servo_handle_t handle, float angle);
```
Set servo angle in degrees. Negative = left, Positive = right.

---

#### `servo_get_angle`
```c
esp_err_t servo_get_angle(servo_handle_t handle, float *angle);
```
Get current servo angle.

---

#### `servo_center`
```c
esp_err_t servo_center(servo_handle_t handle);
```
Move servo to center position.

---

#### `servo_set_trim`
```c
esp_err_t servo_set_trim(servo_handle_t handle, int16_t trim_degrees);
```
Set trim offset to compensate for mechanical alignment.

---

## Web Server

The web server component provides HTTP endpoints for rover control and status monitoring.

### Header File

```c
#include "web_server.h"
```

### Data Structures

```c
typedef struct {
    float speed;           // -100 to 100 (percentage)
    float steering;        // -100 to 100 (percentage)
    bool emergency_stop;   // Emergency stop flag
} rover_command_t;

typedef struct {
    float battery_voltage;
    float motor_velocity;
    float steering_angle;
    bool motor_enabled;
    bool camera_active;
    int wifi_rssi;
    bool button_left;      // Left button state (TTGO only)
    bool button_right;     // Right button state (TTGO only)
} rover_status_t;

typedef void (*command_callback_t)(const rover_command_t *cmd);

typedef struct {
    uint16_t port;                    // HTTP port (default 80)
    command_callback_t on_command;    // Callback for received commands
} web_server_config_t;
```

### Functions

#### `web_server_init`
```c
esp_err_t web_server_init(const web_server_config_t *config);
```
Start the HTTP server with the specified configuration.

---

#### `web_server_update_status`
```c
esp_err_t web_server_update_status(const rover_status_t *status);
```
Update the status data returned by the `/status` endpoint.

---

#### `web_server_get_command_age_ms`
```c
uint32_t web_server_get_command_age_ms(void);
```
Get time since last command was received. Used for watchdog timeout.

---

### HTTP Endpoints

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/` | GET | Serves the web control interface |
| `/stream` | GET | MJPEG video stream (if camera enabled) |
| `/control` | POST | Receive control commands (JSON) |
| `/status` | GET | Return rover status (JSON) |

### Control Command JSON Format

```json
{
    "speed": -100,      // -100 (reverse) to 100 (forward)
    "steering": 50,     // -100 (left) to 100 (right)
    "estop": false      // Emergency stop flag
}
```

### Status Response JSON Format

```json
{
    "velocity": 5.2,    // Current motor velocity (rad/s)
    "battery": 7.4,     // Battery voltage
    "steering": 15.0,   // Current steering angle
    "motor": true,      // Motor enabled
    "camera": false,    // Camera active
    "rssi": -45,        // WiFi signal strength
    "btnL": false,      // Left button pressed (TTGO only)
    "btnR": true        // Right button pressed (TTGO only)
}
```

---

## Camera Module

The camera module wraps the ESP32 camera driver for the OV2640 sensor.

### Header File

```c
#include "camera.h"
```

### Functions

#### `camera_init`
```c
esp_err_t camera_init(void);
```
Initialize the camera with configuration from `config.h`.

---

#### `camera_is_initialized`
```c
bool camera_is_initialized(void);
```
Check if camera was successfully initialized.

---

#### `camera_capture_frame`
```c
esp_err_t camera_capture_frame(uint8_t **data, size_t *len);
```
Capture a JPEG frame. Caller must call `camera_release_frame()` when done.

---

#### `camera_release_frame`
```c
void camera_release_frame(void);
```
Release the captured frame buffer.

---

## LCD Display

The LCD display component provides a driver for the ST7789 135x240 TFT LCD built into the TTGO T-Display.

### Header File

```c
#include "lcd_display.h"
```

### Configuration

```c
typedef struct {
    int pin_sclk;       // SPI clock pin
    int pin_mosi;       // SPI MOSI pin
    int pin_dc;         // Data/Command pin
    int pin_cs;         // Chip select pin
    int pin_rst;        // Reset pin
    int pin_backlight;  // Backlight control pin (-1 to disable)
} lcd_display_config_t;
```

### Status Structure

```c
typedef struct {
    int speed_percent;      // Speed -100 to +100
    int steering_degrees;   // Steering angle in degrees
    float velocity_rads;    // Actual velocity in rad/s
    float battery_volts;    // Battery voltage
    bool connected;         // WiFi client connected
    bool estop;             // Emergency stop active
    bool button_left;       // Left button pressed
    bool button_right;      // Right button pressed
    const char* wifi_ssid;  // WiFi SSID
    const char* wifi_ip;    // IP address
} lcd_rover_status_t;
```

### Functions

#### `lcd_display_init`
```c
esp_err_t lcd_display_init(const lcd_display_config_t *config);
```
Initialize the LCD display with SPI at 26MHz.

**Note:** SPI clock is limited to 26MHz for non-IOMUX pin compatibility.

---

#### `lcd_display_update`
```c
esp_err_t lcd_display_update(const lcd_rover_status_t *status);
```
Update the display with current rover status. Shows:
- Connection status header (green/red)
- Button indicators (L/R)
- Speed bar (bi-directional)
- Steering bar (bi-directional)
- Velocity reading
- Battery voltage with bar
- WiFi info footer

---

#### `lcd_display_splash`
```c
esp_err_t lcd_display_splash(void);
```
Show startup splash screen with "ESP32 ROVER" branding.

---

#### `lcd_display_set_backlight`
```c
esp_err_t lcd_display_set_backlight(uint8_t brightness);
```
Set display backlight brightness (0-100%).

---

#### `lcd_display_clear`
```c
esp_err_t lcd_display_clear(void);
```
Clear the display to black.

---

### Display Layout

```
┌─────────────────────────────┐
│ CONNECTED          [L] [R]  │  Header (20px)
├─────────────────────────────┤
│ !! E-STOP !!                │  E-stop banner (when active)
├─────────────────────────────┤
│ SPEED        +45%           │
│ ▓▓▓▓▓▓▓▓▓▓▓▓░░░░░░░░░░░░░░ │  Speed bar
│ STEER        +12°           │
│ ▓▓▓▓▓▓▓▓▓▓▓▓░░░░░░░░░░░░░░ │  Steering bar
│ VEL          3.2 r/s        │
│ BAT          7.4V ████████  │  Battery bar
├─────────────────────────────┤
│ ESP32-Rover                 │  Footer (30px)
│ 192.168.4.1                 │
└─────────────────────────────┘
```

---

## Configuration Reference

### main/config.h

The `config.h` file contains all hardware-specific configuration. It automatically selects pin mappings based on the build target.

#### Target Selection

```c
// Set one of these (at top of config.h or via compiler flag):
#define ROVER_TARGET_ESP32CAM  1  // For ESP32-CAM AI-Thinker
// OR
#define ROVER_TARGET_TTGO      1  // For TTGO T-Display

// If neither is set, defaults to TTGO
```

#### Common Configuration (all targets)

```c
// WiFi Configuration
#define ROVER_WIFI_MODE_AP  1        // 1 = AP mode, 0 = Station mode
#define WIFI_SSID           "ESP32-Rover"
#define WIFI_PASSWORD       "rover1234"

// Motor Parameters (adjust for your motor)
#define MOTOR_POLE_PAIRS    7
#define MOTOR_VOLTAGE_LIMIT 6.0f
#define MOTOR_VELOCITY_LIMIT 20.0f   // rad/s

// PID Tuning
#define MOTOR_PID_P         0.2f
#define MOTOR_PID_I         2.0f
#define MOTOR_PID_D         0.0f

// Servo
#define STEERING_MAX_ANGLE  45       // degrees

// Safety
#define WATCHDOG_TIMEOUT_MS 500      // Stop if no command received
```

#### TTGO-Specific Configuration

```c
// LCD Display
#define LCD_PIN_SCLK        GPIO_NUM_18
#define LCD_PIN_MOSI        GPIO_NUM_19
#define LCD_PIN_DC          GPIO_NUM_16
#define LCD_PIN_CS          GPIO_NUM_5
#define LCD_PIN_RST         GPIO_NUM_23
#define LCD_PIN_BACKLIGHT   GPIO_NUM_4
#define ENABLE_LCD_DISPLAY  1

// Buttons
#define BUTTON_LEFT_PIN     GPIO_NUM_0
#define BUTTON_RIGHT_PIN    GPIO_NUM_35
#define ENABLE_BUTTONS      1
```

---

## Error Handling

All functions return `esp_err_t`:

- `ESP_OK` (0): Success
- `ESP_ERR_INVALID_ARG`: Invalid parameter
- `ESP_ERR_NO_MEM`: Memory allocation failed
- `ESP_ERR_INVALID_STATE`: Invalid state for operation
- `ESP_ERR_TIMEOUT`: I2C communication timeout
- `ESP_ERR_NOT_SUPPORTED`: SPI clock speed not supported
- `ESP_FAIL`: General failure

Example error handling:

```c
esp_err_t ret = bldc_motor_init(&config, &motor_handle);
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Motor init failed: %s", esp_err_to_name(ret));
    return ret;
}
```

---

## Thread Safety

- **Web server**: Commands are protected by mutex
- **Motor control**: Runs on dedicated core with fixed timing
- **Status updates**: Protected by mutex for cross-core access
- **LCD display**: Runs on Core 0, single-threaded access
- **Button reading**: GPIO reads are atomic

---

## Memory Usage

Typical memory footprint:

| Component | RAM (bytes) |
|-----------|-------------|
| AS5600 | ~100 |
| BLDC Motor | ~500 |
| Servo | ~100 |
| Web Server | ~8000 |
| Web UI HTML | ~7000 |
| LCD Display | ~2000 |
| **Total** | ~18KB |

Free heap after initialization: ~197KB (TTGO with LCD)
