# Mechanical Requirements Specification
## ESP32 Rover Platform

| Document Info | Details |
|---------------|---------|
| **Project** | ESP32 WiFi-Controlled FPV Rover |
| **Document ID** | MRS-ESP32-ROVER-001 |
| **Version** | 1.1 |
| **Date** | 2026-01-25 |
| **Status** | Approved |
| **Author** | Development Team |
| **Classification** | Internal |

---

## Document Control

### Revision History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0 | 2026-01-25 | Dev Team | Initial draft |
| 1.1 | 2026-01-25 | Dev Team | Restructured to professional format with traceability |

### Approval Signatures

| Role | Name | Signature | Date |
|------|------|-----------|------|
| Project Lead | TBD | - | - |
| Technical Lead | TBD | - | - |

---

## Table of Contents

1. [Introduction](#1-introduction)
2. [Scope](#2-scope)
3. [Design Philosophy](#3-design-philosophy)
4. [Requirements](#4-requirements)
5. [Traceability Matrix](#5-traceability-matrix)
6. [Verification Methods](#6-verification-methods)
7. [References](#7-references)
8. [Appendices](#8-appendices)

---

## 1. Introduction

### 1.1 Purpose

This Mechanical Requirements Specification (MRS) defines the mechanical design requirements for the ESP32-based FPV rover platform. It provides a comprehensive set of traceable requirements to guide the mechanical design, fabrication, and testing processes.

### 1.2 Intended Audience

- Mechanical designers
- CAD engineers
- Manufacturing technicians
- Quality assurance team
- Project stakeholders

### 1.3 Product Overview

The ESP32 Rover is a compact, WiFi-controlled tracked vehicle featuring:
- FPV camera streaming
- Tank-style differential drive
- 3D-printable snap-fit chassis
- USB-C rechargeable battery system
- Modular electronics mounting

---

## 2. Scope

### 2.1 In Scope

- Chassis structural design
- Power system (battery, charging, voltage regulation)
- Drive system (motors, tracks/wheels, mounting)
- Electronics integration and mounting
- Assembly and manufacturing processes

### 2.2 Out of Scope

- Firmware specifications (see SRS-ESP32-ROVER-001)
- Electrical schematics
- Component sourcing and procurement

---

## 3. Design Philosophy

### 3.1 Core Principles

| Principle | Description | Benefit |
|-----------|-------------|---------|
| **Modularity** | Components easily replaceable/upgradeable | Maintenance, iteration |
| **Snap-Fit Assembly** | Tool-free enclosure assembly | Accessibility, speed |
| **3D Printability** | FDM-compatible on 200mm build volume | Rapid prototyping |
| **Compactness** | Minimize footprint while housing components | Portability, agility |

### 3.2 Design Constraints

- Maximum build volume: 200mm × 200mm × 200mm
- Target weight: < 300g (all-up)
- Material: PLA/PETG/ABS (FDM compatible)
- Assembly time: < 30 minutes

---

## 4. Requirements

### 4.1 Requirement Structure

Each requirement follows this format:

**REQ-MECH-XXX: [Title]**
- **Priority**: Critical / High / Medium / Low
- **Type**: Functional / Non-Functional / Interface / Constraint
- **Description**: Detailed specification
- **Rationale**: Why this requirement exists
- **Acceptance Criteria**: Measurable success conditions
- **Verification Method**: I (Inspection), T (Test), A (Analysis), D (Demonstration)
- **Dependencies**: Related requirements
- **Status**: Proposed / Approved / Implemented / Verified

---

### 4.2 Enclosure Requirements

#### REQ-MECH-001: Chassis Structure

| Field | Value |
|-------|-------|
| **Priority** | Critical |
| **Type** | Functional |
| **Description** | The chassis shall provide a rigid structural platform capable of supporting all onboard components and withstanding operational loads. |
| **Rationale** | Structural integrity is fundamental to protecting electronics and enabling reliable operation. |
| **Acceptance Criteria** | • Passes 1m drop test without structural failure<br>• Deflection < 2mm under 500g static load<br>• Supports minimum 300g total system weight |
| **Verification Method** | T (Drop test), T (Load test), I (Visual inspection) |
| **Dependencies** | REQ-MECH-002, REQ-MECH-016 |
| **Status** | Approved |

**Specifications**:
- **Type**: Tank-style chassis with independent left/right drive compartments
- **Material**: PLA or PETG (3D-printed FDM)
- **Assembly**: Snap-fit design for tool-free assembly
- **Wall Thickness**: Minimum 2mm for structural components

---

#### REQ-MECH-002: Component Mounting Points

| Field | Value |
|-------|-------|
| **Priority** | Critical |
| **Type** | Interface |
| **Description** | The chassis shall provide secure, vibration-resistant mounting points for all electronic and mechanical components. |
| **Rationale** | Prevents component damage from vibration and ensures electrical connections remain intact during operation. |
| **Acceptance Criteria** | • All components remain secured after 5-minute vibration test<br>• No observable component movement during 30° tilt test<br>• Mounting points accommodate component tolerances (±0.5mm) |
| **Verification Method** | T (Vibration test), T (Tilt test), I (Fit check) |
| **Dependencies** | REQ-MECH-011, REQ-MECH-012, REQ-MECH-013 |
| **Status** | Approved |

**Required Mounting Points**:
1. ESP32-CAM module (or TTGO T-Display)
2. Motor driver board (L298N, TB6612FNG, or DRV8833)
3. Battery holder (2× 18650 cells)
4. Voltage regulator/BMS module
5. Power switch
6. Optional: Sensor brackets, servo mounts

---

#### REQ-MECH-003: Wire Management System

| Field | Value |
|-------|-------|
| **Priority** | Medium |
| **Type** | Non-Functional |
| **Description** | The chassis shall incorporate integrated cable routing channels and strain relief features to protect wiring from damage and maintain neat organization. |
| **Rationale** | Prevents wire chafing, accidental disconnection, and facilitates troubleshooting/maintenance. |
| **Acceptance Criteria** | • All power and signal wires routed through designated channels<br>• Minimum bend radius of 5mm maintained for all wires<br>• Strain relief provided at all connector exit points<br>• Wire replacement possible without full disassembly |
| **Verification Method** | I (Visual inspection), D (Maintenance demonstration) |
| **Dependencies** | REQ-MECH-021 |
| **Status** | Approved |

**Features**:
- Cable routing channels (5mm width)
- Snap-in wire guides or zip-tie anchors
- Strain relief at board connections
- Color-coded routing (power: red, signal: blue, ground: black)

---

### 4.3 Power System Requirements

#### REQ-MECH-004: Battery Configuration

| Field | Value |
|-------|-------|
| **Priority** | Critical |
| **Type** | Functional |
| **Description** | The rover shall utilize 2× 18650 lithium-ion cells in either 1S2P or 2S1P configuration to power all onboard systems. |
| **Rationale** | 18650 cells provide excellent energy density, availability, and cost-effectiveness. 2S configuration recommended for motor torque. |
| **Acceptance Criteria** | • Battery holder accommodates standard 18650 cells (65mm length, 18mm diameter)<br>• Supports both 1S2P and 2S1P configurations via wiring change<br>• Voltage output matches specification (3.0-4.2V for 1S, 6.0-8.4V for 2S) |
| **Verification Method** | T (Voltage measurement), I (Physical fit check) |
| **Dependencies** | REQ-MECH-005, REQ-MECH-006, REQ-MECH-013 |
| **Status** | Approved |

**Configuration Options**:

| Config | Voltage (Nominal) | Capacity | Use Case | Recommendation |
|--------|-------------------|----------|----------|----------------|
| **1S2P** | 3.7V | ~6000mAh | Low-power, long runtime | Suitable for static demos |
| **2S1P** | 7.4V | ~3000mAh | Motor torque, performance | **Recommended** for mobile operation |

---

#### REQ-MECH-005: Battery Retention System

| Field | Value |
|-------|-------|
| **Priority** | High |
| **Type** | Safety |
| **Description** | The battery holder shall securely retain cells during all operational conditions including vibration, impact, and orientation changes. |
| **Rationale** | Battery disconnection during operation causes system failure and potential crash damage. |
| **Acceptance Criteria** | • Batteries remain seated during 5-minute vibration test (10Hz, 2G amplitude)<br>• Batteries remain seated during 30° tilt in all axes<br>• Battery swap time < 30 seconds<br>• Positive retention force > 5N per cell |
| **Verification Method** | T (Vibration test), T (Retention force measurement), D (Swap demonstration) |
| **Dependencies** | REQ-MECH-004, REQ-MECH-019 |
| **Status** | Approved |

**Design Options**:
- Spring-loaded holder (self-tensioning)
- Friction-fit with retention lip (simple, low-cost)
- Hinged lid with latch (tool-free but more complex)

**Safety Features**:
- Polarity protection (physical keying or circuit-based)
- Insulated holder to prevent shorts
- Thermal isolation from heat-generating components

---

#### REQ-MECH-006: USB-C Charging Interface

| Field | Value |
|-------|-------|
| **Priority** | High |
| **Type** | Interface |
| **Description** | The rover shall provide a panel-mounted USB-C port for charging batteries without disassembly. |
| **Rationale** | Eliminates need for external charger, improves user experience, enables charging while powered on. |
| **Acceptance Criteria** | • USB-C port accessible without removing cover<br>• Charging circuit provides CC/CV charging profile<br>• Charge status visible via LED indicators<br>• Over-charge and over-discharge protection functional |
| **Verification Method** | T (Charging test), I (Accessibility check), T (Protection circuit test) |
| **Dependencies** | REQ-MECH-004, REQ-MECH-019 |
| **Status** | Approved |

**Charging Module Specifications**:
- IC: TP4056 or equivalent
- Input: USB-C (5V, up to 2A)
- Output: 4.2V (single cell) or 8.4V (2S via 2× TP4056)
- Protection: Over-charge, over-discharge, short-circuit
- Indicators: Charging (red), Full (green)

---

### 4.4 Drive System Requirements

#### REQ-MECH-007: Motor Specification

| Field | Value |
|-------|-------|
| **Priority** | Critical |
| **Type** | Functional |
| **Description** | The rover shall use 2× TT (Toy Train) gearmotors with compatible voltage and torque specifications for differential drive. |
| **Rationale** | TT motors provide optimal balance of torque, size, cost, and availability for small rover applications. |
| **Acceptance Criteria** | • Motors operate within 3-6V input range<br>• Gearbox provides minimum 1:48 reduction ratio<br>• Output shaft compatible with standard wheel/sprocket hubs (5mm D-shaft or cross)<br>• Motor mounting holes match standard TT bracket (26mm spacing) |
| **Verification Method** | T (Voltage test), I (Mechanical fit check), A (Datasheet review) |
| **Dependencies** | REQ-MECH-008, REQ-MECH-009, REQ-MECH-010 |
| **Status** | Approved |

**Motor Specifications**:
- **Type**: TT gearmotor (DC brushed)
- **Voltage**: 3-6V DC nominal
- **Gearbox**: 1:48 or 1:120 reduction (higher for heavy loads)
- **Speed**: 90-200 RPM at 6V (depending on gear ratio)
- **Torque**: > 1 kg·cm at stall
- **Shaft**: 5mm D-shaft or 3mm cross shaft
- **Mounting**: Standard TT bracket with M3 screw holes (26mm spacing)
- **Quantity**: 2 (left and right independent control)

---

#### REQ-MECH-008: Differential Drive Mechanism

| Field | Value |
|-------|-------|
| **Priority** | Critical |
| **Type** | Functional |
| **Description** | The rover shall implement tank-style steering using independent left and right motor control for forward, reverse, and turning maneuvers. |
| **Rationale** | Differential drive enables zero-radius turning, simple mechanical design, and intuitive control mapping. |
| **Acceptance Criteria** | • Independent motor control verified via firmware<br>• Forward/reverse operation confirmed<br>• Turning radius ≤ rover length/2 (zero-radius capable)<br>• Speed control via PWM (variable throttle) |
| **Verification Method** | D (Operational demonstration), T (Turning radius measurement) |
| **Dependencies** | REQ-MECH-007, REQ-MECH-010 |
| **Status** | Approved |

**Steering Behavior**:
| Maneuver | Left Motor | Right Motor | Result |
|----------|------------|-------------|--------|
| Forward | +100% | +100% | Straight ahead |
| Reverse | -100% | -100% | Straight back |
| Turn Left (gentle) | +50% | +100% | Arc turn left |
| Turn Right (sharp) | +100% | -50% | Sharp turn right |
| Pivot Left | -100% | +100% | Zero-radius rotation CCW |
| Pivot Right | +100% | -100% | Zero-radius rotation CW |

---

#### REQ-MECH-009: Motor Mounting System

| Field | Value |
|-------|-------|
| **Priority** | High |
| **Type** | Non-Functional |
| **Description** | Motor mounts shall provide secure, aligned retention with vibration dampening and ease of maintenance. |
| **Rationale** | Proper motor alignment ensures efficient power transfer, reduces noise/vibration, and prevents premature wear. |
| **Acceptance Criteria** | • Motor alignment tolerance: ±1° to track/wheel axis<br>• Motor remains secured during vibration test<br>• Motor removal/installation time < 2 minutes<br>• Optional rubber isolation reduces vibration transmission |
| **Verification Method** | T (Alignment measurement), T (Vibration test), D (Removal demonstration) |
| **Dependencies** | REQ-MECH-007, REQ-MECH-010 |
| **Status** | Approved |

**Design Features**:
- 3D-printed motor brackets with M3 screw retention
- Alignment pins or keyed slots for repeatable positioning
- Optional rubber grommets for vibration isolation
- Cable management clips integrated into mount
- Modular design for easy motor replacement

---

#### REQ-MECH-010: Wheel/Track System

| Field | Value |
|-------|-------|
| **Priority** | High |
| **Type** | Functional |
| **Description** | The rover shall use either wheels or continuous tracks driven by the TT motors for ground mobility. |
| **Rationale** | Tracks provide better traction, terrain handling, and FPV aesthetics. Wheels offer simplicity and lower friction. |
| **Acceptance Criteria** | • Drive system provides forward/reverse motion<br>• Ground contact sufficient for stability (no tipping during turns)<br>• Compatible with TT motor 5mm shaft<br>• Traction adequate for 20° incline (if using tracks) |
| **Verification Method** | T (Mobility test), T (Incline test), D (Operational demonstration) |
| **Dependencies** | REQ-MECH-007, REQ-MECH-008, REQ-MECH-015 |
| **Status** | Approved |

**Option 1: Wheels**
- Type: 3D-printed or rubber
- Diameter: 40-60mm
- Hub: D-shaft or hex adapter for TT motor shaft
- Traction: Rubber O-ring or TPU tread
- Quantity: 2 driven + 1 passive caster

**Option 2: Continuous Tracks** *(Recommended)*
- Type: 3D-printed links or rubber belt
- Drive sprocket: Mounts to motor D-shaft
- Idler wheels: Front/rear tensioning
- Ground contact length: > 80mm for stability
- Track width: 15-20mm

---

### 4.5 Electronics Integration Requirements

#### REQ-MECH-011: ESP32 Module Mounting

| Field | Value |
|-------|-------|
| **Priority** | Critical |
| **Type** | Interface |
| **Description** | The chassis shall provide secure mounting for ESP32-CAM or TTGO T-Display with proper camera orientation, antenna clearance, and thermal management. |
| **Rationale** | Module positioning affects WiFi range, camera field-of-view, and component lifespan (thermal). |
| **Acceptance Criteria** | • Camera angle adjustable 0-30° tilt (ESP32-CAM only)<br>• Antenna clearance ≥ 10mm from metal/carbon fiber<br>• Ventilation airflow > 0.1 m/s across module<br>• Module secure during vibration test |
| **Verification Method** | I (Clearance measurement), T (Thermal test), T (Vibration test) |
| **Dependencies** | REQ-MECH-002, REQ-MECH-018 |
| **Status** | Approved |

**Design Considerations**:
- Forward-facing camera with adjustable tilt bracket
- Antenna positioned away from metallic components
- Ventilation slots near ESP32 for heat dissipation
- Optional clear acrylic camera lens protector

---

#### REQ-MECH-012: Motor Driver Mounting

| Field | Value |
|-------|-------|
| **Priority** | High |
| **Type** | Interface |
| **Description** | Motor driver board shall be centrally mounted with adequate cooling and short wire runs to motors. |
| **Rationale** | Central placement minimizes wire length (reduces voltage drop), and cooling prevents thermal shutdown. |
| **Acceptance Criteria** | • Motor wire length < 150mm (reduces IR drop)<br>• Heatsink exposed to airflow (not blocked by chassis)<br>• Board temperature < 70°C during continuous 80% load<br>• Terminals labeled or color-coded |
| **Verification Method** | T (Thermal imaging), I (Wire length check), I (Labeling check) |
| **Dependencies** | REQ-MECH-002, REQ-MECH-018 |
| **Status** | Approved |

**Recommended Drivers**:
| IC | Channels | Current | Logic | Pros | Cons |
|----|----------|---------|-------|------|------|
| L298N | 2 | 2A | 5V | Cheap, robust | Large, inefficient |
| TB6612FNG | 2 | 1.2A | 3.3V/5V | Compact, efficient | Lower current |
| DRV8833 | 2 | 1.5A | 3.3V | Small, efficient | Requires PCB |

---

#### REQ-MECH-013: Voltage Regulator Integration

| Field | Value |
|-------|-------|
| **Priority** | Critical |
| **Type** | Interface |
| **Description** | Chassis shall accommodate voltage regulator(s) to convert battery voltage to 5V and 3.3V rails for electronics. |
| **Rationale** | ESP32 requires stable 3.3V, peripherals may need 5V. Battery voltage varies 6.0-8.4V (2S config). |
| **Acceptance Criteria** | • 5V rail capable of ≥ 2A output (ESP32 + camera + servos)<br>• 3.3V rail capable of ≥ 1A output (ESP32 core)<br>• Voltage ripple < 50mV pk-pk<br>• Thermal shutdown functional |
| **Verification Method** | T (Load test), T (Ripple measurement), T (Thermal shutdown test) |
| **Dependencies** | REQ-MECH-004, REQ-MECH-011 |
| **Status** | Approved |

**Regulator Options**:
- **Buck Converter**: LM2596, XL4015 (efficient for 7.4V → 5V)
- **Linear Regulator**: AMS1117-3.3 (for 5V → 3.3V only)
- **Dual Output**: Combine buck (battery → 5V) + LDO (5V → 3.3V)

---

### 4.6 Dimensional & Manufacturing Requirements

#### REQ-MECH-014: 3D Printer Compatibility

| Field | Value |
|-------|-------|
| **Priority** | High |
| **Type** | Constraint |
| **Description** | All chassis components shall be printable on standard FDM 3D printers with 200mm × 200mm × 200mm build volume. |
| **Rationale** | Ensures accessibility for makers using common printers (Ender 3, Prusa Mini, etc.). |
| **Acceptance Criteria** | • All STL files fit within 200mm × 200mm × 200mm envelope<br>• Parts require no support material (or minimal, removable)<br>• Snap-fit tolerances: 0.2-0.3mm clearance<br>• Successful print on ≥ 3 different printer models |
| **Verification Method** | A (CAD bounding box check), T (Multi-printer test) |
| **Dependencies** | REQ-MECH-001, REQ-MECH-020 |
| **Status** | Approved |

**Print Settings (Recommended)**:
- Material: PLA or PETG
- Layer height: 0.2mm
- Wall thickness: 4 perimeters (2mm wall)
- Infill: 20% gyroid
- Print speed: 50 mm/s
- Bed temperature: 60°C (PLA), 70°C (PETG)

---

#### REQ-MECH-015: Overall Dimensions

| Field | Value |
|-------|-------|
| **Priority** | Medium |
| **Type** | Constraint |
| **Description** | Rover shall meet compact size targets while housing all components and maintaining ground clearance. |
| **Rationale** | Compactness improves maneuverability and aesthetics. Ground clearance prevents obstacles from blocking movement. |
| **Acceptance Criteria** | • Length: 120-150mm (excluding antenna)<br>• Width: 80-100mm (track width)<br>• Height: 60-80mm (top of chassis)<br>• Ground clearance: ≥ 10mm |
| **Verification Method** | I (Caliper measurement), T (Obstacle traversal test) |
| **Dependencies** | REQ-MECH-001, REQ-MECH-010 |
| **Status** | Approved |

---

#### REQ-MECH-016: Weight Budget

| Field | Value |
|-------|-------|
| **Priority** | Medium |
| **Type** | Constraint |
| **Description** | Total rover weight shall not exceed 300g to maintain motor efficiency and handling. |
| **Rationale** | Exceeding weight budget reduces runtime, motor torque headroom, and increases wear. |
| **Acceptance Criteria** | • Fully assembled rover (with batteries) ≤ 300g<br>• Component weights match budget breakdown ±10% |
| **Verification Method** | T (Scale measurement), A (Component weight summation) |
| **Dependencies** | REQ-MECH-001, REQ-MECH-007 |
| **Status** | Approved |

**Weight Breakdown**:
| Component | Target Weight | Tolerance |
|-----------|---------------|-----------|
| Chassis (printed parts) | 80g | ±10g |
| Electronics (ESP32, drivers, regulators) | 40g | ±5g |
| Motors (2× TT) | 30g | ±3g |
| Batteries (2× 18650) | 90g | ±5g |
| Wheels/Tracks | 30g | ±5g |
| **Margin** | 30g | - |
| **Total** | **300g** | - |

---

### 4.7 Safety & Durability Requirements

#### REQ-MECH-017: Impact Protection

| Field | Value |
|-------|-------|
| **Priority** | High |
| **Type** | Safety |
| **Description** | Chassis shall protect batteries and electronics from impact damage during crashes, drops, and collisions. |
| **Rationale** | Lithium-ion batteries can ignite if punctured. Electronics are fragile and costly to replace. |
| **Acceptance Criteria** | • Passes 1m drop test onto hard surface without battery/PCB damage<br>• Battery compartment has crush-resistant walls (≥ 3mm thick)<br>• No sharp edges or puncture hazards near batteries |
| **Verification Method** | T (Drop test), I (Wall thickness check), I (Edge inspection) |
| **Dependencies** | REQ-MECH-001, REQ-MECH-005, REQ-MECH-019 |
| **Status** | Approved |

**Protection Features**:
- Battery guard: Reinforced enclosure walls around cells
- Electronics shield: Top cover or plate over PCBs
- Motor protection: Mesh or grille to prevent debris ingestion
- Bumper (optional): Front collision absorber

---

#### REQ-MECH-018: Thermal Management

| Field | Value |
|-------|-------|
| **Priority** | High |
| **Type** | Non-Functional |
| **Description** | Chassis shall provide adequate ventilation and heat dissipation for all heat-generating components. |
| **Rationale** | ESP32, motor driver, and voltage regulators generate heat. Inadequate cooling causes throttling or failure. |
| **Acceptance Criteria** | • Component temperatures < 70°C during 30-minute continuous operation<br>• Ventilation slots sized for ≥ 0.1 m/s airflow<br>• Component spacing ≥ 5mm (no thermal coupling)<br>• Heat-resistant material near motors (PLA+, PETG, or ABS) |
| **Verification Method** | T (Thermal imaging), I (Spacing measurement), I (Material check) |
| **Dependencies** | REQ-MECH-011, REQ-MECH-012, REQ-MECH-013 |
| **Status** | Approved |

**Cooling Strategies**:
- Ventilation slots: 3mm × 20mm slits on enclosure sides
- Component spacing: Minimum 5mm air gap between boards
- Material selection: PETG for motor mounts (higher heat resistance than PLA)
- Optional: Passive heatsinks on voltage regulators

---

#### REQ-MECH-019: Electrical Safety

| Field | Value |
|-------|-------|
| **Priority** | Critical |
| **Type** | Safety |
| **Description** | Chassis design shall prevent electrical shorts, user shock hazards, and battery damage. |
| **Rationale** | Short circuits can cause fires. User safety is paramount. Battery damage reduces lifespan and creates hazards. |
| **Acceptance Criteria** | • No exposed high-current traces or battery terminals<br>• Physical barriers prevent battery terminal contact<br>• Insulated battery holder material (ABS, PLA, PETG)<br>• Optional fuse protection functional (if implemented)<br>• Warning labels present on battery compartment |
| **Verification Method** | I (Visual inspection), T (Short circuit test), I (Label check) |
| **Dependencies** | REQ-MECH-005, REQ-MECH-006 |
| **Status** | Approved |

**Safety Features**:
- Insulated battery holder (non-conductive plastic)
- Physical barriers between + and - terminals
- Recessed terminals (not user-accessible)
- Optional: Fuse on battery + terminal (2A-3A rated)
- Warning label: "Li-ion battery – handle with care"

---

### 4.8 Assembly & Maintenance Requirements

#### REQ-MECH-020: Tool-Free Snap-Fit Assembly

| Field | Value |
|-------|-------|
| **Priority** | Medium |
| **Type** | Non-Functional |
| **Description** | Major enclosure components shall assemble via snap-fit joints without tools. |
| **Rationale** | Reduces assembly complexity, enables rapid prototyping iterations, and improves user experience. |
| **Acceptance Criteria** | • All major enclosure parts join via snap-fits (no screws for top cover)<br>• Snap tolerance: ±0.2mm for reliable engagement<br>• Snap joints survive ≥ 10 assembly/disassembly cycles<br>• Assembly force < 20N (hand pressure only) |
| **Verification Method** | D (Assembly demonstration), T (Cycle test), T (Force measurement) |
| **Dependencies** | REQ-MECH-001, REQ-MECH-014 |
| **Status** | Approved |

**Design Guidelines**:
- Snap-fit types: Cantilever beam, torsional, or annular
- Clearance: 0.2-0.3mm for PLA (adjust for material)
- Chamfers: 45° lead-in for easy engagement
- Reinforcement: Ribs to prevent snap breakage

---

#### REQ-MECH-021: Maintenance Accessibility

| Field | Value |
|-------|-------|
| **Priority** | Medium |
| **Type** | Non-Functional |
| **Description** | Chassis design shall enable rapid access to electronics, batteries, and wiring for troubleshooting and repairs. |
| **Rationale** | Reduces maintenance time, encourages experimentation, and lowers barrier to hardware debugging. |
| **Acceptance Criteria** | • Full electronics access via single top cover removal<br>• Battery swap time < 30 seconds<br>• Wire connections color-coded or labeled<br>• Individual components replaceable without full disassembly |
| **Verification Method** | D (Maintenance demonstration), T (Timed battery swap) |
| **Dependencies** | REQ-MECH-003, REQ-MECH-020 |
| **Status** | Approved |

**Maintenance Features**:
- Top cover: Single snap-fit panel for full access
- Battery compartment: Side-loading with spring retention
- Wire labeling: Silkscreen or labels on chassis
- Modular components: Boards mounted with standoffs

---

#### REQ-MECH-022: Documentation Package

| Field | Value |
|-------|-------|
| **Priority** | Low |
| **Type** | Non-Functional |
| **Description** | Mechanical design shall be accompanied by comprehensive assembly instructions, BOM, and CAD files. |
| **Rationale** | Enables replication, modification, and community contributions. |
| **Acceptance Criteria** | • Assembly guide with step-by-step photos/diagrams<br>• BOM with part numbers and supplier links<br>• STL files organized by print order<br>• Recommended slicer settings documented |
| **Verification Method** | I (Documentation review), D (Third-party assembly test) |
| **Dependencies** | All requirements |
| **Status** | Approved |

**Documentation Deliverables**:
1. Assembly guide (PDF with photos)
2. Bill of Materials (CSV with links)
3. STL files (organized by assembly step)
4. Print settings guide (markdown)
5. CAD source files (STEP format for modification)

---

### 4.9 Optional Features

#### REQ-MECH-023: Expansion Mounting Points

| Field | Value |
|-------|-------|
| **Priority** | Low |
| **Type** | Interface (Optional) |
| **Description** | Chassis may include optional mounting provisions for future accessories and sensors. |
| **Rationale** | Enables experimentation and feature upgrades without chassis redesign. |
| **Acceptance Criteria** | • Mounting points for servos, sensors, and accessories<br>• Standard hole patterns (e.g., M3 grid)<br>• Does not compromise primary requirements |
| **Verification Method** | I (Mounting point check), D (Accessory fitment) |
| **Dependencies** | None (optional) |
| **Status** | Proposed |

**Optional Mounting Provisions**:
- Servo mount for pan/tilt mechanism (2× MG90S)
- Ultrasonic sensor bracket (HC-SR04)
- LED strip mounting channels
- Bumper switch mounts (mechanical collision detection)
- Cargo bed (10g payload capacity)

---

#### REQ-MECH-024: Customization Features

| Field | Value |
|-------|-------|
| **Priority** | Low |
| **Type** | Non-Functional (Optional) |
| **Description** | Chassis design may incorporate features for aesthetic customization and personalization. |
| **Rationale** | Enhances user engagement and product differentiation. |
| **Acceptance Criteria** | • Recessed areas for logos/labels<br>• Multi-material print compatibility<br>• Accessory rails for add-ons |
| **Verification Method** | I (Visual inspection), D (Customization demonstration) |
| **Dependencies** | None (optional) |
| **Status** | Proposed |

**Customization Options**:
- Color coding: Parts designed for multi-material printing (e.g., Prusa MMU)
- Logo recess: 0.5mm deep area for vinyl stickers
- Accessory rails: T-slot or M3 grid for attachments

---

### 4.10 Verification Requirements

#### REQ-MECH-025: Prototype Testing

| Field | Value |
|-------|-------|
| **Priority** | Critical |
| **Type** | Non-Functional |
| **Description** | First article prototype shall undergo comprehensive testing to validate all mechanical requirements. |
| **Rationale** | Early validation prevents costly redesigns and ensures production readiness. |
| **Acceptance Criteria** | • All tests in validation matrix pass<br>• Test reports documented<br>• Non-conformances tracked and resolved |
| **Verification Method** | T (All tests), I (Documentation review) |
| **Dependencies** | All requirements |
| **Status** | Approved |

**Validation Test Matrix**: (See Section 6)

---

## 5. Traceability Matrix

| Requirement | Dependency | Verification Method | Priority | Status |
|-------------|-----------|---------------------|----------|--------|
| REQ-MECH-001 | REQ-MECH-002, 016 | T (Drop test, Load test) | Critical | Approved |
| REQ-MECH-002 | REQ-MECH-011, 012, 013 | T (Vibration, Tilt) | Critical | Approved |
| REQ-MECH-003 | REQ-MECH-021 | I, D | Medium | Approved |
| REQ-MECH-004 | REQ-MECH-005, 006, 013 | T, I | Critical | Approved |
| REQ-MECH-005 | REQ-MECH-004, 019 | T, D | High | Approved |
| REQ-MECH-006 | REQ-MECH-004, 019 | T, I | High | Approved |
| REQ-MECH-007 | REQ-MECH-008, 009, 010 | T, I, A | Critical | Approved |
| REQ-MECH-008 | REQ-MECH-007, 010 | D, T | Critical | Approved |
| REQ-MECH-009 | REQ-MECH-007, 010 | T, D | High | Approved |
| REQ-MECH-010 | REQ-MECH-007, 008, 015 | T, D | High | Approved |
| REQ-MECH-011 | REQ-MECH-002, 018 | I, T | Critical | Approved |
| REQ-MECH-012 | REQ-MECH-002, 018 | T, I | High | Approved |
| REQ-MECH-013 | REQ-MECH-004, 011 | T | Critical | Approved |
| REQ-MECH-014 | REQ-MECH-001, 020 | A, T | High | Approved |
| REQ-MECH-015 | REQ-MECH-001, 010 | I, T | Medium | Approved |
| REQ-MECH-016 | REQ-MECH-001, 007 | T, A | Medium | Approved |
| REQ-MECH-017 | REQ-MECH-001, 005, 019 | T, I | High | Approved |
| REQ-MECH-018 | REQ-MECH-011, 012, 013 | T, I | High | Approved |
| REQ-MECH-019 | REQ-MECH-005, 006 | I, T | Critical | Approved |
| REQ-MECH-020 | REQ-MECH-001, 014 | D, T | Medium | Approved |
| REQ-MECH-021 | REQ-MECH-003, 020 | D, T | Medium | Approved |
| REQ-MECH-022 | All | I, D | Low | Approved |
| REQ-MECH-023 | None | I, D | Low | Proposed |
| REQ-MECH-024 | None | I, D | Low | Proposed |
| REQ-MECH-025 | All | T, I | Critical | Approved |

---

## 6. Verification Methods

### 6.1 Verification Method Definitions

| Code | Method | Description |
|------|--------|-------------|
| **I** | Inspection | Visual, dimensional, or sensory examination |
| **T** | Test | Quantitative measurement via instruments or procedures |
| **A** | Analysis | Mathematical modeling, simulation, or calculation |
| **D** | Demonstration | Functional proof-of-concept under observation |

### 6.2 Test Procedures

#### Test Procedure: Drop Test (REQ-MECH-001, 017)

**Purpose**: Validate structural integrity and impact protection

**Equipment**:
- Rigid surface (concrete or steel plate)
- Measuring tape (1m)
- Camera (slow-motion capture)

**Procedure**:
1. Fully assemble rover with batteries installed
2. Orient rover horizontally (normal operating position)
3. Release from 1m height onto rigid surface
4. Repeat for 6 orientations (top, bottom, left, right, front, rear)
5. Inspect for cracks, deformation, battery damage, PCB damage

**Pass Criteria**:
- No structural cracks or permanent deformation
- Batteries remain seated and undamaged
- Electronics functional after test (power-on test)

---

#### Test Procedure: Vibration Test (REQ-MECH-002, 005, 009)

**Purpose**: Validate component retention during operational vibration

**Equipment**:
- Vibration platform or handheld oscillator
- Accelerometer (optional)
- Timer

**Procedure**:
1. Mount fully assembled rover to vibration platform
2. Apply 10Hz sinusoidal vibration at 2G amplitude
3. Run test for 5 minutes
4. Inspect all components for loosening or detachment

**Pass Criteria**:
- All components remain securely mounted
- No visible movement of batteries, PCBs, or motors
- Electrical connections intact (continuity test)

---

#### Test Procedure: Thermal Test (REQ-MECH-018)

**Purpose**: Validate thermal management and cooling

**Equipment**:
- Thermal camera or IR thermometer
- Timer

**Procedure**:
1. Fully assemble rover with electronics powered
2. Run motors at 80% duty cycle continuously
3. Monitor component temperatures for 30 minutes
4. Record maximum temperatures

**Pass Criteria**:
- ESP32: < 70°C
- Motor driver: < 70°C
- Voltage regulator: < 70°C
- Battery: < 45°C

---

#### Test Procedure: Battery Retention Force (REQ-MECH-005)

**Purpose**: Quantify battery retention force

**Equipment**:
- Spring scale (0-50N range)
- Battery dummy (same dimensions as 18650)

**Procedure**:
1. Insert battery into holder
2. Attach spring scale to battery end
3. Pull until battery releases
4. Record force

**Pass Criteria**:
- Retention force > 5N per cell

---

#### Test Procedure: Snap-Fit Cycle Test (REQ-MECH-020)

**Purpose**: Validate snap-fit durability

**Equipment**:
- Snap-fit test fixture (optional)
- Counter

**Procedure**:
1. Assemble and disassemble snap-fit joints
2. Repeat for 10 cycles
3. Inspect for cracks, permanent deformation, or engagement failure

**Pass Criteria**:
- Snaps engage reliably on all 10 cycles
- No visible cracks or breakage
- Engagement force remains consistent (±20%)

---

### 6.3 Validation Test Matrix

| Test ID | Requirement | Description | Expected Result | Status |
|---------|-------------|-------------|-----------------|--------|
| TV-001 | REQ-MECH-001 | 1m drop test (6 orientations) | No structural damage | Pending |
| TV-002 | REQ-MECH-001 | 500g static load deflection | < 2mm deflection | Pending |
| TV-003 | REQ-MECH-002 | Vibration retention test | All components secure | Pending |
| TV-004 | REQ-MECH-005 | Battery retention force | > 5N per cell | Pending |
| TV-005 | REQ-MECH-006 | USB-C charging functional | Battery charges, LEDs indicate status | Pending |
| TV-006 | REQ-MECH-008 | Differential drive operation | Forward, reverse, turn, pivot confirmed | Pending |
| TV-007 | REQ-MECH-010 | Incline traversal (tracks) | Climbs 20° incline | Pending |
| TV-008 | REQ-MECH-016 | Total weight measurement | ≤ 300g | Pending |
| TV-009 | REQ-MECH-017 | Drop test (impact protection) | Batteries/electronics undamaged | Pending |
| TV-010 | REQ-MECH-018 | Thermal endurance test | Components < 70°C after 30 min | Pending |
| TV-011 | REQ-MECH-020 | Snap-fit cycle test | 10 cycles without failure | Pending |
| TV-012 | REQ-MECH-021 | Battery swap time | < 30 seconds | Pending |

---

## 7. References

### 7.1 Related Documents

| Document ID | Title | Version |
|-------------|-------|---------|
| SRS-ESP32-ROVER-001 | Software Requirements Specification | 1.1 |
| TDS-ESP32-ROVER-001 | Technical Design Specification | Draft |
| UM-ESP32-ROVER-001 | User Manual | Draft |

### 7.2 Standards & Guidelines

- IEEE 29148-2018: Systems and software engineering — Life cycle processes — Requirements engineering
- ISO 10303-242: STEP AP242 for 3D CAD data exchange
- UL 2054: Standard for Household and Commercial Batteries

### 7.3 Component Datasheets

- TT Motor: Generic TT Gearmotor Datasheet
- 18650 Cell: Samsung INR18650-30Q Datasheet
- TP4056: Charging IC Datasheet
- ESP32-CAM: AI-Thinker Module Datasheet
- TTGO T-Display: LilyGO Product Page

---

## 8. Appendices

### Appendix A: Glossary

| Term | Definition |
|------|------------|
| **FPV** | First-Person View (camera streaming) |
| **TT Motor** | Toy Train gearmotor (standardized DC motor form factor) |
| **1S2P** | 1 Series, 2 Parallel (battery configuration) |
| **2S1P** | 2 Series, 1 Parallel (battery configuration) |
| **FDM** | Fused Deposition Modeling (3D printing technology) |
| **BOM** | Bill of Materials |
| **Snap-Fit** | Mechanical joint using elastic deformation for assembly |
| **Differential Drive** | Tank-style steering via independent left/right motor control |

### Appendix B: Risk Register

| Risk ID | Description | Probability | Impact | Mitigation |
|---------|-------------|-------------|--------|------------|
| R-001 | Snap-fits break during assembly | Medium | High | Add chamfers, test cycle life, provide spare parts |
| R-002 | Battery overheating during charge | Low | Critical | Use TP4056 with thermal shutdown, add ventilation |
| R-003 | Motor driver thermal shutdown | Medium | Medium | Add heatsink, reduce duty cycle limit, improve airflow |
| R-004 | Chassis warping during printing | Medium | Medium | Tune print settings, add brims, use PETG |
| R-005 | Weight exceeds 300g budget | Low | Medium | Optimize wall thickness, reduce infill, lightweight components |

### Appendix C: Future Enhancements

**Potential Upgrades** (not currently required):

1. **Suspension System**: Independent wheel suspension for rough terrain
2. **Water Resistance**: IP65-rated sealing (gaskets, sealed ports)
3. **Modular Track System**: Swappable track lengths for different terrains
4. **Onboard Lighting**: LED headlights/taillights for night operation
5. **Sensor Integration**: GPS, IMU, LiDAR mounting provisions
6. **FPV Antenna Tracker**: Pan/tilt mount for directional antenna

---

**END OF DOCUMENT**
