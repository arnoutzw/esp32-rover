# ESP32-CAM Rover Mechanical Design Specification

**Document Version**: 1.0
**Last Updated**: 2026-01-24
**Status**: Design Phase

---

## 1. Overview

This document specifies the mechanical design for a compact, high-performance rover featuring:
- ESP32-CAM for vision and control
- Brushless DC powertrain with Field Oriented Control (FOC)
- Ackerman steering geometry
- Rear differential drive

### 1.1 Design Goals

| Goal | Target | Rationale |
|------|--------|-----------|
| Compact footprint | 160mm × 80mm chassis | Indoor/outdoor versatility |
| Runtime | >30 minutes | Practical operation time |
| Speed range | 0-2 m/s | Controllable for FPV driving |
| Steering radius | <300mm | Tight maneuvering |
| Payload capacity | 100g | Camera + optional sensors |

---

## 2. Bill of Materials

### 2.1 Electronic Components

| Component | Model | Function | Specifications |
|-----------|-------|----------|----------------|
| MCU | ESP32-CAM | Vision & Control | WiFi, OV2640 camera, 520KB SRAM |
| Motor Driver | SimpleFOC Mini | BLDC Control | 3-phase, 2S-3S LiPo, 5A continuous |
| Drive Motor | A2212 | Rear Propulsion | 1000KV, 3.17mm shaft |
| Steering Servo | MG90S | Ackerman Linkage | Metal gear, 1.8kg·cm torque |
| Encoder | AS5600 | Motor Position | I2C, 12-bit, magnetic |
| Battery | 2× 18650 | Power Source | 7.4V nominal, 2600mAh typical |
| Voltage Regulator | Buck Converter | Logic Power | 7.4V → 5V, 3A minimum |

### 2.2 Mechanical Components

| Component | Quantity | Specifications |
|-----------|----------|----------------|
| Differential | 1 | 1/18 scale RC compatible or 3D-printed |
| Pinion Gear | 1 | 12T, 3.17mm bore, M0.5 module |
| Spur Gear | 1 | 48T, M0.5 module (4:1 reduction) |
| Bearings | 8 | 3×6×2mm (MR63ZZ) |
| M2 Screws | 20 | Various lengths (6mm, 10mm, 16mm) |
| M3 Screws | 10 | Various lengths (8mm, 12mm) |
| Push Rod | 2 | 1.5mm steel wire, 40mm length |
| Ball Links | 4 | M2 threaded, 10mm ball-to-ball |

### 2.3 Printed Parts

| Part | Material | Infill | Notes |
|------|----------|--------|-------|
| Lower Chassis | PETG/ABS | 40% | Battery and drivetrain |
| Upper Chassis | PETG/ABS | 30% | Electronics deck |
| Motor Mount | PETG/ABS | 60% | Sliding adjustment |
| Steering Knuckles | PETG/ABS | 80% | High stress component |
| Differential Housing | PETG/ABS | 50% | Gear mesh critical |
| Camera Bracket | PLA/PETG | 30% | 45° angle mount |
| Wheel Hubs | PETG/ABS | 60% | Hex pattern |

---

## 3. Chassis Design

### 3.1 Sandwich-Plate Architecture

The chassis uses a two-deck design for modularity and ease of assembly.

```
┌─────────────────────────────────────────────┐
│              UPPER DECK                      │
│  ┌─────────┐  ┌──────────┐  ┌─────────────┐ │
│  │ESP32-CAM│  │SimpleFOC │  │   MG90S     │ │
│  │ (front) │  │  Mini    │  │   Servo     │ │
│  └─────────┘  └──────────┘  └─────────────┘ │
├─────────────────────────────────────────────┤
│              LOWER DECK                      │
│  ┌─────────────────────────────────────────┐│
│  │      18650 Battery Sled (2 cells)       ││
│  └─────────────────────────────────────────┘│
│           ┌─────────────────┐               │
│           │ Rear Differential│              │
│           │    + A2212      │               │
│           └─────────────────┘               │
└─────────────────────────────────────────────┘
```

### 3.2 Dimensions

#### Lower Deck
- Overall: 160mm × 80mm × 3mm
- Battery cutout: 67mm × 40mm (centered)
- Motor mount area: 40mm × 40mm (rear)
- Differential mount: 30mm × 50mm (rear center)
- Mounting holes: M3, 8mm from edges

#### Upper Deck
- Overall: 140mm × 70mm × 3mm
- ESP32-CAM cutout: 28mm × 42mm (front)
- SimpleFOC mount: 25mm × 25mm (center)
- Servo mount: 23mm × 12mm (center-rear)
- Standoff holes: M3, aligned with lower deck

### 3.3 OpenSCAD Reference Design

```openscad
// ============================================
// ESP32 Rover - Lower Chassis Plate
// ============================================

// Main dimensions
chassis_length = 160;
chassis_width = 80;
chassis_thickness = 3;

// Battery dimensions (18650: 18mm dia × 65mm)
battery_length = 67;
battery_width = 40;
battery_depth = 20;

// Motor mount (A2212: 27.5mm dia, 16mm/19mm hole spacing)
motor_mount_dia = 28;
motor_hole_spacing = 16;

module lower_chassis() {
    difference() {
        // Main plate
        cube([chassis_length, chassis_width, chassis_thickness]);

        // Battery cutout (centered)
        translate([(chassis_length - battery_length) / 2,
                   (chassis_width - battery_width) / 2,
                   -1])
            cube([battery_length, battery_width, chassis_thickness + 2]);

        // Motor mount center hole
        translate([chassis_length - 20, chassis_width / 2, -1])
            cylinder(d = 8, h = chassis_thickness + 2, $fn = 32);

        // Motor mount screw holes (M3)
        for (angle = [0, 90, 180, 270]) {
            translate([chassis_length - 20, chassis_width / 2, 0])
                rotate([0, 0, angle])
                    translate([motor_hole_spacing / 2, 0, -1])
                        cylinder(d = 3.2, h = chassis_thickness + 2, $fn = 16);
        }

        // Deck mounting holes (M3, 8mm from edges)
        for (x = [8, chassis_length - 8]) {
            for (y = [8, chassis_width - 8]) {
                translate([x, y, -1])
                    cylinder(d = 3.2, h = chassis_thickness + 2, $fn = 16);
            }
        }

        // Additional center mounting holes
        for (x = [chassis_length / 2]) {
            for (y = [8, chassis_width - 8]) {
                translate([x, y, -1])
                    cylinder(d = 3.2, h = chassis_thickness + 2, $fn = 16);
            }
        }
    }
}

lower_chassis();
```

---

## 4. Drivetrain Design

### 4.1 Gear Reduction Requirements

The A2212 motor at 1000KV produces:
- At 7.4V: ~7400 RPM no-load
- Required wheel RPM for 1 m/s (40mm wheel): ~480 RPM
- **Minimum reduction ratio: 15:1**

#### Proposed Two-Stage Reduction

| Stage | Pinion | Gear | Ratio |
|-------|--------|------|-------|
| Stage 1 (Motor) | 12T | 48T | 4:1 |
| Stage 2 (Diff Input) | 10T | 40T | 4:1 |
| **Total** | | | **16:1** |

### 4.2 Differential Assembly

Using a 1/18 scale RC differential (WLToys compatible) or custom 3D-printed:

```
                    ┌─────────────┐
                    │   Spur Gear │
                    │    (48T)    │
                    └──────┬──────┘
                           │
              ┌────────────┴────────────┐
              │      Differential       │
              │    ┌─────────────┐      │
              │    │ Spider Gears│      │
              │    └─────────────┘      │
              └────────┬───────┬────────┘
                       │       │
                  ┌────┴───┐ ┌─┴────┐
                  │Left    │ │Right │
                  │Axle    │ │Axle  │
                  └────────┘ └──────┘
```

### 4.3 Motor Mount Design

The motor mount must allow gear mesh adjustment:

- **Type**: Sliding mount with slotted holes
- **Adjustment range**: ±3mm
- **Material**: PETG at 60% infill minimum
- **Shaft clearance**: 3.5mm hole for 3.17mm shaft

**Critical Dimensions**:
- A2212 mounting holes: 16mm × 19mm pattern, M3
- A2212 body diameter: 27.5mm
- A2212 shaft: 3.17mm diameter

---

## 5. Steering System

### 5.1 Ackerman Geometry

For proper Ackerman steering, the steering arm pivot lines must intersect at the rear axle center.

```
        Front Axle
    ┌───────────────────┐
    │                   │
   ╱│                   │╲
  ╱ │                   │ ╲
 ╱  │                   │  ╲
╱   │                   │   ╲
    │                   │
    │                   │
    │                   │
    └───────────────────┘
        Rear Axle
         (pivot point)
```

### 5.2 Steering Specifications

| Parameter | Value | Notes |
|-----------|-------|-------|
| Wheelbase | 100mm | Front to rear axle |
| Track width | 70mm | Wheel center to center |
| Steering angle | ±25° | Maximum lock |
| Servo throw | ±45° | MG90S range |
| Linkage ratio | 1.8:1 | Servo to wheel |

### 5.3 Steering Knuckle Design

```
     ┌─────────────────┐
     │   Kingpin Axis  │
     │        │        │
     │   ┌────┴────┐   │
     │   │Knuckle  │   │
     │   │  Body   │   │
     │   └────┬────┘   │
     │        │        │
     │   ┌────┴────┐   │
     │   │Steering │   │
     │   │  Arm    │───┼──── Push Rod Connection
     │   └─────────┘   │     (1.5mm hole)
     │                 │
     └─────────────────┘
```

**Critical Dimensions**:
- Kingpin bore: 3mm for M3 bolt
- Steering arm length: 15mm (from kingpin to push rod)
- Steering arm angle: Calculate for Ackerman (typically 20-25° inward)
- Push rod hole: 1.5mm diameter for snug wire fit
- Wheel hub bore: 5mm for axle shaft

### 5.4 MG90S Servo Horn

- Spline: 21-tooth, 5.8mm diameter
- Output arm length: 12mm (effective)
- Push rod hole: 1.5mm at 10mm from center

---

## 6. Electronics Layout

### 6.1 GPIO Allocation (ESP32-CAM)

The ESP32-CAM has limited available GPIOs due to camera usage.

| GPIO | Function | Notes |
|------|----------|-------|
| 12 | SimpleFOC PWM A | Must avoid during boot |
| 13 | SimpleFOC PWM B | Safe for output |
| 14 | Servo PWM | Safe for output |
| 15 | SimpleFOC PWM C | Must be LOW during boot |
| 2 | SimpleFOC Enable | Built-in LED |
| 4 | Flash LED | Can be repurposed |

**I2C for AS5600 Encoder**:
| GPIO | Function |
|------|----------|
| 16 | SDA (if available) |
| 17 | SCL (if available) |

*Note: GPIO 16/17 may conflict with PSRAM on some ESP32-CAM modules.*

### 6.2 Power Distribution

```
┌─────────────────────────────────────────────────────────────┐
│                    7.4V Battery Pack                        │
│                    (2× 18650 Series)                        │
└─────────────────────┬───────────────────────────────────────┘
                      │
         ┌────────────┼────────────┐
         │            │            │
         ▼            ▼            ▼
    ┌─────────┐  ┌─────────┐  ┌─────────┐
    │SimpleFOC│  │  Buck   │  │ Power   │
    │  Mini   │  │Converter│  │ Switch  │
    │(Direct) │  │7.4V→5V  │  │         │
    └────┬────┘  └────┬────┘  └─────────┘
         │            │
         │       ┌────┴────┐
         │       │         │
         ▼       ▼         ▼
    ┌─────────┐ ┌───────┐ ┌─────────┐
    │  A2212  │ │ESP32- │ │  MG90S  │
    │  Motor  │ │ CAM   │ │  Servo  │
    └─────────┘ └───────┘ └─────────┘
```

### 6.3 Critical Power Notes

1. **Brown-out Prevention**: Place 1000μF electrolytic capacitor across ESP32-CAM 5V/GND
2. **Servo Spike Protection**: Add 100μF capacitor near servo power pins
3. **SimpleFOC Direct Power**: Connect directly to battery, NOT through buck converter
4. **Wire Gauge**: 18 AWG minimum for motor power, 22 AWG for logic

---

## 7. Assembly Sequence

### 7.1 Lower Deck Assembly

1. Install 18650 battery holder/sled
2. Mount differential assembly with M3 screws
3. Install motor mount (leave screws loose for adjustment)
4. Mount A2212 motor to sliding mount
5. Install pinion gear on motor shaft (use thread locker)
6. Adjust motor position for proper gear mesh (0.1mm backlash)
7. Tighten motor mount screws
8. Install rear axles and wheels

### 7.2 Upper Deck Assembly

1. Mount ESP32-CAM with 45° camera bracket
2. Install SimpleFOC Mini with standoffs
3. Mount MG90S servo in steering position
4. Install servo horn with push rod connections

### 7.3 Final Assembly

1. Connect standoffs between upper and lower decks
2. Install front steering knuckles and axles
3. Connect push rods between servo and steering arms
4. Route and connect all wiring
5. Install front wheels
6. Verify steering geometry
7. Adjust steering trim

---

## 8. CAD File Index

| Filename | Description | Format |
|----------|-------------|--------|
| `lower_chassis.scad` | Lower deck plate | OpenSCAD |
| `lower_chassis.stl` | Lower deck (export) | STL |
| `upper_chassis.scad` | Upper deck plate | OpenSCAD |
| `upper_chassis.stl` | Upper deck (export) | STL |
| `motor_mount.scad` | Sliding motor mount | OpenSCAD |
| `motor_mount.stl` | Motor mount (export) | STL |
| `steering_knuckle.scad` | Front knuckle | OpenSCAD |
| `steering_knuckle.stl` | Knuckle (export) | STL |
| `camera_bracket.scad` | 45° ESP32-CAM mount | OpenSCAD |
| `camera_bracket.stl` | Camera bracket (export) | STL |
| `wheel_hub.scad` | Wheel hub adapter | OpenSCAD |
| `wheel_hub.stl` | Wheel hub (export) | STL |
| `full_assembly.step` | Complete assembly | STEP |

---

## 9. Design Validation Checklist

### 9.1 Mechanical

- [ ] Battery fits in sled with clearance
- [ ] Motor shaft clears chassis
- [ ] Gear mesh is adjustable
- [ ] Differential fits in allocated space
- [ ] Steering linkage has full travel
- [ ] No binding at steering extremes
- [ ] Wheelbase matches design
- [ ] Track width matches design
- [ ] Ground clearance adequate (>10mm)
- [ ] All fasteners accessible

### 9.2 Electrical

- [ ] Wire routing avoids moving parts
- [ ] Connectors accessible for service
- [ ] Heat dissipation adequate for SimpleFOC
- [ ] ESP32-CAM ventilation
- [ ] Battery removable without disassembly
- [ ] Power switch accessible

### 9.3 Software Integration

- [ ] GPIO assignments match firmware
- [ ] Servo direction correct
- [ ] Motor direction correct
- [ ] Encoder orientation correct
- [ ] Camera angle provides good FPV view

---

## 10. Revision History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2026-01-24 | Initial design specification |

---

## Appendix A: Reference Dimensions

### A2212 Motor
- Stator diameter: 22mm
- Stator length: 12mm
- Body diameter: 27.5mm
- Shaft diameter: 3.17mm
- Shaft length: 12mm
- Mounting holes: M3 × 4, 16mm and 19mm patterns

### MG90S Servo
- Body: 22.5mm × 12mm × 22mm
- Mounting tabs: 32mm × 12mm
- Tab holes: 2mm × 2
- Output shaft: 4.8mm (21-tooth spline)
- Wire length: 250mm

### 18650 Cell
- Diameter: 18.4mm (with wrapper)
- Length: 65mm (without button top)
- Length: 67mm (with button top)

### ESP32-CAM
- PCB: 27mm × 40.5mm
- Height: 12mm (without antenna)
- Camera ribbon: 24-pin, 0.5mm pitch
- Mounting holes: None (use bracket)
