# Electronics Implementation Guide
## ESP32 Rover Platform

| Document Info | Details |
|---------------|---------|
| **Project** | ESP32 WiFi-Controlled FPV Rover |
| **Document ID** | ESP32-ROVER-EIG-001 |
| **Version** | 1.0.0 |
| **Date** | 2026-01-25 |
| **Status** | Draft |
| **Classification** | Implementation Guide |

---

## Table of Contents

1. [Overview](#1-overview)
2. [Target Hardware Platforms](#2-target-hardware-platforms)
3. [Pinout Reference](#3-pinout-reference)
4. [Circuit Schematics](#4-circuit-schematics)
5. [Power Distribution](#5-power-distribution)
6. [Sensor Interfaces](#6-sensor-interfaces)
7. [Display Interface](#7-display-interface)
8. [Camera Interface](#8-camera-interface)
9. [Motor Driver Interface](#9-motor-driver-interface)
10. [PCB Design](#10-pcb-design)
11. [Assembly Instructions](#11-assembly-instructions)
12. [Testing Procedures](#12-testing-procedures)
13. [Troubleshooting](#13-troubleshooting)
14. [Bill of Materials](#14-bill-of-materials)

---

## 1. Overview

This document provides complete implementation details for the ESP32 Rover electronics, including pinouts, circuit diagrams, PCB design guidelines, and assembly instructions.

### 1.1 Supported Configurations

| Configuration | Target Module | Primary Features |
|---------------|---------------|------------------|
| FPV Rover | ESP32-CAM | Camera streaming, WiFi control |
| Display Rover | TTGO T-Display | LCD status, buttons, battery monitoring |

### 1.2 Block Diagram

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        ESP32 ROVER ELECTRONICS                               │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   ┌─────────────┐                                    ┌─────────────┐        │
│   │   POWER     │                                    │   MOTORS    │        │
│   ├─────────────┤                                    ├─────────────┤        │
│   │ Li-ion Batt │───┐                           ┌───│ H-Bridge    │        │
│   │ USB-C Input │   │                           │   │ Motor A     │        │
│   │ Charger IC  │   │                           │   │ Motor B     │        │
│   │ 3.3V LDO    │   │                           │   └─────────────┘        │
│   └─────────────┘   │                           │                          │
│                     ▼                           │                          │
│              ┌─────────────┐                    │   ┌─────────────┐        │
│              │             │                    │   │   DISPLAY   │        │
│              │   ESP32     │────────────────────┤   ├─────────────┤        │
│              │   MODULE    │                    ├───│ ST7789 LCD  │        │
│              │             │                    │   │ (TTGO only) │        │
│              │ • WiFi      │                    │   └─────────────┘        │
│              │ • Dual Core │                    │                          │
│              │ • ADC/PWM   │                    │   ┌─────────────┐        │
│              │             │                    │   │   CAMERA    │        │
│              └─────────────┘                    ├───├─────────────┤        │
│                     │                           │   │ OV2640      │        │
│                     │                           │   │ (CAM only)  │        │
│   ┌─────────────┐   │                           │   └─────────────┘        │
│   │   SENSORS   │   │                           │                          │
│   ├─────────────┤   │                           │   ┌─────────────┐        │
│   │ Battery ADC │◄──┤                           └───│   BUTTONS   │        │
│   │ Buttons     │◄──┘                               │ (TTGO only) │        │
│   └─────────────┘                                   └─────────────┘        │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Target Hardware Platforms

### 2.1 ESP32-CAM (AI-Thinker)

**Module Specifications:**
| Parameter | Value |
|-----------|-------|
| MCU | ESP32-S (ESP32-D0WDQ6) |
| Flash | 4MB |
| PSRAM | 4MB (SPIRAM) |
| Camera | OV2640 2MP |
| Dimensions | 40.5 × 27 × 4.5 mm |
| Operating Voltage | 5V (via regulator) or 3.3V direct |

**Module Pinout:**

```
                    ESP32-CAM (AI-Thinker)
                    ┌─────────────────────┐
                    │  ┌───────────────┐  │
                    │  │    CAMERA     │  │
                    │  │    MODULE     │  │
                    │  └───────────────┘  │
              5V ───┤ 1               16 ├─── GND
             GND ───┤ 2               15 ├─── GPIO13 (HS2_DATA3)
          GPIO12 ───┤ 3               14 ├─── GPIO15 (HS2_CMD)
          GPIO13 ───┤ 4               13 ├─── GPIO14 (HS2_CLK)
          GPIO15 ───┤ 5               12 ├─── GPIO2
          GPIO14 ───┤ 6               11 ├─── GPIO4 (Flash LED)
           GPIO2 ───┤ 7               10 ├─── GPIO0 (Boot)
           GPIO4 ───┤ 8                9 ├─── VCC (3.3V)
                    │                     │
                    │  ┌───┐   ┌───────┐  │
                    │  │ANT│   │MICRO  │  │
                    │  │   │   │SD SLOT│  │
                    │  └───┘   └───────┘  │
                    └─────────────────────┘

Side Header (optional):
              U0R ───┤ ├─── U0T
              GND ───┤ ├─── 3V3
              IO16───┤ ├─── VCC
```

**Camera Connector Pinout (Internal):**

| Pin | Signal | GPIO | Description |
|-----|--------|------|-------------|
| 1 | D0 | GPIO5 | Data bit 0 |
| 2 | D1 | GPIO18 | Data bit 1 |
| 3 | D2 | GPIO19 | Data bit 2 |
| 4 | D3 | GPIO21 | Data bit 3 |
| 5 | D4 | GPIO36 | Data bit 4 |
| 6 | D5 | GPIO39 | Data bit 5 |
| 7 | D6 | GPIO34 | Data bit 6 |
| 8 | D7 | GPIO35 | Data bit 7 |
| 9 | XCLK | GPIO0 | Clock input (20MHz) |
| 10 | PCLK | GPIO22 | Pixel clock output |
| 11 | VSYNC | GPIO25 | Vertical sync |
| 12 | HREF | GPIO23 | Horizontal reference |
| 13 | SDA | GPIO26 | SCCB data |
| 14 | SCL | GPIO27 | SCCB clock |
| 15 | PWDN | GPIO32 | Power down |
| 16 | RESET | -1 | Not connected |

### 2.2 TTGO T-Display (LilyGO)

**Module Specifications:**
| Parameter | Value |
|-----------|-------|
| MCU | ESP32-D0WDQ6 |
| Flash | 4MB |
| PSRAM | None |
| Display | ST7789 1.14" 135×240 |
| Buttons | 2 (GPIO0, GPIO35) |
| Battery | JST 1.25mm connector |
| USB | Type-C |
| Dimensions | 51.4 × 25.2 × 8.7 mm |

**Module Pinout:**

```
                    TTGO T-Display v1.1
        ┌─────────────────────────────────────────┐
        │  ┌───────────────────────────────────┐  │
        │  │                                   │  │
        │  │          ST7789 LCD               │  │
        │  │          135 x 240                │  │
        │  │                                   │  │
        │  └───────────────────────────────────┘  │
        │                                         │
   3V3 ─┤ ●                                   ● ├─ GND
   GND ─┤ ●                                   ● ├─ GPIO21
 GPIO36─┤ ●  [BTN1]                   [BTN2]  ● ├─ GPIO22
 GPIO37─┤ ●   (L)                       (R)   ● ├─ GPIO17
 GPIO38─┤ ●                                   ● ├─ GPIO2
 GPIO39─┤ ●                                   ● ├─ GPIO15
 GPIO32─┤ ●                                   ● ├─ GPIO13
 GPIO33─┤ ●                                   ● ├─ GPIO12
 GPIO25─┤ ●                                   ● ├─ GPIO27
 GPIO26─┤ ●                                   ● ├─ GPIO14
    EN ─┤ ●                                   ● ├─ VIN (5V)
        │                                         │
        │    [USB-C]     [JST-BATT]              │
        │    ════════    ═══                     │
        └─────────────────────────────────────────┘

Left Button  = GPIO0  (active LOW)
Right Button = GPIO35 (active LOW)
```

**Internal LCD Connections (fixed on PCB):**

| Function | GPIO | Notes |
|----------|------|-------|
| SCLK | GPIO18 | SPI clock |
| MOSI | GPIO19 | SPI data |
| DC | GPIO16 | Data/Command |
| CS | GPIO5 | Chip select |
| RST | GPIO23 | Reset |
| Backlight | GPIO4 | PWM control |

**Battery ADC (fixed on PCB):**

| Function | GPIO | Notes |
|----------|------|-------|
| VBAT sense | GPIO34 | Through 100K/100K divider |

---

## 3. Pinout Reference

### 3.1 TTGO T-Display GPIO Map

```
┌────────────────────────────────────────────────────────────────────────┐
│                      TTGO T-Display GPIO Allocation                     │
├──────┬────────────────┬───────────┬─────────────────────────────────────┤
│ GPIO │ Function       │ Direction │ Notes                               │
├──────┼────────────────┼───────────┼─────────────────────────────────────┤
│  0   │ Left Button    │ Input     │ Active LOW, bootstrap (pull-up req) │
│  2   │ Available      │ -         │ Bootstrap pin, use with care        │
│  4   │ LCD Backlight  │ Output    │ PWM 5kHz, 0-255 duty                 │
│  5   │ LCD CS         │ Output    │ SPI chip select                     │
│ 12   │ Available      │ -         │ Bootstrap (MTDI), avoid if possible │
│ 13   │ Available      │ I/O       │ Safe to use                         │
│ 14   │ Available      │ I/O       │ Safe to use                         │
│ 15   │ Available      │ -         │ Bootstrap (MTDO), pull-up recommend │
│ 16   │ LCD DC         │ Output    │ Data/Command select                 │
│ 17   │ Available      │ I/O       │ Safe to use                         │
│ 18   │ LCD SCLK       │ Output    │ SPI clock 40MHz                     │
│ 19   │ LCD MOSI       │ Output    │ SPI data                            │
│ 21   │ Available      │ I/O       │ Safe to use (I2C SDA default)       │
│ 22   │ Available      │ I/O       │ Safe to use (I2C SCL default)       │
│ 23   │ LCD RST        │ Output    │ Active LOW reset                    │
│ 25   │ Available      │ I/O       │ DAC1                                │
│ 26   │ Available      │ I/O       │ DAC2                                │
│ 27   │ Available      │ I/O       │ Safe to use                         │
│ 32   │ Available      │ I/O       │ ADC1_CH4                            │
│ 33   │ Available      │ I/O       │ ADC1_CH5                            │
│ 34   │ Battery ADC    │ Input     │ ADC1_CH6, input-only                │
│ 35   │ Right Button   │ Input     │ Active LOW, input-only              │
│ 36   │ Available      │ Input     │ ADC1_CH0, input-only (VP)           │
│ 37   │ Available      │ Input     │ ADC1_CH1, input-only                │
│ 38   │ Available      │ Input     │ ADC1_CH2, input-only                │
│ 39   │ Available      │ Input     │ ADC1_CH3, input-only (VN)           │
├──────┴────────────────┴───────────┴─────────────────────────────────────┤
│ Available GPIOs for expansion: 2, 12, 13, 14, 15, 17, 21, 22, 25-27,   │
│                                32, 33, 36-39                            │
│ Recommended for motors: GPIO13, GPIO14, GPIO27, GPIO32 (avoid boot pins)│
└────────────────────────────────────────────────────────────────────────┘
```

### 3.2 ESP32-CAM GPIO Map

```
┌────────────────────────────────────────────────────────────────────────┐
│                      ESP32-CAM GPIO Allocation                          │
├──────┬────────────────┬───────────┬─────────────────────────────────────┤
│ GPIO │ Function       │ Direction │ Notes                               │
├──────┼────────────────┼───────────┼─────────────────────────────────────┤
│  0   │ Camera XCLK    │ Output    │ 20MHz clock, bootstrap pin          │
│  2   │ Available      │ -         │ Bootstrap, directly to LED driver   │
│  4   │ Flash LED      │ Output    │ High-power white LED                │
│  5   │ Camera D0      │ Input     │ Data bit 0                          │
│ 12   │ Available*     │ -         │ JTAG TDI, bootstrap (3.3V select)   │
│ 13   │ Available*     │ -         │ JTAG TCK                            │
│ 14   │ Available*     │ -         │ JTAG TMS                            │
│ 15   │ Available*     │ -         │ JTAG TDO, bootstrap                 │
│ 16   │ PSRAM          │ -         │ Used by PSRAM (not available)       │
│ 17   │ PSRAM          │ -         │ Used by PSRAM (not available)       │
│ 18   │ Camera D1      │ Input     │ Data bit 1                          │
│ 19   │ Camera D2      │ Input     │ Data bit 2                          │
│ 21   │ Camera D3      │ Input     │ Data bit 3                          │
│ 22   │ Camera PCLK    │ Input     │ Pixel clock                         │
│ 23   │ Camera HREF    │ Input     │ Horizontal sync                     │
│ 25   │ Camera VSYNC   │ Input     │ Vertical sync                       │
│ 26   │ Camera SIOD    │ I/O       │ SCCB data (I2C)                     │
│ 27   │ Camera SIOC    │ Output    │ SCCB clock (I2C)                    │
│ 32   │ Camera PWDN    │ Output    │ Power down control                  │
│ 33   │ Available      │ I/O       │ On-board LED (active LOW)           │
│ 34   │ Camera D6      │ Input     │ Data bit 6, input-only              │
│ 35   │ Camera D7      │ Input     │ Data bit 7, input-only              │
│ 36   │ Camera D4      │ Input     │ Data bit 4, input-only              │
│ 39   │ Camera D5      │ Input     │ Data bit 5, input-only              │
├──────┴────────────────┴───────────┴─────────────────────────────────────┤
│ * GPIO 12-15 available with JTAG_DEBUG=1 build flag                     │
│ Available GPIOs: 2, 12*, 13*, 14*, 15*, 33                              │
│ Very limited GPIO availability due to camera interface                  │
└────────────────────────────────────────────────────────────────────────┘
```

### 3.3 Motor Driver Pin Recommendations

**For TTGO T-Display (recommended configuration):**

```
┌────────────────────────────────────────────────────────────────────────┐
│                    Recommended Motor Driver Pinout                      │
├──────────────┬────────┬────────────────────────────────────────────────┤
│ Function     │ GPIO   │ Notes                                          │
├──────────────┼────────┼────────────────────────────────────────────────┤
│ Motor A PWM  │ GPIO13 │ LEDC channel 0, safe pin                       │
│ Motor A DIR  │ GPIO14 │ Direction control                              │
│ Motor B PWM  │ GPIO27 │ LEDC channel 1, safe pin                       │
│ Motor B DIR  │ GPIO26 │ Direction control                              │
│ Driver STBY  │ GPIO25 │ Standby/enable (active HIGH for TB6612)        │
├──────────────┴────────┴────────────────────────────────────────────────┤
│ Alternative: Use GPIO32/GPIO33 for PWM (have ADC fallback)             │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 4. Circuit Schematics

### 4.1 Power Supply Circuit

```
                              POWER SUPPLY SCHEMATIC

    USB-C                                           To ESP32 Module
    ┌───┐                                           ┌───────────────┐
    │   │    ┌─────────┐      ┌─────────┐          │               │
    │ V+├────┤ ESD     ├──┬───┤ Charger ├───┬──────┤ 5V / VIN      │
    │   │    │ TVS     │  │   │ TP4056  │   │      │               │
    │ D+├────┤         │  │   │ 500mA   │   │      │               │
    │ D-├────┤         │  │   └────┬────┘   │      │               │
    │GND├────┤         ├──┴────────┼────────┤      │               │
    └───┘    └─────────┘           │        │      │               │
                                   │        │      │               │
                              ┌────┴────┐   │      │               │
                              │ Li-ion  │   │      │               │
    Battery                   │  Cell   │   │      │               │
    JST PH 2.0               │ 3.7V    │   │      │               │
    ┌───┐                    │500-2000 │   │      │               │
    │ + ├────────────────────┤  mAh    │   │      │               │
    │   │                    │         │   │      │               │
    │ - ├──┬─────────────────┴─────────┘   │      │               │
    └───┘  │                               │      │               │
           └───────────────────────────────┴──────┤ GND           │
                                                   └───────────────┘

    Note: TTGO T-Display has integrated TP4056 charger and LDO
    Note: ESP32-CAM typically requires external 5V or regulated 3.3V
```

### 4.2 Battery Voltage Sensing (TTGO)

```
                         BATTERY ADC CIRCUIT

    VBAT (3.0-4.2V)
         │
         │
         ├────────────────────────────┐
         │                            │
        ┌┴┐                           │
        │ │ R1                        │
        │ │ 100kΩ ±1%                 │
        └┬┘                           │
         │                            │
         ├─────────┬─────────────────►│ GPIO34 (ADC1_CH6)
         │         │                  │
        ┌┴┐       ─┴─                 │
        │ │ R2    ─┬─ C1              │
        │ │ 100kΩ  │  100nF           │
        │ │ ±1%    │  50V             │
        └┬┘        │                  │
         │         │                  │
         └─────────┴──────────────────┤ GND

    Calculations:
    ─────────────
    Divider ratio: R2/(R1+R2) = 100k/(100k+100k) = 0.5 (2:1)

    At VBAT = 4.2V: VADC = 4.2V × 0.5 = 2.1V ✓ (within ADC range)
    At VBAT = 3.0V: VADC = 3.0V × 0.5 = 1.5V ✓

    Divider current: I = 4.2V / 200kΩ = 21µA (acceptable battery drain)

    ADC Configuration:
    - Attenuation: 11dB (0-3.3V full scale)
    - Resolution: 12-bit (0-4095)
    - Multisampling: 64 samples averaged
    - Filter: IIR low-pass, α=0.02
```

### 4.3 Button Interface (TTGO)

```
                         BUTTON INTERFACE CIRCUIT

    Left Button (GPIO0)                    Right Button (GPIO35)
    ─────────────────────                  ─────────────────────

         VCC (3.3V)                              VCC (3.3V)
            │                                       │
           ┌┴┐                                     ┌┴┐
           │ │ R1                                  │ │ R2
           │ │ 10kΩ                                │ │ 10kΩ
           │ │ (external)                          │ │ (internal OK)
           └┬┘                                     └┬┘
            │                                       │
            ├──────────► GPIO0                      ├──────────► GPIO35
            │            (boot pin)                 │            (input only)
            │                                       │
           ─┴─                                     ─┴─
          │   │ SW1                               │   │ SW2
          │ ○ │ Tactile                           │ ○ │ Tactile
           ─┬─                                     ─┬─
            │                                       │
            └──────────► GND                        └──────────► GND

    Notes:
    ───────
    • GPIO0 requires EXTERNAL 10kΩ pull-up (bootstrap pin)
    • GPIO35 can use internal pull-up (input-only, no bootstrap function)
    • Both buttons are active LOW (pressed = logic 0)
    • Hardware debounce optional (10nF capacitor to GND)
    • Software debounce: 50ms minimum press detection
```

### 4.4 LED Interface (ESP32-CAM)

```
                         FLASH LED CIRCUIT (Built-in)

         GPIO4
            │
            │
           ┌┴┐
           │ │ R1
           │ │ 4.7Ω (current limit)
           └┬┘
            │
            ├──────────► LED Anode
            │            High-power white LED
           ─┴─           (~100mA when on)
          ─────
           ─┬─
            │
            └──────────► GND


                         EXTERNAL LED CIRCUIT (Optional)

         GPIO (any available)
            │
            │
           ┌┴┐
           │ │ R1
           │ │ 150-330Ω
           └┬┘
            │            LED
            ├──────────►├──►├────────► GND
            │
            │

    Resistor calculation for standard LED:
    ────────────────────────────────────
    VCC = 3.3V
    Vf (LED forward voltage) ≈ 2.0V (red), 3.0V (blue/white)
    If (target current) = 10mA

    R = (VCC - Vf) / If = (3.3V - 2.0V) / 10mA = 130Ω → use 150Ω
```

### 4.5 Motor Driver Circuit (TB6612FNG)

```
                         MOTOR DRIVER SCHEMATIC

                              TB6612FNG
                         ┌───────────────────┐
    Motor Power ─────────┤ VM        GND     ├──┬── GND
    (6-12V)              │                   │  │
                         │                   │  │
    VCC (3.3V) ──────────┤ VCC       PWMA    ├◄─┼── GPIO13 (PWM)
                         │                   │  │
    GPIO25 ──────────────┤ STBY      AIN1    ├◄─┼── GPIO14 (DIR)
    (Standby)            │                   │  │
                         │           AIN2    ├◄─┼── (tied to !AIN1)
                         │                   │  │
                         │           AO1     ├──┼──┐
                         │                   │  │  │  Motor A
                         │           AO2     ├──┼──┘
                         │                   │  │
                         │           PWMB    ├◄─┼── GPIO27 (PWM)
                         │                   │  │
                         │           BIN1    ├◄─┼── GPIO26 (DIR)
                         │                   │  │
                         │           BIN2    ├◄─┼── (tied to !BIN1)
                         │                   │  │
                         │           BO1     ├──┼──┐
                         │                   │  │  │  Motor B
                         │           BO2     ├──┼──┘
                         │                   │  │
                         └───────────────────┘  │
                                                │
                         Decoupling:            │
                         100µF electrolytic ────┴── VM to GND
                         100nF ceramic ──────────── VCC to GND

    Direction Control Logic:
    ────────────────────────
    IN1=H, IN2=L: Forward
    IN1=L, IN2=H: Reverse
    IN1=L, IN2=L: Coast (motor freewheels)
    IN1=H, IN2=H: Brake (motor shorts)

    Simplified single-pin direction:
    IN1 = DIR, IN2 = !DIR (via inverter or GPIO)
```

### 4.6 I2C Bus (Generic Expansion)

```
                         I2C BUS EXPANSION

         VCC (3.3V)
            │
            ├────────────────────────────────────────────────┐
            │                                                │
           ┌┴┐ R1                                           ┌┴┐ R2
           │ │ 4.7kΩ                                        │ │ 4.7kΩ
           └┬┘                                              └┬┘
            │                                                │
            │           ┌─────────────────────────┐          │
    GPIO21 ─┼───────────┤ SDA                     │          │
    (SDA)   │           │                         │          │
            │           │      I2C Device         ├──────────┼─── VCC
            │           │      (IMU, etc.)        │          │
    GPIO22 ─┼───────────┤ SCL                     │          │
    (SCL)   │           │                         ├──────────┼─── GND
            │           └─────────────────────────┘          │
            │                                                │
            │           Additional devices connect           │
            │           in parallel to same bus              │
            │                                                │

    I2C Configuration:
    ──────────────────
    • Speed: 100kHz (standard) or 400kHz (fast mode)
    • Pull-up resistors: 4.7kΩ for 100kHz, 2.2kΩ for 400kHz
    • Maximum bus capacitance: 400pF
    • Multiple devices share same bus with unique addresses
```

---

## 5. Power Distribution

### 5.1 Power Tree

```
                              POWER DISTRIBUTION TREE

    USB-C (5V)                        Li-ion Battery (3.7V nominal)
        │                                     │
        ▼                                     │
    ┌───────────┐                             │
    │ Protection│                             │
    │ (TVS, PTC)│                             │
    └─────┬─────┘                             │
          │                                   │
          ▼                                   │
    ┌───────────┐      Charge      ┌──────────┴──────────┐
    │  Charger  │◄─────────────────┤                     │
    │  TP4056   │                  │    Li-ion Cell      │
    │  500mA    │─────────────────►│    3.0V - 4.2V      │
    └───────────┘      Power       │    500-2000mAh      │
                                   └──────────┬──────────┘
                                              │
                                              │ VBAT
                                              │
              ┌───────────────────────────────┼───────────────────┐
              │                               │                   │
              ▼                               ▼                   ▼
        ┌───────────┐                   ┌───────────┐       ┌───────────┐
        │   LDO     │                   │  Voltage  │       │  Motor    │
        │  AMS1117  │                   │  Divider  │       │  Driver   │
        │  3.3V/1A  │                   │  (ADC)    │       │  VM input │
        └─────┬─────┘                   └───────────┘       └───────────┘
              │
              │ 3.3V
              │
    ┌─────────┼─────────┬─────────┬─────────┬─────────┐
    │         │         │         │         │         │
    ▼         ▼         ▼         ▼         ▼         ▼
 ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐
 │ESP32 │ │ LCD  │ │Camera│ │ I2C  │ │Buttons││ LEDs │
 │ Core │ │      │ │      │ │Sensors│       │ │      │
 └──────┘ └──────┘ └──────┘ └──────┘ └──────┘ └──────┘
```

### 5.2 Current Budget

**TTGO T-Display Configuration:**

| Component | Typical (mA) | Maximum (mA) | Notes |
|-----------|--------------|--------------|-------|
| ESP32 core (active) | 50 | 80 | Dual-core @ 240MHz |
| WiFi RX | 95 | 100 | Listening |
| WiFi TX | 180 | 300 | Burst transmit |
| LCD controller | 5 | 10 | ST7789 |
| LCD backlight | 20 | 40 | PWM controlled |
| Voltage divider | 0.02 | 0.02 | Battery sense |
| **Total (typical)** | **170** | **530** | |

**ESP32-CAM Configuration:**

| Component | Typical (mA) | Maximum (mA) | Notes |
|-----------|--------------|--------------|-------|
| ESP32 core (active) | 80 | 100 | With PSRAM |
| WiFi streaming | 150 | 200 | Continuous TX |
| Camera capture | 60 | 100 | OV2640 |
| PSRAM | 10 | 15 | 4MB SPIRAM |
| Flash LED | 0 | 150 | When on |
| **Total (typical)** | **300** | **565** | |

### 5.3 Battery Life Estimation

```
Battery Capacity: 1000mAh (example)

TTGO T-Display (no motor):
─────────────────────────
Average current: 150mA (WiFi + LCD active)
Runtime: 1000mAh / 150mA = 6.7 hours

ESP32-CAM (streaming):
─────────────────────
Average current: 280mA (video streaming)
Runtime: 1000mAh / 280mA = 3.6 hours

Deep Sleep:
───────────
Sleep current: 10µA
Runtime: 1000mAh / 0.01mA = 100,000 hours ≈ 11 years
```

---

## 6. Sensor Interfaces

### 6.1 ADC Calibration

**ESP32 ADC Non-Linearity Correction:**

The ESP32 ADC has known non-linearity. For accurate battery readings:

```c
// Polynomial correction coefficients (example)
// Vactual = a0 + a1*Vraw + a2*Vraw² + a3*Vraw³

float adc_to_voltage(uint32_t raw_value) {
    // Apply attenuation factor first
    float v_adc = (raw_value / 4095.0f) * 3.3f;

    // Apply voltage divider compensation
    float v_battery = v_adc * BATTERY_DIVIDER_RATIO;

    return v_battery;
}
```

**ADC Noise Reduction Techniques:**

1. **Multisampling**: Average 64 samples
2. **Low-pass filter**: IIR filter with α=0.02
3. **Decoupling**: 100nF capacitor on ADC input
4. **Timing**: Sample when WiFi TX is idle

### 6.2 Button Debouncing

**Hardware Debounce (Optional):**

```
         GPIO
          │
          ├──────┬────────► MCU Input
          │      │
         ─┴─    ┌┴┐
         ─┬─    │ │ R (10kΩ)
          │     │ │
          │     └┬┘
          │      │
         ─┴─     │
        │   │────┘
        └───┘
         SW
```

Add 10nF capacitor for hardware RC debounce (τ = 10kΩ × 10nF = 100µs)

**Software Debounce (Implemented):**

```c
#define DEBOUNCE_MS 50

void IRAM_ATTR button_isr_handler(void *arg) {
    static uint32_t last_press = 0;
    uint32_t now = xTaskGetTickCountFromISR();

    if ((now - last_press) > pdMS_TO_TICKS(DEBOUNCE_MS)) {
        // Valid press detected
        last_press = now;
        // Set flag or send notification
    }
}
```

---

## 7. Display Interface

### 7.1 ST7789 Timing Diagram

```
                    SPI WRITE CYCLE (Mode 0)

    CS    ────┐                                          ┌────
              └──────────────────────────────────────────┘

    DC    ────────┐     ┌─────────────────────────────────────
              CMD │     │ DATA
                  └─────┘

    SCLK  ────┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─────────
              └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘
               1   2   3   4   5   6   7   8

    MOSI  ────X───X───X───X───X───X───X───X───────────────
              D7  D6  D5  D4  D3  D2  D1  D0

    Clock frequency: 40 MHz (25ns period)
    Setup time: 10ns before rising edge
    Hold time: 10ns after rising edge
```

### 7.2 LCD Initialization Sequence

```c
// ST7789 initialization commands
const uint8_t init_cmds[] = {
    0x01, 0,                    // Software reset
    0x11, 0,                    // Sleep out
    // Wait 120ms after sleep out
    0x3A, 1, 0x55,              // Pixel format: 16-bit RGB565
    0x36, 1, 0x00,              // Memory access control
    0x2A, 4, 0x00, 0x34, 0x00, 0xBA,  // Column address (52-186)
    0x2B, 4, 0x00, 0x28, 0x01, 0x17,  // Row address (40-279)
    0x21, 0,                    // Inversion on
    0x13, 0,                    // Normal mode
    0x29, 0,                    // Display on
};
```

### 7.3 Backlight PWM Configuration

```c
// LEDC configuration for backlight
ledc_timer_config_t timer_conf = {
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .duty_resolution = LEDC_TIMER_8_BIT,  // 0-255
    .timer_num = LEDC_TIMER_0,
    .freq_hz = 5000,                       // 5kHz PWM
    .clk_cfg = LEDC_AUTO_CLK
};

ledc_channel_config_t channel_conf = {
    .gpio_num = GPIO_NUM_4,
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .channel = LEDC_CHANNEL_0,
    .timer_sel = LEDC_TIMER_0,
    .duty = 200,                           // ~78% brightness
    .hpoint = 0
};
```

---

## 8. Camera Interface

### 8.1 OV2640 Configuration

**SCCB (I2C) Communication:**

```
Device address: 0x30 (7-bit) or 0x60/0x61 (8-bit read/write)

Register Banks:
- DSP registers: 0xFF = 0x00
- Sensor registers: 0xFF = 0x01
```

**Key Registers:**

| Register | Bank | Address | Description |
|----------|------|---------|-------------|
| RA_DLMT | DSP | 0xFF | Bank select |
| QS | DSP | 0x44 | Quantization scale (JPEG quality) |
| HSIZE | DSP | 0x51 | H size (bits 7:0) |
| VSIZE | DSP | 0x52 | V size (bits 7:0) |
| CTRL2 | Sensor | 0x09 | Output drive capability |
| COM7 | Sensor | 0x12 | System reset, output format |
| COM10 | Sensor | 0x15 | HREF, VSYNC options |

### 8.2 Frame Timing

```
                    CAMERA FRAME TIMING

    VSYNC ─────┐                                    ┌─────
               └────────────────────────────────────┘
               │◄─────── Frame Period (~33ms) ─────►│

    HREF  ─────┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ... ┌─┐ ┌─┐ ┌─────
               └─┘ └─┘ └─┘ └─┘       └─┘ └─┘
               │◄► Line (320 pixels @ QVGA)

    PCLK  ─────┴┬┴┬┴┬┴┬┴┬┴┬┴┬┴┬┴┬┴┬ ... ┴┬┴┬┴─────
               Pixel clock (varies with resolution)

    QVGA (320×240) @ 15fps:
    - Frame period: 66.7ms
    - Lines per frame: 240
    - Pixels per line: 320
    - Pixel clock: ~6MHz
```

### 8.3 JPEG Quality Settings

| Quality Value | Approximate Size (QVGA) | Use Case |
|---------------|-------------------------|----------|
| 10 | 15-20 KB | High quality, slow |
| 12 | 10-15 KB | Default, balanced |
| 15 | 8-12 KB | Good compression |
| 20 | 5-8 KB | Fast streaming |
| 30 | 3-5 KB | Low bandwidth |

---

## 9. Motor Driver Interface

### 9.1 TB6612FNG Specifications

| Parameter | Value | Notes |
|-----------|-------|-------|
| Operating voltage (VM) | 2.5V - 13.5V | Motor supply |
| Logic voltage (VCC) | 2.7V - 5.5V | 3.3V compatible |
| Output current | 1.2A (continuous) | Per channel |
| Peak current | 3.2A | Short duration |
| PWM frequency | Up to 100kHz | 20kHz recommended |
| Standby current | <1µA | STBY pin LOW |

### 9.2 PWM Configuration

```c
// Motor PWM configuration (20kHz)
#define MOTOR_PWM_FREQ_HZ    20000
#define MOTOR_PWM_RESOLUTION LEDC_TIMER_10_BIT  // 0-1023

ledc_timer_config_t motor_timer = {
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .duty_resolution = MOTOR_PWM_RESOLUTION,
    .timer_num = LEDC_TIMER_1,
    .freq_hz = MOTOR_PWM_FREQ_HZ,
    .clk_cfg = LEDC_AUTO_CLK
};

// Motor A - Channel 0
ledc_channel_config_t motor_a_channel = {
    .gpio_num = MOTOR_A_PWM_PIN,
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .channel = LEDC_CHANNEL_1,
    .timer_sel = LEDC_TIMER_1,
    .duty = 0,
    .hpoint = 0
};
```

### 9.3 Direction Control Truth Table

| STBY | IN1 | IN2 | OUT1 | OUT2 | Mode |
|------|-----|-----|------|------|------|
| L | X | X | Hi-Z | Hi-Z | Standby |
| H | L | L | L | L | Brake |
| H | L | H | L | H | Reverse |
| H | H | L | H | L | Forward |
| H | H | H | H | H | Brake |

### 9.4 Motor Control Algorithm

```c
void set_motor_speed(int motor, int speed) {
    // speed: -100 to +100 (percentage)

    uint32_t duty = abs(speed) * 1023 / 100;  // Scale to 10-bit
    bool forward = (speed >= 0);

    if (motor == MOTOR_A) {
        gpio_set_level(MOTOR_A_DIR_PIN, forward);
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, duty);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
    }
    // Similar for MOTOR_B
}

void differential_drive(int speed, int steering) {
    // steering: -100 (full left) to +100 (full right)

    int left_speed = speed + steering;
    int right_speed = speed - steering;

    // Clamp to valid range
    left_speed = constrain(left_speed, -100, 100);
    right_speed = constrain(right_speed, -100, 100);

    set_motor_speed(MOTOR_A, left_speed);
    set_motor_speed(MOTOR_B, right_speed);
}
```

---

## 10. PCB Design

### 10.1 Layer Stackup (4-Layer Recommended)

```
    ┌─────────────────────────────────────┐
    │  Layer 1: Top (Components/Signals)  │  35µm copper
    ├─────────────────────────────────────┤
    │  Prepreg (0.2mm)                    │
    ├─────────────────────────────────────┤
    │  Layer 2: GND Plane                 │  35µm copper
    ├─────────────────────────────────────┤
    │  Core (1.0mm)                       │  FR-4
    ├─────────────────────────────────────┤
    │  Layer 3: Power Plane               │  35µm copper
    ├─────────────────────────────────────┤
    │  Prepreg (0.2mm)                    │
    ├─────────────────────────────────────┤
    │  Layer 4: Bottom (Components/Signals)│  35µm copper
    └─────────────────────────────────────┘

    Total thickness: ~1.6mm
```

### 10.2 Design Rules

| Parameter | Value | Notes |
|-----------|-------|-------|
| Minimum trace width | 0.15mm (6mil) | Signal traces |
| Minimum clearance | 0.15mm (6mil) | Trace to trace |
| Power trace width | 0.5mm+ | For >500mA |
| Via drill | 0.3mm | Standard via |
| Via pad | 0.6mm | Annular ring 0.15mm |
| Thermal relief | 0.3mm spoke | Power planes |

### 10.3 Critical Layout Guidelines

**Power:**
- Place decoupling capacitors within 3mm of IC power pins
- Use wide traces (>0.5mm) for power distribution
- Separate analog and digital grounds, connect at single point

**RF (WiFi Antenna):**
- Keep antenna area clear of copper (top and bottom)
- 50Ω controlled impedance for antenna feed
- Minimum 10mm clearance around antenna

**High-Speed Signals:**
- SPI clock requires 22Ω series termination at source
- Match trace lengths for camera parallel bus (±5mm)
- Avoid running high-speed traces over split planes

### 10.4 Component Placement

```
                    PCB LAYOUT RECOMMENDATIONS

    ┌──────────────────────────────────────────────────────────┐
    │                                                          │
    │  ┌──────────┐                          ┌──────────┐     │
    │  │  USB-C   │                          │  Antenna │     │
    │  │Connector │                          │  Area    │     │
    │  └──────────┘                          │ (keep-out)│    │
    │                                        └──────────┘     │
    │  ┌──────────┐                                           │
    │  │ Charger  │    ┌─────────────────────────┐            │
    │  │ TP4056   │    │                         │            │
    │  └──────────┘    │     ESP32 Module        │            │
    │                  │                         │            │
    │  ┌──────────┐    │  (or TTGO/ESP32-CAM)    │            │
    │  │ Battery  │    │                         │            │
    │  │Connector │    └─────────────────────────┘            │
    │  └──────────┘                                           │
    │                                                          │
    │  ┌──────────┐    ┌──────────┐    ┌──────────┐          │
    │  │Decoupling│    │  Motor   │    │ Expansion│          │
    │  │Capacitors│    │  Driver  │    │  Header  │          │
    │  └──────────┘    └──────────┘    └──────────┘          │
    │                                                          │
    └──────────────────────────────────────────────────────────┘
```

---

## 11. Assembly Instructions

### 11.1 Required Tools

| Tool | Purpose |
|------|---------|
| Soldering iron (temperature controlled) | Through-hole/reflow |
| Hot air station | SMD rework |
| Flux (no-clean) | Soldering aid |
| Solder wire (0.5mm, lead-free) | Connections |
| Tweezers (ESD-safe) | SMD handling |
| Multimeter | Testing |
| Oscilloscope (optional) | Signal verification |
| Magnifying glass/microscope | Inspection |

### 11.2 Assembly Sequence

1. **PCB Preparation**
   - Inspect PCB for defects
   - Clean with IPA if needed
   - Apply solder paste (if using reflow)

2. **SMD Components (Bottom)**
   - Place decoupling capacitors first
   - Place small resistors and capacitors
   - Place ICs last
   - Reflow or hand-solder

3. **SMD Components (Top)**
   - Repeat process for top side
   - Ensure module headers are straight

4. **Through-Hole Components**
   - Headers and connectors
   - Large capacitors
   - Battery connector

5. **Module Installation**
   - ESP32-CAM or TTGO T-Display
   - Verify orientation before soldering
   - Check all pins are properly soldered

6. **Final Assembly**
   - Connect battery (with protection)
   - Attach motor driver (if applicable)
   - Install in enclosure

### 11.3 Soldering Guidelines

**Temperature Settings:**
| Component Type | Iron Temperature | Hot Air Temperature |
|----------------|------------------|---------------------|
| SMD passives | 300-320°C | 260-280°C |
| SMD ICs | 300-320°C | 260-280°C |
| Through-hole | 350-380°C | N/A |
| Lead-free | +20-30°C higher | +20°C higher |

**Quality Checks:**
- Shiny, concave fillet on through-hole joints
- Wetting on both pad and lead
- No solder bridges between pins
- No cold joints (grainy appearance)

---

## 12. Testing Procedures

### 12.1 Power-On Test

```
POWER-ON TEST CHECKLIST
═══════════════════════

Before applying power:
□ Visual inspection for shorts/bridges
□ Check polarity of battery connector
□ Measure resistance between VCC and GND (should be >1kΩ)

Initial power-up (with current-limited supply):
□ Set supply to 3.7V, limit to 500mA
□ Connect power
□ Verify current draw <100mA (no WiFi)
□ Check 3.3V rail: 3.3V ±0.1V
□ Touch-test for hot components (none should be hot)

Module boot:
□ Connect USB for serial monitor
□ Observe boot messages
□ Verify WiFi AP appears
□ Test web interface connectivity
```

### 12.2 GPIO Verification

```c
// GPIO test routine
void test_gpio_outputs(void) {
    const int test_pins[] = {4, 5, 13, 14, 16, 18, 19, 23, 25, 26, 27};

    for (int i = 0; i < sizeof(test_pins)/sizeof(test_pins[0]); i++) {
        gpio_set_direction(test_pins[i], GPIO_MODE_OUTPUT);
        gpio_set_level(test_pins[i], 1);
        vTaskDelay(pdMS_TO_TICKS(100));
        // Verify with multimeter/oscilloscope
        gpio_set_level(test_pins[i], 0);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

### 12.3 ADC Calibration Verification

```
ADC CALIBRATION TEST
════════════════════

Equipment: Precision voltage source, multimeter

Test points:
1. Apply 2.0V to ADC input
   - Expected reading: ~2048 counts (12-bit)
   - Tolerance: ±100 counts

2. Apply 1.0V to ADC input
   - Expected reading: ~1024 counts
   - Tolerance: ±100 counts

3. Battery voltage correlation:
   - Measure actual battery voltage
   - Compare to ADC-calculated voltage
   - Tolerance: ±50mV
```

### 12.4 Communication Tests

```
WiFi Test:
□ AP mode: SSID visible, connectable
□ STA mode: Connects to known network
□ HTTP: Web UI loads correctly
□ WebSocket: Real-time updates work

SPI Test (LCD):
□ Display initializes without errors
□ All pixels addressable
□ No visible artifacts or noise

I2C Test (if applicable):
□ Device detected at expected address
□ Read/write operations successful
```

---

## 13. Troubleshooting

### 13.1 Common Issues

| Symptom | Possible Cause | Solution |
|---------|----------------|----------|
| No power LED | Blown fuse, bad solder | Check fuse, reflow connections |
| Module not booting | GPIO0 held low | Check button/boot circuitry |
| WiFi weak/no signal | Antenna obstructed | Clear antenna area |
| LCD white screen | SPI not working | Check connections, clock signal |
| High current drain | Short circuit | Inspect for bridges |
| ADC reading wrong | Divider values off | Verify resistor values |
| Camera no image | PSRAM issue | Check sdkconfig, PSRAM init |
| Buttons not working | Missing pull-up | Add external pull-up resistor |

### 13.2 Debug Points

```
                    TEST POINT LOCATIONS

    TP1: VCC (3.3V) ─────────── Verify power supply
    TP2: VBAT ───────────────── Battery voltage
    TP3: GND ────────────────── Ground reference
    TP4: SPI_CLK ────────────── LCD clock signal
    TP5: UART_TX ────────────── Serial debug output
    TP6: ADC_IN ─────────────── Battery ADC input

    All test points should be accessible probing pads
```

### 13.3 Serial Debug Messages

```
Key boot messages to verify:

I (XXX) cpu_start: Starting scheduler on PRO CPU.
I (XXX) wifi:wifi driver task: ...
I (XXX) wifi:Init data frame dynamic ...
I (XXX) wifi:mode : softAP (XX:XX:XX:XX:XX:XX)
I (XXX) ROVER: WiFi AP started: ESP32-Rover
I (XXX) ROVER: IP Address: 192.168.4.1
I (XXX) ROVER: Web server started
I (XXX) ROVER: Battery voltage: 3.85V

Error indicators:
E (XXX) ... - Error condition
W (XXX) ... - Warning (may be recoverable)
```

---

## 14. Bill of Materials

### 14.1 Core Components

| Ref | Description | Value | Package | Qty | Notes |
|-----|-------------|-------|---------|-----|-------|
| U1 | ESP32 Module | TTGO T-Display or ESP32-CAM | Module | 1 | Main controller |
| U2 | Motor Driver | TB6612FNG | SSOP-24 | 1 | Optional |
| U3 | Battery Charger | TP4056 | SOP-8 | 1 | If not on module |

### 14.2 Passive Components

| Ref | Description | Value | Package | Qty | Notes |
|-----|-------------|-------|---------|-----|-------|
| R1, R2 | Resistor | 100kΩ 1% | 0603 | 2 | Battery divider |
| R3 | Resistor | 10kΩ 5% | 0603 | 1 | Button pull-up |
| R4-R5 | Resistor | 4.7kΩ 5% | 0603 | 2 | I2C pull-up |
| C1 | Capacitor | 100µF 10V | Electrolytic | 1 | Power input |
| C2-C5 | Capacitor | 100nF 50V | 0603 | 4 | Decoupling |
| C6 | Capacitor | 100nF 50V | 0603 | 1 | ADC filter |

### 14.3 Connectors

| Ref | Description | Value | Package | Qty | Notes |
|-----|-------------|-------|---------|-----|-------|
| J1 | USB Connector | USB-C | SMD | 1 | Power/programming |
| J2 | Battery Connector | JST PH 2.0 | 2-pin | 1 | Battery |
| J3-J4 | Motor Connector | JST XH 2.5 | 2-pin | 2 | Motor output |
| J5 | Expansion Header | 2.54mm | 10-pin | 1 | I2C, GPIO |

### 14.4 Mechanical

| Ref | Description | Value | Qty | Notes |
|-----|-------------|-------|-----|-------|
| SW1-SW2 | Tactile Switch | 6×6mm | 2 | TTGO only (built-in) |
| - | Standoffs | M2.5×10mm | 4 | PCB mounting |
| - | Screws | M2.5×6mm | 8 | Mounting |
| - | Heat shrink | Various | 1 set | Wire protection |

### 14.5 Optional Components

| Ref | Description | Value | Package | Qty | Notes |
|-----|-------------|-------|---------|-----|-------|
| U4 | IMU | MPU6050 | Module | 1 | Motion sensing |
| U5 | GPS | NEO-6M | Module | 1 | Location |
| LED1 | Status LED | 3mm | Through-hole | 1 | External indicator |
| F1 | PTC Fuse | 500mA | 1206 | 1 | Overcurrent protection |
| D1 | TVS Diode | SMBJ5.0A | SMB | 1 | ESD protection |

---

## Appendix A: Gerber File Checklist

When generating Gerber files for manufacturing:

```
Required Files:
□ Top Copper (GTL)
□ Bottom Copper (GBL)
□ Top Solder Mask (GTS)
□ Bottom Solder Mask (GBS)
□ Top Silkscreen (GTO)
□ Bottom Silkscreen (GBO)
□ Drill File (DRL or XLN)
□ Board Outline (GKO or GML)
□ Paste Stencil Top (GTP) - for reflow
□ Paste Stencil Bottom (GBP) - for reflow

Settings:
□ Units: mm
□ Format: 4.5 (integer.decimal)
□ Zero suppression: Leading
□ Coordinate format: Absolute

Include:
□ Assembly drawing (PDF)
□ BOM (CSV/Excel)
□ Pick and place file (CPL)
□ Schematic (PDF)
```

---

## Appendix B: Firmware Configuration Reference

```c
// Key configuration defines from config.h

// TTGO T-Display Pin Definitions
#define LCD_PIN_SCLK            GPIO_NUM_18
#define LCD_PIN_MOSI            GPIO_NUM_19
#define LCD_PIN_DC              GPIO_NUM_16
#define LCD_PIN_CS              GPIO_NUM_5
#define LCD_PIN_RST             GPIO_NUM_23
#define LCD_PIN_BACKLIGHT       GPIO_NUM_4

#define BUTTON_LEFT_PIN         GPIO_NUM_0
#define BUTTON_RIGHT_PIN        GPIO_NUM_35

#define BATTERY_ADC_PIN         GPIO_NUM_34
#define BATTERY_ADC_CHANNEL     ADC_CHANNEL_6
#define BATTERY_DIVIDER_RATIO   2.0f

// ESP32-CAM Pin Definitions
#define CAM_PIN_PWDN            GPIO_NUM_32
#define CAM_PIN_RESET           -1
#define CAM_PIN_XCLK            GPIO_NUM_0
#define CAM_PIN_SIOD            GPIO_NUM_26
#define CAM_PIN_SIOC            GPIO_NUM_27
#define CAM_PIN_D7              GPIO_NUM_35
#define CAM_PIN_D6              GPIO_NUM_34
#define CAM_PIN_D5              GPIO_NUM_39
#define CAM_PIN_D4              GPIO_NUM_36
#define CAM_PIN_D3              GPIO_NUM_21
#define CAM_PIN_D2              GPIO_NUM_19
#define CAM_PIN_D1              GPIO_NUM_18
#define CAM_PIN_D0              GPIO_NUM_5
#define CAM_PIN_VSYNC           GPIO_NUM_25
#define CAM_PIN_HREF            GPIO_NUM_23
#define CAM_PIN_PCLK            GPIO_NUM_22
```

---

## Appendix C: Revision History

| Rev | Date | Author | Changes |
|-----|------|--------|---------|
| 1.0 | 2026-01-25 | Dev Team | Initial release |

---

*End of Document*
