# ESP32 Rover Firmware - Build System Guide

This guide explains the build system for the ESP32 Rover firmware, including setup, building, flashing, and troubleshooting.

---

## Table of Contents

1. [Overview](#overview)
2. [First-Time Setup](#first-time-setup)
3. [Building Firmware](#building-firmware)
   - [Build Commands](#build-commands)
   - [Build Targets](#build-targets)
   - [Clean Builds](#clean-builds)
4. [Flashing Firmware](#flashing-firmware)
   - [USB Serial Flashing](#usb-serial-flashing)
   - [OTA Flashing](#ota-flashing)
   - [JTAG Flashing](#jtag-flashing)
5. [Binary Archive System](#binary-archive-system)
6. [Configuration System](#configuration-system)
7. [Build Verification](#build-verification)
8. [Troubleshooting](#troubleshooting)
9. [Advanced Topics](#advanced-topics)

---

## Overview

The ESP32 Rover firmware uses a self-contained build system based on ESP-IDF v5.2.2. The build system:

- Supports two hardware targets: **ESP32-CAM** and **TTGO T-Display**
- Automatically archives all builds with git metadata
- Enforces clean git state before building
- Runs unit tests before every build
- Generates configuration from YAML files

### Directory Structure

```
esp32-rover-firmware/
├── firmware/
│   ├── esp-idf/              # Embedded ESP-IDF v5.2.2
│   ├── main/                 # Main application code
│   ├── components/           # Custom components
│   ├── build/                # Build output (generated)
│   └── sdkconfig.defaults.*  # Target-specific configurations
├── scripts/
│   ├── setup.sh              # First-time tool installation
│   ├── build.sh              # Main build script
│   ├── ota.sh                # OTA flashing script
│   ├── jtag-flash.sh         # JTAG flashing script
│   └── generate_config.py    # Configuration generator
├── binaries/
│   ├── esp32cam/             # Archived ESP32-CAM builds
│   └── ttgo/                 # Archived TTGO builds
├── config/
│   ├── rover_config.yaml     # Main configuration
│   └── secrets.yaml          # WiFi passwords, API keys (gitignored)
└── test/                     # Unit tests
```

---

## First-Time Setup

Before building for the first time, you need to install the ESP-IDF toolchain.

### Prerequisites

- Python 3.8 or later
- Git
- macOS, Linux, or Windows with WSL

### Installation

Run the setup script once:

```bash
./scripts/setup.sh
```

This installs:
- Xtensa toolchain for ESP32
- Python virtual environment with ESP-IDF tools
- Required Python packages

The setup takes approximately 5-10 minutes on first run.

### Verification

After setup, verify the installation:

```bash
./scripts/build.sh ttgo
```

If the build completes successfully, the setup is correct.

---

## Building Firmware

### Build Commands

The main build script is `./scripts/build.sh`. All builds should go through this script.

```bash
# Basic syntax
./scripts/build.sh <target> [command] [options]
```

**Available commands:**

| Command | Description |
|---------|-------------|
| `build` | Build the project (default) |
| `flash` | Build and flash via USB serial |
| `monitor` | Open serial monitor |
| `clean` | Clean build directory |
| `fullclean` | Full clean (removes sdkconfig) |
| `menuconfig` | Open ESP-IDF configuration menu |

### Build Targets

The firmware supports two hardware targets:

| Target | Hardware | Features |
|--------|----------|----------|
| `esp32cam` | ESP32-CAM AI-Thinker | Camera (OV2640), LED flash, no LCD |
| `ttgo` | TTGO T-Display | LCD display (135x240), buttons, no camera |

**Examples:**

```bash
# Build for TTGO T-Display
./scripts/build.sh ttgo

# Build for ESP32-CAM
./scripts/build.sh esp32cam

# Build and flash TTGO via USB
./scripts/build.sh ttgo flash

# Build and flash to specific USB port
./scripts/build.sh esp32cam flash -p /dev/ttyUSB0
```

### Clean Builds

**IMPORTANT:** Always use clean builds before flashing to prevent stale build artifacts.

```bash
# Recommended: Clean build
./scripts/build.sh ttgo clean && ./scripts/build.sh ttgo

# Full clean (when switching targets or after SDK changes)
./scripts/build.sh ttgo fullclean && ./scripts/build.sh ttgo
```

**Why clean builds are important:**

The build system caches the git commit hash during compilation. Incremental builds may not regenerate this, causing the firmware to report an incorrect version. This makes debugging extremely difficult.

---

## Flashing Firmware

### USB Serial Flashing

Direct USB connection to the device's serial port.

```bash
# Build and flash
./scripts/build.sh ttgo flash

# Flash to specific port
./scripts/build.sh ttgo flash -p /dev/tty.usbserial-0001

# Flash without building (use existing binary)
cd firmware && idf.py flash -p /dev/tty.usbserial-0001
```

**Common serial ports:**
- macOS: `/dev/tty.usbserial-*` or `/dev/cu.usbserial-*`
- Linux: `/dev/ttyUSB0` or `/dev/ttyACM0`
- Windows: `COM3`, `COM4`, etc.

### OTA Flashing

Over-The-Air flashing via WiFi. Requires the device to be running and connected to the network.

```bash
# Set OTA password (required)
export OTA_PASSWORD=your_password

# Flash to TTGO via mDNS name
./scripts/ota.sh ttgo

# Flash to ESP32-CAM via mDNS name
./scripts/ota.sh esp32cam

# Flash to specific IP address
./scripts/ota.sh ttgo 192.168.1.100

# List available archived binaries
./scripts/ota.sh ttgo --list-binaries

# Flash specific archived binary
./scripts/ota.sh ttgo --binary-file esp32-rover_ttgo_20260125_214954_77b4f74.bin
```

**Default mDNS hostnames:**
- TTGO: `ttgo-rover.local`
- ESP32-CAM: `esp32-rover.local`

**OTA considerations:**
- ESP32-CAM uses rate limiting (50 KB/s) to prevent memory issues
- OTA updates take 20-30 seconds for TTGO, 2-3 minutes for ESP32-CAM
- Device automatically reboots after successful update

### JTAG Flashing

For development and debugging with an ESP-PROG or similar JTAG adapter.

```bash
# Flash latest binary for target
./scripts/jtag-flash.sh ttgo
./scripts/jtag-flash.sh esp32cam

# Flash specific binary file
./scripts/jtag-flash.sh ttgo path/to/firmware.bin
```

See [ESP-PROG JTAG Guide](esp-prog-jtag-guide.md) for wiring and setup details.

---

## Binary Archive System

Every successful build is automatically archived with metadata.

### Archive Location

```
binaries/
├── esp32cam/
│   ├── esp32-rover_esp32cam_20260125_143022_abc1234.bin
│   ├── esp32-rover_esp32cam_20260125_143022_abc1234.sha256
│   ├── esp32-rover_esp32cam_20260125_143022_abc1234.txt
│   └── latest.bin -> esp32-rover_esp32cam_20260125_143022_abc1234.bin
└── ttgo/
    ├── esp32-rover_ttgo_20260125_214954_77b4f74.bin
    ├── esp32-rover_ttgo_20260125_214954_77b4f74.sha256
    ├── esp32-rover_ttgo_20260125_214954_77b4f74.txt
    └── latest.bin -> esp32-rover_ttgo_20260125_214954_77b4f74.bin
```

### Filename Format

```
esp32-rover_<target>_<date>_<time>_<git-hash>.bin
```

Example: `esp32-rover_ttgo_20260125_214954_77b4f74.bin`
- Target: ttgo
- Date: 2026-01-25
- Time: 21:49:54
- Git hash: 77b4f74

### Metadata Files

Each binary has accompanying metadata:

**`.txt` file:**
```
Target: ttgo
Built: 20260125_214954
Git Commit: 77b4f74
Git Branch: develop
Binary Size: 1011232 bytes
SHA256: a72aa5a6b68fd15775b097d509626e83d33c54307129401fa87aa4b63e841926
```

**`.sha256` file:**
```
a72aa5a6b68fd15775b097d509626e83d33c54307129401fa87aa4b63e841926  esp32-rover_ttgo_20260125_214954_77b4f74.bin
```

### Listing Available Binaries

```bash
./scripts/ota.sh ttgo --list-binaries
```

---

## Configuration System

The firmware uses a YAML-based configuration system.

### Configuration Files

| File | Purpose | Git tracked |
|------|---------|-------------|
| `config/rover_config.yaml` | Main configuration (WiFi mode, features, timing) | Yes |
| `config/secrets.yaml` | Passwords, API keys, sensitive data | No (gitignored) |

### Generating Configuration

Configuration is automatically regenerated during builds. To manually regenerate:

```bash
python scripts/generate_config.py
```

This creates `firmware/main/config_generated.h` with C defines.

### Configuration Options

**WiFi Mode:**
```yaml
wifi:
  mode: sta_first  # sta_first, ap_only, or sta_only
  sta:
    ssid: "YourNetwork"
    password: "..."  # In secrets.yaml
  ap:
    ssid: "ESP32-Rover"
    password: "..."  # In secrets.yaml
```

**Feature Flags:**
```yaml
features:
  rest_api: true
  mqtt: false
  camera: true      # ESP32-CAM only
  lcd_display: true # TTGO only
```

**Timing:**
```yaml
timing:
  status_update_ms: 50   # 20 Hz
  mqtt_publish_ms: 5000  # 5 seconds
  watchdog_timeout_ms: 5000
```

---

## Build Verification

### Pre-Flash Verification

Before flashing, verify the binary contains the correct git hash:

```bash
# Get expected hash
git rev-parse --short=7 HEAD
# Example output: 77b4f74

# Verify hash is in binary
strings binaries/ttgo/latest.bin | grep "77b4f74"
# Expected output:
# v2.0.3-6-g77b4f74
# 77b4f74
```

### Post-Flash Verification

After flashing, verify the device is running the correct firmware:

```bash
# Query device status
curl -s http://ttgo-rover.local/status | jq '.diag.buildFingerprint'
# Expected output: "77b4f74"

# Full version info
curl -s http://ttgo-rover.local/status | jq '.diag | {buildFingerprint, buildVersion, buildTime}'
```

---

## Troubleshooting

### Build Errors

**"Error: You have uncommitted changes"**

The build system requires clean git state. Commit your changes:
```bash
git add -A
git commit -m "Your commit message"
git push origin develop
```

**"Error: You have unpushed commits"**

Push your commits to the remote:
```bash
git push origin develop
```

**"Error: ESP-IDF not found"**

Run the setup script:
```bash
./scripts/setup.sh
```

**"Unit tests failed"**

Fix the failing tests before building. Tests run automatically before every build:
```bash
cd test && make test
```

### OTA Errors

**"Cannot reach OTA host"**

- Verify device is powered on and connected to WiFi
- Check network connectivity
- Try IP address instead of mDNS name:
  ```bash
  ./scripts/ota.sh ttgo 192.168.1.100
  ```

**"OTA_PASSWORD environment variable not set"**

Set the OTA password:
```bash
export OTA_PASSWORD=your_password
./scripts/ota.sh ttgo
```

**Device shows wrong firmware version after OTA**

This indicates stale build artifacts. Do a clean rebuild:
```bash
./scripts/build.sh ttgo clean && ./scripts/build.sh ttgo
# Verify hash before flashing
strings binaries/ttgo/latest.bin | grep $(git rev-parse --short=7 HEAD)
# Then flash
./scripts/ota.sh ttgo
```

### Serial Port Issues

**Permission denied on /dev/ttyUSB0**

Add yourself to the dialout group (Linux):
```bash
sudo usermod -a -G dialout $USER
# Log out and back in
```

**Device not recognized**

- Check USB cable (must be data cable, not charge-only)
- Install USB-to-serial drivers if needed (CH340, CP2102, FTDI)
- Try different USB port

---

## Advanced Topics

### Build Metrics

Build duration and binary sizes are logged to `build_metrics.csv`:

```csv
date,target,duration_s,size_bytes,commit
2026-01-25_21:49:54,ttgo,25,1011232,77b4f74
```

### Environment Variables

| Variable | Purpose |
|----------|---------|
| `OTA_PASSWORD` | Required for OTA flashing |
| `ALLOW_DIRTY` | Bypass clean git check (not recommended) |
| `ROVER_TARGET` | Current build target (set by build.sh) |

### Manual ESP-IDF Usage

For advanced users who want to use `idf.py` directly:

```bash
# Source ESP-IDF environment
source firmware/esp-idf/export.sh

# Navigate to firmware directory
cd firmware

# Use idf.py commands
idf.py build
idf.py flash monitor
idf.py menuconfig
```

**Note:** The build script is preferred as it handles:
- Target-specific configuration
- Binary archiving
- Build verification
- Unit test execution

### Continuous Integration

The build system supports CI/CD:

```bash
# Build without prompts
./scripts/build.sh ttgo build

# Release script (creates version tag, builds both targets)
./scripts/release.sh v2.0.4
```

---

## Quick Reference

### Common Workflows

**Daily development:**
```bash
# Make changes, commit, build, flash
git add -A && git commit -m "description" && git push
./scripts/build.sh ttgo clean && ./scripts/build.sh ttgo
strings binaries/ttgo/latest.bin | grep $(git rev-parse --short=7 HEAD)
OTA_PASSWORD=xxx ./scripts/ota.sh ttgo
```

**Testing on ESP32-CAM:**
```bash
./scripts/build.sh esp32cam clean && ./scripts/build.sh esp32cam
OTA_PASSWORD=xxx ./scripts/ota.sh esp32cam
```

**Debugging via serial:**
```bash
./scripts/build.sh ttgo flash -p /dev/tty.usbserial-0001
./scripts/build.sh ttgo monitor
```

### Script Reference

| Script | Purpose |
|--------|---------|
| `setup.sh` | Install ESP-IDF tools (run once) |
| `build.sh` | Build firmware for target |
| `ota.sh` | Flash via WiFi OTA |
| `jtag-flash.sh` | Flash via JTAG debugger |
| `generate_config.py` | Generate C config from YAML |
| `lint.sh` | Run code linting |
| `release.sh` | Create tagged release |

---

*Last updated: 2026-01-25*
