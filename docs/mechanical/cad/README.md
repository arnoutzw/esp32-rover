# ESP32 Rover CAD Files

## Overview

This directory contains OpenSCAD parametric CAD files for the ESP32 Rover mechanical components.

## File List

| File | Description | Print Qty |
|------|-------------|-----------|
| `lower_chassis.scad` | Lower deck - battery and drivetrain | 1 |
| `upper_chassis.scad` | Upper deck - electronics | 1 |
| `motor_mount.scad` | Sliding mount for A2212 motor | 1 |
| `steering_knuckle.scad` | Front steering knuckle | 2 (mirror) |
| `camera_bracket.scad` | 45° ESP32-CAM mount | 1 |
| `wheel_hub.scad` | Wheel hub adapter | 4 |
| `differential_housing.scad` | Rear differential housing | 1 |

## Generating STL Files

### Using OpenSCAD CLI

```bash
# Generate all STL files
for f in *.scad; do
    openscad -o "${f%.scad}.stl" "$f"
done
```

### Using OpenSCAD GUI

1. Open the `.scad` file
2. Press F5 to preview
3. Press F6 to render
4. File → Export → Export as STL

## Print Settings

### Recommended Settings (PETG/ABS)

| Part | Layer Height | Infill | Supports |
|------|--------------|--------|----------|
| Lower Chassis | 0.2mm | 40% | No |
| Upper Chassis | 0.2mm | 30% | No |
| Motor Mount | 0.2mm | 60% | No |
| Steering Knuckle | 0.16mm | 80% | Yes |
| Camera Bracket | 0.2mm | 30% | Yes |
| Wheel Hub | 0.16mm | 60% | No |
| Differential | 0.16mm | 50% | Yes |

### Material Recommendations

- **Chassis plates**: PETG or ABS for durability
- **Motor mount**: ABS preferred (heat resistance)
- **Steering knuckles**: PETG at high infill (strength critical)
- **Camera bracket**: PLA acceptable (low stress)
- **Wheel hubs**: PETG or ABS (impact resistance)
- **Differential**: PETG or ABS (gear mesh critical)

## Customization

All files use parametric design. Key variables are defined at the top of each file.

### Common Modifications

**Change wheelbase**:
Edit `lower_chassis.scad`:
```openscad
chassis_length = 180;  // Increase from 160mm
```

**Different motor**:
Edit `motor_mount.scad`:
```openscad
motor_diameter = 35;  // For larger motor
mount_hole_spacing = 25;  // Different hole pattern
```

**Different battery**:
Edit `lower_chassis.scad`:
```openscad
battery_length = 75;  // For longer cells
battery_width = 50;   // For wider pack
```

## Assembly Notes

1. Print steering knuckles as mirrored pair
2. Differential housing can be split for easier printing
3. Test fit all parts before final assembly
4. Use M3 heat-set inserts for stronger threads

## Dependencies

- OpenSCAD 2021.01 or later
- No external libraries required
