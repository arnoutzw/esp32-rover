# ESP32 Rover Mechanical Requirements

## Overview

This document defines the mechanical design requirements for the ESP32-based FPV (First-Person View) rover platform. The design emphasizes modularity, ease of assembly, and 3D printability for rapid prototyping and iteration.

## Design Philosophy

- **Modular Design**: Components should be easily replaceable and upgradeable
- **Snap-Fit Assembly**: No screws or adhesives for main enclosure assembly
- **3D Printable**: All structural components printable on standard FDM printers
- **Compact Footprint**: Minimize size while accommodating all required components

## Enclosure Requirements

### REQ-MECH-001: Chassis Structure
- **Type**: Tank-style chassis with independent left/right drive
- **Material**: PLA or PETG for 3D printing
- **Assembly**: Snap-fit design for tool-free assembly
- **Accessibility**: Easy access to electronics for debugging and maintenance

### REQ-MECH-002: Component Mounting
The chassis shall provide secure mounting for:
- **ESP32-CAM module** (or TTGO T-Display)
- **Motor driver board** (e.g., L298N, TB6612FNG, or DRV8833)
- **Battery pack** (2x 18650 cells)
- **Voltage regulator/BMS** (USB-C charging circuit)
- **Power switch**
- **Optional components**: Sensors, servo mounts, bumpers

### REQ-MECH-003: Wire Management
- **Cable Routing**: Integrated channels for wire routing
- **Strain Relief**: Prevent wire damage at connection points
- **Accessibility**: Allow easy wire replacement without disassembly

## Power System

### REQ-MECH-004: Battery Configuration
**Battery Type**: 18650 Lithium-ion cells

**Configuration Options**:
1. **1S2P (3.7V nominal, ~6000mAh)**:
   - 2 cells in parallel
   - Voltage: 3.0V - 4.2V
   - Suitable for 3.3V/5V regulated systems

2. **2S1P (7.4V nominal, ~3000mAh)**:
   - 2 cells in series
   - Voltage: 6.0V - 8.4V
   - Better motor performance, requires 5V/3.3V step-down

**Recommended**: 2S configuration for improved motor torque

### REQ-MECH-005: Battery Mounting
- **Holder Type**: Spring-loaded or friction-fit battery holder
- **Accessibility**: Easy battery removal for charging/replacement
- **Polarity Protection**: Physical or circuit-based reverse polarity protection
- **Retention**: Secure mounting to prevent disconnection during movement

### REQ-MECH-006: Charging Interface
- **Connector**: USB-C port for charging
- **Mounting**: Panel-mount USB-C port accessible without disassembly
- **Circuit**: TP4056 or similar charging module with:
  - Overcharge protection
  - Over-discharge protection
  - Short circuit protection
  - Charge status LED indicators

## Drive System

### REQ-MECH-007: Motor Type
**Motor**: TT (Toy Train) gearmotor (Arduino-compatible)

**Specifications**:
- **Voltage**: 3-6V DC (compatible with 2S battery)
- **Gearbox**: 1:48 or 1:120 reduction ratio
- **Output Shaft**: 5mm diameter with D-shaft or cross shaft
- **Mounting**: Standard TT motor bracket (26mm spacing)

**Quantity**: 2 motors (left and right tracks)

### REQ-MECH-008: Drive Mechanism
**Type**: Tank steering (differential drive)

**Control**:
- Independent left/right motor control
- Forward/reverse operation
- Variable speed control (PWM)

**Steering Method**:
- Turn left: Right motor forward, left motor slower/reverse
- Turn right: Left motor forward, right motor slower/reverse
- Pivot turn: Motors rotating opposite directions

### REQ-MECH-009: Motor Mounting
- **Bracket**: 3D-printed motor mounts with:
  - Secure motor retention
  - Alignment to drive wheels/tracks
  - Vibration dampening (optional rubber isolation)
- **Accessibility**: Easy motor removal for maintenance

### REQ-MECH-010: Wheel/Track System

**Option 1: Wheels**
- **Type**: 3D-printed or rubber wheels
- **Diameter**: 40-60mm
- **Hub**: D-shaft or hex hub for TT motor shaft
- **Traction**: Rubber O-ring or TPU tread

**Option 2: Tracks**
- **Type**: 3D-printed track links or rubber tracks
- **Drive Sprocket**: Mounts to motor shaft
- **Idler Wheels**: Front/rear tensioning wheels
- **Ground Contact**: Sufficient track length for stability

**Recommended**: Tracks for better terrain handling and FPV aesthetics

## Electronics Integration

### REQ-MECH-011: ESP32-CAM Mounting
- **Camera Orientation**: Forward-facing, adjustable tilt (0-30°)
- **Antenna Clearance**: Minimum 10mm clearance around antenna
- **Heat Dissipation**: Ventilation for module cooling
- **Protection**: Optional clear cover for camera lens

### REQ-MECH-012: Motor Driver Mounting
- **Placement**: Centrally located for short motor wire runs
- **Cooling**: Airflow access to heatsink/driver IC
- **Wiring**: Labeled terminals or color-coded wire management

### REQ-MECH-013: Voltage Regulator
- **Input**: Battery voltage (3.7V or 7.4V depending on config)
- **Outputs**:
  - 5V for ESP32-CAM and motor driver logic
  - 3.3V for ESP32 core (if not using onboard regulator)
- **Current**: Minimum 2A capacity for ESP32-CAM + accessories

## Dimensional Constraints

### REQ-MECH-014: Printer Compatibility
- **Build Volume**: Fits within 200mm x 200mm x 200mm build volume
- **Part Splitting**: Large components split into printable sections
- **Snap-Fit Tolerance**: 0.2mm-0.3mm clearance for snap features

### REQ-MECH-015: Overall Dimensions
**Target Size**:
- **Length**: 120-150mm (excluding antenna)
- **Width**: 80-100mm (track width)
- **Height**: 60-80mm (ground to top of chassis)

**Ground Clearance**: Minimum 10mm for obstacle traversal

### REQ-MECH-016: Weight Budget
**Target Weight**: < 300g (including batteries)

**Component Breakdown**:
- Chassis: ~80g (PLA)
- Electronics: ~40g (ESP32-CAM, drivers, regulators)
- Motors: ~30g (2x TT motors)
- Batteries: ~90g (2x 18650 cells)
- Wheels/Tracks: ~30g
- Margin: ~30g

## Safety and Durability

### REQ-MECH-017: Impact Protection
- **Battery Guard**: Prevent direct impact to batteries
- **Electronics Shield**: Protect PCBs from drops and collisions
- **Motor Protection**: Prevent debris ingestion into gearbox

### REQ-MECH-018: Thermal Management
- **Ventilation**: Airflow slots for heat dissipation
- **Component Spacing**: Minimum 5mm clearance between heat-generating components
- **Material**: Heat-resistant filament (PLA+, PETG, or ABS) for motor mounts

### REQ-MECH-019: Electrical Safety
- **Insulation**: No exposed high-current traces
- **Fusing**: Optional fuse protection for battery output
- **Short Circuit Protection**: Physical barriers between battery terminals
- **User Access**: Warning labels for battery compartment

## Assembly and Maintenance

### REQ-MECH-020: Tool-Free Assembly
- **Snap Fits**: All major enclosure components use snap-fit joints
- **Tolerance**: ±0.2mm tolerance for reliable snap engagement
- **Reassembly**: Snaps survive minimum 10 assembly/disassembly cycles

### REQ-MECH-021: Maintenance Access
- **Top Cover Removal**: Access to all electronics via single removable panel
- **Battery Swap**: < 30 seconds to replace batteries
- **Wire Routing**: Color-coded or labeled connections
- **Modularity**: Individual components replaceable without full disassembly

### REQ-MECH-022: Documentation
- **Assembly Instructions**: Step-by-step assembly guide
- **Bill of Materials (BOM)**: Complete parts list with sources
- **STL Files**: Organized STL files with descriptive names
- **Print Settings**: Recommended slicer settings per component

## Optional Features

### REQ-MECH-023: Expansion Points
**Mounting Provisions** (optional):
- Servo mount for pan/tilt mechanism
- Ultrasonic sensor brackets (HC-SR04)
- LED strip mounting channels
- Bumper/collision sensor mounts
- Cargo bed or payload area

### REQ-MECH-024: Customization
- **Color Coding**: Parts designed for multi-color printing
- **Branding**: Recessed areas for logos/labels
- **Accessory Rails**: Standard mounting slots for add-ons

## Design Verification

### REQ-MECH-025: Prototype Testing
**Validation Tests**:
- [ ] Snap-fit assembly without tools
- [ ] Battery fitment and retention
- [ ] Motor mounting alignment
- [ ] Ground clearance verification
- [ ] Weight target (< 300g)
- [ ] Structural integrity (1m drop test)
- [ ] Thermal performance (30 min continuous operation)
- [ ] Print quality on standard FDM printer

## Future Enhancements

**Potential Upgrades** (not currently required):
- Suspension system for rough terrain
- Water-resistant sealing (IP65)
- Modular track system (swappable track lengths)
- Onboard LED lighting
- GPS/IMU sensor integration
- FPV antenna tracker mount

---

**Document Version**: 1.0
**Last Updated**: 2026-01-25
**Status**: Draft
