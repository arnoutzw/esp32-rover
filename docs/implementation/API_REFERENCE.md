# ESP32 Rover Firmware - API Reference

This document provides detailed API documentation for all firmware components.

## Table of Contents

- [Overview](#overview)
- [Build Targets](#build-targets)
- [Component Architecture](#component-architecture)
- [Web Server](#web-server)
- [Camera Module](#camera-module)
- [LCD Display](#lcd-display)
- [MQTT Service](#mqtt-service)
- [Log Buffer](#log-buffer)
- [Resource Guard](#resource-guard)
- [Build Info](#build-info)
- [Development Tools](#development-tools)

---

## Overview

The ESP32 Rover firmware is built using ESP-IDF v5.2.2 and follows a modular component architecture. Each component is designed to be reusable and has a clean C API.

**Note**: Motor/servo/encoder components have been removed from the codebase as of v1.6. The web control interface (speed, steering, emergency stop) is preserved for future motor implementation.

### Supported Hardware

| Target | Board | Camera | LCD | Buttons | PSRAM | Notes |
|--------|-------|--------|-----|---------|-------|-------|
| **ESP32-CAM** | AI-Thinker ESP32-CAM | Yes | No | No | Required | Full features with live video |
| **TTGO T-Display** | LilyGO TTGO T-Display | No | Yes | Yes | Not available | LCD status display + buttons |

### Self-Contained Project

This project includes ESP-IDF v5.2.2 embedded in the `firmware/esp-idf/` directory. No external ESP-IDF installation required.

### Directory Structure

```
esp32-rover-firmware/
├── firmware/                    # Firmware source code
│   ├── main/                    # Application entry point
│   │   ├── main.c
│   │   ├── config.h             # Hardware configuration
│   │   └── config_generated.h   # Generated from YAML config
│   ├── components/              # Reusable ESP-IDF components
│   │   ├── camera/              # Camera module (ESP32-CAM only)
│   │   ├── lcd_display/         # ST7789 LCD driver (TTGO only)
│   │   ├── web_server/          # HTTP server with control UI
│   │   ├── mqtt_service/        # MQTT telemetry publisher
│   │   ├── log_buffer/          # Serial log capture ring buffer
│   │   ├── resource_guard/      # Memory/stack safety guards
│   │   └── build_info/          # Auto-generated build information
│   ├── esp-idf/                 # Embedded ESP-IDF v5.2.2 (submodule)
│   └── sdkconfig.defaults.*     # Target-specific SDK configs
├── scripts/                     # Build and utility scripts
├── config/                      # Configuration files
├── test/                        # Unit tests
└── docs/                        # Documentation
```

---

## Build Targets

### First-Time Setup

Run once to install the ESP-IDF toolchain:

```bash
./scripts/setup.sh
```

### Quick Start

Use the build script for easy target selection:

```bash
# Build for ESP32-CAM (with camera)
./scripts/build.sh esp32cam

# Build for TTGO T-Display (with LCD + buttons)
./scripts/build.sh ttgo

# Build and flash
./scripts/build.sh esp32cam flash

# Build, flash, and monitor
./scripts/build.sh ttgo flash monitor

# Flash to specific port
./scripts/build.sh ttgo flash -p /dev/cu.usbserial-0001
```

### Target Selection in Code

The target is controlled by preprocessor defines:

```c
// Defined via ROVER_TARGET environment variable:
// ROVER_TARGET=esp32cam -> ROVER_TARGET_ESP32CAM=1
// ROVER_TARGET=ttgo -> ROVER_TARGET_TTGO=1

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
| Flash LED | 4 | On-board white LED |
| Camera | Many | See config.h |
| JTAG TDI | 12 | JTAG mode only |
| JTAG TCK | 13 | JTAG mode only |
| JTAG TMS | 14 | JTAG mode only |
| JTAG TDO | 15 | JTAG mode only |

#### TTGO T-Display Pins
| Function | GPIO | Notes |
|----------|------|-------|
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
│  web_server   │  │  mqtt_service   │  │   log_buffer        │
│  (Core 0)     │  │  (Core 0)       │  │   (All cores)       │
└───────────────┘  └─────────────────┘  └─────────────────────┘
        │
        ▼
┌───────────────┐  ┌─────────────────┐  ┌─────────────────────┐
│    camera     │  │   lcd_display   │  │  resource_guard     │
│  (ESP32-CAM)  │  │  (TTGO only)    │  │  (Safety checks)    │
└───────────────┘  └─────────────────┘  └─────────────────────┘
```

### Core Allocation

- **Core 0**: WiFi stack, HTTP server, camera capture, LCD updates, MQTT, status updates
- **Core 1**: Available for future motor control implementation

### Task Allocation

| Task | Core | Priority | Frequency | Notes |
|------|------|----------|-----------|-------|
| Status update | 0 | 2 | 20 Hz | Feeds task watchdog |
| LCD update | 0 | 1 | ~60 Hz | TTGO only |
| Web server | 0 | - | Event-driven | HTTP requests |
| MQTT publish | 0 | 2 | Configurable | Telemetry |

---

## Web Server

The web server component provides HTTP endpoints for rover control, status monitoring, and diagnostics.

### Header File

```c
#include "web_server.h"
```

### Data Structures

#### `rover_command_t`
```c
typedef struct {
    float speed;           // -100 to 100 (percentage)
    float steering;        // -100 to 100 (percentage, negative = left)
    bool emergency_stop;   // Emergency stop flag
} rover_command_t;
```

#### `rover_status_t`
```c
typedef struct {
    // Basic status
    float battery_voltage;
    bool camera_active;
    int wifi_rssi;
    bool button_left;       // Left button state (TTGO only)
    bool button_right;      // Right button state (TTGO only)

    // Diagnostic data
    const char* wifi_ssid;
    const char* wifi_ip;
    const char* mac_addr;
    uint8_t wifi_channel;
    uint8_t connected_clients;
    int8_t wifi_tx_power;
    uint32_t free_heap;
    uint32_t min_free_heap;
    uint32_t total_heap;
    uint32_t free_internal;
    uint32_t uptime_secs;
    float cpu_freq_mhz;
    uint8_t task_count;
    uint8_t tasks_core0;
    uint8_t tasks_core1;
    uint8_t tasks_no_affinity;

    // Service status
    bool rest_api_enabled;
    bool mqtt_enabled;
    bool mqtt_connected;
    bool internet_connected;

    // Time
    const char* local_time;
    bool ntp_synced;

    // Build information
    const char* build_version;      // e.g., "2.0.1"
    const char* build_fingerprint;  // Git commit hash (short)
    const char* build_time;         // Build timestamp (ISO 8601)
    const char* build_branch;       // Git branch name
    bool build_dirty;               // Uncommitted changes at build time
} rover_status_t;
```

#### `web_server_config_t`
```c
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

#### `web_server_stop`
```c
esp_err_t web_server_stop(void);
```
Stop the HTTP server.

---

#### `web_server_update_status`
```c
esp_err_t web_server_update_status(const rover_status_t *status);
```
Update the status data returned by the `/status` endpoint.

---

#### `web_server_get_last_command`
```c
esp_err_t web_server_get_last_command(rover_command_t *cmd);
```
Get the most recently received command.

---

#### `web_server_is_running`
```c
bool web_server_is_running(void);
```
Check if the server is currently running.

---

#### `web_server_get_command_age_ms`
```c
uint32_t web_server_get_command_age_ms(void);
```
Get time since last command was received. Used for watchdog timeout.

---

#### `web_server_get_handle`
```c
httpd_handle_t web_server_get_handle(void);
```
Get HTTP server handle for adding custom endpoints.

---

### HTTP Endpoints

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/` | GET | Web control interface |
| `/stream` | GET | MJPEG video stream (ESP32-CAM only) |
| `/control` | POST | Send control commands (JSON) |
| `/status` | GET | Return rover status (JSON) |
| `/camera` | GET/POST | Get/set camera stream state |
| `/logs` | GET | Get buffered logs |
| `/logs/stream` | GET | SSE stream of live logs |
| `/logs` | DELETE | Clear log buffer |
| `/ota` | POST | Upload firmware update |

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
    "target": "esp32cam",
    "battery": 7.4,
    "camera": true,
    "rssi": -45,
    "btnL": false,
    "btnR": true,
    "diag": {
        "ssid": "MyNetwork",
        "ip": "192.168.1.50",
        "mac": "AA:BB:CC:DD:EE:FF",
        "channel": 6,
        "clients": 1,
        "txPower": 20,
        "freeHeap": 150000,
        "minFreeHeap": 120000,
        "totalHeap": 320000,
        "freeInternal": 100000,
        "uptime": 3600,
        "cpuFreq": 240,
        "taskCount": 12,
        "tasksCore0": 8,
        "tasksCore1": 4,
        "tasksNoAffinity": 0,
        "localTime": "14:30:45",
        "ntpSynced": true,
        "internet": true,
        "restApi": true,
        "mqttEnabled": true,
        "mqttConnected": false,
        "buildVersion": "2.0.1",
        "buildFingerprint": "e4b11fc",
        "buildTime": "2026-01-25T09:54:04Z",
        "buildBranch": "main",
        "buildDirty": false
    }
}
```

---

## Camera Module

The camera module wraps the ESP32 camera driver for the OV2640 sensor (ESP32-CAM only).

### Header File

```c
#include "camera.h"
```

### Configuration

```c
typedef struct {
    // Pin configuration
    int pin_pwdn;
    int pin_reset;
    int pin_xclk;
    int pin_sccb_sda;
    int pin_sccb_scl;
    int pin_d7, pin_d6, pin_d5, pin_d4;
    int pin_d3, pin_d2, pin_d1, pin_d0;
    int pin_vsync;
    int pin_href;
    int pin_pclk;

    // Camera settings
    uint32_t xclk_freq;
    pixformat_t pixel_format;
    framesize_t frame_size;
    int jpeg_quality;
    int fb_count;
} camera_config_params_t;
```

### Functions

#### `camera_module_init`
```c
esp_err_t camera_module_init(const camera_config_params_t *config);
```
Initialize the camera with the specified configuration.

---

#### `camera_module_deinit`
```c
esp_err_t camera_module_deinit(void);
```
Deinitialize camera and free resources.

---

#### `camera_capture_frame`
```c
camera_fb_t* camera_capture_frame(void);
```
Capture a JPEG frame. Returns NULL if streaming is disabled.

---

#### `camera_return_frame`
```c
void camera_return_frame(camera_fb_t *fb);
```
Return frame buffer after use.

---

#### `camera_set_resolution`
```c
esp_err_t camera_set_resolution(framesize_t frame_size);
```
Set camera resolution.

---

#### `camera_set_quality`
```c
esp_err_t camera_set_quality(int quality);
```
Set JPEG quality (0-63, lower is better).

---

#### `camera_set_vflip` / `camera_set_hmirror`
```c
esp_err_t camera_set_vflip(bool flip);
esp_err_t camera_set_hmirror(bool mirror);
```
Set vertical flip or horizontal mirror.

---

#### `camera_is_initialized`
```c
bool camera_is_initialized(void);
```
Check if camera was successfully initialized.

---

#### `camera_stream_set_enabled` / `camera_stream_is_enabled`
```c
void camera_stream_set_enabled(bool enabled);
bool camera_stream_is_enabled(void);
```
Enable/disable camera streaming. When disabled, `camera_capture_frame()` returns NULL. Useful for freeing resources during OTA updates.

---

### Flash LED Control

```c
esp_err_t flash_led_init(void);
void flash_led_on(void);
void flash_led_off(void);
void flash_led_set(bool on);
bool flash_led_get_state(void);
void flash_led_blink(int count, int on_ms, int off_ms);
```

Control the on-board flash LED (GPIO 4).

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
    int pin_backlight;  // Backlight control pin
} lcd_display_config_t;
```

### Status Structures

#### `lcd_rover_status_t`
```c
typedef struct {
    int speed_percent;      // Speed -100 to +100
    int steering_degrees;   // Steering angle in degrees
    float battery_volts;    // Battery voltage
    bool connected;         // WiFi client connected
    bool estop;             // Emergency stop active
    bool button_left;       // Left button pressed
    bool button_right;      // Right button pressed
    const char* wifi_ssid;  // WiFi SSID
    const char* wifi_ip;    // IP address
    const char* mac_addr;   // MAC address string
    const char* mdns_hostname; // mDNS hostname
    uint32_t uptime_secs;   // Uptime in seconds
} lcd_rover_status_t;
```

#### `lcd_wifi_diag_t`
```c
typedef struct {
    const char* ssid;
    const char* ip_addr;
    const char* mdns_hostname;
    uint8_t channel;
    uint8_t connected_stations;
    int8_t tx_power;
    uint32_t free_heap;
    uint32_t min_free_heap;
    uint32_t total_heap;
    uint32_t free_internal;
    uint32_t free_psram;
    uint32_t uptime_secs;
    float battery_volts;
    bool rest_api_enabled;
    bool mqtt_enabled;
    bool mqtt_connected;
    const char* local_time;
    bool ntp_synced;
    const char* build_version;
    const char* build_fingerprint;
} lcd_wifi_diag_t;
```

### Functions

#### `lcd_display_init`
```c
esp_err_t lcd_display_init(const lcd_display_config_t *config);
```
Initialize the LCD display with SPI at 26MHz.

---

#### `lcd_display_update`
```c
esp_err_t lcd_display_update(const lcd_rover_status_t *status);
```
Update the display with current rover status.

---

#### `lcd_display_splash`
```c
esp_err_t lcd_display_splash(void);
```
Show startup splash screen.

---

#### `lcd_display_diagnostics`
```c
esp_err_t lcd_display_diagnostics(const lcd_wifi_diag_t *diag);
```
Show diagnostic screen with WiFi and system info.

---

#### `lcd_display_sleep_screen`
```c
esp_err_t lcd_display_sleep_screen(void);
```
Show sleep screen with animated Snorlax before entering deep sleep.

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

#### `lcd_display_reset_state`
```c
void lcd_display_reset_state(void);
```
Reset display state (call when exiting diagnostic mode).

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
│ BAT          7.4V ████████  │  Battery bar
├─────────────────────────────┤
│ ESP32-Rover                 │  Footer (30px)
│ 192.168.4.1                 │
│ Uptime: 01:23:45            │
└─────────────────────────────┘
```

---

## MQTT Service

The MQTT service provides telemetry publishing for remote monitoring.

### Header File

```c
#include "mqtt_service.h"
```

### Configuration

```c
typedef struct {
    const char *broker_host;        // MQTT broker hostname or IP
    uint16_t broker_port;           // MQTT broker port (default 1883)
    const char *username;           // MQTT username (can be NULL)
    const char *password;           // MQTT password (can be NULL)
    const char *client_id;          // MQTT client ID
    const char *topic_prefix;       // Topic prefix for all messages
    uint32_t publish_interval_ms;   // Interval between status publishes
    int qos;                        // QoS level (0, 1, or 2)
} mqtt_service_config_t;
```

### Functions

#### `mqtt_service_init`
```c
esp_err_t mqtt_service_init(const mqtt_service_config_t *config);
```
Initialize and start the MQTT service.

---

#### `mqtt_service_stop`
```c
esp_err_t mqtt_service_stop(void);
```
Stop the MQTT service.

---

#### `mqtt_service_update_status`
```c
esp_err_t mqtt_service_update_status(const rover_status_t *status);
```
Update the rover status for MQTT publishing.

---

#### `mqtt_service_is_connected`
```c
bool mqtt_service_is_connected(void);
```
Check if MQTT service is connected to the broker.

---

## Log Buffer

The log buffer component captures serial logs in a ring buffer for web UI access.

### Header File

```c
#include "log_buffer.h"
```

### Constants

```c
#define LOG_BUFFER_SIZE      (16 * 1024)  // 16KB buffer
#define LOG_ENTRY_MAX_SIZE   512          // Max single entry size

// Log levels (matching ESP-IDF)
#define LOG_LEVEL_NONE       0
#define LOG_LEVEL_ERROR      1
#define LOG_LEVEL_WARN       2
#define LOG_LEVEL_INFO       3
#define LOG_LEVEL_DEBUG      4
#define LOG_LEVEL_VERBOSE    5
```

### Statistics Structure

```c
typedef struct {
    size_t entry_count;       // Number of log entries in buffer
    size_t bytes_used;        // Bytes currently used in buffer
    size_t bytes_dropped;     // Total bytes dropped due to buffer full
    size_t buffer_size;       // Total buffer size
} log_buffer_stats_t;
```

### Functions

#### `log_buffer_init`
```c
esp_err_t log_buffer_init(void);
```
Initialize log buffer and install vprintf hook. Should be called early in app_main() to capture boot logs.

---

#### `log_buffer_deinit`
```c
void log_buffer_deinit(void);
```
Deinitialize log buffer and restore original vprintf.

---

#### `log_buffer_is_initialized`
```c
bool log_buffer_is_initialized(void);
```
Check if log buffer is initialized.

---

#### `log_buffer_get_text`
```c
esp_err_t log_buffer_get_text(char **out_text, size_t *out_len, uint8_t min_level);
```
Get all logs as formatted text. Caller must free the returned string.

---

#### `log_buffer_get_stats`
```c
esp_err_t log_buffer_get_stats(log_buffer_stats_t *stats);
```
Get log buffer statistics.

---

#### `log_buffer_clear`
```c
void log_buffer_clear(void);
```
Clear all buffered logs.

---

#### `log_buffer_get_read_position` / `log_buffer_read_next`
```c
size_t log_buffer_get_read_position(void);
bool log_buffer_read_next(size_t *position, char *json_out, size_t max_len, uint8_t min_level);
```
SSE streaming support. Get read position and read next entry as JSON.

---

## Resource Guard

The resource guard component provides runtime checks to prevent resource exhaustion crashes.

### Header File

```c
#include "resource_guard.h"
```

### Thresholds

```c
#define RESOURCE_GUARD_MIN_FREE_HEAP        (32 * 1024)  // 32KB minimum
#define RESOURCE_GUARD_MIN_FREE_INTERNAL    (16 * 1024)  // 16KB minimum
#define RESOURCE_GUARD_MIN_STACK_WATERMARK  512          // 512 bytes
#define RESOURCE_GUARD_MAX_TASKS            32           // Max tasks
#define RESOURCE_GUARD_WARN_HEAP_PERCENT    80           // Warn threshold
```

### Result Structures

```c
typedef struct {
    // Heap statistics
    size_t total_heap;
    size_t free_heap;
    size_t min_free_heap;
    size_t free_internal;
    size_t largest_free_block;

    // Task statistics
    uint32_t task_count;
    uint32_t tasks_core0;
    uint32_t tasks_core1;

    // Check results
    bool heap_ok;
    bool internal_ok;
    bool fragmentation_ok;
    bool task_count_ok;
    bool all_ok;
} resource_check_result_t;

typedef struct {
    const char *task_name;
    uint32_t stack_watermark;
    uint32_t stack_size;
    bool stack_ok;
} task_stack_result_t;
```

### Functions

#### `resource_guard_init`
```c
esp_err_t resource_guard_init(void);
```
Initialize resource guard (called automatically).

---

#### `resource_guard_check_all`
```c
esp_err_t resource_guard_check_all(resource_check_result_t *result);
```
Perform all resource checks. Returns `ESP_OK` if all pass.

---

#### `resource_guard_check_heap`
```c
bool resource_guard_check_heap(resource_check_result_t *result);
```
Check heap resources only.

---

#### `resource_guard_can_alloc`
```c
bool resource_guard_can_alloc(size_t size);
```
Check if allocation of given size is safe.

---

#### `resource_guard_check_stack`
```c
bool resource_guard_check_stack(void *task_handle, task_stack_result_t *result);
```
Check task stack usage. Pass NULL for current task.

---

#### `resource_guard_check_all_stacks`
```c
int resource_guard_check_all_stacks(void);
```
Check all task stacks and log warnings. Returns count of low watermarks.

---

#### `resource_guard_get_heap_usage_percent`
```c
uint8_t resource_guard_get_heap_usage_percent(void);
```
Get current heap usage percentage (0-100).

---

#### `resource_guard_log_status`
```c
void resource_guard_log_status(void);
```
Log current resource status to console.

---

### Unity Test Macros

```c
TEST_ASSERT_RESOURCE_OK()           // Assert all resources OK
TEST_ASSERT_HEAP_OK()               // Assert heap only
TEST_ASSERT_CAN_ALLOC(size)         // Assert allocation is safe
TEST_ASSERT_STACK_OK(handle)        // Assert task stack OK
```

---

## Build Info

Auto-generated build information is available via the `build_info.h` header.

### Header File

```c
#include "build_info.h"
```

### Defines

```c
#define BUILD_GIT_HASH       "e4b11fc"              // Short commit hash
#define BUILD_GIT_HASH_FULL  "e4b11fc..."           // Full commit hash
#define BUILD_GIT_BRANCH     "main"                 // Branch name
#define BUILD_GIT_DIRTY      true/false             // Uncommitted changes
#define BUILD_TIME           "2026-01-25T09:54:04Z" // ISO 8601 timestamp
#define BUILD_TIMESTAMP      1769334844UL           // Unix epoch
#define BUILD_IDF_VERSION    "v5.2.2"               // ESP-IDF version
#define BUILD_FINGERPRINT    "e4b11fc"              // Same as short hash
#define BUILD_VERSION        "2.0.1"                // From git tags
```

This header is regenerated at each build by `scripts/generate_build_info.sh`.

---

## Error Handling

All functions return `esp_err_t`:

- `ESP_OK` (0): Success
- `ESP_ERR_INVALID_ARG`: Invalid parameter
- `ESP_ERR_NO_MEM`: Memory allocation failed
- `ESP_ERR_INVALID_STATE`: Invalid state for operation
- `ESP_ERR_TIMEOUT`: Communication timeout
- `ESP_FAIL`: General failure

Example error handling:

```c
esp_err_t ret = web_server_init(&config);
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Web server init failed: %s", esp_err_to_name(ret));
    return ret;
}
```

---

## Thread Safety

- **Web server**: Commands are protected by mutex
- **Status updates**: Protected by mutex for cross-core access
- **LCD display**: Runs on Core 0, single-threaded access
- **Button reading**: GPIO reads are atomic
- **Log buffer**: Thread-safe ring buffer with mutex protection
- **MQTT service**: Internal mutex for status updates

---

## Memory Usage

Typical memory footprint (varies by configuration):

| Component | RAM (bytes) |
|-----------|-------------|
| Web Server | ~8000 |
| Web UI HTML | ~7000 |
| LCD Display | ~2000 |
| Log Buffer | ~16000 |
| MQTT Service | ~2000 |
| Camera buffers | ~40000 (ESP32-CAM) |
| **Total** | ~35KB (TTGO), ~75KB (ESP32-CAM) |

Free heap after initialization:
- TTGO: ~180KB
- ESP32-CAM: ~150KB (with PSRAM available for camera buffers)

---

## Development Tools

### Bug Report & Feature Request Tools

The project includes tools for filing bug reports and feature requests in a structured format that enables AI-assisted investigation and implementation.

#### GUI Tool (file_report.py)

Desktop application using tkinter for filing reports locally.

```bash
# Open main menu
python scripts/file_report.py

# Open bug report form directly
python scripts/file_report.py --bug

# Open feature request form directly
python scripts/file_report.py --feature

# Pre-fill version info for bug reports
python scripts/file_report.py --version 2.0.3 --hash abc1234
```

**Features:**
- Bug report form with version, git hash, title, description, steps to reproduce
- Feature request form with structured fields (what, why, how)
- Fetch build info from device via REST API
- Saves reports as markdown in `docs/bugreport/` and `docs/feature_requests/`

#### Web Tool (file_report_web.py)

Flask web application providing the same functionality accessible via browser (locally or remotely via VPN).

```bash
# Setup (first time)
cd esp32-rover-firmware
python3 -m venv .venv
source .venv/bin/activate
pip install flask

# Run the web server
python3 scripts/file_report_web.py

# With custom host/port
python3 scripts/file_report_web.py --host 0.0.0.0 --port 8080
```

**Access:** `http://localhost:5000` (or via VPN for remote access)

**Features:**
- Same functionality as GUI tool but browser-based
- Dark theme matching ESP32 Rover web UI
- Fetch build info from device via REST API
- Mobile-friendly responsive design

#### Report File Structure

**Bug Reports:** `docs/bugreport/v{VERSION}/{HASH}/Bugreport_*.md`
```markdown
# Bug Report: [Title]

## Build Information
| Field | Value |
|-------|-------|
| **Version** | v2.0.3 |
| **Git Hash** | abc1234 |
| **Status** | Open |

## Description
[Bug description]

## Steps to Reproduce
[Reproduction steps]
```

**Feature Requests:** `docs/feature_requests/Feature_*.md`
```markdown
# Feature Request: [Title]

| Field | Value |
|-------|-------|
| **Requested** | 2026-01-25 23:35:38 |
| **Status** | New |

## What I Want
[Feature description]

## Why I Need It
[Justification]

## How I Imagine It Working
[Expected behavior]
```

**Status Values:**
- Bug reports: `Open` → `Fixed (hash)`
- Feature requests: `New` → `Implemented (hash)`
