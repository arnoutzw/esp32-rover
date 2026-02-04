# Electrical Requirements Specification
## ESP32 Rover Platform

| Document Info | Details |
|---------------|---------|
| **Project** | ESP32 WiFi-Controlled FPV Rover |
| **Document ID** | ERS-ESP32-ROVER-001 |
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
| 1.0.0 | 2026-01-25 | Dev Team | Initial draft based on hardware analysis |

### Approval Signatures

| Role | Name | Signature | Date |
|------|------|-----------|------|
| Hardware Lead | TBD | - | - |
| Systems Engineer | TBD | - | - |
| Quality Assurance | TBD | - | - |

### Related Documents

| Document ID | Title | Relationship |
|-------------|-------|--------------|
| ESP32-ROVER-SRS-001 | Software Requirements Specification | Parent |
| MRS-ESP32-ROVER-001 | Mechanical Requirements Specification | Sibling |
| MTRS-ESP32-ROVER-001 | Mechatronics Requirements Specification | Sibling |
| ESP32-ROVER-ICD-001 | Interface Control Document | Derived |

---

## Table of Contents

1. [Introduction](#1-introduction)
2. [Scope](#2-scope)
3. [Power System Requirements](#3-power-system-requirements)
4. [Microcontroller Requirements](#4-microcontroller-requirements)
5. [GPIO Interface Requirements](#5-gpio-interface-requirements)
6. [Analog Interface Requirements](#6-analog-interface-requirements)
7. [Digital Interface Requirements](#7-digital-interface-requirements)
8. [Communication Interface Requirements](#8-communication-interface-requirements)
9. [Display Interface Requirements](#9-display-interface-requirements)
10. [Camera Interface Requirements](#10-camera-interface-requirements)
11. [Motor Driver Interface Requirements](#11-motor-driver-interface-requirements)
12. [EMC and Safety Requirements](#12-emc-and-safety-requirements)
13. [Thermal Requirements](#13-thermal-requirements)
14. [Traceability Matrix](#14-traceability-matrix)
15. [Verification Methods](#15-verification-methods)
16. [References](#16-references)
17. [Appendices](#17-appendices)

---

## 1. Introduction

### 1.1 Purpose

This Electrical Requirements Specification (ERS) defines the electrical design requirements for the ESP32 Rover platform. It specifies power distribution, signal interfaces, component selection criteria, and electrical safety requirements.

### 1.2 Document Conventions

- **SHALL**: Mandatory requirement
- **SHOULD**: Recommended but not mandatory
- **MAY**: Optional feature
- **Priority Levels**: Critical (C), High (H), Medium (M), Low (L)
- **Verification Methods**: I (Inspection), T (Test), A (Analysis), D (Demonstration)
- **Implementation Status**: ✅ Implemented, ⚠️ Partial, ❌ Not Implemented, 🔮 Planned

### 1.3 Intended Audience

- Hardware engineers
- PCB designers
- Systems integrators
- Test engineers
- Component procurement

### 1.4 Reference Standards

- IEC 61000-4: EMC testing
- IEC 62368-1: Audio/video, IT equipment safety
- ESP32 Hardware Design Guidelines (Espressif)

---

## 2. Scope

### 2.1 System Boundary

This specification covers:
- Power supply and distribution
- Microcontroller electrical characteristics
- GPIO allocation and electrical levels
- Peripheral interfaces (SPI, I2C, UART, ADC, PWM)
- RF considerations for WiFi
- ESD and surge protection

### 2.2 Supported Targets

| Target | Module | Flash | PSRAM | Form Factor |
|--------|--------|-------|-------|-------------|
| ESP32-CAM | AI-Thinker | 4MB | 4MB | 40.5×27mm |
| TTGO T-Display | LilyGO | 4MB | None | 51.5×25mm |

---

## 3. Power System Requirements

### 3.1 Input Power

| Req ID | Requirement | Priority | Status | Verification |
|--------|-------------|----------|--------|--------------|
| **ERS-PWR-001** | The system SHALL operate from single-cell Li-ion battery (3.0V-4.2V) | C | ✅ | T |
| **ERS-PWR-002** | The system SHALL accept USB power (5V ±5%) for charging | H | ✅ | T |
| **ERS-PWR-003** | The system SHALL support simultaneous charge and operation | M | ⚠️ | T |
| **ERS-PWR-004** | The system SHALL limit inrush current to <500mA | M | ⚠️ | T |

### 3.2 Voltage Rails

| Rail | Nominal | Min | Max | Ripple | Source |
|------|---------|-----|-----|--------|--------|
| VBAT | 3.7V | 3.0V | 4.2V | N/A | Li-ion cell |
| VCC_3V3 | 3.3V | 3.135V | 3.465V | <50mVpp | LDO regulator |
| VUSB | 5.0V | 4.75V | 5.25V | <100mVpp | USB-C |

| Req ID | Requirement | Priority | Status | Verification |
|--------|-------------|----------|--------|--------------|
| **ERS-PWR-010** | The 3.3V rail SHALL be regulated with <50mV ripple | H | ✅ | T |
| **ERS-PWR-011** | The 3.3V LDO SHALL provide minimum 500mA output | H | ✅ | A |
| **ERS-PWR-012** | The system SHALL include reverse polarity protection | H | ⚠️ | I |

### 3.3 Power Consumption

| Mode | Typical | Maximum | Notes |
|------|---------|---------|-------|
| Active (WiFi AP + LCD) | 120mA | 180mA | TTGO T-Display |
| Active (WiFi AP + Camera) | 250mA | 350mA | ESP32-CAM streaming |
| WiFi TX burst | +180mA | +300mA | Peak during transmission |
| Camera capture | +60mA | +100mA | Per frame capture |
| LCD backlight (100%) | +25mA | +40mA | PWM at full brightness |
| Deep sleep | 10µA | 15µA | RTC + wake GPIO enabled |
| Hibernation | 5µA | 10µA | RTC off, GPIO wake only |

| Req ID | Requirement | Priority | Status | Verification |
|--------|-------------|----------|--------|--------------|
| **ERS-PWR-020** | The system SHALL consume <200mA average in active mode (TTGO) | H | ✅ | T |
| **ERS-PWR-021** | The system SHALL consume <350mA average in active mode (ESP32-CAM) | H | ✅ | T |
| **ERS-PWR-022** | The system SHALL consume <15µA in deep sleep | M | ✅ | T |
| **ERS-PWR-023** | The system SHALL handle 500mA peak current transients | H | ✅ | A |

### 3.4 Battery Management

| Req ID | Requirement | Priority | Status | Verification |
|--------|-------------|----------|--------|--------------|
| **ERS-PWR-030** | The charging circuit SHALL limit charge current to 500mA (USB) | H | ✅ | T |
| **ERS-PWR-031** | The charging circuit SHALL terminate at 4.2V ±1% | C | ✅ | T |
| **ERS-PWR-032** | The system SHALL provide over-discharge protection at 2.8V | C | ⚠️ | T |
| **ERS-PWR-033** | The system SHALL provide over-charge protection at 4.25V | C | ✅ | T |
| **ERS-PWR-034** | The battery connector SHALL support >1A continuous current | H | ✅ | I |

**Battery Specifications:**
- Chemistry: Li-ion / Li-Po
- Nominal voltage: 3.7V
- Capacity: 500-2000mAh (application dependent)
- Discharge cutoff: 3.0V (firmware), 2.8V (hardware protection)
- Charge termination: 4.2V
- Recommended connector: JST PH 2.0mm

### 3.5 Power Sequencing

| Req ID | Requirement | Priority | Status | Verification |
|--------|-------------|----------|--------|--------------|
| **ERS-PWR-040** | The 3.3V rail SHALL stabilize within 10ms of power-on | H | ✅ | T |
| **ERS-PWR-041** | GPIO states SHALL be defined during power-up | M | ⚠️ | A |
| **ERS-PWR-042** | The system SHALL enter bootloader if GPIO0 is LOW at reset | H | ✅ | T |

---

## 4. Microcontroller Requirements

### 4.1 ESP32 Core Specifications

| Parameter | Value | Notes |
|-----------|-------|-------|
| MCU | ESP32-D0WDQ6 | Dual-core Xtensa LX6 |
| Clock frequency | 240 MHz | Maximum |
| Flash | 4MB | QIO mode |
| PSRAM | 4MB (ESP32-CAM) / None (TTGO) | SPIRAM |
| Operating voltage | 3.0V - 3.6V | Nominal 3.3V |
| Operating temperature | -40°C to +85°C | Industrial grade |

| Req ID | Requirement | Priority | Status | Verification |
|--------|-------------|----------|--------|--------------|
| **ERS-MCU-001** | The MCU SHALL operate at 240MHz clock frequency | H | ✅ | T |
| **ERS-MCU-002** | The MCU SHALL have minimum 4MB flash storage | H | ✅ | I |
| **ERS-MCU-003** | The ESP32-CAM target SHALL have 4MB PSRAM | H | ✅ | I |
| **ERS-MCU-004** | The MCU SHALL support 802.11 b/g/n WiFi | C | ✅ | T |

### 4.2 Clock and Reset

| Req ID | Requirement | Priority | Status | Verification |
|--------|-------------|----------|--------|--------------|
| **ERS-MCU-010** | The crystal oscillator SHALL be 40MHz ±10ppm | H | ✅ | T |
| **ERS-MCU-011** | The RTC crystal SHALL be 32.768kHz ±20ppm | M | ✅ | T |
| **ERS-MCU-012** | The reset circuit SHALL provide minimum 100µs reset pulse | H | ✅ | T |
| **ERS-MCU-013** | The EN pin SHALL have RC filter (10kΩ, 100nF) | H | ✅ | I |

### 4.3 Memory Requirements

| Memory Type | Minimum | Typical | Purpose |
|-------------|---------|---------|---------|
| DRAM | 320KB | 520KB | Runtime data, stacks |
| IRAM | 128KB | 200KB | Critical code, ISR |
| Flash | 4MB | 4MB | Firmware, OTA, NVS |
| PSRAM | 0MB (TTGO) / 4MB (CAM) | - | Frame buffers |

| Req ID | Requirement | Priority | Status | Verification |
|--------|-------------|----------|--------|--------------|
| **ERS-MCU-020** | The system SHALL have minimum 150KB free heap at runtime | H | ✅ | T |
| **ERS-MCU-021** | The system SHALL reserve 1MB for OTA partition | H | ✅ | I |
| **ERS-MCU-022** | The system SHALL use PSRAM for camera framebuffers | H | ✅ | T |

---

## 5. GPIO Interface Requirements

### 5.1 General GPIO Characteristics

| Parameter | Specification | Notes |
|-----------|---------------|-------|
| Logic HIGH | >2.475V (0.75×VDD) | VDD = 3.3V |
| Logic LOW | <0.825V (0.25×VDD) | VDD = 3.3V |
| Output HIGH | >2.64V @ 12mA | Source current |
| Output LOW | <0.33V @ 12mA | Sink current |
| Maximum source/sink | 40mA | Per GPIO |
| Total GPIO current | 1200mA | All GPIOs combined |
| Input leakage | <50nA | High-Z state |
| Internal pull-up | 45kΩ typical | 30-80kΩ range |
| Internal pull-down | 45kΩ typical | 30-80kΩ range |

### 5.2 GPIO Allocation - TTGO T-Display

| GPIO | Function | Direction | Type | Notes |
|------|----------|-----------|------|-------|
| 0 | Left Button / Boot | Input | Digital | Active LOW, internal pull-up |
| 4 | LCD Backlight | Output | PWM | 5kHz, 8-bit duty |
| 5 | LCD CS | Output | Digital | SPI chip select |
| 16 | LCD DC | Output | Digital | Data/Command select |
| 18 | LCD SCLK | Output | SPI | 40MHz clock |
| 19 | LCD MOSI | Output | SPI | SPI data out |
| 23 | LCD RST | Output | Digital | Active LOW reset |
| 34 | Battery ADC | Input | Analog | ADC1_CH6, input only |
| 35 | Right Button | Input | Digital | Active LOW, input only |

| Req ID | Requirement | Priority | Status | Verification |
|--------|-------------|----------|--------|--------------|
| **ERS-GPIO-001** | GPIO0 SHALL have 10kΩ external pull-up for reliable boot | H | ✅ | I |
| **ERS-GPIO-002** | GPIO34/35 SHALL be used for input-only functions | H | ✅ | I |
| **ERS-GPIO-003** | Unused GPIOs SHOULD be left floating or weakly pulled | L | ⚠️ | I |

### 5.3 GPIO Allocation - ESP32-CAM

| GPIO | Function | Direction | Type | Notes |
|------|----------|-----------|------|-------|
| 0 | Camera XCLK | Output | Clock | 20MHz camera clock |
| 2 | - | - | - | Bootstrap, avoid |
| 4 | Flash LED | Output | Digital | High = LED on |
| 5 | Camera D0 | Input | Digital | Data bit 0 |
| 12 | - | - | - | Bootstrap (JTAG TDI) |
| 13 | - | - | - | JTAG TCK |
| 14 | - | - | - | JTAG TMS |
| 15 | - | - | - | Bootstrap (JTAG TDO) |
| 18 | Camera D1 | Input | Digital | Data bit 1 |
| 19 | Camera D2 | Input | Digital | Data bit 2 |
| 21 | Camera D3 | Input | Digital | Data bit 3 |
| 22 | Camera PCLK | Input | Clock | Pixel clock |
| 23 | Camera HREF | Input | Digital | Horizontal sync |
| 25 | Camera VSYNC | Input | Digital | Vertical sync |
| 26 | Camera SIOD | I/O | I2C | SCCB data |
| 27 | Camera SIOC | Output | I2C | SCCB clock |
| 32 | Camera PWDN | Output | Digital | Power down |
| 34 | Camera D6 | Input | Digital | Data bit 6 |
| 35 | Camera D7 | Input | Digital | Data bit 7 |
| 36 | Camera D4 | Input | Digital | Data bit 4 |
| 39 | Camera D5 | Input | Digital | Data bit 5 |

### 5.4 Bootstrap Pin Requirements

| GPIO | Function | Boot Mode | Required State |
|------|----------|-----------|----------------|
| 0 | Boot select | Download | LOW |
| 0 | Boot select | Normal | HIGH or floating |
| 2 | Boot select | Download | Don't care |
| 2 | Boot select | Normal | LOW or floating |
| 12 | MTDI | VDD_SDIO | LOW = 3.3V, HIGH = 1.8V |
| 15 | MTDO | Debug | Don't care (pull-up recommended) |

| Req ID | Requirement | Priority | Status | Verification |
|--------|-------------|----------|--------|--------------|
| **ERS-GPIO-010** | GPIO0 SHALL be HIGH or floating for normal boot | C | ✅ | T |
| **ERS-GPIO-011** | GPIO2 SHALL be LOW or floating for normal boot | C | ✅ | I |
| **ERS-GPIO-012** | GPIO12 SHALL be LOW for 3.3V flash operation | C | ✅ | I |
| **ERS-GPIO-013** | GPIO15 SHOULD have weak pull-up to suppress boot messages | L | ⚠️ | I |

---

## 6. Analog Interface Requirements

### 6.1 ADC Specifications

| Parameter | ADC1 | ADC2 | Notes |
|-----------|------|------|-------|
| Resolution | 12-bit | 12-bit | 0-4095 counts |
| Channels | 8 (GPIO32-39) | 10 (GPIO0,2,4,12-15,25-27) | ADC2 unavailable with WiFi |
| Input range | 0-3.3V | 0-3.3V | With attenuation |
| Sample rate | Up to 2Msps | Up to 2Msps | Per channel |
| INL | ±7 LSB | ±7 LSB | Typical |
| DNL | ±1 LSB | ±1 LSB | Typical |
| Input impedance | >10MΩ | >10MΩ | At sampling |

### 6.2 ADC Attenuation Settings

| Attenuation | Full-scale | Effective range | Accuracy |
|-------------|------------|-----------------|----------|
| 0dB | 1.1V | 100-950mV | ±6% |
| 2.5dB | 1.5V | 100-1250mV | ±6% |
| 6dB | 2.2V | 150-1750mV | ±6% |
| 11dB (12dB) | 3.3V | 150-2450mV | ±6% |

| Req ID | Requirement | Priority | Status | Verification |
|--------|-------------|----------|--------|--------------|
| **ERS-ADC-001** | Battery ADC SHALL use ADC1 (GPIO34) to avoid WiFi conflicts | C | ✅ | I |
| **ERS-ADC-002** | Battery ADC SHALL use 11dB attenuation for 0-3.3V range | H | ✅ | T |
| **ERS-ADC-003** | Battery voltage divider SHALL scale 4.2V to <3.3V | H | ✅ | A |
| **ERS-ADC-004** | ADC input impedance SHALL be <10kΩ for accuracy | M | ✅ | A |

### 6.3 Battery Voltage Divider

```
VBAT ───┬─── R1 (100kΩ) ───┬─── R2 (100kΩ) ───┬─── GND
        │                   │                   │
        │                   └─── GPIO34 (ADC)   │
        │                   │                   │
        │                   └─── C1 (100nF)  ───┘
        │
        └─── 4.2V max input
```

| Req ID | Requirement | Priority | Status | Verification |
|--------|-------------|----------|--------|--------------|
| **ERS-ADC-010** | Voltage divider ratio SHALL be 2:1 (±1%) | H | ✅ | T |
| **ERS-ADC-011** | Filter capacitor SHALL be 100nF for noise reduction | M | ✅ | I |
| **ERS-ADC-012** | Divider current drain SHALL be <50µA at 4.2V | M | ✅ | A |

**Calculation:**
- VBAT max: 4.2V
- Divider ratio: 2:1 (R1=R2=100kΩ)
- ADC input: 4.2V / 2 = 2.1V (within 11dB range)
- Current: 4.2V / 200kΩ = 21µA ✅

---

## 7. Digital Interface Requirements

### 7.1 Button Interface

```
VCC ─── R1 (10kΩ) ───┬─── GPIO (0 or 35)
                      │
                      └─── SW ─── GND
```

| Req ID | Requirement | Priority | Status | Verification |
|--------|-------------|----------|--------|--------------|
| **ERS-DIG-001** | Button inputs SHALL be active LOW with pull-up | H | ✅ | I |
| **ERS-DIG-002** | Button debounce time SHALL be >10ms | M | ✅ | T |
| **ERS-DIG-003** | GPIO0 button SHALL have external 10kΩ pull-up | H | ✅ | I |
| **ERS-DIG-004** | GPIO35 MAY use internal pull-up (input-only pin) | M | ⚠️ | I |

### 7.2 LED Interface

```
GPIO4 ─── R1 (330Ω) ─── LED ─── GND
```

| Req ID | Requirement | Priority | Status | Verification |
|--------|-------------|----------|--------|--------------|
| **ERS-DIG-010** | LED current SHALL be limited to <20mA | H | ✅ | A |
| **ERS-DIG-011** | LED resistor SHALL be sized for 10-15mA operation | M | ✅ | A |
| **ERS-DIG-012** | Flash LED (ESP32-CAM) draws ~100mA when on | H | ✅ | T |

**LED Resistor Calculation (standard LED):**
- VCC: 3.3V
- LED Vf: 2.0V (typical)
- Target current: 10mA
- R = (3.3V - 2.0V) / 10mA = 130Ω → use 150Ω or 180Ω

---

## 8. Communication Interface Requirements

### 8.1 SPI Interface (LCD)

| Parameter       | Specification | Notes              |
| --------------- | ------------- | ------------------ |
| Clock frequency | 40 MHz        | Maximum for ST7789 |
| Mode            | SPI Mode 0    | CPOL=0, CPHA=0     |
| Bit order       | MSB first     | Standard           |
| Data width      | 8-bit         | Byte transfers     |
| CS polarity     | Active LOW    | Standard           |

| Req ID | Requirement | Priority | Status | Verification |
|--------|-------------|----------|--------|--------------|
| **ERS-SPI-001** | SPI clock SHALL operate at 40MHz | H | ✅ | T |
| **ERS-SPI-002** | SPI signals SHALL have <50Ω impedance traces | M | ⚠️ | A |
| **ERS-SPI-003** | SPI clock SHALL have 22Ω series termination | L | ⚠️ | I |
| **ERS-SPI-004** | LCD cable length SHALL be <10cm for 40MHz | M | ✅ | I |

**SPI Timing:**
```
Clock period: 25ns (40MHz)
Setup time: 10ns
Hold time: 10ns
Propagation delay: <5ns (short trace)
```

### 8.2 I2C Interface (Camera SCCB)

| Parameter | Specification | Notes |
|-----------|---------------|-------|
| Speed | 100kHz / 400kHz | Standard/Fast mode |
| Pull-up resistors | 2.2kΩ - 4.7kΩ | External required |
| Voltage levels | 3.3V | Open-drain |

| Req ID | Requirement | Priority | Status | Verification |
|--------|-------------|----------|--------|--------------|
| **ERS-I2C-001** | I2C pull-ups SHALL be 4.7kΩ for 100kHz operation | H | ✅ | I |
| **ERS-I2C-002** | I2C bus capacitance SHALL be <400pF | M | ✅ | A |
| **ERS-I2C-003** | Camera SCCB SHALL operate at 100kHz | H | ✅ | T |

### 8.3 WiFi RF Interface

| Parameter | Specification | Notes |
|-----------|---------------|-------|
| Frequency | 2.4GHz (2400-2483.5MHz) | ISM band |
| TX power | +20dBm maximum | Configurable 2-20dBm |
| RX sensitivity | -98dBm @ 1Mbps | Typical |
| Antenna | PCB trace / external | Module dependent |
| Impedance | 50Ω | RF path |

| Req ID | Requirement | Priority | Status | Verification |
|--------|-------------|----------|--------|--------------|
| **ERS-RF-001** | Antenna impedance SHALL be 50Ω ±10% | H | ✅ | A |
| **ERS-RF-002** | Antenna clearance SHALL be >10mm from ground plane | H | ✅ | I |
| **ERS-RF-003** | TX power SHALL be configurable via software | H | ✅ | T |
| **ERS-RF-004** | WiFi operation SHALL not affect ADC readings | M | ✅ | T |

---

## 9. Display Interface Requirements

### 9.1 ST7789 LCD Specifications

| Parameter | Value | Notes |
|-----------|-------|-------|
| Resolution | 135×240 pixels | Portrait orientation |
| Color depth | 16-bit RGB565 | 65K colors |
| Interface | 4-wire SPI | CS, DC, SCLK, MOSI |
| Operating voltage | 3.3V | VDD |
| Backlight | LED, PWM dimmable | Separate supply |

| Req ID | Requirement | Priority | Status | Verification |
|--------|-------------|----------|--------|--------------|
| **ERS-LCD-001** | LCD SPI interface SHALL support 40MHz clock | H | ✅ | T |
| **ERS-LCD-002** | LCD reset pulse SHALL be minimum 10µs | H | ✅ | T |
| **ERS-LCD-003** | LCD initialization sequence SHALL complete in <100ms | M | ✅ | T |
| **ERS-LCD-004** | Backlight PWM SHALL operate at 5kHz | M | ✅ | T |

### 9.2 Backlight Driver

```
GPIO4 ─── PWM ─── Gate Driver ─── LED String ─── GND
                      │
                      └─── Current limit resistor
```

| Req ID | Requirement | Priority | Status | Verification |
|--------|-------------|----------|--------|--------------|
| **ERS-LCD-010** | Backlight current SHALL be limited to 40mA max | H | ✅ | T |
| **ERS-LCD-011** | Backlight duty cycle SHALL be 0-100% adjustable | M | ✅ | T |
| **ERS-LCD-012** | Backlight off current SHALL be <1mA | L | ✅ | T |

---

## 10. Camera Interface Requirements

### 10.1 OV2640 Camera Specifications

| Parameter | Value | Notes |
|-----------|-------|-------|
| Sensor | 2MP CMOS | 1600×1200 max |
| Output formats | JPEG, RGB565, YUV | Configurable |
| Interface | DVP parallel (8-bit) | + control signals |
| SCCB | I2C compatible | 100kHz |
| Power supply | 2.8V (core), 1.8V (I/O) | Regulated on module |
| Clock input | 10-48MHz | 20MHz typical |

| Req ID | Requirement | Priority | Status | Verification |
|--------|-------------|----------|--------|--------------|
| **ERS-CAM-001** | Camera XCLK SHALL be 20MHz | H | ✅ | T |
| **ERS-CAM-002** | Camera data bus SHALL be 8-bit parallel | H | ✅ | I |
| **ERS-CAM-003** | Camera power-down (PWDN) SHALL be controllable | M | ✅ | T |
| **ERS-CAM-004** | Camera reset SHALL be active LOW | M | ⚠️ | I |

### 10.2 Camera Timing

| Signal | Frequency | Duty Cycle | Notes |
|--------|-----------|------------|-------|
| XCLK | 20MHz | 50% | Input clock |
| PCLK | Variable | 50% | Pixel clock out |
| VSYNC | ~30Hz | Variable | Frame sync |
| HREF | ~15kHz | Variable | Line sync |

| Req ID | Requirement | Priority | Status | Verification |
|--------|-------------|----------|--------|--------------|
| **ERS-CAM-010** | PCLK trace length SHALL be matched to data lines ±5mm | M | ⚠️ | I |
| **ERS-CAM-011** | Camera frame rate SHALL be >10fps at QVGA | H | ✅ | T |
| **ERS-CAM-012** | Camera data lines SHALL have <100pF capacitance | M | ✅ | A |

---

## 11. Motor Driver Interface Requirements (Planned)

### 11.1 Motor Driver Specifications

| Parameter | Target Value | Notes |
|-----------|--------------|-------|
| Driver type | H-bridge (TB6612, DRV8833) | Dual channel |
| Voltage rating | 2.5V - 13.5V | Motor supply |
| Current rating | 1.2A continuous | Per channel |
| PWM frequency | 20kHz | Ultrasonic |
| Logic voltage | 3.3V compatible | ESP32 compatible |

| Req ID | Requirement | Priority | Status | Verification |
|--------|-------------|----------|--------|--------------|
| **ERS-MOT-001** | Motor driver SHALL support 3.3V logic | H | 🔮 | T |
| **ERS-MOT-002** | Motor driver SHALL provide >1A per channel | H | 🔮 | T |
| **ERS-MOT-003** | PWM frequency SHALL be 15-25kHz | M | 🔮 | T |
| **ERS-MOT-004** | Motor driver SHALL have thermal shutdown | H | 🔮 | I |

### 11.2 Motor Interface Pinout (Planned)

| GPIO | Function | Notes |
|------|----------|-------|
| TBD | Motor A PWM | LEDC channel |
| TBD | Motor A DIR | Direction |
| TBD | Motor B PWM | LEDC channel |
| TBD | Motor B DIR | Direction |
| TBD | Driver STBY | Standby control |

| Req ID | Requirement | Priority | Status | Verification |
|--------|-------------|----------|--------|--------------|
| **ERS-MOT-010** | Motor PWM GPIOs SHALL support LEDC peripheral | H | 🔮 | I |
| **ERS-MOT-011** | Motor GPIOs SHALL avoid bootstrap pins (0, 2, 12, 15) | C | 🔮 | I |
| **ERS-MOT-012** | Motor driver enable SHALL default to OFF at boot | C | 🔮 | T |

---

## 12. EMC and Safety Requirements

### 12.1 ESD Protection

| Parameter | Specification | Notes |
|-----------|---------------|-------|
| Human Body Model (HBM) | ±2kV | All external connectors |
| Machine Model (MM) | ±200V | All external connectors |
| Charged Device Model (CDM) | ±500V | IC package level |

| Req ID | Requirement | Priority | Status | Verification |
|--------|-------------|----------|--------|--------------|
| **ERS-EMC-001** | External connectors SHALL have ±2kV ESD protection | H | ⚠️ | T |
| **ERS-EMC-002** | USB connector SHALL have ESD protection diodes | H | ✅ | I |
| **ERS-EMC-003** | Antenna input SHALL have ESD protection | M | ⚠️ | I |

### 12.2 Electromagnetic Compatibility

| Req ID | Requirement | Priority | Status | Verification |
|--------|-------------|----------|--------|--------------|
| **ERS-EMC-010** | WiFi emissions SHALL comply with FCC Part 15 | H | ✅ | T |
| **ERS-EMC-011** | PWM signals SHALL have EMI filtering at connector | M | ⚠️ | A |
| **ERS-EMC-012** | Motor driver SHALL have decoupling capacitors | H | 🔮 | I |

### 12.3 Electrical Safety

| Req ID | Requirement | Priority | Status | Verification |
|--------|-------------|----------|--------|--------------|
| **ERS-SAF-001** | Battery circuit SHALL have PTC fuse (500mA) | H | ⚠️ | I |
| **ERS-SAF-002** | USB input SHALL have polyfuse protection | H | ✅ | I |
| **ERS-SAF-003** | No user-accessible voltages >25V | C | ✅ | I |
| **ERS-SAF-004** | Reverse polarity SHALL not damage electronics | H | ⚠️ | T |

---

## 13. Thermal Requirements

### 13.1 Operating Temperature

| Component | Min | Typical | Max | Notes |
|-----------|-----|---------|-----|-------|
| ESP32 | -40°C | +25°C | +85°C | Industrial grade |
| Li-ion battery | 0°C | +25°C | +45°C | Charging |
| Li-ion battery | -20°C | +25°C | +60°C | Discharging |
| LCD module | -20°C | +25°C | +70°C | Operating |
| Camera module | -30°C | +25°C | +70°C | Operating |

| Req ID | Requirement | Priority | Status | Verification |
|--------|-------------|----------|--------|--------------|
| **ERS-THM-001** | System SHALL operate from 0°C to +45°C ambient | H | ✅ | T |
| **ERS-THM-002** | System SHALL not charge battery below 0°C | C | ⚠️ | T |
| **ERS-THM-003** | System SHALL not charge battery above 45°C | C | ⚠️ | T |

### 13.2 Thermal Dissipation

| Component | Power Dissipation | Thermal Resistance |
|-----------|-------------------|-------------------|
| ESP32 module | 0.5W typical | ~50°C/W (natural convection) |
| Voltage regulator | 0.2W typical | ~100°C/W |
| Motor driver | 0.5W typical | ~50°C/W (with heatsink) |

| Req ID | Requirement | Priority | Status | Verification |
|--------|-------------|----------|--------|--------------|
| **ERS-THM-010** | ESP32 junction temperature SHALL not exceed +105°C | C | ✅ | A |
| **ERS-THM-011** | LDO case temperature SHALL not exceed +85°C | H | ✅ | T |
| **ERS-THM-012** | Motor driver SHALL have thermal relief in PCB | M | 🔮 | I |

---

## 14. Traceability Matrix

### 14.1 Requirements to Components

| Req ID | Component | Part Number | Notes |
|--------|-----------|-------------|-------|
| ERS-MCU-001 | MCU Module | AI-Thinker ESP32-CAM | ESP32-D0WDQ6 |
| ERS-MCU-001 | MCU Module | LilyGO TTGO T-Display | ESP32-D0WDQ6 |
| ERS-PWR-030 | Battery Charger | TP4056 / IP5306 | Module integrated |
| ERS-LCD-001 | LCD | ST7789 1.14" | 135×240 RGB |
| ERS-CAM-001 | Camera | OV2640 | 2MP CMOS |

### 14.2 Requirements to Software Requirements

| ERS Req | SRS Req | Relationship |
|---------|---------|--------------|
| ERS-ADC-001 | REQ-SW-005 | Implements |
| ERS-GPIO-001 | REQ-SW-006 | Enables |
| ERS-SPI-001 | REQ-SW-008 | Implements |
| ERS-RF-003 | REQ-SW-009 | Enables |

---

## 15. Verification Methods

### 15.1 Test Equipment

| Equipment | Purpose | Specification |
|-----------|---------|---------------|
| Digital Multimeter | Voltage, current measurement | 4.5 digit, 0.05% accuracy |
| Oscilloscope | Signal integrity | 100MHz, 1GSa/s |
| Logic Analyzer | Protocol analysis | 24 channels, 100MHz |
| Current Probe | Power measurement | 100mA - 10A range |
| RF Analyzer | WiFi testing | 2.4GHz capable |
| Thermal Camera | Temperature monitoring | ±2°C accuracy |

### 15.2 Test Procedures

| Test | Equipment | Pass Criteria |
|------|-----------|---------------|
| Power-on current | Current probe | <500mA inrush |
| Deep sleep current | µA meter | <15µA |
| ADC accuracy | Precision source | ±50mV |
| SPI signal integrity | Oscilloscope | Clean edges, no ringing |
| WiFi TX power | RF analyzer | ≤20dBm |
| Operating temp | Thermal chamber | Functions 0-45°C |

---

## 16. References

1. ESP32 Technical Reference Manual v5.0
2. ESP32 Hardware Design Guidelines v3.0
3. OV2640 Datasheet
4. ST7789 Datasheet
5. TB6612FNG Datasheet (motor driver reference)
6. IEC 61000-4 series (EMC)
7. IEC 62368-1 (Safety)

---

## 17. Appendices

### Appendix A: Schematic Symbols

```
Power:
  VCC ──┬──     GND ──┴──     VBAT ──●──

Resistor:     Capacitor:     LED:
  ─┤├─          ─┤├─          ─▷├─

Switch:       MOSFET:        Diode:
  ─○ ○─        ─┤├┤           ─▷├─
```

### Appendix B: PCB Design Guidelines

**Layer Stack (4-layer recommended):**
1. Top: Components, signals
2. Inner 1: GND plane
3. Inner 2: Power plane
4. Bottom: Components, signals

**Design Rules:**
| Parameter | Value |
|-----------|-------|
| Minimum trace width | 0.15mm (6mil) |
| Minimum clearance | 0.15mm (6mil) |
| Via drill | 0.3mm |
| Via pad | 0.6mm |
| RF trace width | 1.0mm (50Ω) |

### Appendix C: Bill of Materials (Reference)

| Item | Description | Quantity | Notes |
|------|-------------|----------|-------|
| U1 | ESP32-CAM or TTGO T-Display | 1 | Main module |
| C1 | 100µF 10V electrolytic | 1 | Power input |
| C2 | 100nF 50V ceramic | 4 | Decoupling |
| R1-R2 | 100kΩ 1% | 2 | Battery divider |
| R3 | 10kΩ 5% | 1 | Button pull-up |
| SW1-SW2 | Tactile switch | 2 | TTGO only |
| J1 | USB-C connector | 1 | Power/programming |
| J2 | JST PH 2.0 2P | 1 | Battery |

### Appendix D: Power Budget

**TTGO T-Display:**
| Component | Current (mA) | Duty Cycle | Average (mA) |
|-----------|--------------|------------|--------------|
| ESP32 core | 50 | 100% | 50 |
| WiFi RX | 95 | 50% | 47.5 |
| WiFi TX | 180 | 10% | 18 |
| LCD controller | 5 | 100% | 5 |
| LCD backlight | 25 | 80% | 20 |
| Voltage divider | 0.02 | 100% | 0.02 |
| **Total** | | | **140.5mA** |

**ESP32-CAM (streaming):**
| Component | Current (mA) | Duty Cycle | Average (mA) |
|-----------|--------------|------------|--------------|
| ESP32 core | 80 | 100% | 80 |
| WiFi RX | 95 | 30% | 28.5 |
| WiFi TX | 180 | 30% | 54 |
| Camera active | 60 | 80% | 48 |
| PSRAM | 10 | 100% | 10 |
| **Total** | | | **220.5mA** |

---

*End of Document*
