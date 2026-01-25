# Mechatronics Requirements Specification
## ESP32 Rover Platform

| Document Info | Details |
|---------------|---------|
| **Project** | ESP32 WiFi-Controlled FPV Rover |
| **Document ID** | MTRS-ESP32-ROVER-001 |
| **Version** | 1.0.0 |
| **Date** | 2026-01-25 |
| **Status** | Draft |
| **Author** | Development Team |
| **Classification** | Technical Specification |

---

## Document Control

### Revision History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0.0 | 2026-01-25 | Dev Team | Initial draft based on codebase analysis |

### Approval Signatures

| Role | Name | Signature | Date |
|------|------|-----------|------|
| Technical Lead | TBD | - | - |
| Systems Engineer | TBD | - | - |
| Quality Assurance | TBD | - | - |

### Related Documents

| Document ID | Title | Relationship |
|-------------|-------|--------------|
| ESP32-ROVER-SRS-001 | Software Requirements Specification | Parent |
| MRS-ESP32-ROVER-001 | Mechanical Requirements Specification | Parent |
| ESP32-ROVER-ICD-001 | Interface Control Document | Derived |

---

## Table of Contents

1. [Introduction](#1-introduction)
2. [Scope](#2-scope)
3. [System Architecture](#3-system-architecture)
4. [Sensing Requirements](#4-sensing-requirements)
5. [Actuation Requirements](#5-actuation-requirements)
6. [Control System Requirements](#6-control-system-requirements)
7. [Power Management Requirements](#7-power-management-requirements)
8. [Communication Requirements](#8-communication-requirements)
9. [Safety Requirements](#9-safety-requirements)
10. [Human-Machine Interface Requirements](#10-human-machine-interface-requirements)
11. [Environmental Requirements](#11-environmental-requirements)
12. [Traceability Matrix](#12-traceability-matrix)
13. [Verification Methods](#13-verification-methods)
14. [References](#14-references)
15. [Appendices](#15-appendices)

---

## 1. Introduction

### 1.1 Purpose

This Mechatronics Requirements Specification (MTRS) defines the integrated electro-mechanical system requirements for the ESP32 Rover platform. It bridges the mechanical design (MRS-ESP32-ROVER-001) and software implementation (ESP32-ROVER-SRS-001) by specifying the requirements for sensors, actuators, control systems, and their interfaces.

### 1.2 Document Conventions

- **SHALL**: Mandatory requirement
- **SHOULD**: Recommended but not mandatory
- **MAY**: Optional feature
- **Priority Levels**: Critical (C), High (H), Medium (M), Low (L)
- **Verification Methods**: I (Inspection), T (Test), A (Analysis), D (Demonstration)
- **Implementation Status**: ✅ Implemented, ⚠️ Partial, ❌ Not Implemented, 🔮 Planned

### 1.3 Intended Audience

- Systems engineers
- Firmware developers
- Hardware designers
- Test engineers
- Integration specialists

### 1.4 Product Scope

The ESP32 Rover is an integrated mechatronic system featuring:
- Dual-target support (ESP32-CAM, TTGO T-Display)
- WiFi-based remote control
- Real-time sensor feedback
- Visual feedback via camera and/or LCD
- Safety-critical control features

---

## 2. Scope

### 2.1 System Boundary

This specification covers the interface between:
- Electronic sensors and the firmware
- Actuators and control signals
- Power management systems
- User interfaces (physical and virtual)

### 2.2 Current Implementation Status

| Subsystem | Status | Notes |
|-----------|--------|-------|
| Motor Control | ❌ | Infrastructure ready, hardware removed v1.6 |
| Battery Sensing | ✅ | ADC with filtering, TTGO only |
| Button Input | ✅ | GPIO ISR, TTGO only |
| Camera Streaming | ✅ | MJPEG, ESP32-CAM only |
| LCD Display | ✅ | ST7789, TTGO only |
| WiFi Control | ✅ | AP/STA modes |
| Safety Systems | ✅ | Watchdog, E-stop |
| Deep Sleep | ✅ | RTC wake, TTGO only |

---

## 3. System Architecture

### 3.1 Block Diagram

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         ESP32 ROVER SYSTEM                               │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  ┌──────────────┐     ┌──────────────┐     ┌──────────────┐            │
│  │   SENSORS    │     │  CONTROLLER  │     │  ACTUATORS   │            │
│  ├──────────────┤     ├──────────────┤     ├──────────────┤            │
│  │ Battery ADC  │────▶│              │────▶│ Motors (TBD) │            │
│  │ Buttons      │────▶│   ESP32      │────▶│ LCD Display  │            │
│  │ Camera       │────▶│   MCU        │────▶│ Backlight    │            │
│  │ WiFi RSSI    │────▶│              │────▶│ Flash LED    │            │
│  └──────────────┘     └──────────────┘     └──────────────┘            │
│                              │                                          │
│                              ▼                                          │
│                       ┌──────────────┐                                  │
│                       │ COMMUNICATION│                                  │
│                       ├──────────────┤                                  │
│                       │ WiFi AP/STA  │                                  │
│                       │ REST API     │                                  │
│                       │ MQTT         │                                  │
│                       │ mDNS         │                                  │
│                       └──────────────┘                                  │
└─────────────────────────────────────────────────────────────────────────┘
```

### 3.2 Target-Specific Features

| Feature | ESP32-CAM | TTGO T-Display |
|---------|-----------|----------------|
| Camera Module | ✅ OV2640 | ❌ |
| LCD Display | ❌ | ✅ ST7789 |
| Battery ADC | ❌ | ✅ GPIO34 |
| Buttons | ❌ | ✅ GPIO0/35 |
| Deep Sleep | ❌ | ✅ |
| PSRAM | ✅ 4MB | ❌ |
| Flash LED | ✅ GPIO4 | ❌ |

---

## 4. Sensing Requirements

### 4.1 Battery Voltage Sensing

| Req ID | Requirement | Priority | Status | Verification |
|--------|-------------|----------|--------|--------------|
| **MTRS-SEN-001** | The system SHALL measure battery voltage via ADC with ±50mV accuracy | H | ✅ | T |
| **MTRS-SEN-002** | The system SHALL use a voltage divider to scale battery voltage to ADC range (0-3.3V) | H | ✅ | I |
| **MTRS-SEN-003** | The system SHALL apply a low-pass filter to reduce ADC noise (α=0.02, ~10s time constant) | M | ✅ | T |
| **MTRS-SEN-004** | The system SHALL detect battery levels: Empty (3.0V), Low (3.4V), Medium (3.7V), Full (4.2V) | H | ✅ | T |
| **MTRS-SEN-005** | The system SHALL sample battery voltage at minimum 20Hz | M | ✅ | T |

**Implementation Details:**
- GPIO: 34 (ADC1 Channel 6)
- Voltage divider ratio: 2.0 (100kΩ/100kΩ)
- ADC resolution: 12-bit
- Attenuation: ADC_ATTEN_DB_12
- Filter: IIR low-pass, α=0.02

### 4.2 Button Input Sensing

| Req ID | Requirement | Priority | Status | Verification |
|--------|-------------|----------|--------|--------------|
| **MTRS-SEN-010** | The system SHALL detect button press/release via GPIO interrupt | H | ✅ | T |
| **MTRS-SEN-011** | The system SHALL support left button on GPIO0 (active LOW) | H | ✅ | I |
| **MTRS-SEN-012** | The system SHALL support right button on GPIO35 (active LOW) | H | ✅ | I |
| **MTRS-SEN-013** | The system SHALL detect simultaneous button press for mode switching | M | ✅ | D |
| **MTRS-SEN-014** | The system SHALL detect long-press (≥3s) for diagnostic mode entry | M | ✅ | D |
| **MTRS-SEN-015** | The system SHALL detect long-press (≥5s) on left button for deep sleep | M | ✅ | D |

**Implementation Details:**
- Interrupt mode: Both edges (rising and falling)
- Pull-up: Internal enabled
- ISR: Minimal (set flag only)
- Debounce: Software-based via state machine

### 4.3 Camera Sensing (ESP32-CAM Only)

| Req ID | Requirement | Priority | Status | Verification |
|--------|-------------|----------|--------|--------------|
| **MTRS-SEN-020** | The system SHALL initialize OV2640 camera sensor at boot | H | ✅ | T |
| **MTRS-SEN-021** | The system SHALL capture frames at QVGA resolution (320×240) | H | ✅ | T |
| **MTRS-SEN-022** | The system SHALL output JPEG-compressed frames (quality ≤12) | H | ✅ | T |
| **MTRS-SEN-023** | The system SHALL provide camera clock at 20MHz (XCLK) | H | ✅ | I |
| **MTRS-SEN-024** | The system SHALL support dual framebuffers for smooth streaming | M | ✅ | T |
| **MTRS-SEN-025** | The system SHALL allow runtime enable/disable of camera streaming | M | ✅ | D |

**Implementation Details:**
- Sensor: OV2640
- Frame size: FRAMESIZE_QVGA
- JPEG quality: 12 (0-63, lower=better)
- Framebuffer count: 2
- Location: PSRAM

### 4.4 WiFi Signal Sensing

| Req ID | Requirement | Priority | Status | Verification |
|--------|-------------|----------|--------|--------------|
| **MTRS-SEN-030** | The system SHALL measure WiFi RSSI with 1dBm resolution | M | ✅ | T |
| **MTRS-SEN-031** | The system SHALL report RSSI via status API and display | M | ✅ | D |
| **MTRS-SEN-032** | The system SHALL detect internet connectivity via ICMP ping | L | ✅ | T |

**Implementation Details:**
- RSSI: Via `esp_wifi_sta_get_ap_info()`
- Ping target: 1.1.1.1 (Cloudflare DNS)
- Ping timeout: 2000ms per packet
- Packet count: 3

---

## 5. Actuation Requirements

### 5.1 Motor Control (Planned)

| Req ID | Requirement | Priority | Status | Verification |
|--------|-------------|----------|--------|--------------|
| **MTRS-ACT-001** | The system SHALL control motor speed via PWM (0-100%) | H | ❌ | T |
| **MTRS-ACT-002** | The system SHALL support differential steering (-100 to +100) | H | ❌ | T |
| **MTRS-ACT-003** | The system SHALL implement speed ramping (5 units/iteration) | M | ❌ | T |
| **MTRS-ACT-004** | The system SHALL stop motors on communication loss | C | ⚠️ | T |
| **MTRS-ACT-005** | The system SHALL support emergency stop command | C | ✅ | T |

**Implementation Status:**
- Control command structure: ✅ Implemented (`rover_command_t`)
- Web API endpoints: ✅ Implemented (`POST /control`)
- Motor driver integration: ❌ Not implemented
- PWM output: ❌ Not implemented

**Planned GPIO Allocation:**
```c
// Motor control pins (TBD)
#define MOTOR_LEFT_PWM_PIN    GPIO_NUM_XX
#define MOTOR_LEFT_DIR_PIN    GPIO_NUM_XX
#define MOTOR_RIGHT_PWM_PIN   GPIO_NUM_XX
#define MOTOR_RIGHT_DIR_PIN   GPIO_NUM_XX
```

### 5.2 LCD Display Actuation

| Req ID | Requirement | Priority | Status | Verification |
|--------|-------------|----------|--------|--------------|
| **MTRS-ACT-010** | The system SHALL drive ST7789 LCD via SPI at 40MHz | H | ✅ | T |
| **MTRS-ACT-011** | The system SHALL support 135×240 pixel resolution | H | ✅ | I |
| **MTRS-ACT-012** | The system SHALL control backlight via PWM (0-100%) | M | ✅ | T |
| **MTRS-ACT-013** | The system SHALL update display at minimum 30Hz | M | ✅ | T |
| **MTRS-ACT-014** | The system SHALL support partial screen updates for efficiency | L | ✅ | A |

**Implementation Details:**
- SPI clock: 40MHz
- Backlight PWM: GPIO4, 5000Hz
- Display driver: `esp_lcd` component
- Color depth: 16-bit RGB565

**GPIO Configuration:**
```c
#define LCD_PIN_SCLK        GPIO_NUM_18
#define LCD_PIN_MOSI        GPIO_NUM_19
#define LCD_PIN_DC          GPIO_NUM_16
#define LCD_PIN_CS          GPIO_NUM_5
#define LCD_PIN_RST         GPIO_NUM_23
#define LCD_PIN_BACKLIGHT   GPIO_NUM_4
```

### 5.3 LED Actuation (ESP32-CAM Only)

| Req ID | Requirement | Priority | Status | Verification |
|--------|-------------|----------|--------|--------------|
| **MTRS-ACT-020** | The system SHALL control on-board flash LED | L | ✅ | D |
| **MTRS-ACT-021** | The system SHALL blink LED 3 times at startup | L | ✅ | D |
| **MTRS-ACT-022** | The system SHALL support LED on/off via REST API | L | ✅ | T |

**Implementation Details:**
- GPIO: 4
- Control: Digital on/off
- API: `GET/POST /led`

---

## 6. Control System Requirements

### 6.1 Command Processing

| Req ID | Requirement | Priority | Status | Verification |
|--------|-------------|----------|--------|--------------|
| **MTRS-CTL-001** | The system SHALL process control commands within 50ms | H | ✅ | T |
| **MTRS-CTL-002** | The system SHALL validate command parameters before execution | H | ✅ | T |
| **MTRS-CTL-003** | The system SHALL reject out-of-range values (-100 to +100) | H | ✅ | T |
| **MTRS-CTL-004** | The system SHALL prioritize emergency stop over all commands | C | ✅ | T |

**Command Structure:**
```c
typedef struct {
    float speed;        // -100 to +100 (percentage)
    float steering;     // -100 to +100 (left/right)
    bool emergency_stop;
} rover_command_t;
```

### 6.2 Control Loop Timing

| Req ID | Requirement | Priority | Status | Verification |
|--------|-------------|----------|--------|--------------|
| **MTRS-CTL-010** | The system SHALL run status update task at 20Hz | H | ✅ | T |
| **MTRS-CTL-011** | The system SHALL run LCD update task at 60Hz | M | ✅ | T |
| **MTRS-CTL-012** | The system SHALL feed watchdog within 5 second window | C | ✅ | T |

**Task Configuration:**
| Task | Update Rate | Stack Size | Priority |
|------|-------------|------------|----------|
| Status Update | 20Hz (50ms) | 2560-4096B | tskIDLE+2 |
| LCD Update | 60Hz (16ms) | 3072-4096B | tskIDLE+1 |
| HTTP Server | Event-driven | Per-config | tskIDLE+5 |

### 6.3 Feedback Systems

| Req ID | Requirement | Priority | Status | Verification |
|--------|-------------|----------|--------|--------------|
| **MTRS-CTL-020** | The system SHALL report current velocity to UI | H | ✅ | D |
| **MTRS-CTL-021** | The system SHALL report steering angle to UI | H | ✅ | D |
| **MTRS-CTL-022** | The system SHALL report battery level with color coding | H | ✅ | D |
| **MTRS-CTL-023** | The system SHALL report connection status | H | ✅ | D |

---

## 7. Power Management Requirements

### 7.1 Operating Modes

| Req ID | Requirement | Priority | Status | Verification |
|--------|-------------|----------|--------|--------------|
| **MTRS-PWR-001** | The system SHALL operate from 3.0V to 4.2V battery voltage | H | ✅ | T |
| **MTRS-PWR-002** | The system SHALL enter deep sleep mode on user command | M | ✅ | T |
| **MTRS-PWR-003** | The system SHALL wake from deep sleep via button press | M | ✅ | T |
| **MTRS-PWR-004** | The system SHALL consume <15µA in deep sleep | L | ⚠️ | T |

**Deep Sleep Configuration:**
- Wake source: GPIO35 (right button)
- Wake trigger: LOW level
- Power domains disabled: RTC_SLOW_MEM, RTC_FAST_MEM
- Peripheral state: WiFi stopped, LCD off

### 7.2 Power Consumption Targets

| Mode | Current Draw | Notes |
|------|--------------|-------|
| Active (WiFi + LCD) | ~180mA | Normal operation |
| Active (WiFi + Camera) | ~300mA | Streaming video |
| Deep Sleep | <15µA | RTC wake enabled |

### 7.3 Battery Protection

| Req ID | Requirement | Priority | Status | Verification |
|--------|-------------|----------|--------|--------------|
| **MTRS-PWR-010** | The system SHALL warn user at 3.4V battery level | H | ✅ | D |
| **MTRS-PWR-011** | The system SHOULD auto-shutdown at 3.0V | M | ❌ | T |
| **MTRS-PWR-012** | The system SHALL not damage battery through over-discharge | H | ⚠️ | A |

---

## 8. Communication Requirements

### 8.1 WiFi Connectivity

| Req ID | Requirement | Priority | Status | Verification |
|--------|-------------|----------|--------|--------------|
| **MTRS-COM-001** | The system SHALL support Access Point mode | H | ✅ | T |
| **MTRS-COM-002** | The system SHALL support Station mode | H | ✅ | T |
| **MTRS-COM-003** | The system SHALL support STA-first with AP fallback mode | M | ✅ | T |
| **MTRS-COM-004** | The system SHALL retry STA connection up to 3 times | H | ✅ | T |
| **MTRS-COM-005** | The system SHALL fallback to AP if STA fails after timeout (30s) | H | ✅ | T |

**WiFi Configuration:**
```c
// AP Mode
#define WIFI_AP_SSID        "ESP32-Rover"
#define WIFI_AP_PASSWORD    "rover1234"
#define WIFI_AP_CHANNEL     1
#define WIFI_AP_MAX_CONN    4

// STA Mode
#define WIFI_STA_CONNECT_TIMEOUT_S  10
#define WIFI_STA_MAX_RETRIES        3
```

### 8.2 Protocol Support

| Req ID | Requirement | Priority | Status | Verification |
|--------|-------------|----------|--------|--------------|
| **MTRS-COM-010** | The system SHALL provide REST API for control | H | ✅ | T |
| **MTRS-COM-011** | The system SHALL provide MJPEG streaming endpoint | H | ✅ | T |
| **MTRS-COM-012** | The system SHALL support MQTT telemetry publishing | M | ✅ | T |
| **MTRS-COM-013** | The system SHALL support mDNS hostname resolution | M | ✅ | T |
| **MTRS-COM-014** | The system SHALL sync time via NTP | L | ✅ | T |

**REST API Endpoints:**
| Endpoint | Method | Purpose |
|----------|--------|---------|
| `/` | GET | Web UI |
| `/status` | GET | JSON diagnostics |
| `/control` | POST | Motor commands |
| `/stream` | GET | MJPEG video |
| `/camera` | GET/POST | Stream control |
| `/ota` | POST | Firmware update |

### 8.3 Data Rates

| Req ID | Requirement | Priority | Status | Verification |
|--------|-------------|----------|--------|--------------|
| **MTRS-COM-020** | The system SHALL support >5 fps video streaming | H | ✅ | T |
| **MTRS-COM-021** | The system SHALL respond to control commands within 100ms | H | ✅ | T |
| **MTRS-COM-022** | The system SHALL publish MQTT telemetry every 5 seconds | L | ✅ | T |

---

## 9. Safety Requirements

### 9.1 Watchdog Systems

| Req ID | Requirement | Priority | Status | Verification |
|--------|-------------|----------|--------|--------------|
| **MTRS-SAF-001** | The system SHALL implement command watchdog (500ms timeout) | C | ✅ | T |
| **MTRS-SAF-002** | The system SHALL stop actuators on watchdog timeout | C | ⚠️ | T |
| **MTRS-SAF-003** | The system SHALL implement task watchdog (30s timeout) | H | ✅ | T |
| **MTRS-SAF-004** | The system SHALL reboot on task watchdog timeout | H | ✅ | T |

**Watchdog Configuration:**
```c
#define WATCHDOG_TIMEOUT_MS         500     // Command timeout
#define TASK_WDT_TIMEOUT_SEC        30      // Task watchdog
#define TASK_WDT_PANIC_ON_TIMEOUT   1       // Reboot on timeout
```

### 9.2 Emergency Stop

| Req ID | Requirement | Priority | Status | Verification |
|--------|-------------|----------|--------|--------------|
| **MTRS-SAF-010** | The system SHALL provide emergency stop command | C | ✅ | T |
| **MTRS-SAF-011** | The system SHALL stop all actuators on E-stop | C | ⚠️ | T |
| **MTRS-SAF-012** | The system SHALL indicate E-stop status visually | H | ✅ | D |
| **MTRS-SAF-013** | The system SHALL require explicit command to clear E-stop | H | ✅ | T |

### 9.3 Fault Detection

| Req ID | Requirement | Priority | Status | Verification |
|--------|-------------|----------|--------|--------------|
| **MTRS-SAF-020** | The system SHALL detect low battery condition | H | ✅ | T |
| **MTRS-SAF-021** | The system SHALL detect WiFi disconnection | H | ✅ | T |
| **MTRS-SAF-022** | The system SHALL log all fault conditions | M | ✅ | I |
| **MTRS-SAF-023** | The system SHALL display fault indicators on UI | H | ✅ | D |

---

## 10. Human-Machine Interface Requirements

### 10.1 Web Interface

| Req ID | Requirement | Priority | Status | Verification |
|--------|-------------|----------|--------|--------------|
| **MTRS-HMI-001** | The system SHALL provide responsive web UI | H | ✅ | D |
| **MTRS-HMI-002** | The system SHALL provide virtual joystick control | H | ✅ | D |
| **MTRS-HMI-003** | The system SHALL display live video (ESP32-CAM) | H | ✅ | D |
| **MTRS-HMI-004** | The system SHALL display real-time status | H | ✅ | D |
| **MTRS-HMI-005** | The system SHALL support touch input | M | ✅ | D |

### 10.2 LCD Interface (TTGO Only)

| Req ID | Requirement | Priority | Status | Verification |
|--------|-------------|----------|--------|--------------|
| **MTRS-HMI-010** | The system SHALL display main status screen | H | ✅ | D |
| **MTRS-HMI-011** | The system SHALL display diagnostic screen | M | ✅ | D |
| **MTRS-HMI-012** | The system SHALL display deep sleep animation | L | ✅ | D |
| **MTRS-HMI-013** | The system SHALL use color coding for battery level | M | ✅ | D |
| **MTRS-HMI-014** | The system SHALL display WiFi connection status | H | ✅ | D |

**LCD Display Elements:**
| Screen | Elements |
|--------|----------|
| Main | Speed bar, steering, battery, RSSI, buttons, uptime |
| Diagnostic | WiFi info, memory, services, build info, NTP status |
| Sleep | Snorlax sprite, "Zzz..." animation |

### 10.3 Physical Controls (TTGO Only)

| Req ID | Requirement | Priority | Status | Verification |
|--------|-------------|----------|--------|--------------|
| **MTRS-HMI-020** | The system SHALL respond to button press within 100ms | H | ✅ | T |
| **MTRS-HMI-021** | The system SHALL support button combinations | M | ✅ | D |
| **MTRS-HMI-022** | The system SHALL provide visual feedback for button actions | M | ✅ | D |

---

## 11. Environmental Requirements

### 11.1 Operating Conditions

| Req ID | Requirement | Priority | Status | Verification |
|--------|-------------|----------|--------|--------------|
| **MTRS-ENV-001** | The system SHALL operate at 0°C to 45°C ambient | H | ⚠️ | T |
| **MTRS-ENV-002** | The system SHALL operate at 20-80% relative humidity | M | ⚠️ | T |
| **MTRS-ENV-003** | The system SHOULD operate indoors only | - | - | A |

### 11.2 Storage Conditions

| Req ID | Requirement | Priority | Status | Verification |
|--------|-------------|----------|--------|--------------|
| **MTRS-ENV-010** | The system SHALL store at -20°C to 60°C | L | ⚠️ | A |
| **MTRS-ENV-011** | The system SHALL store at 10-90% relative humidity | L | ⚠️ | A |

---

## 12. Traceability Matrix

### 12.1 Requirements to Implementation

| Req ID | Source File | Function/Component | Test Case |
|--------|-------------|-------------------|-----------|
| MTRS-SEN-001 | main.c:172-211 | `read_battery_voltage()` | TC-BAT-001 |
| MTRS-SEN-010 | main.c:63-112 | `button_isr_handler()` | TC-BTN-001 |
| MTRS-SEN-020 | camera.c | `camera_init()` | TC-CAM-001 |
| MTRS-ACT-010 | lcd_display.c | `lcd_display_init()` | TC-LCD-001 |
| MTRS-CTL-001 | web_server.c | `control_handler()` | TC-CTL-001 |
| MTRS-SAF-001 | main.c | watchdog logic | TC-SAF-001 |
| MTRS-COM-001 | main.c:215-478 | WiFi initialization | TC-COM-001 |
| MTRS-HMI-001 | web_ui.c | HTML/JS UI | TC-HMI-001 |

### 12.2 Requirements to Software Requirements

| MTRS Req | SRS Req | Relationship |
|----------|---------|--------------|
| MTRS-SEN-001 | REQ-SW-005 | Derives from |
| MTRS-SEN-010 | REQ-SW-006 | Derives from |
| MTRS-ACT-010 | REQ-SW-008 | Derives from |
| MTRS-COM-001 | REQ-SW-009 | Derives from |
| MTRS-SAF-001 | REQ-SW-023 | Derives from |

### 12.3 Requirements to Mechanical Requirements

| MTRS Req | MRS Req | Relationship |
|----------|---------|--------------|
| MTRS-SEN-001 | MRS-PWR-001 | Battery interface |
| MTRS-ACT-001 | MRS-DRV-001 | Motor mounting |
| MTRS-ENV-001 | MRS-ENV-001 | Operating temp |

---

## 13. Verification Methods

### 13.1 Test Categories

| Method | Code | Description |
|--------|------|-------------|
| Inspection | I | Visual examination of design/code |
| Test | T | Functional verification via testing |
| Analysis | A | Mathematical or logical analysis |
| Demonstration | D | Hands-on demonstration of capability |

### 13.2 Test Equipment

| Equipment | Purpose | Calibration |
|-----------|---------|-------------|
| Multimeter | Voltage measurement | Annual |
| Oscilloscope | Signal analysis | Annual |
| WiFi analyzer | RF testing | N/A |
| Test firmware | Automated testing | Per-release |

### 13.3 Test Procedures

Detailed test procedures are documented in `/test/` directory:
- Unit tests: `test/src/`
- Integration tests: Manual verification
- System tests: End-to-end validation

---

## 14. References

1. ESP-IDF Programming Guide v5.2.2
2. OV2640 Camera Module Datasheet
3. ST7789 LCD Controller Datasheet
4. ESP32 Technical Reference Manual
5. IEEE 29148-2018 Requirements Engineering
6. ESP32-ROVER-SRS-001 Software Requirements Specification
7. MRS-ESP32-ROVER-001 Mechanical Requirements Specification

---

## 15. Appendices

### Appendix A: GPIO Allocation Summary

#### A.1 TTGO T-Display

| GPIO | Function | Direction | Notes |
|------|----------|-----------|-------|
| 0 | Left Button | Input | Active LOW, bootstrap |
| 4 | LCD Backlight | Output | PWM |
| 5 | LCD CS | Output | SPI |
| 16 | LCD DC | Output | SPI |
| 18 | LCD SCLK | Output | SPI |
| 19 | LCD MOSI | Output | SPI |
| 23 | LCD RST | Output | Active LOW |
| 34 | Battery ADC | Input | ADC1_CH6 |
| 35 | Right Button | Input | Active LOW |

#### A.2 ESP32-CAM

| GPIO | Function | Direction | Notes |
|------|----------|-----------|-------|
| 0 | XCLK | Output | Camera clock |
| 4 | Flash LED | Output | Active HIGH |
| 5 | D0 | Input | Camera data |
| 18 | D1 | Input | Camera data |
| 19 | D2 | Input | Camera data |
| 21 | D3 | Input | Camera data |
| 22 | PCLK | Input | Pixel clock |
| 23 | HREF | Input | Horizontal ref |
| 25 | VSYNC | Input | Vertical sync |
| 26 | SIOD | I/O | I2C data |
| 27 | SIOC | Output | I2C clock |
| 32 | PWDN | Output | Power down |
| 34 | D6 | Input | Camera data |
| 35 | D7 | Input | Camera data |
| 36 | D4 | Input | Camera data |
| 39 | D5 | Input | Camera data |

### Appendix B: Configuration Parameters

```c
// Battery thresholds (V)
#define BATTERY_VOLTAGE_EMPTY_V     3.0f
#define BATTERY_VOLTAGE_LOW_V       3.4f
#define BATTERY_VOLTAGE_MED_V       3.7f
#define BATTERY_VOLTAGE_FULL_V      4.2f

// Timing (ms)
#define WATCHDOG_TIMEOUT_MS         500
#define CONNECTION_TIMEOUT_MS       1000
#define WIFI_STA_CONNECT_TIMEOUT_S  10

// Control limits
#define MAX_SPEED                   100
#define SPEED_RAMP_RATE             5
```

### Appendix C: Status JSON Structure

```json
{
  "target": "ttgo",
  "velocity": 0.0,
  "battery": 3.85,
  "camera": false,
  "rssi": -65,
  "btnL": false,
  "btnR": false,
  "diag": {
    "ssid": "HomeNetwork",
    "ip": "192.168.1.100",
    "wifiMode": "sta",
    "channel": 6,
    "freeHeap": 150000,
    "uptime": 3600,
    "buildFingerprint": "abc1234",
    "buildDirty": false
  }
}
```

---

*End of Document*
