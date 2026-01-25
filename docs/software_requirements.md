# ESP32 Rover Software Requirements

## Overview

The ESP32 Rover firmware provides a web-based control interface with camera streaming capabilities.

**Current Status**: Motor/encoder/servo components have been removed from the codebase. The web GUI control interface (speed, steering, emergency stop) is preserved for future motor implementation.

## Table of Contents

1. [GPIO Allocation](#gpio-allocation)
2. [Build & Configuration](#build--configuration) (REQ-01 to REQ-04)
3. [Hardware & Drivers](#hardware--drivers) (REQ-05 to REQ-11)
4. [Networking & Connectivity](#networking--connectivity) (REQ-12 to REQ-16)
5. [User Interface](#user-interface) (REQ-17 to REQ-23)
6. [Control & Safety](#control--safety) (REQ-24 to REQ-28)
7. [Power Management](#power-management) (REQ-30)
8. [Diagnostics & Logging](#diagnostics--logging) (REQ-31, REQ-35, REQ-36)
9. [Testing & Quality](#testing--quality) (REQ-29)
10. [System Reliability](#system-reliability) (REQ-37, REQ-38)
11. [Future Requirements](#future-requirements)

---

## GPIO Allocation

All GPIO pins are defined in `main/config.h`. Pin assignments differ between hardware targets.

### TTGO T-Display Pin Map

| GPIO | Function | Direction | Notes |
|------|----------|-----------|-------|
| 0 | Button LEFT | Input | ⚠️ Bootstrap pin - pressing during boot enters download mode |
| 4 | LCD Backlight | PWM Output | Brightness control |
| 5 | LCD CS | Output | SPI chip select |
| 16 | LCD DC | Output | Data/command select |
| 18 | LCD SCLK | Output | SPI clock |
| 19 | LCD MOSI | Output | SPI data |
| 23 | LCD RST | Output | Display reset |
| 34 | Battery ADC | Input | ⚡ Input-only pin |
| 35 | Button RIGHT | Input | ⚡ Input-only pin |

**Note**: GPIO 21-22, 25-27, 32-33 available for future motor/encoder expansion.

### ESP32-CAM Pin Map

| GPIO | Function | Direction | Notes |
|------|----------|-----------|-------|
| 0 | CAM XCLK | Output | ⚠️ Bootstrap pin - needs external pull-up |
| 2 | SD Card D0 | I/O | ⚠️ Bootstrap pin - used by SD card in debug mode |
| 5 | CAM D0 | Output | Camera data |
| 12 | JTAG TDI / Available | I/O | ⛔ **CRITICAL** - Controls flash voltage at boot, requires pull-down |
| 13 | JTAG TCK / Available | I/O | Available for future use |
| 14 | JTAG TMS / SD CLK | Shared | Can be used for SD card clock or future motor/I2C |
| 15 | JTAG TDO / SD CMD | Shared | Can be used for SD card command or future motor/I2C |
| 18 | CAM D1 | Output | Camera data |
| 19 | CAM D2 | Output | Camera data |
| 21 | CAM D3 | Output | Camera data |
| 22 | CAM PCLK | Input | Camera pixel clock |
| 23 | CAM HREF | Input | Camera horizontal reference |
| 25 | CAM VSYNC | Input | Camera vertical sync |
| 26 | CAM SIOD | Open-drain | Camera I2C data (SCCB) |
| 27 | CAM SIOC | Open-drain | Camera I2C clock (SCCB) |
| 32 | CAM PWDN | Output | Camera power down |
| 33 | Status LED | Output | WiFi status indicator (inverted logic) |
| 34 | CAM D6 | Input | ⚡ Input-only pin |
| 35 | CAM D7 | Input | ⚡ Input-only pin |
| 36 | CAM D4 | Input | ⚡ Input-only pin |
| 39 | CAM D5 | Input | ⚡ Input-only pin |

**Note**: GPIO 12-15 available for JTAG debugging or SD card when enabled via build flags.

### Pin Legend

| Symbol | Meaning |
|--------|---------|
| ⛔ | Critical hardware requirement |
| ⚠️ | Bootstrap pin - affects boot behavior |
| ⚡ | Input-only GPIO (34-39) |

### Bootstrap Pin Notes

| Pin | Function | Boot Requirement |
|-----|----------|------------------|
| GPIO 0 | Download mode select | HIGH = normal boot, LOW = download mode |
| GPIO 2 | Boot strapping | Should be LOW or floating at boot |
| GPIO 12 | Flash voltage select | **Must be LOW** for 3.3V flash operation |
| GPIO 15 | SDIO timing | Controls debug output, can be floating |

### Hardware Considerations

1. **GPIO 12 on ESP32-CAM**: Requires external pull-down resistor to ground. If HIGH during boot, ESP32 selects 1.8V flash mode which will fail.

2. **GPIO 14/15 Sharing on ESP32-CAM**: These pins serve dual purposes (motor control + encoder I2C). Requires careful hardware design with pull-up resistors for I2C.

3. **Button on GPIO 0 (TTGO)**: Holding the left button during power-on will enter download mode. This is expected behavior.

**Configuration File**: `main/config.h` (lines 84-184)

---

## Build & Configuration

### REQ-01: Configuration YAML [IMPLEMENTED]

**Requirement**: A configuration YAML shall be created that configures certain options at compile time.

**Implementation**:
- Created `rover_config.yaml` in the firmware root directory
- Created `generate_config.py` script to convert YAML to C header
- Generated `main/config_generated.h` with preprocessor definitions
- Configures: WiFi settings, REST API, MQTT, motor/servo parameters, debug options

**Files**:
- `rover_config.yaml`
- `generate_config.py`
- `main/config_generated.h`

---

### REQ-02: Hardware Target Selection [IMPLEMENTED]

**Requirement**: The firmware shall support multiple hardware targets (TTGO T-Display and ESP32-CAM) with compile-time selection.

**Implementation**:
- CMake-based target selection via `ROVER_TARGET` environment variable or CMake variable
- Two supported targets: `ttgo` (TTGO T-Display) and `esp32cam` (ESP32-CAM with camera)
- Compile-time definitions: `ROVER_TARGET_TTGO=1` or `ROVER_TARGET_ESP32CAM=1`
- Default target: TTGO T-Display

**Usage**:
```bash
# Build for TTGO T-Display (default)
idf.py build

# Build for ESP32-CAM
ROVER_TARGET=esp32cam idf.py build

# Or via CMake variable
idf.py build -DROVER_TARGET=esp32cam
```

**Files**:
- `main/CMakeLists.txt` - Target selection logic and compile definitions
- `rover_config.yaml` - `target` setting

---

### REQ-03: OTA Update [IMPLEMENTED]

**Requirement**: Add OTA Update to the Configuration YAML and firmware, make sure the HOSTNAME of the ESP32-ROVER is always fixed and the OTA password shall be rover1234.

**Implementation**:
- HTTP POST endpoint `/ota` for firmware upload
- Password protected (requires `X-OTA-Password` header)
- Fixed hostname set via `esp_netif_set_hostname()`
- Compile-time toggle: `ENABLE_OTA` (0 or 1)

**Configuration Options**:
```yaml
ota:
  enabled: true
  hostname: "esp32-rover"
  password: "rover1234"
```

**OTA Endpoint**:
- URL: `POST /ota`
- Header: `X-OTA-Password: rover1234`
- Body: Binary firmware file
- Response: 200 OK on success, device reboots automatically

**Usage**:
```bash
curl -X POST -H "X-OTA-Password: rover1234" \
     --data-binary @build/esp32-rover.bin \
     http://esp32-rover.local/ota
```

**Files**:
- `rover_config.yaml` - OTA configuration section
- `generate_config.py` - Generates OTA defines
- `main/config_generated.h` - `ENABLE_OTA`, `OTA_HOSTNAME`, `OTA_PASSWORD`
- `main/main.c` - `ota_update_handler()` and `init_ota_endpoint()`

---

### REQ-04: Multi-Core Task Management [IMPLEMENTED]

**Requirement**: The rover shall distribute tasks across both ESP32 cores for optimal performance.

**Implementation**:
- FreeRTOS task pinning to specific cores
- Core 0: WiFi, networking, web server, status updates, LCD
- Core 1: Motor control (dedicated for real-time performance)
- Task priority configuration for real-time requirements

**Task Allocation**:
| Task | Core | Priority | Frequency | Description |
|------|------|----------|-----------|-------------|
| Motor Control | 1 | 5 (High) | 100 Hz | FOC loop, servo control |
| Status Update | 0 | 2 (Medium) | 20 Hz | Button polling, status collection |
| LCD Update | 0 | 1 (Low) | ~60 Hz | Display refresh, diagnostics |
| Web Server | 0 | - | Event-driven | HTTP request handling |
| WiFi | 0 | High | - | Network stack (ESP-IDF managed) |
| MQTT | 0 | 2 | 1 Hz | Telemetry publish (if enabled) |

*Source: `main/main.c` lines 1456-1488*

**Monitoring**:
- Task count per core displayed in diagnostics
- Uses `xTaskGetAffinity()` for core identification
- See REQ-21 for per-core task display

**Files**:
- `main/main.c` - Task creation with core affinity
- All component files - Task priorities and core assignments

---

## Hardware & Drivers

### REQ-05: BLDC Motor Control with FOC [IMPLEMENTED]

**Requirement**: The rover shall use a brushless DC motor with Field Oriented Control (FOC) for smooth, precise velocity control.

**Implementation**:
- Custom `bldc_motor` component implementing sinusoidal commutation
- Three-phase PWM output using ESP32 LEDC peripheral
- Configurable PWM frequency (default: 25kHz)
- Velocity control with configurable max voltage limit
- Motor enable/disable functionality
- Direction control (forward/reverse)

**Configuration Options**:
```yaml
motor:
  pwm_frequency: 25000
  max_voltage: 8.0
  pole_pairs: 7
  pins:
    phase_a: 25
    phase_b: 26
    phase_c: 27
    enable: 33
```

**API**:
- `bldc_motor_init()` - Initialize motor driver
- `bldc_motor_set_velocity()` - Set target velocity
- `bldc_motor_enable()` / `bldc_motor_disable()` - Enable/disable motor
- `bldc_motor_get_velocity()` - Get current velocity

**Files**:
- `components/bldc_motor/bldc_motor.c` - Motor control implementation
- `components/bldc_motor/include/bldc_motor.h` - Public API
- `rover_config.yaml` - Motor configuration section

---

### REQ-06: Motor Voltage Limiting [IMPLEMENTED]

**Requirement**: The motor controller shall limit maximum voltage applied to the motor to prevent damage.

**Implementation**:
- Configurable maximum voltage in YAML config
- Voltage scaling based on battery level
- Prevents over-voltage even at 100% throttle

**Configuration**:
```yaml
motor:
  max_voltage: 8.0    # Maximum motor voltage
```

**Files**:
- `components/bldc_motor/bldc_motor.c` - Voltage limiting
- `rover_config.yaml` - `motor.max_voltage` setting

---

### REQ-07: Differential Drive Steering [IMPLEMENTED]

**Requirement**: The rover shall use differential drive (tank-style) steering with two independently controlled motors.

**Implementation**:
- 3-wheel configuration: 2 front wheels (motorized), 1 rear wheel (passive caster)
- Steering achieved by varying speed difference between left and right motors
- Motor 1 (left) and Motor 2 (right) controlled via BLDC FOC
- AS5600 magnetic encoders for position feedback (one per motor)
- Note: Currently single motor due to AS5600 I2C address conflict (0x36 fixed)

**Differential Drive Calculation**:
```c
// Base velocity from speed input (-100 to 100)
float base_velocity = (speed / 100.0f) * MOTOR_VELOCITY_LIMIT;

// Steering factor from steering input (-100 to 100)
float steering_factor = (steering / 100.0f) * STEERING_SENSITIVITY;
float differential = base_velocity * steering_factor;

// Individual wheel velocities
float left_velocity = base_velocity + differential;
float right_velocity = base_velocity - differential;
```

**Steering Behavior**:
- `steering = 0`: Both wheels same speed (straight)
- `steering > 0`: Turn right (left wheel faster)
- `steering < 0`: Turn left (right wheel faster)
- `steering = ±100` with `speed = 0`: Pivot turn (wheels opposite direction)

**Configuration** (`main/config.h`):
```c
#define DIFF_DRIVE_TRACK_WIDTH_MM   200  // Distance between wheels
#define DIFF_DRIVE_WHEEL_RADIUS_MM  40   // Wheel radius
#define STEERING_SENSITIVITY        1.0f // 1.0 = full differential
```

**Hardware Note**:
Both AS5600 encoders have fixed I2C address 0x36. Options for dual encoder:
1. I2C multiplexer (TCA9548A) - recommended
2. Bit-banged software I2C on separate pins
3. Single encoder with velocity estimation

**Files**:
- `main/config.h` - Motor 1/2 pin definitions, differential drive parameters
- `main/main.c` - Differential drive control in motor task
- `components/bldc_motor/` - BLDC motor control with FOC

**Status**: IMPLEMENTED (single motor, dual motor pending I2C mux)

---

### REQ-08: AS5600 Magnetic Encoder [IMPLEMENTED]

**Requirement**: The rover shall use an AS5600 magnetic encoder for motor position feedback via I2C.

**Implementation**:
- I2C communication with AS5600 magnetic rotary encoder
- 12-bit resolution (4096 positions per revolution)
- Raw angle and filtered angle reading
- Configurable I2C pins and speed

**Configuration**:
- I2C Address: 0x36 (fixed by AS5600)
- Default I2C speed: 400kHz

**API**:
- `as5600_init()` - Initialize encoder on I2C bus
- `as5600_get_raw_angle()` - Get raw 12-bit angle value
- `as5600_get_angle_deg()` - Get angle in degrees (0-360)
- `as5600_get_angle_rad()` - Get angle in radians
- `as5600_get_cumulative_angle()` - Get cumulative angle (tracks full rotations)
- `as5600_get_velocity()` - Get angular velocity
- `as5600_get_magnet_status()` - Get magnet detection status and strength

**Files**:
- `components/as5600/as5600.c` - Encoder driver implementation
- `components/as5600/include/as5600.h` - Public API

---

### REQ-09: Battery Voltage Monitoring [IMPLEMENTED]

**Requirement**: The rover shall monitor battery voltage via ADC and display it on LCD and web interface.

**Implementation**:
- ESP32 ADC reading on GPIO 34
- Voltage divider support for higher voltage batteries (2.0 ratio)
- Low-pass filtering for stable readings
- Real-time display on LCD and web UI

**Configuration** (hardcoded in `main/config.h`):
```c
#define BATTERY_ADC_PIN      GPIO_NUM_34
#define BATTERY_DIVIDER_RATIO 2.0f
#define ENABLE_BATTERY_ADC   1
```

*Note: Battery configuration is currently hardcoded rather than in rover_config.yaml*

**Display**:
- LCD: Shows "7.4V" format next to battery icon
- Web UI: Battery voltage in status section

**Files**:
- `main/main.c` - ADC reading and voltage calculation
- `main/config.h` - Battery pin and divider configuration
- `components/lcd_display/lcd_display.c` - Battery display
- `components/web_server/web_ui.c` - Battery status in web UI

---

### REQ-10: Button Input with GPIO ISR [IMPLEMENTED]

**Requirement**: The rover shall support two physical buttons (left and right) with interrupt-driven input handling.

**Implementation**:
- GPIO interrupt service routines for button press detection
- Debouncing in software (configurable debounce time)
- Button state available to LCD and web UI
- Used for diagnostic mode entry (hold both 3s)

**Button Pins** (TTGO T-Display):
- Left button: GPIO 0
- Right button: GPIO 35

**Features**:
- Edge-triggered interrupts (falling edge)
- Software debounce (default: 50ms)
- Press and release detection
- Long-press detection for diagnostic mode

**Files**:
- `main/main.c` - GPIO ISR setup and button handling
- `components/lcd_display/lcd_display.c` - Button state display

---

### REQ-11: Camera MJPEG Streaming [IMPLEMENTED]

**Requirement**: When built for ESP32-CAM target, the rover shall provide MJPEG video streaming over HTTP.

**Implementation**:
- OV2640 camera support via ESP32-CAM module
- MJPEG streaming endpoint at `/stream`
- Configurable resolution and quality
- Only compiled when `ROVER_TARGET_ESP32CAM=1`

**Configuration**:
- Frame size: QVGA (320x240) default
- JPEG quality: 12 (0-63, lower = better)

**Streaming Endpoint**:
- URL: `GET /stream`
- Content-Type: `multipart/x-mixed-replace`

**Files**:
- `components/camera/camera.c` - Camera initialization and streaming
- `components/camera/include/camera.h` - Public API

### REQ-40: SD Card Debug Mode [NOT IMPLEMENTED]

**Requirement**: When `SD_CARD_DEBUG=1` build flag is set, enable the microSD card slot in 1-bit mode on ESP32-CAM. This frees GPIO 12 and 13 for other use.

**Rationale**:
- SD card shares pins with motor control - mutually exclusive features
- 1-bit mode reduces pin usage while maintaining SD functionality
- Enables log storage and data capture for debugging purposes

**Pin Mapping (1-bit mode)**:

| MicroSD Card | ESP32 GPIO | Notes |
|--------------|------------|-------|
| CLK | GPIO 14 | Clock |
| CMD | GPIO 15 | Command |
| DATA0 | GPIO 2 | Data line (1-bit mode) |
| DATA1/flashlight | GPIO 4 | Unused in 1-bit mode |
| DATA2 | GPIO 12 | Unused in 1-bit mode (freed) |
| DATA3 | GPIO 13 | Unused in 1-bit mode (freed) |

**Implementation**:
- CMake environment variable: `SD_CARD_DEBUG=1`
- Compile-time define: `ENABLE_SD_CARD=1`
- Mutually exclusive with motor control (motor disabled when SD enabled)
- Uses ESP-IDF SDMMC driver in 1-bit mode

**Usage**:
```bash
SD_CARD_DEBUG=1 ROVER_TARGET=esp32cam idf.py build
```

**API**:
```c
esp_err_t sd_card_init(void);
esp_err_t sd_card_deinit(void);
bool sd_card_is_mounted(void);
const char* sd_card_get_mount_point(void);  // Returns "/sdcard"
```

**Files** (to be created):
- `components/sd_card/CMakeLists.txt` - Component build config
- `components/sd_card/include/sd_card.h` - Public API
- `components/sd_card/sd_card.c` - SDMMC 1-bit mode implementation
- `main/CMakeLists.txt` - SD_CARD_DEBUG flag handling
- `main/config.h` - SD card pin defines for ESP32-CAM

**Status**: NOT IMPLEMENTED

---

## Networking & Connectivity

### REQ-12: WiFi STA-First Mode [IMPLEMENTED]

**Requirement**: Change WiFi behavior to first attempt connecting to a specified network, falling back to AP mode if unavailable. The behavior shall be configurable at compile time.

**Network Configuration**:
- STA credentials: Configured in `secrets.yaml`
- AP Network: ESP32-Rover (configurable in `rover_config.yaml`)
- AP Password: Configured in `secrets.yaml`
- Connection timeout: 30 seconds (configurable)

**Implementation**:
- Three WiFi modes: `ap_only`, `sta_only`, `sta_first`
- STA-first mode tries station connection, falls back to AP on timeout
- Timeout configurable via `wifi.sta.connect_timeout` in YAML
- Compile-time flags: `WIFI_MODE_AP_ONLY`, `WIFI_MODE_STA_ONLY`, `WIFI_MODE_STA_FIRST`

**Files**:
- `components/wifi/wifi.c` - Connection logic with fallback
- `rover_config.yaml` - WiFi configuration section

---

### REQ-13: Internet Connectivity Check [IMPLEMENTED]

**Requirement**: During WiFi init do a check for internet connectivity by pinging 1.1.1.1, if the response is valid show a green "Internet: Connected" on the status screen for both LCD and web GUI.

**Implementation**:
- Uses ESP-IDF ping API (`ping/ping_sock.h`) to ping 1.1.1.1
- Ping performed after WiFi connection in STA mode
- 3 ping attempts with 1 second timeout each
- Status displayed on both LCD diagnostic screen and Web GUI

**LCD Diagnostic Screen**:
- Shows "Internet: Connected" (green) or "Internet: Offline" (red)

**Web UI Diagnostics Panel**:
- Shows "Connected" (green) or "Offline" (red) in Services section

**Files**:
- `main/main.c` - `check_internet_connectivity()` function with ping callbacks
- `components/lcd_display/include/lcd_display.h` - Added `internet_connected` to `lcd_wifi_diag_t`
- `components/web_server/include/web_server.h` - Added `internet_connected` to `rover_status_t`
- `components/lcd_display/lcd_display.c` - Internet status display
- `components/web_server/web_ui.c` - Internet status in diagnostics panel
- `components/web_server/web_server.c` - JSON `internet` field

---

### REQ-39: Status LED Indicator [NOT IMPLEMENTED]

**Requirement**: The on-board red LED (GPIO 33 on ESP32-CAM) shall indicate WiFi connection status:
- LED uses **inverted logic** (LOW = on, HIGH = off)
- **Solid ON**: Powered but no WiFi connection
- **1Hz blink**: STA WiFi connection active (500ms on, 500ms off)
- **2Hz blink**: AP mode active (250ms on, 250ms off)

**Rationale**:
- Provides visual feedback without needing serial connection or web UI
- Helps diagnose WiFi connectivity issues
- Low-cost indicator using existing hardware

**Hardware**:
- GPIO 33 on ESP32-CAM module (on-board red LED next to RST button)
- Inverted logic: `gpio_set_level(GPIO_NUM_33, 0)` = LED ON
- ESP32-CAM only (TTGO uses GPIO 33 for motor enable)

**Implementation**:
- New `components/status_led/` component
- Called from `status_update_task` at 20Hz
- Uses `wifi_is_sta_mode()` and `wifi_is_sta_connected()` for state detection
- Simple GPIO control (no PWM required)

**Configuration** (`rover_config.yaml`):
```yaml
status_led:
  enabled: true
  sta_blink_period_ms: 1000   # 1Hz
  ap_blink_period_ms: 500     # 2Hz
```

**API**:
```c
esp_err_t status_led_init(void);
void status_led_update(bool is_sta_mode, bool is_connected);
```

**Files** (to be created):
- `components/status_led/CMakeLists.txt` - Component build config
- `components/status_led/include/status_led.h` - Public API
- `components/status_led/status_led.c` - Implementation
- `main/config.h` - Add `STATUS_LED_GPIO` for ESP32-CAM
- `main/main.c` - Initialize and call from status task

**Status**: NOT IMPLEMENTED

---



### REQ-14: HTTP REST API [IMPLEMENTED]

**Requirement**: Add an HTTP REST API endpoint serving JSON with all sensor and system diagnostic data. The API presence shall be configurable at compile time.

**Implementation**:
- `/status` endpoint returns JSON with full diagnostics
- Includes: velocity, battery, steering, motor/camera status, WiFi info, heap memory, uptime, CPU frequency, task count
- Compile-time toggle: `ENABLE_REST_API` (0 or 1)
- Conditional compilation using `#if ENABLE_REST_API`

**JSON Response Fields**:
```json
{
  "velocity": 0.0,
  "battery": 7.4,
  "steering": 0.0,
  "motor": true,
  "camera": false,
  "rssi": -45,
  "btnL": false,
  "btnR": false,
  "diag": {
    "ssid": "...",
    "ip": "...",
    "mac": "...",
    "channel": 1,
    "clients": 1,
    "txPower": 20,
    "freeHeap": 123456,
    "minHeap": 100000,
    "totalHeap": 320000,
    "freeInternal": 80000,
    "uptime": 3600,
    "cpuFreq": 240,
    "tasks": 12,
    "tasksCore0": 8,
    "tasksCore1": 4,
    "tasksNoAffinity": 0,
    "restApi": true,
    "mqttEnabled": true,
    "mqttConnected": false
  }
}
```

**Files**:
- `components/web_server/web_server.c` - Status handler
- `rover_config.yaml` - `rest_api.enabled` setting

---

### REQ-15: HTTP Control Endpoint [IMPLEMENTED]

**Requirement**: The rover shall accept control commands via HTTP POST requests.

**Implementation**:
- `POST /control` endpoint for rover commands
- JSON request body with speed, steering, and flags
- Immediate command execution
- Watchdog timer reset on valid command

**Request Format**:
```json
{
  "speed": 50.0,
  "steering": -25.0,
  "emergency_stop": false
}
```

**Response**:
```json
{
  "status": "ok"
}
```

**Parameters**:
- `speed`: -100 to 100 (percentage, negative = reverse)
- `steering`: -100 to 100 (percentage, negative = left)
- `emergency_stop`: true/false

**Files**:
- `components/web_server/web_server.c` - Control endpoint handler

---

### REQ-16: MQTT Telemetry Service [IMPLEMENTED]

**Requirement**: Add an MQTT service that publishes diagnostic data. The service shall be configurable at compile time.

**Implementation**:
- Separate `mqtt_service` component
- Publishes to configurable topic prefix (default: `esp32-rover`)
- Configurable broker host, port, credentials
- Configurable publish interval (default: 5000ms)
- Only starts when in STA mode (requires external network)
- Compile-time toggle: `ENABLE_MQTT` (0 or 1)

**Configuration Options**:
```yaml
mqtt:
  enabled: true
  broker:
    host: "192.168.1.100"
    port: 1883
    username: ""
    password: ""
  client_id: "esp32-rover"
  topic_prefix: "esp32-rover"
  publish_interval_ms: 5000
  qos: 0
```

**Files**:
- `components/mqtt_service/mqtt_service.c`
- `components/mqtt_service/include/mqtt_service.h`
- `rover_config.yaml` - MQTT configuration section

---

## User Interface

### REQ-17: Web Control Interface [IMPLEMENTED]

**Requirement**: The rover shall provide a web-based control interface accessible via browser.

**Implementation**:
- Single-page web application served from ESP32
- Real-time joystick control using touch/mouse input
- Live status display showing velocity, steering, battery
- Responsive design for mobile and desktop
- WebSocket-like polling for low latency control

**Features**:
- Virtual joystick for speed/steering control
- Battery voltage indicator
- WiFi signal strength display
- Motor enable/disable toggle
- Emergency stop button
- Diagnostic panel with system info

**Endpoints**:
- `GET /` - Main control interface HTML
- `POST /control` - Send control commands
- `GET /status` - Get current status (JSON)

**Files**:
- `components/web_server/web_ui.c` - HTML/CSS/JavaScript UI
- `components/web_server/web_server.c` - HTTP request handlers

---

### REQ-18: LCD Display with Optimized Updates [IMPLEMENTED]

**Requirement**: The rover shall display status on the TTGO T-Display LCD with optimized partial updates.

**Implementation**:
- ST7789 LCD driver (135x240 resolution)
- SPI interface with DMA transfers
- Partial screen updates to reduce flicker
- Custom font rendering for status display
- Two display modes: Main status and Diagnostics

**Main Status Screen**:
- Speed bar graph (-100% to +100%)
- Steering angle indicator
- Battery voltage
- WiFi connection status
- Button states

**Diagnostic Screen** (See REQ-19):
- WiFi details (SSID, IP, MAC, channel)
- System info (heap, CPU, tasks)
- Service status (REST, MQTT, Internet)
- Uptime and local time

**Optimization**:
- Only redraws changed elements
- Background preserved between updates
- Configurable refresh rate

**Files**:
- `components/lcd_display/lcd_display.c` - Display driver and rendering
- `components/lcd_display/include/lcd_display.h` - Public API
- `components/lcd_display/fonts/` - Font data

---

### REQ-19: Diagnostic Mode Entry/Exit [IMPLEMENTED]

**Requirement**: The diagnostic screen is entered by long pressing both buttons together for 3s. The diagnostic menu can exited by a short press of any of the 2 buttons.

**Implementation**:

**State Machine**:
```
DIAG_MODE_OFF
    │
    ├─► Both buttons pressed ──► DIAG_MODE_ENTERING
    │                                │
    │   ┌─ Released early ◄──────────┤
    │   │                            │
    │   └──► DIAG_MODE_OFF           └─► Hold 3s ──► DIAG_MODE_WAIT_RELEASE
    │                                                     │
    │                                                     └─► Release ──► DIAG_MODE_ON
    │                                                                        │
    │                                    Short press any button ─────────────┘
    │                                              │
    │                                              └──► DIAG_MODE_EXITING ──► DIAG_MODE_OFF
```

**Entry**: Hold both buttons for 3 seconds
**Stay**: Releasing buttons keeps you in diagnostic mode
**Exit**: Short press any button (left or right)

**Files**:
- `main/main.c` - Diagnostic mode state machine in `lcd_update_task()`
- `components/lcd_display/lcd_display.c` - Footer text "Press any btn to exit"

---

### REQ-20: Service Status on Diagnostic Screens [IMPLEMENTED]

**Requirement**: On the diagnostic screen (LCD and web-UI) it should be possible to see the status of both HTTP REST API and MQTT.

**Implementation**:

**LCD Diagnostic Screen**:
- Added "Services" section showing:
  - REST: ON (green) / OFF (red)
  - MQTT: OK (green) / ... (yellow, connecting) / OFF (red)

**Web UI Diagnostics Panel**:
- Added "Services" section showing:
  - REST API: ON / OFF with color coding
  - MQTT: Connected / Disconnected / OFF with color coding

**Status Fields Added**:
- `rest_api_enabled` - Compile-time REST API status
- `mqtt_enabled` - Compile-time MQTT status
- `mqtt_connected` - Runtime MQTT broker connection status

**Files**:
- `components/web_server/include/web_server.h` - Added status fields to `rover_status_t`
- `components/lcd_display/include/lcd_display.h` - Added fields to `lcd_wifi_diag_t`
- `components/lcd_display/lcd_display.c` - Services section rendering
- `components/web_server/web_ui.c` - Services section HTML/JS
- `components/web_server/web_server.c` - JSON response with service status
- `main/main.c` - Populates service status fields

---

### REQ-21: Per-Core Task Display [IMPLEMENTED]

**Requirement**: The diagnostic that shows the number of tasks running on the rover will give a split over the cores and show which task is running on which core. This should be visible on both LCD and web GUI.

**Implementation**:
- LCD displays: `Tasks: C0:X C1:Y` showing per-core task counts
- Web GUI displays: Total tasks with `C0:X C1:Y` subtitle
- Uses FreeRTOS `uxTaskGetSystemState()` and `xTaskGetAffinity()` APIs

**Files**:
- `main/main.c` - `get_task_core_counts()` helper function
- `components/lcd_display/lcd_display.c` - Per-core display
- `components/web_server/web_server.c` - JSON fields `tasksCore0`, `tasksCore1`, `tasksNoAffinity`
- `components/web_server/web_ui.c` - JavaScript to display per-core counts

---

### REQ-22: NTP Clock on Diagnostics Screen [IMPLEMENTED]

**Requirement**: Show a NTP clock on the main screen (LCD and web-GUI) that shows the local time of the device based on location of the IP address.

**Implementation**:
- Uses ESP-IDF SNTP API (`esp_sntp.h`) with pool.ntp.org and time.google.com servers
- Timezone set to CET/CEST (Central European Time with DST)
- Time displayed on both LCD main screen and Web GUI
- Sync status indicator (yellow if not synced, green when synced)

**LCD Diagnostic Screen**:
- Shows "Time: HH:MM:SS" (green when synced, yellow when not synced)

**Web UI Diagnostics Panel**:
- Shows local time in Services section with sync status

**JSON Response Fields**:
```json
{
  "diag": {
    "localTime": "14:30:45",
    "ntpSynced": true
  }
}
```

**Files**:
- `main/main.c` - `init_sntp()`, `update_local_time_string()`, `get_local_time_str()` functions
- `components/lcd_display/include/lcd_display.h` - Added `local_time` and `ntp_synced` to `lcd_wifi_diag_t`
- `components/web_server/include/web_server.h` - Added `local_time` and `ntp_synced` to `rover_status_t`
- `components/lcd_display/lcd_display.c` - Time display in diagnostics
- `components/web_server/web_ui.c` - Time display in web diagnostics
- `components/web_server/web_server.c` - JSON fields `localTime` and `ntpSynced`

---

### REQ-23: Uptime Counter on Diagnostics Screen [IMPLEMENTED]

**Requirement**: An uptime counter shall be visible on the diagnostics screen under 'System' for both LCD diagnostics and web-GUI.

**Implementation**:
- Uptime tracked since boot using `esp_timer_get_time()` converted to seconds
- Displayed in HH:MM:SS format on both LCD and web UI
- Updates in real-time on both interfaces

**LCD Diagnostic Screen**:
- Shows uptime as "HH:MM:SS" next to battery voltage
- Green color, caps at 99:59:59 for display

**Web UI Diagnostics Panel**:
- Shows uptime under "System" section as "HH:MM:SS"
- Uses `formatUptime()` JavaScript function for formatting

**JSON Response Fields**:
```json
{
  "diag": {
    "uptime": 3600
  }
}
```
Note: `uptime` is in seconds, converted to HH:MM:SS by the UI

**Files**:
- `main/main.c` - Populates `uptime_secs` in status structures
- `components/lcd_display/lcd_display.c` - Uptime display
- `components/web_server/web_ui.c` - `formatUptime()` function and display element
- `components/web_server/web_server.c` - JSON `uptime` field

---

## Control & Safety

### REQ-24: Speed Ramp Limiting [IMPLEMENTED]

**Requirement**: The rover shall implement acceleration/deceleration ramping to prevent jerky motion and reduce mechanical stress.

**Implementation**:
- Configurable ramp rate (units per control loop iteration)
- Applied to both forward and reverse directions
- Smooth transitions between speed setpoints

**Configuration**:
```yaml
control:
  speed_ramp_rate: 5    # Max speed change per iteration
```

**Behavior**:
- Speed changes are rate-limited by `CFG_SPEED_RAMP_RATE`
- Same rate applies to acceleration and deceleration
- E-Stop bypasses ramping (immediate stop)

**Files**:
- `main/main.c` - Speed ramping in control loop

---

### REQ-25: Command Watchdog [IMPLEMENTED]

**Requirement**: The rover shall implement a command watchdog that stops the motors if no commands are received within a configurable timeout.

**Implementation**:
- Configurable timeout period (default: 500ms)
- Automatic motor stop when timeout expires
- Watchdog reset on each valid command received
- Prevents runaway if controller disconnects

**Configuration**:
```yaml
control:
  watchdog_timeout_ms: 500
```

**Behavior**:
- Timer starts when motor is enabled
- Each control command resets the timer
- If timer expires: motor velocity set to 0, steering centered
- Motor remains enabled (ready for new commands)

**Files**:
- `main/main.c` - Watchdog timer implementation in control loop
- `rover_config.yaml` - `control.watchdog_timeout_ms` setting

---

### REQ-26: Emergency Stop (E-Stop) [IMPLEMENTED]

**Requirement**: The rover shall support an emergency stop function that immediately disables the motor.

**Implementation**:
- E-Stop command via web interface
- Immediate motor disable (not just velocity=0)
- E-Stop status displayed on LCD and web UI
- Requires explicit motor re-enable to resume

**Activation Methods**:
- Web UI: E-Stop button
- REST API: `POST /control` with `emergency_stop: true`

**Behavior**:
- Motor driver disabled immediately
- Velocity commands ignored until E-Stop cleared
- Visual indication on LCD (red E-STOP indicator)

**Files**:
- `main/main.c` - E-Stop handling in command processing
- `components/web_server/web_ui.c` - E-Stop button UI
- `components/lcd_display/lcd_display.c` - E-Stop indicator

---

## Power Management

### REQ-30: Deep Sleep Power Save Mode [IMPLEMENTED]

**Requirement**: To save power and extend battery life, implement a deep sleep function that activates by holding the left button for 5 seconds. The function shall turn off the LCD backlight, disable all radios (WiFi), and put the ESP32 into deep sleep mode.

**Implementation**:
- Long-press detection on left button (GPIO 0) for 5 seconds
- Only activates when left button is held alone (not during diagnostic mode)
- Sleep entry sequence:
  1. Display sleep screen with pixelated Snorlax sprite and "Zzz..." animation
  2. Show "Press RIGHT btn to wake up" message
  3. Wait 2 seconds for user to see the screen
  4. Stop motor and center servo
  5. Turn off LCD backlight
  6. Stop and deinitialize WiFi
  7. Wait for left button release (prevents immediate wake)
  8. Configure right button (GPIO 35) as RTC EXT0 wake source with pull-up
  9. Power down RTC memory domains
  10. Enter deep sleep mode

**Wake-up Method**:
- Press RIGHT button (GPIO 35 configured as RTC EXT0 wake source, triggers on LOW)
- Device performs full reboot on wake

**Power Consumption**:
- Active mode: ~180mA (typical)
- Deep sleep: ~10µA (ESP32 spec)

**Configuration** (`rover_config.yaml`):
```yaml
power:
  deep_sleep_enabled: true
  sleep_button_hold_time_ms: 5000
```

**Notes**:
- Only applicable to TTGO T-Display target (has accessible buttons)
- ESP32-CAM lacks user buttons - feature disabled for that target
- GPIO 35 is an RTC GPIO and supports ext0 wakeup despite being input-only
- Internal RTC pull-up enabled to prevent false wake triggers

**Files**:
- `main/main.c` - Sleep state machine in `lcd_update_task()`, `enter_deep_sleep()` function
- `main/config.h` - `ENABLE_DEEP_SLEEP`, `SLEEP_BUTTON_PIN`, `SLEEP_BUTTON_HOLD_TIME_MS`
- `components/lcd_display/lcd_display.c` - `lcd_display_sleep_screen()` with Snorlax sprite
- `rover_config.yaml` - Power management settings

**Status**: Implemented and tested

---

## Diagnostics & Logging

### REQ-31: Serial Log Capture and Web Display [NOT implemented]

**Requirement**: All ESP_LOG serial output since reboot shall be captured in a ring buffer and displayed in the web GUI diagnostics panel with live streaming via Server-Sent Events (SSE). 

New requirement: The ringbuffer shall be stored on the sd-card, any writefailure shall be dealt with gracefully and not cause a crash

**Rationale**:
- Enables remote debugging without physical serial connection
- Captures boot-time logs that may be missed when connecting later
- Provides real-time log visibility in the browser
- Essential for diagnosing issues on deployed devices

**Proposed Implementation**:

1. **Log Buffer Component** (`components/log_buffer/`):
   - Ring buffer using ESP-IDF `esp_ringbuf` (32KB default, configurable)
   - Hook into logging via `esp_log_set_vprintf()`
   - Passthrough to UART maintains normal serial output
   - Oldest entries automatically discarded when buffer is full
   - Thread-safe for multi-core access

2. **Log Entry Structure**:
   ```c
   typedef struct {
       uint32_t timestamp_ms;  // esp_log_timestamp()
       uint8_t  level;         // E=1, W=2, I=3, D=4, V=5
       uint8_t  tag_len;
       uint16_t msg_len;
       // Followed by: tag string + message string (no null terminators)
   } log_entry_header_t;
   ```

3. **REST API Endpoints**:
   - `GET /logs` - Download all buffered logs as plain text
   - `GET /logs/stream` - SSE stream for live log updates
   - `DELETE /logs` - Clear the log buffer
   - Query params: `level` (filter by minimum level)

4. **Web UI - Logs Panel** (in diagnostics section):
   ```html
   <div class="diag-section">
       <div class="diag-section-title">
           <span>System Logs</span>
           <select id="log-level-filter">
               <option value="1">Errors</option>
               <option value="2">Warnings+</option>
               <option value="3" selected>Info+</option>
               <option value="4">Debug+</option>
           </select>
       </div>
       <div id="log-container">
           <pre id="log-entries"></pre>
       </div>
   </div>
   ```

5. **JavaScript** (SSE client):
   ```javascript
   const logSource = new EventSource('/logs/stream');
   logSource.onmessage = (e) => {
       const entry = JSON.parse(e.data);
       appendLogEntry(entry);
   };
   ```

**Memory Budget**:
| Component | Size | Notes |
|-----------|------|-------|
| Ring buffer | 32 KB | ~400-600 log entries depending on message length |
| SSE task stack | 4 KB | Per active SSE connection |
| JSON formatting | 1 KB | Temporary, on stack |
| **Total** | ~37 KB | Per SSE client connection |

**Buffer Sizing Rationale**:
- 32KB ring buffer chosen as balance between history depth and memory usage
- No time-based rotation - simply overwrites oldest entries when full
- At typical log rates (~10 entries/second during activity), holds ~1-2 minutes of recent logs
- Sufficient to capture boot sequence and recent activity

**SSE Stream Format**:
```
event: log
data: {"t":12345,"l":"I","tag":"MAIN","msg":"Rover started"}

event: log
data: {"t":12350,"l":"W","tag":"WIFI","msg":"Reconnecting..."}
```

**Log Display Format** (text):
```
[   12345] I MAIN: ESP32 Rover starting...
[   12350] W WIFI: Connection lost, reconnecting...
[timestamp] [level] [tag]: [message]
```

**Files** (to be created/modified):
- `components/log_buffer/CMakeLists.txt` - New component
- `components/log_buffer/include/log_buffer.h` - Public API
- `components/log_buffer/log_buffer.c` - Ring buffer + vprintf hook
- `main/main.c` - Call `log_buffer_init()` early in `app_main()`
- `components/web_server/CMakeLists.txt` - Add log_buffer dependency
- `components/web_server/web_server.c` - Add `/logs` and `/logs/stream` endpoints
- `components/web_server/web_ui.c` - Add logs panel to diagnostics

**API**:
```c
// Initialize log buffer and install vprintf hook
esp_err_t log_buffer_init(void);

// Get all logs as formatted text (caller must free)
esp_err_t log_buffer_get_text(char **out_text, size_t *out_len, uint8_t min_level);

// Get buffer statistics
esp_err_t log_buffer_get_stats(size_t *count, size_t *bytes_used, size_t *bytes_dropped);

// Clear all buffered logs
void log_buffer_clear(void);

// Iterator for SSE streaming
typedef struct log_buffer_iterator log_buffer_iterator_t;
log_buffer_iterator_t* log_buffer_iterator_create(uint8_t min_level);
bool log_buffer_iterator_next(log_buffer_iterator_t *iter, char *json_out, size_t max_len);
void log_buffer_iterator_destroy(log_buffer_iterator_t *iter);
```

**Status**: Implemented

**Files**:
- `components/log_buffer/CMakeLists.txt` - Component build config
- `components/log_buffer/include/log_buffer.h` - Public API
- `components/log_buffer/log_buffer.c` - Ring buffer + vprintf hook
- `main/main.c` - `log_buffer_init()` called at start of `app_main()`
- `components/web_server/CMakeLists.txt` - Added log_buffer dependency
- `components/web_server/web_server.c` - `/logs`, `/logs/stream`, `DELETE /logs` endpoints
- `components/web_server/web_ui.c` - Logs panel in diagnostics with SSE streaming

---

### REQ-35: Log Download Button [IMPLEMENTED]

**Requirement**: The web interface shall have a button to download the buffered logs as a text file.

**Implementation**:
- "Download" button in the System Logs section of diagnostics panel
- Button triggers browser download via `/logs?download=1`
- Server adds `Content-Disposition: attachment; filename="esp32_logs.txt"` header
- Also includes "Clear" button to clear the log buffer

**Files**:
- `components/web_server/web_server.c` - `download` query param handling in `/logs` endpoint
- `components/web_server/web_ui.c` - Download and Clear buttons in logs panel

**Status**: Implemented (as part of REQ-31)

---

### REQ-36: Resource Consumption Guards and Unit Tests [IMPLEMENTED]

**Requirement**: The firmware shall include unit tests and runtime guards to protect against resource exhaustion (heap, stack, tasks) that could cause system crashes.

**Rationale**:
- ESP32 has limited RAM (~320KB internal DRAM + 4MB PSRAM if available)
- Memory exhaustion causes hard crashes without useful error messages
- Stack overflows corrupt memory silently before crashing
- Task leaks gradually consume heap until system fails
- OTA updates require sufficient free heap to succeed

**Implementation**:

1. **Resource Guard Component** (`components/resource_guard/`):
   - Runtime checks for heap, internal RAM, stack watermarks
   - Configurable thresholds via preprocessor defines
   - Safe allocation check: `resource_guard_can_alloc(size)`
   - Comprehensive status logging: `resource_guard_log_status()`

2. **Resource Thresholds** (based on ESP-IDF recommendations):
   | Resource | Minimum Threshold | Rationale |
   |----------|------------------|-----------|
   | Free Heap | 32 KB | Below this, allocations may fail |
   | Internal DRAM | 16 KB | Critical for DMA, WiFi buffers |
   | Stack Watermark | 512 bytes | Minimum safe stack remaining |
   | Max Tasks | 32 | Prevent task proliferation |

3. **Unit Tests** (`test/test_resource_guard.c`):
   - Heap above minimum threshold after boot
   - Internal DRAM above minimum threshold
   - Heap watermark (min free since boot) safe
   - Heap fragmentation acceptable
   - Safe allocation prediction works
   - Allocation cycle doesn't leak memory
   - Task stack watermarks safe
   - Task create/delete cycle doesn't leak
   - System stable after memory pressure

**API**:
```c
// Check all resources, returns ESP_OK if all pass
esp_err_t resource_guard_check_all(resource_check_result_t *result);

// Check if allocation of size bytes is safe
bool resource_guard_can_alloc(size_t size);

// Check task stack watermark
bool resource_guard_check_stack(void *task_handle, task_stack_result_t *result);

// Log current resource status
void resource_guard_log_status(void);
```

**Unity Test Macros**:
```c
TEST_ASSERT_RESOURCE_OK()        // Assert all resources OK
TEST_ASSERT_HEAP_OK()            // Assert heap only
TEST_ASSERT_CAN_ALLOC(size)      // Assert allocation is safe
TEST_ASSERT_STACK_OK(handle)     // Assert task stack OK
```

**Running Tests**:
```bash
# Build test application
cd test && idf.py build

# Flash and monitor
idf.py -p /dev/ttyUSB0 flash monitor
```

**Files**:
- `components/resource_guard/CMakeLists.txt` - Component build config
- `components/resource_guard/include/resource_guard.h` - Public API
- `components/resource_guard/resource_guard.c` - Implementation
- `test/test_resource_guard.c` - Unit tests
- `test/CMakeLists.txt` - Test application build
- `test/main/CMakeLists.txt` - Test main component

**ESP-IDF Documentation References**:
- [Heap Memory](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/mem_alloc.html)
- [Heap Debugging](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/heap_debug.html)
- [FreeRTOS Tasks](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/freertos.html)
- [Unit Testing](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/unit-tests.html)

**Status**: Implemented

---

## Testing & Quality

### REQ-29: Unit Tests [IMPLEMENTED]

**Requirement**: All requirements shall have unit tests.

**Implementation**:

**Test Framework**: Custom minimal Unity-compatible framework for host-based testing

**Test Files**:
- `test/test_config.c` - Tests for configuration (15 tests)
- `test/test_diag_state_machine.c` - Tests for diagnostic mode (12 tests)
- `test/test_runner.c` - Main test runner
- `test/unity.h` - Minimal test framework
- `test/Makefile` - Build system

**Running Tests**:
```bash
cd test
make test
```

**Test Coverage**:
| Category | Tests | Description |
|----------|-------|-------------|
| Config | 5 | Target, WiFi AP, motor, servo, control config |
| WiFi | 3 | WiFi mode flags, STA settings, timeout |
| REST API | 2 | REST API flag, cache interval |
| MQTT | 4 | MQTT flag, broker settings, publish interval, QoS |
| Services | 1 | Service status flags availability |
| Diagnostics | 12 | State machine transitions, entry/exit logic |

**Total**: 27 tests

---

## Networking (continued)

### REQ-32: mDNS Hostname [IMPLEMENTED]

**Requirement**: The ESP32_Rover shall have a fixed hostname with mDNS so it is reachable via `<hostname>.local`. The hostname shall be target-specific to allow multiple devices on the same network.

**Implementation**:
- Uses ESP-IDF mDNS component (`mdns.h`)
- Target-specific hostnames configured in `rover_config.yaml`
- Instance name: "ESP32 Rover Control"
- HTTP service registered: `_http._tcp` on port 80

**Target-Specific Hostnames**:
| Target | Hostname | URL |
|--------|----------|-----|
| ESP32-CAM | `esp32-rover` | `http://esp32-rover.local` |
| TTGO T-Display | `ttgo-rover` | `http://ttgo-rover.local` |

**Configuration** (`rover_config.yaml`):
```yaml
mdns:
  hostname_esp32cam: "esp32-rover"
  hostname_ttgo: "ttgo-rover"
  instance_name: "ESP32 Rover Control"
```

**mDNS Services**:
- HTTP web server: `_http._tcp` port 80

**Usage**:
```bash
# Access ESP32-CAM web interface
open http://esp32-rover.local

# Access TTGO web interface
open http://ttgo-rover.local

# OTA firmware update via mDNS (ESP32-CAM example)
curl -X POST -H "X-OTA-Password: rover1234" \
     --data-binary @build/esp32-rover.bin \
     http://esp32-rover.local/ota

# OTA firmware update (TTGO example)
curl -X POST -H "X-OTA-Password: rover1234" \
     --data-binary @build/esp32-rover.bin \
     http://ttgo-rover.local/ota
```

**Tested**: OTA flashing via mDNS confirmed working - 1MB firmware uploads in ~15 seconds.

**Files**:
- `rover_config.yaml` - mDNS hostname configuration
- `generate_config.py` - Generates target-specific `MDNS_HOSTNAME` define
- `main/config_generated.h` - `MDNS_HOSTNAME`, `MDNS_INSTANCE_NAME`
- `main/main.c` - mDNS initialization after WiFi
- `main/idf_component.yml` - Added `espressif/mdns` component dependency

---

### REQ-33: Conditionally Visible UI Items [IMPLEMENTED]

**Requirement**: Depending on the build target, certain web UI elements shall be visible or hidden.

| Element | ESP32-CAM | TTGO T-Display |
|---------|-----------|----------------|
| Camera stream | Visible | Hidden |
| Camera toggle button (REQ-34) | Visible | Hidden |
| Flash LED button | Visible | Hidden |
| Hardware buttons (L/R) | Hidden | Visible |
| Page title | "ESP32-CAM Rover" | "TTGO Rover" |

**Implementation**:
- `/status` endpoint returns `"target":"esp32cam"` or `"target":"ttgo"` field
- JavaScript `configureUIForTarget()` function hides/shows elements on first status fetch
- Camera stream initialization deferred until target is known (only initialized for ESP32-CAM)
- Title dynamically updated based on target

**API Response**:
```json
{
  "target": "esp32cam",
  "velocity": 0.0,
  ...
}
```

**Web UI Visibility Logic**:
```javascript
if (target === 'ttgo') {
    // Hide camera panel and flash LED
    // Hardware buttons remain visible
} else {
    // Hide hardware button indicators
    // Camera and flash LED remain visible
}
```

**Files**:
- `components/web_server/web_server.c` - Added `target` field to status JSON
- `components/web_server/web_ui.c` - Added `configureUIForTarget()` function

**Status**: Implemented and tested

---

### REQ-34: Camera Stream Toggle Button [IMPLEMENTED]

**Requirement**: The web interface for ESP32-CAM shall have a button to terminate (and restart) the camera stream. The intention is to save resources on the ESP32-CAM when the camera feed is not needed.

**Rationale**:
- Camera streaming consumes significant CPU and memory resources
- OTA updates may fail when camera stream is active (timeouts observed at ~10% progress)
- Users may not always need live video feed
- Allows better resource management during other operations

**Implementation**:
- "CAM ON/OFF" toggle button in web UI (only visible on ESP32-CAM target)
- Button state reflects current camera stream status
- When OFF:
  - `camera_capture_frame()` returns NULL
  - Stream task waits instead of sending frames
  - Camera placeholder shows "Camera Paused"
- When ON:
  - Camera stream resumes normal operation
- State persists until user changes it or device reboots (default: enabled)

**API**:
- `POST /camera` with JSON body `{"enabled": true/false}`
- `GET /camera` returns `{"enabled": true/false}`

**Web UI**:
- Button next to Flash LED button (ESP32-CAM only)
- Green when streaming, gray when stopped
- Label: "CAM ON" / "CAM OFF"

**Files**:
- `components/camera/include/camera.h` - `camera_stream_set_enabled()`, `camera_stream_is_enabled()` API
- `components/camera/camera.c` - Stream enable/disable state and functions
- `components/web_server/web_server.c` - `/camera` GET and POST endpoints
- `components/web_server/web_ui.c` - Camera toggle button UI and JavaScript

**Status**: Implemented

---

## System Reliability

### REQ-37: Task Watchdog Timer [IMPLEMENTED]

**Requirement**: Add a watchdog timer that monitors critical tasks (motor control, status update) and triggers an automatic reboot if any task becomes unresponsive.

**Rationale**:
- ESP32 tasks can hang due to deadlocks, infinite loops, or resource exhaustion
- Camera stream may freeze under high load or memory pressure
- Web server may become unresponsive during network issues
- Unresponsive rover is a safety hazard (runaway motor, no control)
- Automatic recovery preferred over manual intervention

**Implementation**:

1. **Task Watchdog Configuration** (`rover_config.yaml`):
   ```yaml
   task_watchdog:
     enabled: true
     timeout_sec: 30
     panic_on_timeout: true  # false = just log, true = reboot
   ```

2. **Generated Defines** (`config_generated.h`):
   ```c
   #define ENABLE_TASK_WATCHDOG 1
   #define TASK_WDT_TIMEOUT_SEC 30
   #define TASK_WDT_PANIC_ON_TIMEOUT 1
   ```

3. **Tasks Monitored**:
   | Task | Feed Interval | Description |
   |------|---------------|-------------|
   | `motor_ctrl` | 10ms (100Hz loop) | Motor FOC control loop |
   | `status` | 50ms (20Hz loop) | Status update and button polling |

4. **Feed Points**:
   - Motor control: Feed at end of each FOC loop iteration
   - Status task: Feed after each status update cycle

5. **Timeout Behavior**:
   - Uses ESP-IDF Task Watchdog Timer (TWDT) API
   - If `panic_on_timeout = true`: System reboots via panic handler
   - If `panic_on_timeout = false`: Log warning, continue monitoring

**API** (ESP-IDF native):
```c
// Initialize TWDT in app_main()
esp_task_wdt_config_t wdt_config = {
    .timeout_ms = TASK_WDT_TIMEOUT_SEC * 1000,
    .idle_core_mask = 0,
    .trigger_panic = TASK_WDT_PANIC_ON_TIMEOUT,
};
esp_task_wdt_init(&wdt_config);

// Subscribe task (at start of task function)
esp_task_wdt_add(NULL);

// Feed watchdog (in main loop)
esp_task_wdt_reset();
```

**Files**:
- `rover_config.yaml` - `task_watchdog:` configuration section
- `generate_config.py` - Generates watchdog defines
- `main/config_generated.h` - `ENABLE_TASK_WATCHDOG`, `TASK_WDT_TIMEOUT_SEC`, `TASK_WDT_PANIC_ON_TIMEOUT`
- `main/main.c` - TWDT initialization in `app_main()`, subscription and feed in `motor_control_task()` and `status_update_task()`
- `main/CMakeLists.txt` - Added `esp_system` to REQUIRES for `esp_task_wdt.h`

**ESP-IDF Documentation**:
- [Task Watchdog Timer](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/wdts.html)

**Status**: IMPLEMENTED

---

### REQ-38: JTAG Debugging Mode [IMPLEMENTED]

**Requirement**: Create a build flag that disables motor control GPIO pins and makes them available for JTAG debugging. This allows hardware debugging without disconnecting motor wires.

**Rationale**:
- ESP32 JTAG uses GPIO 12, 13, 14, 15 - same pins as motor control on ESP32-CAM
- Physical disconnection of motor wires is inconvenient for debugging
- Build-time flag provides clean separation of debug vs production builds
- Enables use of OpenOCD, GDB, and hardware breakpoints

**JTAG Pin Requirements**:
| Signal | GPIO | Normal Function (ESP32-CAM) |
|--------|------|------------------------------|
| TDI | GPIO 12 | Motor IN1 (Phase A) |
| TCK | GPIO 13 | Motor IN2 (Phase B) |
| TMS | GPIO 14 | Motor IN3 (Phase C) / I2C SDA |
| TDO | GPIO 15 | Motor EN / I2C SCL |

**Implementation**:

1. **Build-Time Flag**:
   - Set via environment variable: `JTAG_DEBUG=1`
   - Or via CMake: `idf.py build -DJTAG_DEBUG=1`
   - Defines `ENABLE_JTAG_DEBUG=1` when enabled

2. **CMake Integration** (`main/CMakeLists.txt`):
   ```cmake
   # REQ-38: JTAG Debug Mode - disables motor control to free GPIO 12-15 for JTAG
   if(DEFINED ENV{JTAG_DEBUG})
       set(JTAG_DEBUG $ENV{JTAG_DEBUG})
   endif()

   if(JTAG_DEBUG)
       target_compile_definitions(${COMPONENT_LIB} PUBLIC ENABLE_JTAG_DEBUG=1)
       message(STATUS "JTAG debug mode ENABLED - motor control DISABLED (GPIO 12-15 available for JTAG)")
   endif()
   ```

3. **Default Define** (`main/config.h`):
   ```c
   // REQ-38: JTAG Debug Mode
   #ifndef ENABLE_JTAG_DEBUG
   #define ENABLE_JTAG_DEBUG   0
   #endif
   ```

4. **Conditional Compilation** (`main/main.c`):
   - When JTAG_DEBUG enabled:
     - Encoder initialization skipped
     - Motor initialization skipped
     - Servo initialization skipped
     - Motor control task not created
     - Handles set to NULL for safe status reporting
   - Log message: "JTAG DEBUG MODE - Motor control DISABLED (GPIO 12-15 available for JTAG)"

**Usage**:
```bash
# Build for ESP32-CAM with JTAG debug mode
JTAG_DEBUG=1 ROVER_TARGET=esp32cam idf.py build

# Or via CMake
idf.py build -DROVER_TARGET=esp32cam -DJTAG_DEBUG=1

# Flash and start OpenOCD
idf.py -p /dev/cu.usbserial-110 flash
openocd -f interface/ftdi/esp32_devkitj_v1.cfg -f target/esp32.cfg

# Connect GDB
xtensa-esp32-elf-gdb -ex "target remote :3333" build/esp32-rover.elf
```

**JTAG Adapter Connections** (for reference):
| ESP32-CAM | JTAG Adapter |
|-----------|--------------|
| GPIO 12 | TDI |
| GPIO 13 | TCK |
| GPIO 14 | TMS |
| GPIO 15 | TDO |
| GND | GND |
| 3V3 | VCC (reference only) |

**Files**:
- `main/CMakeLists.txt` - JTAG_DEBUG environment variable and CMake variable handling
- `main/config.h` - Default `ENABLE_JTAG_DEBUG` define (0)
- `main/main.c` - Conditional compilation for motor/encoder/servo init and motor task creation

**Notes**:
- Only applicable to ESP32-CAM target (TTGO uses different motor pins)
- GPIO 12 boot state: External pull-down recommended for reliable boot
- JTAG debugging requires USB-to-JTAG adapter (ESP-PROG, FT2232H, etc.)

**Status**: IMPLEMENTED

---

## Future Requirements

(Add new requirements here as they are defined)