# ESP32 Rover Software Requirements

## Table of Contents

1. [GPIO Allocation](#gpio-allocation)
2. [Build & Configuration](#build--configuration) (REQ-01 to REQ-04)
3. [Hardware & Drivers](#hardware--drivers) (REQ-05 to REQ-11)
4. [Networking & Connectivity](#networking--connectivity) (REQ-12 to REQ-16)
5. [User Interface](#user-interface) (REQ-17 to REQ-22)
6. [Control & Safety](#control--safety) (REQ-23 to REQ-28)
7. [Testing & Quality](#testing--quality) (REQ-29)
8. [Future Requirements](#future-requirements) (REQ-30+)

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
| 21 | Encoder I2C SDA | Open-drain | AS5600 data |
| 22 | Encoder I2C SCL | Open-drain | AS5600 clock |
| 23 | LCD RST | Output | Display reset |
| 25 | Motor IN1 | PWM Output | Phase A |
| 26 | Motor IN2 | PWM Output | Phase B |
| 27 | Motor IN3 | PWM Output | Phase C |
| 32 | Servo PWM | PWM Output | Steering servo |
| 33 | Motor EN | Output | Motor enable |
| 34 | Battery ADC | Input | ⚡ Input-only pin |
| 35 | Button RIGHT | Input | ⚡ Input-only pin |

### ESP32-CAM Pin Map

| GPIO | Function | Direction | Notes |
|------|----------|-----------|-------|
| 0 | CAM XCLK | Output | ⚠️ Bootstrap pin - needs external pull-up |
| 2 | Servo PWM | PWM Output | ⚠️ Bootstrap pin |
| 5 | CAM D0 | Output | Camera data |
| 12 | Motor IN1 | PWM Output | ⛔ **CRITICAL** - Controls flash voltage at boot, requires pull-down |
| 13 | Motor IN2 | PWM Output | Phase B |
| 14 | Motor IN3 / I2C SDA | Shared | ⚠️ Dual-use: Motor phase C and encoder I2C |
| 15 | Motor EN / I2C SCL | Shared | ⚠️ Dual-use: Motor enable and encoder I2C |
| 18 | CAM D1 | Output | Camera data |
| 19 | CAM D2 | Output | Camera data |
| 21 | CAM D3 | Output | Camera data |
| 22 | CAM PCLK | Input | Camera pixel clock |
| 23 | CAM HREF | Input | Camera horizontal reference |
| 25 | CAM VSYNC | Input | Camera vertical sync |
| 26 | CAM SIOD | Open-drain | Camera I2C data (SCCB) |
| 27 | CAM SIOC | Open-drain | Camera I2C clock (SCCB) |
| 32 | CAM PWDN | Output | Camera power down |
| 34 | CAM D6 | Input | ⚡ Input-only pin |
| 35 | CAM D7 | Input | ⚡ Input-only pin |
| 36 | CAM D4 | Input | ⚡ Input-only pin |
| 39 | CAM D5 | Input | ⚡ Input-only pin |

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

### REQ-07: Servo Steering Control [IMPLEMENTED]

**Requirement**: The rover shall use a servo motor for steering with configurable center position and travel limits.

**Implementation**:
- Standard PWM servo control using ESP32 LEDC peripheral
- Configurable pulse width range (default: 500-2500µs)
- Center position calibration
- Configurable steering angle limits
- Smooth angle transitions

**Configuration Options**:
```yaml
servo:
  pin: 32
  min_pulse_us: 500
  max_pulse_us: 2500
  center_pulse_us: 1500
  max_angle: 45
```

**API**:
- `servo_init()` - Initialize servo
- `servo_set_angle()` - Set steering angle (-max to +max degrees)
- `servo_center()` - Return to center position
- `servo_set_trim()` - Set steering trim offset
- `servo_set_pulse()` - Set pulse width directly (µs)

**Files**:
- `components/servo_control/servo_control.c` - Servo control implementation
- `components/servo_control/include/servo_control.h` - Public API
- `rover_config.yaml` - Servo configuration section

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

## Implemented Features

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

### REQ-31: Serial Log Capture and Web Display [IMPLEMENTED]

**Requirement**: All serial logging since reboot shall be logged and printed on the web-gui page, logging shall only rotate after 2 hours.

**Implementation**:
- Custom `log_buffer` component that hooks into ESP-IDF's `esp_log_set_vprintf()`
- Captures all ESP_LOG output to a circular buffer (300 entries, ~42KB)
- 2-hour rotation period - logs older than 2 hours are excluded from display
- REST API endpoints for log retrieval
- Web GUI with real-time log display panel

**REST API Endpoints**:
- `GET /logs` - Get logs as JSON (supports `level`, `tag`, `since`, `limit` query params)
- `GET /logs/stream` - SSE stream of new logs (real-time)
- `DELETE /logs` - Clear log buffer

**Log Entry Format**:
```json
{
  "t": 12345,      // Timestamp (ms since boot)
  "l": "I",        // Level: E/W/I/D/V
  "tag": "WIFI",   // Component tag
  "msg": "Connected to AP"  // Message
}
```

**Web UI Features**:
- Log panel in diagnostics section with color-coded levels
- Level filter dropdown (All, Errors, Warnings+, Info+, Debug+)
- Auto-scroll toggle
- Clear button
- Entry count and dropped count display

**Memory Usage**:
- ~42KB heap for 300 entries
- Each entry: 16 bytes timestamp + 16 bytes tag + 128 bytes message + 1 byte level

**Files**:
- `components/log_buffer/log_buffer.c` - Circular buffer and vprintf hook
- `components/log_buffer/include/log_buffer.h` - Public API
- `components/web_server/web_server.c` - `/logs` endpoints
- `components/web_server/web_ui.c` - Log display panel JavaScript

---

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

(Add new requirements here as they are defined) 