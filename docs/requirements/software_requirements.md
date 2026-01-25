# ESP32 Rover Firmware - Software Requirements Specification

## Document Control

| Field | Value |
|-------|-------|
| **Document ID** | ESP32-ROVER-SRS-001 |
| **Version** | 2.4.0 |
| **Status** | Approved |
| **Last Updated** | 2026-01-25 |
| **Author** | ESP32 Rover Development Team |
| **Classification** | Technical Specification |

### Revision History

| Version | Date | Author | Description |
|---------|------|--------|-------------|
| 1.0.0 | 2024-11-15 | Team | Initial requirements specification |
| 1.5.0 | 2025-01-10 | Team | Added motor control removal notes |
| 2.0.0 | 2026-01-20 | Team | Added CI/CD, version control, LCD improvements |
| 2.1.0 | 2026-01-25 | Team | Restructured to professional format with traceability |
| 2.2.0 | 2026-01-25 | Team | Added REQ-SW-032 Live Telemetry Chart |
| 2.3.0 | 2026-01-25 | Team | Added REQ-SW-033 Dual-Axis Telemetry Chart |
| 2.4.0 | 2026-01-25 | Team | Added REQ-SW-034 WiFi Mode Switch via Button |

### Approval Signatures

| Role | Name | Signature | Date |
|------|------|-----------|------|
| **Technical Lead** | _Pending_ | ___________ | _____ |
| **Software Architect** | _Pending_ | ___________ | _____ |
| **Quality Assurance** | _Pending_ | ___________ | _____ |

---

## Table of Contents

1. [Introduction](#1-introduction)
2. [Scope](#2-scope)
3. [System Architecture](#3-system-architecture)
4. [Hardware Platform Requirements](#4-hardware-platform-requirements)
5. [Functional Requirements](#5-functional-requirements)
6. [Non-Functional Requirements](#6-non-functional-requirements)
7. [Traceability Matrix](#7-traceability-matrix)
8. [Verification Methods](#8-verification-methods)
9. [Validation Test Matrix](#9-validation-test-matrix)
10. [References](#10-references)
11. [Appendices](#11-appendices)

---

## 1. Introduction

### 1.1 Purpose

This Software Requirements Specification (SRS) defines the functional and non-functional requirements for the ESP32 WiFi Rover firmware. The firmware provides a WiFi-controlled platform with camera streaming, web-based control interface, and diagnostic capabilities.

### 1.2 Document Conventions

- **SHALL**: Mandatory requirement
- **SHOULD**: Recommended but not mandatory
- **MAY**: Optional feature
- **Priority Levels**: Critical, High, Medium, Low
- **Verification Methods**: I (Inspection), T (Test), A (Analysis), D (Demonstration)

### 1.3 Intended Audience

- Firmware developers
- System integrators
- Test engineers
- Hardware designers
- Project managers

### 1.4 Product Scope

The ESP32 Rover firmware is an embedded system providing:
- WiFi connectivity (AP and STA modes)
- Web-based control interface
- Live video streaming (ESP32-CAM only)
- LCD status display (TTGO only)
- OTA firmware updates
- MQTT telemetry
- Diagnostic logging

### 1.5 Standards Compliance

- **IEEE 29148-2018**: Systems and software engineering — Life cycle processes — Requirements engineering
- **ISO/IEC 12207**: Software life cycle processes
- **ESP-IDF v5.2.2**: Espressif IoT Development Framework

---

## 2. Scope

### 2.1 Product Features

**Current Status**: Motor/encoder/servo components have been removed from the codebase as of v1.6. The web control interface (speed, steering, emergency stop) is preserved for future motor implementation. The firmware now focuses on WiFi connectivity, camera streaming, LCD display, and diagnostic features.

### 2.2 User Classes and Characteristics

| User Class | Characteristics | Needs |
|------------|----------------|-------|
| **End User** | Non-technical, operates rover via web UI | Simple, intuitive control interface |
| **Developer** | Technical, modifies firmware, integrates sensors | Well-documented APIs, build system |
| **Tester** | Technical, validates functionality | Unit tests, diagnostic tools |
| **System Integrator** | Technical, deploys to production | OTA updates, remote monitoring |

### 2.3 Operating Environment

- **Hardware**: ESP32 SoC (ESP32-CAM or TTGO T-Display)
- **Network**: WiFi 802.11 b/g/n (2.4 GHz)
- **Power**: 3.3V regulated supply, battery-powered
- **Temperature**: 0°C to 70°C operating range
- **Software**: ESP-IDF v5.2.2, FreeRTOS

### 2.4 Design and Implementation Constraints

- **Memory**: 320KB internal DRAM, 4MB PSRAM (ESP32-CAM only)
- **Flash**: 4MB SPI flash
- **CPU**: Dual-core Xtensa LX6 @ 240 MHz
- **Network**: WiFi only (no Bluetooth)
- **Programming Language**: C11 standard
- **Real-time**: FreeRTOS task scheduling

### 2.5 Assumptions and Dependencies

- ESP-IDF v5.2.2 toolchain installed
- Python 3.7+ for configuration generation
- Web browser with JavaScript enabled
- WiFi access point or router for STA mode
- MQTT broker for telemetry (optional)

---

## 3. System Architecture

### 3.1 High-Level Architecture

```
┌─────────────────────────────────────────────────┐
│                 Web Interface                    │
│  (HTML/CSS/JavaScript served from ESP32)        │
└──────────────────┬──────────────────────────────┘
                   │ HTTP/WebSocket
┌──────────────────▼──────────────────────────────┐
│            HTTP Server (httpd)                   │
│  /control  /status  /stream  /ota  /logs       │
└──────────────────┬──────────────────────────────┘
                   │
┌──────────────────▼──────────────────────────────┐
│              Application Layer                   │
│  Control Logic  Status  Diagnostics  Logging    │
└─┬────────┬─────────┬──────────┬─────────────┬──┘
  │        │         │          │             │
┌─▼──┐ ┌──▼───┐ ┌───▼────┐ ┌───▼───┐   ┌────▼────┐
│WiFi│ │Camera│ │  LCD   │ │ MQTT  │   │Log Buffer│
└─┬──┘ └──┬───┘ └───┬────┘ └───┬───┘   └────┬────┘
  │       │         │          │            │
┌─▼───────▼─────────▼──────────▼────────────▼────┐
│           ESP-IDF Hardware Abstraction          │
│  WiFi Driver  SPI  I2C  ADC  GPIO  Logging     │
└──────────────────────┬──────────────────────────┘
                       │
┌──────────────────────▼──────────────────────────┐
│            ESP32 Hardware (SoC)                  │
│  Dual-core CPU  RAM  Flash  Peripherals         │
└─────────────────────────────────────────────────┘
```

### 3.2 Task Architecture

**Core 0**: WiFi, Web server, Camera capture, LCD updates, Status updates, MQTT
**Core 1**: Available for future motor control implementation

| Task | Core | Priority | Frequency | Description |
|------|------|----------|-----------|-------------|
| Status update | 0 | 2 | 20 Hz | Button polling, status collection |
| LCD update | 0 | 1 | ~60 Hz | Display refresh, diagnostics |
| Web server | 0 | - | Event-driven | HTTP request handling |
| MQTT publish | 0 | 2 | 1 Hz | Telemetry publish (if enabled) |
| Log buffer | 0 | - | Event-driven | Serial log capture |

### 3.3 Component Dependencies

```
main
├── wifi
├── web_server
│   ├── log_buffer
│   └── camera (ESP32-CAM only)
├── lcd_display (TTGO only)
├── mqtt_service
├── log_buffer
├── resource_guard
└── build_info
```

---

## 4. Hardware Platform Requirements

### 4.1 Supported Hardware Targets

| Target | Board | Camera | LCD | Buttons | Description |
|--------|-------|--------|-----|---------|-------------|
| `esp32cam` | AI-Thinker ESP32-CAM | Yes | No | No | Full features with live video |
| `ttgo` | LilyGO TTGO T-Display | No | Yes | Yes | LCD status display + buttons |

### 4.2 GPIO Allocation

#### TTGO T-Display Pin Map

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

**Available GPIOs**: 21-22, 25-27, 32-33 (future expansion)

#### ESP32-CAM Pin Map

| GPIO | Function | Direction | Notes |
|------|----------|-----------|-------|
| 0 | CAM XCLK | Output | ⚠️ Bootstrap pin |
| 2 | Available | I/O | ⚠️ Bootstrap pin - should be LOW/floating at boot |
| 4 | Flash LED | Output | On-board white LED |
| 12 | JTAG TDI / Available | I/O | ⛔ **CRITICAL** - requires pull-down for boot |
| 13 | JTAG TCK / Available | I/O | Available for future use |
| 14 | JTAG TMS | I/O | Can be used for JTAG or future motor/I2C |
| 15 | JTAG TDO | I/O | Can be used for JTAG or future motor/I2C |
| 33 | Status LED | Output | WiFi status indicator (inverted logic) |
| Various | Camera | Input/Output | See detailed camera pinout in config.h |

**Available GPIOs**: 12-15 (normal mode), 21-22, 25-27, 32 (future expansion)

### 4.3 Pin Legend

| Symbol | Meaning |
|--------|---------|
| ⛔ | Critical hardware requirement |
| ⚠️ | Bootstrap pin - affects boot behavior |
| ⚡ | Input-only GPIO (34-39) |

### 4.4 Bootstrap Pin Requirements

| Pin | Function | Boot Requirement |
|-----|----------|------------------|
| GPIO 0 | Download mode select | HIGH = normal boot, LOW = download mode |
| GPIO 2 | Boot strapping | Should be LOW or floating at boot |
| GPIO 12 | Flash voltage select | **Must be LOW** for 3.3V flash operation |
| GPIO 15 | SDIO timing | Controls debug output, can be floating |

---

## 5. Functional Requirements

### 5.1 Build & Configuration

#### REQ-SW-001: YAML Configuration System

| Field | Value |
|-------|-------|
| **Priority** | Critical |
| **Type** | Functional |
| **Description** | The firmware SHALL provide a YAML-based configuration system that generates compile-time C header definitions for WiFi settings, REST API, MQTT, motor/servo parameters, and debug options. |
| **Rationale** | Compile-time configuration enables optimization, reduces runtime overhead, and prevents misconfiguration. YAML provides human-readable format for complex hierarchical settings. |
| **Acceptance Criteria** | • Configuration file `rover_config.yaml` exists<br>• `generate_config.py` script converts YAML to C header<br>• Generated `config_generated.h` contains all preprocessor definitions<br>• Changes to YAML require rebuild to take effect<br>• Invalid YAML syntax produces clear error messages |
| **Verification Method** | I (Code review), T (Build test with config changes) |
| **Dependencies** | None |
| **Status** | Approved |

**Implementation Files**:
- `config/rover_config.yaml`
- `scripts/generate_config.py`
- `firmware/main/config_generated.h`

---

#### REQ-SW-002: Hardware Target Selection

| Field | Value |
|-------|-------|
| **Priority** | Critical |
| **Type** | Functional |
| **Description** | The firmware SHALL support multiple hardware targets (TTGO T-Display and ESP32-CAM) with compile-time selection via `ROVER_TARGET` environment variable or CMake variable. |
| **Rationale** | Single codebase for multiple hardware variants reduces maintenance burden. Compile-time selection enables dead code elimination and optimization for each target. |
| **Acceptance Criteria** | • Two supported targets: `ttgo` and `esp32cam`<br>• Target selection via `ROVER_TARGET` environment variable<br>• CMake generates `ROVER_TARGET_TTGO=1` or `ROVER_TARGET_ESP32CAM=1` define<br>• Default target is TTGO T-Display<br>• Target-specific features conditionally compiled |
| **Verification Method** | T (Build test for both targets) |
| **Dependencies** | None |
| **Status** | Approved |

**Usage**:
```bash
ROVER_TARGET=esp32cam idf.py build  # ESP32-CAM build
ROVER_TARGET=ttgo idf.py build      # TTGO build (default)
```

---

#### REQ-SW-003: OTA Firmware Updates

| Field | Value |
|-------|-------|
| **Priority** | High |
| **Type** | Functional |
| **Description** | The firmware SHALL support Over-The-Air (OTA) firmware updates via HTTP POST to `/ota` endpoint with password protection and automatic reboot upon successful flash. |
| **Rationale** | OTA updates eliminate need for physical access to device, enabling remote deployment and bug fixes. Critical for production devices. |
| **Acceptance Criteria** | • HTTP POST endpoint `/ota` accepts binary firmware file<br>• Requires `X-OTA-Password` header matching configured password<br>• Validates firmware image before flashing<br>• Writes to OTA partition using ESP-IDF OTA API<br>• Reboots automatically on success<br>• Returns error response on failure (wrong password, invalid image, insufficient space)<br>• Configurable hostname via `ota.hostname` in YAML |
| **Verification Method** | T (OTA flash test), D (Remote update demonstration) |
| **Dependencies** | REQ-SW-001 (configuration system), REQ-SW-012 (WiFi) |
| **Status** | Approved |

**Configuration** (`rover_config.yaml`):
```yaml
ota:
  enabled: true
  hostname: "esp32-rover"  # or "ttgo-rover" for TTGO
  password: "rover1234"
```

---

#### REQ-SW-004: Multi-Core Task Management

| Field | Value |
|-------|-------|
| **Priority** | High |
| **Type** | Functional |
| **Description** | The firmware SHALL distribute tasks across both ESP32 cores using FreeRTOS task pinning to optimize performance and ensure real-time responsiveness. |
| **Rationale** | ESP32 dual-core architecture enables parallel execution. Core 0 handles network stack (required by ESP-IDF), Core 1 available for time-critical motor control. |
| **Acceptance Criteria** | • Core 0: WiFi, networking, web server, status updates, LCD, MQTT<br>• Core 1: Reserved for future motor control implementation<br>• Task priorities configured per requirements<br>• Task watchdog monitors critical tasks<br>• Diagnostics display per-core task counts |
| **Verification Method** | I (Code review), T (Task affinity verification), A (Performance analysis) |
| **Dependencies** | None |
| **Status** | Approved |

**Task Allocation**:
| Task | Core | Priority | Frequency | Description |
|------|------|----------|-----------|-------------|
| Status update | 0 | 2 (Medium) | 20 Hz | Button polling, status collection |
| LCD update | 0 | 1 (Low) | ~60 Hz | Display refresh, diagnostics |
| Web Server | 0 | - | Event-driven | HTTP request handling |
| WiFi | 0 | High | - | Network stack (ESP-IDF managed) |
| MQTT | 0 | 2 | 1 Hz | Telemetry publish (if enabled) |
| (Future: Motor Control) | 1 | 5 (High) | 100 Hz | FOC loop, servo control |

---

### 5.2 Hardware Drivers & Interfaces

#### REQ-SW-005: Battery Voltage Monitoring

| Field | Value |
|-------|-------|
| **Priority** | High |
| **Type** | Functional |
| **Description** | The firmware SHALL monitor battery voltage via ADC on GPIO 34 with voltage divider support and low-pass filtering, displaying the value on LCD and web UI. |
| **Rationale** | Battery monitoring prevents over-discharge damage and provides user feedback on remaining runtime. Critical for autonomous operation. |
| **Acceptance Criteria** | • ADC reads GPIO 34 at configured sample rate<br>• Voltage divider ratio configurable (default 2.0x)<br>• Low-pass filter for stable readings<br>• Displayed on LCD (TTGO) with color coding (green >3.7V, yellow 3.4-3.7V, red <3.4V)<br>• Included in `/status` JSON response<br>• Accuracy ±0.1V |
| **Verification Method** | T (Calibration test with known voltages) |
| **Dependencies** | REQ-SW-018 (LCD display), REQ-SW-017 (Web UI) |
| **Status** | Approved |

**Configuration** (`firmware/main/config.h`):
```c
#define BATTERY_ADC_PIN      GPIO_NUM_34
#define BATTERY_DIVIDER_RATIO 2.0f
#define ENABLE_BATTERY_ADC   1
```

---

#### REQ-SW-006: Button Input with GPIO ISR

| Field | Value |
|-------|-------|
| **Priority** | Medium |
| **Type** | Functional |
| **Description** | The firmware SHALL support two physical buttons (left and right) on TTGO T-Display with interrupt-driven input handling and software debouncing. |
| **Rationale** | Physical buttons provide direct user interface without requiring web connection. Essential for diagnostic mode entry and deep sleep activation. |
| **Acceptance Criteria** | • GPIO interrupt service routines for both buttons<br>• Software debounce (configurable, default 50ms)<br>• Edge-triggered interrupts (falling edge for active-LOW buttons)<br>• Button state available to LCD and web UI<br>• Long-press detection for diagnostic mode (3s) and deep sleep (5s) |
| **Verification Method** | T (Button press test), D (Debouncing demonstration) |
| **Dependencies** | REQ-SW-019 (Diagnostic mode), REQ-SW-030 (Deep sleep) |
| **Status** | Approved |

**Button Pins** (TTGO T-Display):
- Left button: GPIO 0
- Right button: GPIO 35

---

#### REQ-SW-007: Camera MJPEG Streaming

| Field | Value |
|-------|-------|
| **Priority** | High |
| **Type** | Functional |
| **Description** | When built for ESP32-CAM target, the firmware SHALL provide MJPEG video streaming from OV2640 camera over HTTP at `/stream` endpoint with configurable resolution and quality. |
| **Rationale** | Live video feed is core feature for ESP32-CAM variant, enabling remote visual monitoring. MJPEG chosen for simplicity and browser compatibility. |
| **Acceptance Criteria** | • OV2640 camera initialization on ESP32-CAM boot<br>• `/stream` endpoint serves MJPEG stream (`multipart/x-mixed-replace`)<br>• Configurable frame size (default QVGA 320x240)<br>• Configurable JPEG quality (0-63, default 12)<br>• Stream can be paused/resumed via `/camera` endpoint (REQ-SW-034)<br>• Only compiled when `ROVER_TARGET_ESP32CAM=1` |
| **Verification Method** | T (Stream playback test), A (Frame rate measurement) |
| **Dependencies** | REQ-SW-002 (Target selection), REQ-SW-034 (Camera toggle) |
| **Status** | Approved |

**Implementation Files**:
- `firmware/components/camera/camera.c`
- `firmware/components/camera/include/camera.h`

---

#### REQ-SW-008: LCD Status Display

| Field | Value |
|-------|-------|
| **Priority** | High |
| **Type** | Functional |
| **Description** | The firmware SHALL display real-time status on TTGO T-Display ST7789 LCD (135x240) with optimized partial updates to minimize flicker. |
| **Rationale** | On-device display provides immediate visual feedback without web interface. Critical for field operation and debugging. |
| **Acceptance Criteria** | • ST7789 driver initializes LCD via SPI with DMA<br>• Two display modes: Main status and Diagnostics<br>• Partial screen updates (only redraw changed elements)<br>• Main screen shows: speed bar, steering, battery, WiFi, buttons<br>• Diagnostic screen shows: WiFi details, system info, services, uptime, time<br>• Configurable refresh rate (default ~60 Hz)<br>• Only compiled when `ROVER_TARGET_TTGO=1` |
| **Verification Method** | I (Visual inspection), T (Update rate measurement) |
| **Dependencies** | REQ-SW-002 (Target selection), REQ-SW-019 (Diagnostic mode) |
| **Status** | Approved |

**Implementation Files**:
- `firmware/components/lcd_display/lcd_display.c`
- `firmware/components/lcd_display/include/lcd_display.h`
- `firmware/components/lcd_display/fonts/`

---

### 5.3 Networking & Connectivity

#### REQ-SW-009: WiFi STA-First Mode with AP Fallback

| Field | Value |
|-------|-------|
| **Priority** | Critical |
| **Type** | Functional |
| **Description** | The firmware SHALL attempt WiFi station (STA) connection to configured network first, falling back to Access Point (AP) mode if unavailable. WiFi behavior SHALL be configurable at compile time. |
| **Rationale** | STA-first mode enables deployment on existing networks while providing AP fallback for initial setup. Configurable modes support different deployment scenarios. |
| **Acceptance Criteria** | • Three WiFi modes: `ap_only`, `sta_only`, `sta_first`<br>• STA-first mode attempts connection for configurable timeout (default 30s)<br>• Falls back to AP mode on STA timeout or connection failure<br>• STA credentials from `secrets.yaml`<br>• AP SSID and password configurable<br>• WiFi mode selected via `wifi.mode` in `rover_config.yaml` |
| **Verification Method** | T (Connection test for all three modes) |
| **Dependencies** | REQ-SW-001 (Configuration system) |
| **Status** | Approved |

**Configuration** (`rover_config.yaml`):
```yaml
wifi:
  mode: sta_first  # ap_only, sta_only, or sta_first
  ap:
    ssid: "ESP32-Rover"
  sta:
    connect_timeout: 30
```

**Secrets** (`secrets.yaml`):
```yaml
wifi_sta_ssid: "YourNetwork"
wifi_sta_password: "YourPassword"
wifi_ap_password: "rover1234"
```

---

#### REQ-SW-010: Internet Connectivity Check

| Field | Value |
|-------|-------|
| **Priority** | Medium |
| **Type** | Functional |
| **Description** | During WiFi initialization in STA mode, the firmware SHALL ping 1.1.1.1 to verify internet connectivity and display status (Connected/Offline) on LCD and web GUI. |
| **Rationale** | Internet connectivity verification enables users to diagnose network issues. Required for NTP time sync and MQTT broker connection to external servers. |
| **Acceptance Criteria** | • Ping 1.1.1.1 after STA connection established<br>• 3 ping attempts with 1 second timeout each<br>• Uses ESP-IDF ping API (`ping/ping_sock.h`)<br>• LCD shows "Internet: Connected" (green) or "Internet: Offline" (red)<br>• Web UI diagnostics shows same status with color coding<br>• Status included in `/status` JSON response |
| **Verification Method** | T (Network connectivity test) |
| **Dependencies** | REQ-SW-009 (WiFi STA mode), REQ-SW-018 (LCD), REQ-SW-017 (Web UI) |
| **Status** | Approved |

---

#### REQ-SW-011: HTTP REST API

| Field | Value |
|-------|-------|
| **Priority** | High |
| **Type** | Functional |
| **Description** | The firmware SHALL provide HTTP REST API endpoint `/status` serving JSON with full system diagnostics including velocity, battery, WiFi info, heap memory, uptime, CPU frequency, and task counts. |
| **Rationale** | REST API enables programmatic access to rover status for automation, monitoring dashboards, and integration with external systems. |
| **Acceptance Criteria** | • `/status` endpoint returns JSON with all diagnostic fields<br>• Includes: target, battery voltage, camera status, RSSI, button states<br>• Diagnostics: SSID, IP, MAC, heap, uptime, CPU freq, task counts per core<br>• Service status: REST API, MQTT, internet connectivity<br>• Time fields: local time, NTP sync status<br>• Compile-time toggle via `ENABLE_REST_API` define |
| **Verification Method** | T (API endpoint test), I (JSON schema validation) |
| **Dependencies** | REQ-SW-012 (HTTP server) |
| **Status** | Approved |

**JSON Response Format**:
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
    "freeHeap": 150000,
    "uptime": 3600,
    "tasksCore0": 8,
    "tasksCore1": 4,
    "localTime": "14:30:45",
    "ntpSynced": true,
    "internet": true,
    "restApi": true,
    "mqttEnabled": true,
    "mqttConnected": false,
    "buildFingerprint": "5dcefb4",
    "buildTime": "2026-01-25T06:48:35Z"
  }
}
```

---

#### REQ-SW-012: HTTP Control Endpoint

| Field | Value |
|-------|-------|
| **Priority** | High |
| **Type** | Functional |
| **Description** | The firmware SHALL accept control commands via HTTP POST to `/control` endpoint with JSON body containing speed, steering, and emergency stop flag. |
| **Rationale** | HTTP control interface provides simple, browser-compatible command interface. Enables both web UI and programmatic control. |
| **Acceptance Criteria** | • `POST /control` accepts JSON request body<br>• Parameters: `speed` (-100 to 100), `steering` (-100 to 100), `emergency_stop` (bool)<br>• Commands executed immediately<br>• Watchdog timer reset on valid command<br>• Returns JSON `{"status": "ok"}` on success<br>• Returns 400 Bad Request on invalid JSON or out-of-range values |
| **Verification Method** | T (Control command test) |
| **Dependencies** | REQ-SW-025 (Command watchdog) |
| **Status** | Approved |

**Request Format**:
```json
{
  "speed": 50.0,
  "steering": -25.0,
  "emergency_stop": false
}
```

---

#### REQ-SW-013: MQTT Telemetry Service

| Field | Value |
|-------|-------|
| **Priority** | Medium |
| **Type** | Functional |
| **Description** | The firmware SHALL optionally publish diagnostic data to MQTT broker with configurable host, port, credentials, topic prefix, and publish interval. |
| **Rationale** | MQTT enables integration with home automation systems, time-series databases, and monitoring dashboards. Industry-standard IoT protocol. |
| **Acceptance Criteria** | • MQTT client connects to configurable broker<br>• Publishes to topic prefix (default `esp32-rover`)<br>• Configurable publish interval (default 5000ms)<br>• Only starts in STA mode (requires external network)<br>• Compile-time toggle via `ENABLE_MQTT` define<br>• Publishes: battery, RSSI, heap, uptime, connection status<br>• Supports authentication (username/password) |
| **Verification Method** | T (MQTT broker subscription test) |
| **Dependencies** | REQ-SW-009 (WiFi STA mode), REQ-SW-001 (Configuration) |
| **Status** | Approved |

**Configuration** (`rover_config.yaml`):
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

---

#### REQ-SW-014: mDNS Hostname Resolution

| Field | Value |
|-------|-------|
| **Priority** | High |
| **Type** | Functional |
| **Description** | The firmware SHALL register target-specific mDNS hostname to enable network access via `<hostname>.local` instead of IP address. |
| **Rationale** | mDNS eliminates need to track dynamic IP addresses. Industry-standard zero-configuration networking. Essential for OTA updates and web access. |
| **Acceptance Criteria** | • Uses ESP-IDF mDNS component (`mdns.h`)<br>• Target-specific hostnames: `esp32-rover.local` (ESP32-CAM), `ttgo-rover.local` (TTGO)<br>• Registers HTTP service `_http._tcp` on port 80<br>• Instance name: "ESP32 Rover Control"<br>• Accessible via browser and OTA tools<br>• Responds to mDNS queries on local network |
| **Verification Method** | T (mDNS resolution test), D (Browser access via .local) |
| **Dependencies** | REQ-SW-009 (WiFi), REQ-SW-002 (Target selection) |
| **Status** | Approved |

**Usage Examples**:
```bash
# Access web interface
open http://esp32-rover.local

# OTA update
curl -X POST -H "X-OTA-Password: rover1234" \
     --data-binary @firmware.bin \
     http://ttgo-rover.local/ota
```

---

### 5.4 User Interface

#### REQ-SW-015: Web Control Interface

| Field | Value |
|-------|-------|
| **Priority** | Critical |
| **Type** | Functional |
| **Description** | The firmware SHALL provide single-page web application served from ESP32 with virtual joystick control, live status display, battery indicator, WiFi signal, and diagnostic panel. |
| **Rationale** | Web-based interface provides universal access from any device with browser. No app installation required. |
| **Acceptance Criteria** | • Single-page HTML/CSS/JavaScript served from `/` endpoint<br>• Virtual joystick for speed/steering control (touch and mouse compatible)<br>• Live status display: velocity, steering, battery voltage<br>• WiFi signal strength indicator (RSSI)<br>• Emergency stop button<br>• Diagnostic panel with system info (scrollable section)<br>• Responsive design for mobile and desktop<br>• Polling-based updates (configurable interval, default 100ms) |
| **Verification Method** | D (User interface demonstration), T (Browser compatibility test) |
| **Dependencies** | REQ-SW-011 (REST API), REQ-SW-012 (Control endpoint) |
| **Status** | Approved |

**Features**:
- Virtual joystick for speed/steering
- Battery voltage with color coding
- WiFi signal strength (RSSI)
- Camera stream placeholder (ESP32-CAM)
- Emergency stop button
- Motor enable/disable toggle
- Diagnostics panel (WiFi, system, services, logs)

---

#### REQ-SW-016: Diagnostic Mode Entry/Exit

| Field | Value |
|-------|-------|
| **Priority** | Medium |
| **Type** | Functional |
| **Description** | On TTGO T-Display, the firmware SHALL enter diagnostic LCD screen when both buttons are held for 3 seconds, and exit when any button is short-pressed. |
| **Rationale** | Diagnostic mode provides detailed system information on LCD for troubleshooting without web interface. Physical button control ensures accessibility. |
| **Acceptance Criteria** | • Entry: Hold both buttons for 3 seconds simultaneously<br>• State machine transitions: OFF → ENTERING → WAIT_RELEASE → ON<br>• Releasing early returns to OFF state<br>• Diagnostic screen persists until user exits<br>• Exit: Short press any button (left or right)<br>• Footer shows "Press any btn to exit" instruction |
| **Verification Method** | T (Button sequence test), D (State machine demonstration) |
| **Dependencies** | REQ-SW-006 (Button input), REQ-SW-018 (LCD display) |
| **Status** | Approved |

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

---

#### REQ-SW-017: Service Status Indicators

| Field | Value |
|-------|-------|
| **Priority** | Medium |
| **Type** | Functional |
| **Description** | Both LCD diagnostic screen and web UI diagnostics panel SHALL display real-time status of HTTP REST API and MQTT services with color-coded indicators. |
| **Rationale** | Service status visibility aids troubleshooting connectivity and configuration issues. Users can quickly identify disabled or disconnected services. |
| **Acceptance Criteria** | • LCD shows "Services" section with REST and MQTT status<br>• REST: ON (green) / OFF (red)<br>• MQTT: OK (green) / ... (yellow, connecting) / OFF (red)<br>• Web UI shows same status in diagnostics panel<br>• Compile-time service enable/disable reflected in status<br>• Runtime MQTT connection state updated in real-time |
| **Verification Method** | I (Visual inspection), T (Service toggle test) |
| **Dependencies** | REQ-SW-011 (REST API), REQ-SW-013 (MQTT), REQ-SW-018 (LCD) |
| **Status** | Approved |

---

#### REQ-SW-018: Per-Core Task Display

| Field | Value |
|-------|-------|
| **Priority** | Low |
| **Type** | Functional |
| **Description** | Diagnostics panel on LCD and web GUI SHALL display number of FreeRTOS tasks split by core affinity (Core 0, Core 1, No Affinity). |
| **Rationale** | Per-core task counts enable verification of multi-core task distribution and detection of task proliferation or leaks. |
| **Acceptance Criteria** | • LCD displays: `Tasks: C0:X C1:Y`<br>• Web GUI displays: Total tasks with `C0:X C1:Y` subtitle<br>• Uses FreeRTOS `uxTaskGetSystemState()` and `xTaskGetAffinity()` APIs<br>• Counts updated in real-time<br>• Matches actual FreeRTOS task registry |
| **Verification Method** | T (Task count verification), A (FreeRTOS API validation) |
| **Dependencies** | REQ-SW-004 (Multi-core tasks), REQ-SW-018 (LCD), REQ-SW-015 (Web UI) |
| **Status** | Approved |

---

#### REQ-SW-019: NTP Clock Display

| Field | Value |
|-------|-------|
| **Priority** | Medium |
| **Type** | Functional |
| **Description** | LCD main screen and web UI diagnostics panel SHALL display local time synchronized via NTP with sync status indicator. |
| **Rationale** | Accurate time required for logging timestamps, scheduled operations, and user awareness. NTP provides reliable time sync over network. |
| **Acceptance Criteria** | • Uses ESP-IDF SNTP API (`esp_sntp.h`)<br>• NTP servers: pool.ntp.org and time.google.com<br>• Timezone: CET/CEST (Central European Time with DST)<br>• LCD shows "Time: HH:MM:SS" (green when synced, yellow when not synced)<br>• Web UI shows local time with sync status in diagnostics<br>• JSON fields: `localTime` (string), `ntpSynced` (bool) |
| **Verification Method** | T (NTP sync test), I (Time accuracy check) |
| **Dependencies** | REQ-SW-009 (WiFi STA mode), REQ-SW-010 (Internet connectivity) |
| **Status** | Approved |

---

#### REQ-SW-020: Uptime Counter

| Field | Value |
|-------|-------|
| **Priority** | Low |
| **Type** | Functional |
| **Description** | Diagnostics screen on LCD and web UI SHALL display system uptime in HH:MM:SS format, updated in real-time. |
| **Rationale** | Uptime counter helps diagnose crash/reboot issues and verify system stability. Common metric for embedded systems. |
| **Acceptance Criteria** | • Uptime tracked since boot using `esp_timer_get_time()`<br>• Displayed in HH:MM:SS format<br>• LCD shows uptime on separate line with "Up:" label<br>• Web UI shows uptime in System section<br>• Caps at 99:59:59 for display (overflow handling)<br>• JSON field: `uptime` (seconds) |
| **Verification Method** | T (Uptime display test) |
| **Dependencies** | REQ-SW-018 (LCD), REQ-SW-015 (Web UI) |
| **Status** | Approved |

---

#### REQ-SW-021: Conditionally Visible UI Elements

| Field | Value |
|-------|-------|
| **Priority** | Medium |
| **Type** | Functional |
| **Description** | Web UI SHALL show/hide elements based on build target: camera stream (ESP32-CAM only), hardware buttons (TTGO only), page title (target-specific). |
| **Rationale** | Target-specific UI prevents confusion and provides clean interface tailored to hardware capabilities. |
| **Acceptance Criteria** | • `/status` endpoint returns `target` field ("esp32cam" or "ttgo")<br>• JavaScript `configureUIForTarget()` function adjusts UI on load<br>• ESP32-CAM: Camera stream visible, buttons hidden, title "ESP32-CAM Rover"<br>• TTGO: Camera hidden, buttons visible, title "TTGO Rover"<br>• Camera initialization deferred until target confirmed |
| **Verification Method** | I (UI inspection on both targets), T (Element visibility test) |
| **Dependencies** | REQ-SW-002 (Target selection), REQ-SW-015 (Web UI) |
| **Status** | Approved |

---

#### REQ-SW-022: Camera Stream Toggle

| Field | Value |
|-------|-------|
| **Priority** | Medium |
| **Type** | Functional |
| **Description** | Web UI for ESP32-CAM target SHALL provide button to enable/disable camera stream to conserve resources during OTA updates or when video not needed. |
| **Rationale** | Camera streaming consumes significant CPU and memory. OTA updates may fail when stream is active. Toggle provides user control over resource usage. |
| **Acceptance Criteria** | • "CAM ON/OFF" toggle button visible on ESP32-CAM target only<br>• Button state reflects current stream status<br>• POST `/camera` with `{"enabled": true/false}` toggles stream<br>• GET `/camera` returns `{"enabled": bool}`<br>• When disabled: `camera_capture_frame()` returns NULL, placeholder shows "Camera Paused"<br>• Default state: enabled |
| **Verification Method** | T (Stream toggle test), D (Resource usage measurement) |
| **Dependencies** | REQ-SW-007 (Camera streaming), REQ-SW-021 (Conditional UI) |
| **Status** | Approved |

---

#### REQ-SW-032: Live Telemetry Chart

| Field | Value |
|-------|-------|
| **Priority** | Medium |
| **Type** | Functional |
| **Description** | The web UI SHALL display a live telemetry chart showing steering input history (placeholder for future velocity data) with configurable time windows (6s, 30s, 60s). |
| **Rationale** | Visual telemetry enables users to analyze control input patterns, diagnose issues, and understand system behavior over time. Steering is used as placeholder until velocity sensors are implemented. |
| **Acceptance Criteria** | • Chart.js library loaded via CDN for visualization<br>• Real-time line chart displays steering input (-100% to +100%)<br>• `/status` endpoint includes `steeringHistory` array with timestamps<br>• Ring buffer stores last 600 samples (60s at 10Hz)<br>• Time window buttons: 6s (default), 30s, 60s<br>• Chart updates at status polling rate (10Hz)<br>• X-axis shows relative time (seconds ago)<br>• Y-axis shows steering percentage<br>• Graceful fallback if Chart.js CDN unavailable |
| **Verification Method** | D (Visual demonstration), T (Chart update test) |
| **Dependencies** | REQ-SW-011 (REST API), REQ-SW-015 (Web UI), REQ-SW-012 (Control endpoint) |
| **Status** | Approved |

**Implementation Files**:
- `firmware/components/web_server/web_ui.c` - Chart UI and JavaScript
- `firmware/components/web_server/web_server.c` - Steering history buffer and JSON serialization

**JSON Response Extension** (`/status`):
```json
{
  "steeringHistory": [
    {"t": 123456, "v": -25.5},
    {"t": 123556, "v": -20.0},
    ...
  ]
}
```

**Future Evolution**:
When velocity sensors are implemented (REQ-VEL-01), this chart will be extended to show actual velocity data alongside or replacing steering input.

---

#### REQ-SW-033: Dual-Axis Telemetry Chart (Speed + Steering)

| Field | Value |
|-------|-------|
| **Priority** | Medium |
| **Type** | Functional |
| **Description** | The web UI telemetry chart SHALL display both speed and steering inputs simultaneously on a single chart with dual Y-axes and distinct colors for each data series. |
| **Rationale** | Combined visualization of speed and steering enables correlation analysis between throttle and turning inputs, improving debugging and control tuning. Dual Y-axes allow independent scaling for each metric. |
| **Acceptance Criteria** | • Single Chart.js chart displays both speed and steering data<br>• Speed data uses left Y-axis with distinct color (e.g., orange/red)<br>• Steering data uses right Y-axis with distinct color (e.g., blue)<br>• Both axes range from -100% to +100%<br>• `/status` endpoint includes `speedHistory` array alongside existing `steeringHistory`<br>• Ring buffer stores last 600 samples of speed (60s at 10Hz)<br>• Time window buttons (6s, 30s, 60s) apply to both series<br>• Legend shows both series with color coding<br>• Chart updates synchronously for both data series |
| **Verification Method** | D (Visual demonstration), T (Chart update test) |
| **Dependencies** | REQ-SW-032 (Live Telemetry Chart), REQ-SW-011 (REST API), REQ-SW-012 (Control endpoint) |
| **Status** | Approved |

**Implementation Files**:
- `firmware/components/web_server/web_ui.c` - Dual-axis chart configuration
- `firmware/components/web_server/web_server.c` - Speed history buffer and JSON serialization

**JSON Response Extension** (`/status`):
```json
{
  "steeringHistory": [
    {"t": 123456, "v": -25.5},
    ...
  ],
  "speedHistory": [
    {"t": 123456, "v": 50.0},
    ...
  ]
}
```

**Chart Configuration**:
- Left Y-axis: Speed (%) - color: #e94560 (coral red)
- Right Y-axis: Steering (%) - color: #3282b8 (blue)
- Both axes: min -100, max 100
- Legend position: top

---

### 5.5 Safety & Control

#### REQ-SW-023: Command Watchdog Timer

| Field | Value |
|-------|-------|
| **Priority** | Critical |
| **Type** | Safety |
| **Description** | The firmware SHALL implement command watchdog that stops motors if no valid control commands received within configurable timeout (default 500ms). |
| **Rationale** | Network disconnections or controller crashes could leave motors running. Watchdog provides fail-safe to prevent runaway vehicle. |
| **Acceptance Criteria** | • Configurable timeout via `control.watchdog_timeout_ms` in YAML<br>• Timer starts when motor enabled<br>• Each control command resets timer<br>• On timeout: motor velocity set to 0, steering centered<br>• Motor remains enabled (ready for new commands)<br>• Default timeout: 500ms |
| **Verification Method** | T (Timeout test), D (Safety demonstration) |
| **Dependencies** | REQ-SW-012 (Control endpoint) |
| **Status** | Approved |

---

#### REQ-SW-024: Emergency Stop Function

| Field | Value |
|-------|-------|
| **Priority** | Critical |
| **Type** | Safety |
| **Description** | The firmware SHALL support emergency stop (E-Stop) function that immediately disables motor driver and ignores velocity commands until explicitly cleared. |
| **Rationale** | E-Stop provides immediate vehicle immobilization for safety. Required for emergency situations. |
| **Acceptance Criteria** | • E-Stop activated via web UI button or `/control` API with `emergency_stop: true`<br>• Immediate motor driver disable (not just velocity=0)<br>• Velocity commands ignored until E-Stop cleared<br>• Visual indication on LCD (red "E-STOP" indicator)<br>• Requires explicit motor re-enable to resume operation |
| **Verification Method** | T (E-Stop response time test), D (Safety demonstration) |
| **Dependencies** | REQ-SW-012 (Control endpoint), REQ-SW-015 (Web UI) |
| **Status** | Approved |

---

### 5.6 Power Management

#### REQ-SW-025: Deep Sleep Mode

| Field | Value |
|-------|-------|
| **Priority** | Medium |
| **Type** | Functional |
| **Description** | On TTGO T-Display, the firmware SHALL enter ESP32 deep sleep mode by holding left button for 5 seconds, waking via right button press. |
| **Rationale** | Deep sleep extends battery life from hours to days/weeks. Essential for battery-powered deployment. ESP32 deep sleep consumes ~10µA vs ~180mA active. |
| **Acceptance Criteria** | • Long-press detection on left button (GPIO 0) for 5 seconds<br>• Does not activate during diagnostic mode entry<br>• Sleep sequence: display sleep screen, stop motor, disable LCD backlight, stop WiFi, wait for release<br>• Configure right button (GPIO 35) as RTC EXT0 wake source (pull-up enabled)<br>• Wake on right button press (LOW trigger)<br>• Full reboot on wake<br>• Sleep screen shows Snorlax sprite with "Zzz..." animation |
| **Verification Method** | T (Sleep entry/wake test), A (Power consumption measurement) |
| **Dependencies** | REQ-SW-006 (Button input), REQ-SW-018 (LCD display) |
| **Status** | Approved |

**Power Consumption**:
- Active mode: ~180mA
- Deep sleep: ~10µA

**Configuration** (`rover_config.yaml`):
```yaml
power:
  deep_sleep_enabled: true
  sleep_button_hold_time_ms: 5000
```

---

#### REQ-SW-034: WiFi Mode Switch via Button

| Field                   | Value                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   |     |
| ----------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | --- |
| **Priority**            | Medium                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  |     |
| **Type**                | Functional                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              |     |
| **Description**         | On TTGO T-Display, the firmware SHALL switch from WiFi STA mode to AP mode when the right button is held for 5 seconds, with LCD feedback during the transition.                                                                                                                                                                                                                                                                                                                                                        |     |
| **Rationale**           | Allows user to enable AP mode when internet/router is unreliable, without needing to reflash or modify configuration files. Essential for field operation.                                                                                                                                                                                                                                                                                                                                                              |     |
| **Acceptance Criteria** | • Long-press detection on right button (GPIO 35) for 5 seconds<br>• Does not activate during diagnostic mode or sleep mode<br>• LCD shows progress indicator: "WiFi AP: 3..." countdown<br>• On 5s hold complete: stop WiFi STA, start WiFi AP with configured SSID/password<br>• LCD shows confirmation: "AP Mode Active" with SSID<br>• Mode is not persisted - reboot returns to configured default mode<br>• If already in AP mode, shows "Already in AP mode"<br>• Web server remains accessible on new AP network |     |
| **Verification Method** | T (Mode switch test), D (LCD feedback demonstration)                                                                                                                                                                                                                                                                                                                                                                                                                                                                    |     |
| **Dependencies**        | REQ-SW-006 (Button input), REQ-SW-018 (LCD display), REQ-SW-007 (WiFi AP), REQ-SW-008 (WiFi STA)                                                                                                                                                                                                                                                                                                                                                                                                                        |     |
| **Status**              | Approved                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                |     |
|                         |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         |     |

**User Interaction Flow**:
1. User holds right button
2. At 1s: LCD shows "WiFi AP: 5..."
3. At 2s: LCD shows "WiFi AP: 4..."
4. At 3s: LCD shows "WiFi AP: 3..."
5. At 4s: LCD shows "WiFi AP: 2..."
6. At 5s: LCD shows "WiFi AP: 1..."
7. On release after 5s: Switch to AP mode
8. LCD shows "AP Mode Active" with SSID for 3 seconds

**Implementation Notes**:
- Uses same 5s threshold as sleep mode for consistency
- Right button monitors press duration in `lcd_update_task()`
- WiFi switch uses `esp_wifi_set_mode()` and `esp_wifi_start()`
- Does not require reboot - runtime mode change

---

### 5.7 Diagnostics & Logging

#### REQ-SW-026: Serial Log Capture

| Field | Value |
|-------|-------|
| **Priority** | High |
| **Type** | Functional |
| **Description** | The firmware SHALL capture all ESP_LOG serial output since reboot in ring buffer and provide HTTP endpoints for log retrieval and live streaming via Server-Sent Events (SSE). |
| **Rationale** | Remote log access enables debugging without physical serial connection. Captures boot logs that may be missed when connecting after boot. Critical for production diagnostics. |
| **Acceptance Criteria** | • Ring buffer using ESP-IDF `esp_ringbuf` (16KB default, configurable)<br>• Hook into logging via `esp_log_set_vprintf()`<br>• Passthrough to UART maintains normal serial output<br>• Thread-safe for multi-core access<br>• GET `/logs` returns all buffered logs as plain text<br>• GET `/logs/stream` provides SSE stream for live updates<br>• DELETE `/logs` clears buffer<br>• Query param `level` filters by minimum log level |
| **Verification Method** | T (Log capture test), D (SSE streaming demonstration) |
| **Dependencies** | REQ-SW-015 (Web UI), REQ-SW-011 (REST API) |
| **Status** | Approved |

**API Endpoints**:
- `GET /logs` - Download all buffered logs
- `GET /logs/stream` - SSE stream for live logs
- `DELETE /logs` - Clear log buffer
- Query params: `level` (1=Error, 2=Warning, 3=Info, 4=Debug, 5=Verbose)

**Memory Budget**:
- Ring buffer: 16KB (~200-300 log entries)
- SSE task stack: 4KB per connection
- JSON formatting: 1KB temporary

---

#### REQ-SW-027: Log Download Button

| Field | Value |
|-------|-------|
| **Priority** | Low |
| **Type** | Functional |
| **Description** | Web UI diagnostics panel SHALL include "Download" button to save buffered logs as text file and "Clear" button to clear buffer. |
| **Rationale** | Log download enables offline analysis and archival. Clear function prevents buffer overflow with old data. |
| **Acceptance Criteria** | • "Download" button triggers browser download via `/logs?download=1`<br>• Server adds `Content-Disposition: attachment; filename="esp32_logs.txt"` header<br>• "Clear" button sends DELETE request to `/logs`<br>• Buttons in System Logs section of diagnostics panel |
| **Verification Method** | T (Download functionality test) |
| **Dependencies** | REQ-SW-026 (Log capture), REQ-SW-015 (Web UI) |
| **Status** | Approved |

---

#### REQ-SW-028: Resource Consumption Guards

| Field | Value |
|-------|-------|
| **Priority** | High |
| **Type** | Reliability |
| **Description** | The firmware SHALL include runtime guards to protect against resource exhaustion (heap, stack, tasks) with configurable thresholds and comprehensive status logging. |
| **Rationale** | ESP32 limited RAM (~320KB) makes memory exhaustion a real risk. Silent stack overflows corrupt memory before crashing. Proactive guards prevent hard-to-debug failures. |
| **Acceptance Criteria** | • Resource guard component checks heap, internal DRAM, stack watermarks<br>• Configurable thresholds: 32KB free heap, 16KB internal DRAM, 512B stack watermark<br>• API: `resource_guard_can_alloc(size)` predicts safe allocation<br>• API: `resource_guard_check_stack(task)` validates stack usage<br>• API: `resource_guard_log_status()` logs comprehensive resource report<br>• Unit tests verify guards function correctly |
| **Verification Method** | T (Unit tests), A (Memory pressure testing) |
| **Dependencies** | REQ-SW-029 (Unit tests) |
| **Status** | Approved |

**Resource Thresholds**:
| Resource | Minimum | Rationale |
|----------|---------|-----------|
| Free Heap | 32 KB | Below this, allocations may fail |
| Internal DRAM | 16 KB | Critical for DMA, WiFi buffers |
| Stack Watermark | 512 bytes | Minimum safe stack remaining |
| Max Tasks | 32 | Prevent task proliferation |

---

### 5.8 System Reliability

#### REQ-SW-029: Task Watchdog Timer

| Field | Value |
|-------|-------|
| **Priority** | Critical |
| **Type** | Reliability |
| **Description** | The firmware SHALL monitor critical tasks (motor control, status update) using ESP-IDF Task Watchdog Timer (TWDT) and trigger automatic reboot if any task becomes unresponsive. |
| **Rationale** | Tasks can hang due to deadlocks, infinite loops, or resource exhaustion. Automatic recovery preferred over manual intervention. Critical for autonomous operation. |
| **Acceptance Criteria** | • Uses ESP-IDF TWDT API with configurable timeout (default 30s)<br>• Status task feeds watchdog every 50ms<br>• (Future: Motor task feeds every 10ms)<br>• Configurable panic behavior: log only or reboot<br>• Configuration via `task_watchdog` section in YAML<br>• Watchdog enabled by default |
| **Verification Method** | T (Hang detection test), D (Auto-reboot demonstration) |
| **Dependencies** | REQ-SW-001 (Configuration system) |
| **Status** | Approved |

**Configuration** (`rover_config.yaml`):
```yaml
task_watchdog:
  enabled: true
  timeout_sec: 30
  panic_on_timeout: true  # false = log only, true = reboot
```

**Monitored Tasks**:
| Task | Feed Interval | Description |
|------|---------------|-------------|
| `status` | 50ms (20Hz loop) | Status update and button polling |
| (Future: `motor_ctrl`) | 10ms (100Hz loop) | Motor FOC control loop |

---

#### REQ-SW-030: JTAG Debug Build Flag

| Field | Value |
|-------|-------|
| **Priority** | Low |
| **Type** | Development |
| **Description** | The firmware SHALL provide build flag `JTAG_DEBUG` that disables motor control to free GPIO 12-15 for JTAG debugging on ESP32-CAM. |
| **Rationale** | ESP32 JTAG uses GPIO 12-15, conflicting with motor pins. Build-time flag enables hardware debugging without disconnecting motor wires. |
| **Acceptance Criteria** | • Set via environment variable `JTAG_DEBUG=1` or CMake `-DJTAG_DEBUG=1`<br>• Defines `ENABLE_JTAG_DEBUG=1` when enabled<br>• Conditional compilation skips: encoder init, motor init, servo init, motor task<br>• Log message: "JTAG DEBUG MODE - Motor control DISABLED"<br>• Only applicable to ESP32-CAM target (TTGO uses different pins)<br>• Compatible with OpenOCD and GDB |
| **Verification Method** | I (Code review), T (JTAG connection test) |
| **Dependencies** | REQ-SW-002 (Target selection) |
| **Status** | Approved |

**Usage**:
```bash
JTAG_DEBUG=1 ROVER_TARGET=esp32cam idf.py build
openocd -f interface/ftdi/esp32_devkitj_v1.cfg -f target/esp32.cfg
xtensa-esp32-elf-gdb -ex "target remote :3333" build/esp32-rover.elf
```

---

#### REQ-SW-031: Build Fingerprint System

| Field | Value |
|-------|-------|
| **Priority** | High |
| **Type** | Traceability |
| **Description** | The firmware SHALL embed git commit information at build time, accessible via serial logs and REST API, to enable verification of deployed firmware version. |
| **Rationale** | Post-OTA verification of firmware version essential for debugging. Bug reports must link to exact code revision. Detects uncommitted changes. |
| **Acceptance Criteria** | • Build script `generate_build_info.sh` extracts git hash, branch, dirty state, timestamp<br>• Auto-generated header `build_info.h` with build metadata<br>• Serial log at boot shows: git hash, build time, ESP-IDF version<br>• REST API `/status` includes: `buildFingerprint`, `buildTime`, `buildBranch`, `buildDirty`<br>• Header regenerated on every build |
| **Verification Method** | I (Build log inspection), T (REST API verification) |
| **Dependencies** | REQ-SW-011 (REST API) |
| **Status** | Approved |

**Generated Defines** (`build_info.h`):
```c
#define BUILD_GIT_HASH       "5dcefb4"
#define BUILD_GIT_HASH_FULL  "5dcefb41c65e057d8e5b3b8295db9fa6f1cf02f4"
#define BUILD_GIT_BRANCH     "main"
#define BUILD_GIT_DIRTY      false
#define BUILD_TIME           "2026-01-25T06:48:35Z"
#define BUILD_FINGERPRINT    "5dcefb4"
#define BUILD_VERSION        "2.0.1"
```

---

## 6. Non-Functional Requirements

### 6.1 Performance

#### REQ-NFR-001: Control Loop Latency

| Field | Value |
|-------|-------|
| **Priority** | High |
| **Type** | Performance |
| **Description** | The system SHALL process control commands with end-to-end latency <100ms from HTTP request to motor response. |
| **Rationale** | Low latency essential for responsive remote control. Human perception threshold ~100ms. |
| **Acceptance Criteria** | • HTTP request → motor velocity update <100ms (95th percentile)<br>• Measured via timestamp logging<br>• Worst-case latency <200ms |
| **Verification Method** | A (Latency measurement), T (Performance test) |
| **Dependencies** | REQ-SW-012 (Control endpoint) |
| **Status** | Approved |

---

#### REQ-NFR-002: Camera Frame Rate

| Field | Value |
|-------|-------|
| **Priority** | Medium |
| **Type** | Performance |
| **Description** | ESP32-CAM SHALL deliver MJPEG stream at ≥15 FPS for QVGA resolution (320x240). |
| **Rationale** | 15 FPS minimum for smooth video perception. Higher resolution reduces frame rate due to bandwidth constraints. |
| **Acceptance Criteria** | • QVGA (320x240) ≥15 FPS sustained<br>• VGA (640x480) ≥8 FPS acceptable<br>• Measured via frame timestamp analysis |
| **Verification Method** | A (Frame rate measurement) |
| **Dependencies** | REQ-SW-007 (Camera streaming) |
| **Status** | Approved |

---

#### REQ-NFR-003: WiFi Range

| Field | Value |
|-------|-------|
| **Priority** | Medium |
| **Type** | Performance |
| **Description** | The system SHALL maintain WiFi connection at ≥30 meters line-of-sight from access point/router. |
| **Rationale** | Minimum operational range for practical rover deployment. Typical indoor obstacle penetration. |
| **Acceptance Criteria** | • 30m LOS with RSSI >-70 dBm<br>• 10m through walls with RSSI >-80 dBm<br>• Graceful degradation beyond range (no crash) |
| **Verification Method** | T (Range test in open field and indoor) |
| **Dependencies** | REQ-SW-009 (WiFi) |
| **Status** | Approved |

---

#### REQ-NFR-004: Memory Usage

| Field | Value |
|-------|-------|
| **Priority** | Critical |
| **Type** | Resource |
| **Description** | The firmware SHALL maintain ≥32KB free heap during normal operation to prevent allocation failures and enable OTA updates. |
| **Rationale** | OTA updates require significant free heap. Below 32KB, system becomes unstable. |
| **Acceptance Criteria** | • Minimum free heap ≥32KB during idle<br>• ≥48KB recommended for OTA updates<br>• Logged via diagnostics<br>• Resource guard warns if threshold breached |
| **Verification Method** | A (Heap monitoring), T (OTA update test) |
| **Dependencies** | REQ-SW-028 (Resource guards) |
| **Status** | Approved |

---

### 6.2 Reliability

#### REQ-NFR-005: Uptime

| Field | Value |
|-------|-------|
| **Priority** | High |
| **Type** | Reliability |
| **Description** | The system SHALL achieve ≥24 hours continuous operation without crash or reboot under normal conditions. |
| **Rationale** | Stability essential for autonomous operation. Memory leaks or resource exhaustion detected within 24h. |
| **Acceptance Criteria** | • 24h soak test without crash<br>• No memory leaks (heap stable)<br>• All services remain responsive<br>• Task watchdog not triggered |
| **Verification Method** | T (Long-duration soak test) |
| **Dependencies** | REQ-SW-029 (Task watchdog), REQ-SW-028 (Resource guards) |
| **Status** | Approved |

---

#### REQ-NFR-006: WiFi Reconnection

| Field | Value |
|-------|-------|
| **Priority** | High |
| **Type** | Reliability |
| **Description** | In STA mode, the system SHALL automatically reconnect to WiFi network after temporary disconnection within 30 seconds. |
| **Rationale** | Network interruptions common. Automatic recovery prevents manual intervention. |
| **Acceptance Criteria** | • Detects WiFi disconnection within 5s<br>• Attempts reconnection with exponential backoff<br>• Reconnects within 30s of network restoration<br>• Max 5 retry attempts before logging error |
| **Verification Method** | T (Disconnection recovery test) |
| **Dependencies** | REQ-SW-009 (WiFi STA mode) |
| **Status** | Approved |

---

### 6.3 Usability

#### REQ-NFR-007: Web UI Responsiveness

| Field | Value |
|-------|-------|
| **Priority** | Medium |
| **Type** | Usability |
| **Description** | Web UI SHALL load within 2 seconds on modern browser over WiFi connection and remain responsive with status updates ≤200ms. |
| **Rationale** | Fast load time improves user experience. Responsive UI essential for real-time control. |
| **Acceptance Criteria** | • Initial page load <2s<br>• Status updates every 100ms (configurable)<br>• Joystick input lag <50ms<br>• No UI freezing during operations |
| **Verification Method** | T (Load time measurement), D (User experience test) |
| **Dependencies** | REQ-SW-015 (Web UI) |
| **Status** | Approved |

---

#### REQ-NFR-008: LCD Readability

| Field | Value |
|-------|-------|
| **Priority** | Medium |
| **Type** | Usability |
| **Description** | TTGO LCD display SHALL be readable in normal indoor lighting (200-500 lux) with adjustable backlight brightness. |
| **Rationale** | On-device display must be readable without external tools. Brightness control adapts to environment. |
| **Acceptance Criteria** | • Text legible at 30cm distance<br>• Backlight PWM adjustable 0-100%<br>• Default brightness 80%<br>• Color coding for status (green=good, yellow=warning, red=error) |
| **Verification Method** | I (Visual inspection), D (Readability test) |
| **Dependencies** | REQ-SW-018 (LCD display) |
| **Status** | Approved |

---

### 6.4 Maintainability

#### REQ-NFR-009: Code Documentation

| Field | Value |
|-------|-------|
| **Priority** | Medium |
| **Type** | Maintainability |
| **Description** | All public APIs SHALL have Doxygen-compatible documentation with function purpose, parameters, return values, and usage examples. |
| **Rationale** | Documentation reduces onboarding time and enables independent development. Essential for open-source projects. |
| **Acceptance Criteria** | • All public functions documented with Doxygen comments<br>• Each component has README.md<br>• API reference generated from source<br>• Examples provided for complex APIs |
| **Verification Method** | I (Documentation review), A (Doxygen generation) |
| **Dependencies** | None |
| **Status** | Approved |

---

#### REQ-NFR-010: Unit Test Coverage

| Field | Value |
|-------|-------|
| **Priority** | High |
| **Type** | Quality |
| **Description** | The firmware SHALL maintain ≥70% code coverage for unit-testable components with automated test execution. |
| **Rationale** | Unit tests prevent regressions, document expected behavior, and improve code quality. 70% achievable target for embedded systems. |
| **Acceptance Criteria** | • ≥70% line coverage for core components<br>• ≥80% coverage for critical safety functions<br>• Automated test execution via `make test`<br>• Coverage report generated with `make coverage-html`<br>• Tests run on host (no hardware required) |
| **Verification Method** | A (Coverage analysis), T (Automated test execution) |
| **Dependencies** | None |
| **Status** | Approved |

**Test Categories**:
- Configuration (5 tests)
- WiFi (3 tests)
- REST API (2 tests)
- MQTT (4 tests)
- Services (1 test)
- Diagnostics (12 tests)
- **Total**: 27 tests

---

### 6.5 Security

#### REQ-NFR-011: OTA Password Protection

| Field | Value |
|-------|-------|
| **Priority** | Critical |
| **Type** | Security |
| **Description** | OTA updates SHALL require password authentication via `X-OTA-Password` HTTP header to prevent unauthorized firmware modification. |
| **Rationale** | Unauthorized firmware updates could compromise device security or functionality. Password protection essential. |
| **Acceptance Criteria** | • OTA endpoint rejects requests without correct password (401 Unauthorized)<br>• Password configured in `secrets.yaml` (not committed to git)<br>• Default password "rover1234" for development only<br>• Production deployments MUST change password |
| **Verification Method** | T (Authentication test with wrong password) |
| **Dependencies** | REQ-SW-003 (OTA updates) |
| **Status** | Approved |

---

#### REQ-NFR-012: WiFi AP Password

| Field | Value |
|-------|-------|
| **Priority** | High |
| **Type** | Security |
| **Description** | WiFi Access Point mode SHALL use WPA2 encryption with configurable password (minimum 8 characters). |
| **Rationale** | Open WiFi networks expose control interface to unauthorized users. WPA2 provides adequate security for local network. |
| **Acceptance Criteria** | • WPA2-PSK encryption enabled<br>• Password minimum 8 characters<br>• Password configured in `secrets.yaml`<br>• Default password "rover1234" for initial setup<br>• Users prompted to change default password |
| **Verification Method** | T (WiFi security test), I (Configuration review) |
| **Dependencies** | REQ-SW-009 (WiFi AP mode) |
| **Status** | Approved |

---

## 7. Traceability Matrix

### 7.1 Requirements Cross-Reference

| Requirement | Depends On | Verified By | Implemented In | Status |
|-------------|------------|-------------|----------------|--------|
| REQ-SW-001 | - | I, T | rover_config.yaml, generate_config.py | ✓ |
| REQ-SW-002 | - | T | main/CMakeLists.txt | ✓ |
| REQ-SW-003 | REQ-SW-001, REQ-SW-012 | T, D | main/main.c (OTA handler) | ✓ |
| REQ-SW-004 | - | I, T, A | main/main.c (task creation) | ✓ |
| REQ-SW-005 | REQ-SW-018, REQ-SW-017 | T | main/main.c (ADC) | ✓ |
| REQ-SW-006 | REQ-SW-019, REQ-SW-030 | T, D | main/main.c (GPIO ISR) | ✓ |
| REQ-SW-007 | REQ-SW-002, REQ-SW-034 | T, A | components/camera/ | ✓ |
| REQ-SW-008 | REQ-SW-002, REQ-SW-019 | I, T | components/lcd_display/ | ✓ |
| REQ-SW-009 | REQ-SW-001 | T | components/wifi/ | ✓ |
| REQ-SW-010 | REQ-SW-009, REQ-SW-018, REQ-SW-017 | T | main/main.c (ping) | ✓ |
| REQ-SW-011 | REQ-SW-012 | T, I | components/web_server/ | ✓ |
| REQ-SW-012 | REQ-SW-025 | T | components/web_server/ | ✓ |
| REQ-SW-013 | REQ-SW-009, REQ-SW-001 | T | components/mqtt_service/ | ✓ |
| REQ-SW-014 | REQ-SW-009, REQ-SW-002 | T, D | main/main.c (mDNS) | ✓ |
| REQ-SW-015 | REQ-SW-011, REQ-SW-012 | D, T | components/web_server/web_ui.c | ✓ |
| REQ-SW-016 | REQ-SW-006, REQ-SW-018 | T, D | main/main.c (diag state machine) | ✓ |
| REQ-SW-017 | REQ-SW-011, REQ-SW-013, REQ-SW-018 | I, T | components/web_server/, lcd_display/ | ✓ |
| REQ-SW-018 | REQ-SW-004, REQ-SW-018, REQ-SW-015 | T, A | main/main.c (task counts) | ✓ |
| REQ-SW-019 | REQ-SW-009, REQ-SW-010 | T, I | main/main.c (NTP) | ✓ |
| REQ-SW-020 | REQ-SW-018, REQ-SW-015 | T | main/main.c (uptime) | ✓ |
| REQ-SW-021 | REQ-SW-002, REQ-SW-015 | I, T | components/web_server/web_ui.c | ✓ |
| REQ-SW-022 | REQ-SW-007, REQ-SW-021 | T, D | components/camera/, web_server/ | ✓ |
| REQ-SW-023 | REQ-SW-012 | T, D | main/main.c (watchdog) | ✓ |
| REQ-SW-024 | REQ-SW-012, REQ-SW-015 | T, D | main/main.c (E-Stop) | ✓ |
| REQ-SW-025 | REQ-SW-006, REQ-SW-018 | T, A | main/main.c (sleep) | ✓ |
| REQ-SW-026 | REQ-SW-015, REQ-SW-011 | T, D | components/log_buffer/ | ✓ |
| REQ-SW-027 | REQ-SW-026, REQ-SW-015 | T | components/web_server/ | ✓ |
| REQ-SW-028 | REQ-SW-029 | T, A | components/resource_guard/ | ✓ |
| REQ-SW-029 | REQ-SW-001 | T, D | main/main.c (TWDT) | ✓ |
| REQ-SW-030 | REQ-SW-002 | I, T | main/CMakeLists.txt, main.c | ✓ |
| REQ-SW-031 | REQ-SW-011 | I, T | scripts/generate_build_info.sh | ✓ |
| REQ-SW-032 | REQ-SW-011, REQ-SW-015, REQ-SW-012 | D, T | components/web_server/ | ✓ |
| REQ-SW-033 | REQ-SW-032, REQ-SW-011, REQ-SW-012 | D, T | components/web_server/ | ✓ |
| REQ-SW-034 | REQ-SW-006, REQ-SW-018, REQ-SW-007, REQ-SW-008 | D, T | main/main.c | ⏳ |
| REQ-NFR-001 | REQ-SW-012 | A, T | - | ✓ |
| REQ-NFR-002 | REQ-SW-007 | A | - | ✓ |
| REQ-NFR-003 | REQ-SW-009 | T | - | ✓ |
| REQ-NFR-004 | REQ-SW-028 | A, T | - | ✓ |
| REQ-NFR-005 | REQ-SW-029, REQ-SW-028 | T | - | ✓ |
| REQ-NFR-006 | REQ-SW-009 | T | - | ✓ |
| REQ-NFR-007 | REQ-SW-015 | T, D | - | ✓ |
| REQ-NFR-008 | REQ-SW-018 | I, D | - | ✓ |
| REQ-NFR-009 | - | I, A | - | ✓ |
| REQ-NFR-010 | - | A, T | test/ | ✓ |
| REQ-NFR-011 | REQ-SW-003 | T | - | ✓ |
| REQ-NFR-012 | REQ-SW-009 | T, I | - | ✓ |

---

## 8. Verification Methods

### 8.1 Method Definitions

| Code | Method | Description | Applicable To |
|------|--------|-------------|---------------|
| **I** | Inspection | Visual examination of code, documentation, or design | Code quality, documentation, configuration |
| **T** | Test | Execution of test procedures with pass/fail criteria | Functionality, integration, performance |
| **A** | Analysis | Mathematical, computational, or measurement-based verification | Performance metrics, resource usage |
| **D** | Demonstration | Interactive demonstration of feature to stakeholders | User interfaces, workflows |

### 8.2 Test Procedures

#### TP-001: YAML Configuration Test

**Objective**: Verify YAML configuration system generates correct C header

**Prerequisites**:
- Python 3.7+ installed
- `rover_config.yaml` and `secrets.yaml` present

**Procedure**:
1. Modify `rover_config.yaml` WiFi mode to `ap_only`
2. Run `python3 scripts/generate_config.py`
3. Verify `config_generated.h` contains `#define WIFI_MODE_AP_ONLY 1`
4. Modify WiFi mode to `sta_first`
5. Regenerate and verify `#define WIFI_MODE_STA_FIRST 1`
6. Introduce YAML syntax error
7. Verify script produces clear error message

**Pass Criteria**:
- All defines match YAML configuration
- Invalid YAML produces actionable error
- No warnings during generation

---

#### TP-002: OTA Update Test

**Objective**: Verify OTA firmware update with password protection

**Prerequisites**:
- Device connected to WiFi
- Test firmware binary built
- Web browser or curl

**Procedure**:
1. Attempt OTA without password header: `curl -X POST --data-binary @firmware.bin http://esp32-rover.local/ota`
2. Verify 401 Unauthorized response
3. Attempt OTA with wrong password: `curl -X POST -H "X-OTA-Password: wrong" --data-binary @firmware.bin http://esp32-rover.local/ota`
4. Verify 401 Unauthorized response
5. Perform OTA with correct password: `curl -X POST -H "X-OTA-Password: rover1234" --data-binary @firmware.bin http://esp32-rover.local/ota`
6. Verify 200 OK response
7. Wait for automatic reboot (~5 seconds)
8. Verify device boots with new firmware (check build fingerprint)

**Pass Criteria**:
- Wrong password rejected with 401
- Correct password accepted
- Firmware flashed successfully
- Device reboots automatically
- New firmware confirmed via `/status` API

---

#### TP-003: WiFi STA-First Fallback Test

**Objective**: Verify WiFi attempts STA connection and falls back to AP

**Prerequisites**:
- `rover_config.yaml` configured with `wifi.mode: sta_first`
- Invalid STA credentials in `secrets.yaml`

**Procedure**:
1. Flash firmware with STA-first mode
2. Monitor serial output during boot
3. Verify STA connection attempt logged
4. Wait for timeout (30s)
5. Verify fallback to AP mode logged
6. Connect to ESP32-Rover AP
7. Access web interface at `http://192.168.4.1`

**Pass Criteria**:
- STA connection attempted for 30s
- Fallback to AP mode on timeout
- AP accessible with configured SSID/password
- Web UI loads successfully

---

#### TP-004: Camera Stream Toggle Test

**Objective**: Verify camera stream can be enabled/disabled via web UI

**Prerequisites**:
- ESP32-CAM target built and flashed
- Device connected to WiFi
- Web browser

**Procedure**:
1. Open web UI and verify camera stream visible
2. Click "CAM OFF" button
3. Verify stream stops and placeholder shows "Camera Paused"
4. Verify `/camera` endpoint returns `{"enabled": false}`
5. Click "CAM ON" button
6. Verify stream resumes
7. Verify `/camera` endpoint returns `{"enabled": true}`
8. Monitor heap usage before/after toggle

**Pass Criteria**:
- Stream stops within 1s of OFF command
- Stream resumes within 1s of ON command
- Heap usage decreases when stream disabled
- UI reflects current stream state

---

#### TP-005: Deep Sleep Power Test

**Objective**: Verify deep sleep mode reduces power consumption

**Prerequisites**:
- TTGO T-Display target
- Multimeter or power monitor
- Fresh battery

**Procedure**:
1. Measure active current consumption
2. Hold left button for 5 seconds
3. Verify sleep screen displays (Snorlax sprite)
4. Wait 2 seconds for WiFi shutdown
5. Measure current consumption in deep sleep
6. Press right button to wake
7. Verify device reboots and returns to normal operation

**Pass Criteria**:
- Active current ~180mA ± 20mA
- Deep sleep current <50µA (ideally ~10µA)
- Wake-up via right button successful
- Full reboot after wake

---

#### TP-006: Task Watchdog Test

**Objective**: Verify task watchdog detects hung task and reboots

**Prerequisites**:
- Firmware with watchdog enabled (`task_watchdog.enabled: true`)
- Device with serial monitor

**Procedure**:
1. Flash firmware with modified status task containing 60s delay (exceeds 30s timeout)
2. Monitor serial output
3. Verify status task created and subscribed to watchdog
4. Wait for 30s timeout
5. Verify watchdog timeout logged
6. Verify automatic reboot triggered
7. Verify normal boot after reboot

**Pass Criteria**:
- Watchdog timeout detected at ~30s
- Reboot triggered automatically
- System recovers and boots normally
- No infinite reboot loop

---

#### TP-007: Log Capture and SSE Streaming Test

**Objective**: Verify serial logs captured and streamed to web UI

**Prerequisites**:
- Device connected to WiFi
- Web browser with SSE support
- Serial monitor

**Procedure**:
1. Flash firmware and reboot device
2. Open web UI diagnostics panel
3. Verify boot logs visible in System Logs section
4. Click "Download" button
5. Verify logs download as text file
6. Generate new log entries via serial (e.g., trigger WiFi reconnect)
7. Verify new entries appear in web UI within 1s
8. Change log level filter to "Errors"
9. Verify only error-level logs displayed
10. Click "Clear" button
11. Verify log buffer cleared

**Pass Criteria**:
- Boot logs captured in buffer
- SSE stream delivers new logs <1s
- Log level filter works correctly
- Download produces valid text file
- Clear button empties buffer

---

#### TP-008: Per-Core Task Count Test

**Objective**: Verify task counts split by core affinity

**Prerequisites**:
- Device running with multiple tasks
- LCD or web UI access

**Procedure**:
1. Access diagnostics screen (LCD or web UI)
2. Note task counts: C0, C1, No Affinity
3. Use FreeRTOS `vTaskList()` to dump actual task registry via serial
4. Manually count tasks per core from task list
5. Compare manual count with displayed count

**Pass Criteria**:
- Displayed counts match actual FreeRTOS task registry
- Core 0 count includes WiFi, web server, status, LCD tasks
- Core 1 count matches expected (currently 0 or minimal)
- No affinity tasks counted separately

---

#### TP-009: Unit Test Coverage Test

**Objective**: Verify unit test coverage meets ≥70% requirement

**Prerequisites**:
- `gcov` and `lcov` installed
- Test framework built

**Procedure**:
1. Run `cd test && make clean`
2. Run `make coverage-html`
3. Verify tests execute successfully
4. Open `coverage_report/index.html`
5. Check overall line coverage percentage
6. Identify uncovered critical sections
7. Verify safety-critical functions have ≥80% coverage

**Pass Criteria**:
- All 27 tests pass
- Overall coverage ≥70%
- Safety functions (E-Stop, watchdog) ≥80%
- Coverage report generated without errors

---

## 9. Validation Test Matrix

### 9.1 Functional Validation Tests

| Test ID | Requirement | Test Type | Environment | Pass Criteria | Priority |
|---------|-------------|-----------|-------------|---------------|----------|
| VT-F-001 | REQ-SW-001 | Unit | Host | Config header matches YAML | Critical |
| VT-F-002 | REQ-SW-002 | Integration | Device | Both targets build and boot | Critical |
| VT-F-003 | REQ-SW-003 | Integration | Device + Network | OTA succeeds with password | High |
| VT-F-004 | REQ-SW-005 | Hardware | Device + DMM | Battery voltage ±0.1V accuracy | High |
| VT-F-005 | REQ-SW-006 | Hardware | Device | Button presses detected, debounced | Medium |
| VT-F-006 | REQ-SW-007 | Integration | Device + Browser | MJPEG stream plays smoothly | High |
| VT-F-007 | REQ-SW-008 | Hardware | Device | LCD displays status clearly | High |
| VT-F-008 | REQ-SW-009 | Integration | Device + Router | STA connects, AP fallback works | Critical |
| VT-F-009 | REQ-SW-010 | Integration | Device + Internet | Ping succeeds, status displayed | Medium |
| VT-F-010 | REQ-SW-011 | Integration | Device + Browser | /status returns valid JSON | High |
| VT-F-011 | REQ-SW-012 | Integration | Device + Browser | Control commands execute | High |
| VT-F-012 | REQ-SW-013 | Integration | Device + MQTT Broker | Telemetry published | Medium |
| VT-F-013 | REQ-SW-014 | Integration | Device + Network | mDNS resolves .local | High |
| VT-F-014 | REQ-SW-015 | Integration | Device + Browser | Web UI loads, joystick works | Critical |
| VT-F-015 | REQ-SW-016 | Hardware | Device | Diag mode entry/exit successful | Medium |
| VT-F-016 | REQ-SW-019 | Integration | Device + Internet | NTP syncs, time displayed | Medium |
| VT-F-017 | REQ-SW-022 | Integration | Device + Browser | Camera toggle works | Medium |
| VT-F-018 | REQ-SW-023 | Integration | Device | Watchdog stops motor on timeout | Critical |
| VT-F-019 | REQ-SW-024 | Integration | Device | E-Stop disables motor immediately | Critical |
| VT-F-020 | REQ-SW-025 | Hardware | Device + DMM | Deep sleep reduces current to <50µA | Medium |
| VT-F-021 | REQ-SW-026 | Integration | Device + Browser | Logs captured and streamed | High |
| VT-F-022 | REQ-SW-029 | Integration | Device | Task watchdog reboots on hang | Critical |
| VT-F-023 | REQ-SW-031 | Integration | Device | Build fingerprint accessible | High |
| VT-F-024 | REQ-SW-032 | Integration | Device + Browser | Live telemetry chart updates | Medium |
| VT-F-025 | REQ-SW-033 | Integration | Device + Browser | Dual-axis chart shows speed+steering | Medium |
| VT-F-026 | REQ-SW-034 | Integration | TTGO Device | Hold right button 5s switches to AP mode | Medium |

### 9.2 Non-Functional Validation Tests

| Test ID | Requirement | Test Type | Measurement | Target | Priority |
|---------|-------------|-----------|-------------|--------|----------|
| VT-NF-001 | REQ-NFR-001 | Performance | Control latency | <100ms | High |
| VT-NF-002 | REQ-NFR-002 | Performance | Camera FPS | ≥15 FPS QVGA | Medium |
| VT-NF-003 | REQ-NFR-003 | Performance | WiFi range | ≥30m LOS | Medium |
| VT-NF-004 | REQ-NFR-004 | Resource | Free heap | ≥32KB | Critical |
| VT-NF-005 | REQ-NFR-005 | Reliability | Uptime | ≥24h no crash | High |
| VT-NF-006 | REQ-NFR-006 | Reliability | WiFi reconnect | <30s | High |
| VT-NF-007 | REQ-NFR-007 | Usability | Web UI load time | <2s | Medium |
| VT-NF-008 | REQ-NFR-008 | Usability | LCD readability | Text clear at 30cm | Medium |
| VT-NF-010 | REQ-NFR-010 | Quality | Test coverage | ≥70% | High |
| VT-NF-011 | REQ-NFR-011 | Security | OTA auth | Wrong password rejected | Critical |
| VT-NF-012 | REQ-NFR-012 | Security | WiFi AP encryption | WPA2 enabled | High |

---

## 10. References

### 10.1 Related Documents

| Document | Version | Description |
|----------|---------|-------------|
| [Mechanical Requirements](mechanical_requirements.md) | 1.1.0 | Chassis, battery, motor mechanical specs |
| [API Reference](../implementation/API_REFERENCE.md) | - | Component APIs and configuration |
| [Web UI User Manual](../user/WEBUI_USER_MANUAL.md) | - | User guide for web control interface |
| [ESP-PROG JTAG Guide](esp-prog-jtag-guide.md) | - | Hardware debugging setup instructions |
| [Development Lessons](../implementation/DEVELOPMENT_LESSONS.md) | - | Lessons learned during development |

### 10.2 Standards

| Standard | Title | Relevance |
|----------|-------|-----------|
| IEEE 29148-2018 | Systems and software engineering — Requirements engineering | Document structure and format |
| ISO/IEC 12207 | Software life cycle processes | Development process framework |
| RFC 6762 | Multicast DNS | mDNS hostname resolution |
| RFC 1889 | RTP: A Transport Protocol for Real-Time Applications | MJPEG streaming considerations |

### 10.3 External References

| Reference | URL | Description |
|-----------|-----|-------------|
| ESP-IDF Documentation | https://docs.espressif.com/projects/esp-idf/en/v5.2.2/ | Espressif IoT Development Framework v5.2.2 |
| ESP32 Technical Reference | https://www.espressif.com/sites/default/files/documentation/esp32_technical_reference_manual_en.pdf | ESP32 SoC hardware manual |
| FreeRTOS Reference | https://www.freertos.org/Documentation/RTOS_book.html | Real-time operating system documentation |
| MQTT Specification | https://mqtt.org/mqtt-specification/ | MQTT protocol v3.1.1 |

### 10.4 Component Datasheets

| Component | Manufacturer | Datasheet URL |
|-----------|--------------|---------------|
| ESP32-WROOM-32 | Espressif | https://www.espressif.com/sites/default/files/documentation/esp32-wroom-32_datasheet_en.pdf |
| OV2640 Camera | OmniVision | https://www.uctronics.com/download/cam_module/OV2640DS.pdf |
| ST7789 LCD Controller | Sitronix | https://www.displayfuture.com/Display/datasheet/controller/ST7789.pdf |
| AS5600 Encoder | AMS | https://ams.com/documents/20143/36005/AS5600_DS000365_5-00.pdf |

---

## 11. Appendices

### Appendix A: Glossary

| Term | Definition |
|------|------------|
| **ADC** | Analog-to-Digital Converter - converts analog voltage to digital value |
| **AP** | Access Point - WiFi mode where device creates network |
| **API** | Application Programming Interface - set of functions for interaction |
| **Bootstrap Pin** | GPIO pin with special function during ESP32 boot sequence |
| **CMake** | Cross-platform build system generator used by ESP-IDF |
| **DMA** | Direct Memory Access - peripheral-to-memory transfer without CPU |
| **DRAM** | Dynamic Random Access Memory - volatile working memory |
| **E-Stop** | Emergency Stop - immediate motor disable for safety |
| **ESP-IDF** | Espressif IoT Development Framework - official ESP32 SDK |
| **FreeRTOS** | Free Real-Time Operating System - used by ESP-IDF |
| **GPIO** | General Purpose Input/Output - configurable digital pin |
| **I2C** | Inter-Integrated Circuit - two-wire serial communication protocol |
| **ISR** | Interrupt Service Routine - function called on hardware interrupt |
| **JTAG** | Joint Test Action Group - hardware debugging interface |
| **LCD** | Liquid Crystal Display - screen display technology |
| **mDNS** | Multicast DNS - zero-configuration hostname resolution (.local) |
| **MJPEG** | Motion JPEG - video format as series of JPEG images |
| **MQTT** | Message Queuing Telemetry Transport - lightweight IoT protocol |
| **NTP** | Network Time Protocol - clock synchronization over network |
| **OTA** | Over-The-Air - wireless firmware update |
| **PSRAM** | Pseudo-Static RAM - external RAM chip (4MB on ESP32-CAM) |
| **PWM** | Pulse Width Modulation - analog-like output via digital pulses |
| **REST** | Representational State Transfer - HTTP-based API style |
| **RSSI** | Received Signal Strength Indicator - WiFi signal strength (dBm) |
| **SNTP** | Simple Network Time Protocol - simplified NTP implementation |
| **SPI** | Serial Peripheral Interface - synchronous serial communication |
| **SSE** | Server-Sent Events - HTTP-based server push technology |
| **STA** | Station - WiFi mode where device connects to existing network |
| **TWDT** | Task Watchdog Timer - monitors task responsiveness |
| **UART** | Universal Asynchronous Receiver/Transmitter - serial communication |
| **WPA2** | Wi-Fi Protected Access 2 - wireless network encryption standard |
| **YAML** | YAML Ain't Markup Language - human-readable data serialization |

### Appendix B: Acronyms

| Acronym | Expansion |
|---------|-----------|
| **API** | Application Programming Interface |
| **HTTP** | Hypertext Transfer Protocol |
| **JSON** | JavaScript Object Notation |
| **LOS** | Line of Sight |
| **QoS** | Quality of Service |
| **TCP** | Transmission Control Protocol |
| **UDP** | User Datagram Protocol |
| **URI** | Uniform Resource Identifier |
| **URL** | Uniform Resource Locator |

### Appendix C: Configuration Example

**rover_config.yaml**:
```yaml
wifi:
  mode: sta_first  # ap_only, sta_only, or sta_first
  ap:
    ssid: "ESP32-Rover"
  sta:
    connect_timeout: 30

rest_api:
  enabled: true

mqtt:
  enabled: true
  broker:
    host: "192.168.1.100"
    port: 1883
  publish_interval_ms: 5000

control:
  watchdog_timeout_ms: 500

task_watchdog:
  enabled: true
  timeout_sec: 30
  panic_on_timeout: true

power:
  deep_sleep_enabled: true
  sleep_button_hold_time_ms: 5000

mdns:
  hostname_esp32cam: "esp32-rover"
  hostname_ttgo: "ttgo-rover"
```

**secrets.yaml** (not committed to git):
```yaml
wifi_sta_ssid: "YourNetworkName"
wifi_sta_password: "YourNetworkPassword"
wifi_ap_password: "rover1234"
ota_password: "rover1234"
mqtt_username: ""
mqtt_password: ""
```

### Appendix D: Risk Register

| Risk ID | Description | Likelihood | Impact | Mitigation | Owner |
|---------|-------------|------------|--------|------------|-------|
| R-SW-001 | Memory exhaustion during OTA | Medium | High | REQ-SW-028 (Resource guards), disable camera before OTA | Firmware Team |
| R-SW-002 | WiFi disconnection during operation | High | Medium | REQ-NFR-006 (Auto reconnect), REQ-SW-023 (Command watchdog) | Firmware Team |
| R-SW-003 | Task hang causing system freeze | Low | Critical | REQ-SW-029 (Task watchdog with auto-reboot) | Firmware Team |
| R-SW-004 | Camera stream overload | Medium | Medium | REQ-SW-022 (Stream toggle), frame rate limiting | Firmware Team |
| R-SW-005 | Unauthorized OTA access | Low | Critical | REQ-NFR-011 (Password protection), change default password | Security Team |
| R-SW-006 | GPIO 12 boot failure on ESP32-CAM | Medium | High | Hardware pull-down resistor, boot testing | Hardware Team |
| R-SW-007 | mDNS resolution failure | Medium | Medium | Fallback to IP address, clear documentation | Integration Team |
| R-SW-008 | Log buffer overflow | Low | Low | Ring buffer with automatic overwrite, configurable size | Firmware Team |
| R-SW-009 | NTP sync failure (no internet) | Medium | Low | Graceful degradation, yellow status indicator | Firmware Team |
| R-SW-010 | Deep sleep wake failure | Low | Medium | Wake-up test procedure, fallback power cycle | Hardware Team |

### Appendix E: Future Enhancements

| ID | Feature | Description | Priority | Dependencies |
|----|---------|-------------|----------|--------------|
| FE-001 | Bluetooth Control | BLE remote control as WiFi alternative | Low | - |
| FE-002 | SD Card Logging | Log to SD card for offline analysis | Medium | SPI bus availability |
| FE-003 | Voice Control | Google Assistant / Alexa integration | Low | Cloud connectivity |
| FE-004 | Autonomous Navigation | GPS waypoint following | Low | GPS module, motor control |
| FE-005 | Battery Management | Smart charging, fuel gauge IC | Medium | Hardware redesign |
| FE-006 | Multi-Device Support | Control multiple rovers from one UI | Low | MQTT discovery |
| FE-007 | Video Recording | Save camera stream to SD card | Medium | SD card, codec |
| FE-008 | Sensor Fusion | IMU-based stabilization | Medium | IMU module |
| FE-009 | Web Authentication | User login for security | Medium | Session management |
| FE-010 | Configuration Web UI | Web-based config editor (no YAML) | Low | Form validation |

---

**End of Software Requirements Specification**

---

**Document Approval**

This document has been reviewed and approved by the following stakeholders:

| Role | Name | Signature | Date |
|------|------|-----------|------|
| **Technical Lead** | _Pending_ | ___________ | _____ |
| **Software Architect** | _Pending_ | ___________ | _____ |
| **Quality Assurance** | _Pending_ | ___________ | _____ |
| **Project Manager** | _Pending_ | ___________ | _____ |

