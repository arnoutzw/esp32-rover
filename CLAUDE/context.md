# ESP32 Rover Firmware - Project Context

This document provides the technical context and reference information that AI assistants need to understand and work with this codebase.

For behavioral rules and standards, see [rules.md](rules.md).

---

## Table of Contents

1. [Project Overview](#project-overview)
   - [Project Structure](#project-structure)
   - [ESP-IDF Location](#esp-idf-location)
   - [Target Configuration](#target-configuration)
   - [Key Files](#key-files)
2. [Common Commands](#common-commands)
   - [Build Commands](#build-commands)
   - [Flash Commands](#flash-commands)
   - [JTAG Flashing](#jtag-flashing)
3. [Architecture](#architecture)
   - [Component Architecture](#component-architecture)
   - [Main Application Tasks](#main-application-tasks)
   - [Configuration Generation Pipeline](#configuration-generation-pipeline)
4. [Hardware Reference](#hardware-reference)
   - [Memory Budgets](#memory-budgets)
   - [Hardware Pin Maps](#hardware-pin-maps)
5. [API Reference](#api-reference)
   - [REST API Quick Reference](#rest-api-quick-reference)
   - [Status JSON Structure](#status-json-structure)
6. [Testing](#testing)
   - [Test Organization](#test-organization)
   - [Running Tests](#running-tests)
7. [Development Patterns](#development-patterns)
   - [Common Development Tasks](#common-development-tasks)
   - [Key Implementation Patterns](#key-implementation-patterns)
8. [Troubleshooting](#troubleshooting)
   - [Quick Reference](#quick-reference)
   - [File Locations](#file-locations)

---

## Project Overview

### Project Structure

```
esp32-rover-firmware/
├── firmware/           # Firmware source code
│   ├── main/           # Main application
│   ├── components/     # Custom ESP-IDF components
│   └── esp-idf/        # ESP-IDF SDK (submodule)
├── scripts/            # Build and utility scripts
├── config/             # Configuration files
├── docs/
│   ├── requirements/   # Requirements specifications
│   ├── implementation/ # Developer documentation
│   └── user/           # User guides
├── test/               # Unit tests
├── binaries/           # Archived firmware binaries
└── CLAUDE/             # AI assistant documentation
    ├── context.md      # This file - project context
    ├── rules.md        # Behavioral rules
    └── plans/          # Implementation plans
```

### ESP-IDF Location

ESP-IDF is embedded within this project (not installed globally):

```bash
# Source ESP-IDF environment before running Python scripts or builds
source ./firmware/esp-idf/export.sh
```

### Target Configuration

The firmware supports two hardware targets:

| Target | Hardware | Key Features |
|--------|----------|--------------|
| `esp32cam` | ESP32-CAM AI-Thinker | Camera, Flash LED, 4MB PSRAM |
| `ttgo` | TTGO T-Display | 135x240 LCD, 2 buttons, battery ADC |

The build target is determined by the build command:
- `./scripts/build.sh esp32cam` → hostname `esp32-rover.local`
- `./scripts/build.sh ttgo` → hostname `ttgo-rover.local`

The build script automatically regenerates `config_generated.h` with target-specific settings.

### Key Files

| File | Purpose |
|------|---------|
| `config/rover_config.yaml` | Main configuration file |
| `config/secrets.yaml` | WiFi passwords, OTA password (not in git) |
| `scripts/generate_config.py` | Generates `config_generated.h` from YAML |
| `firmware/main/config.h` | Hardware pin definitions |
| `firmware/main/config_generated.h` | Auto-generated config defines |
| `firmware/main/main.c` | Main application entry point |

---

## Common Commands

### Build Commands

```bash
# Build for ESP32-CAM
./scripts/build.sh esp32cam

# Build for TTGO T-Display
./scripts/build.sh ttgo

# Regenerate config manually (normally done automatically by build.sh)
source ./firmware/esp-idf/export.sh && ROVER_TARGET=esp32cam python scripts/generate_config.py
```

### Flash Commands

```bash
# OTA flash to ESP32-CAM (auto rate-limited for stability)
./scripts/ota.sh esp32cam

# OTA flash to TTGO
./scripts/ota.sh ttgo

# OTA flash to specific IP
./scripts/ota.sh esp32cam 192.168.2.88
./scripts/ota.sh ttgo 192.168.2.75

# List available binaries
./scripts/ota.sh esp32cam --list-binaries

# Flash specific archived binary
./scripts/ota.sh esp32cam --binary-file esp32-rover_esp32cam_20240115_143022_abc1234.bin
```

### JTAG Flashing

Flash firmware directly via JTAG using openocd-esp32. Useful when UART is unavailable.

**Prerequisites:**
```bash
# Install build dependencies (macOS)
brew install automake autoconf libtool pkg-config libusb libftdi texinfo

# Build openocd-esp32
./scripts/build-openocd.sh
```

**ESP-PROG JTAG Wiring:**

| ESP-PROG | ESP32-CAM |
|----------|-----------|
| TDI      | GPIO12    |
| TCK      | GPIO13    |
| TMS      | GPIO14    |
| TDO      | GPIO15    |
| GND      | GND       |
| 3V3      | 3V3       |

**Flash Commands:**
```bash
# Flash latest binary for target
./scripts/jtag-flash.sh esp32cam
./scripts/jtag-flash.sh ttgo

# Flash specific binary
./scripts/jtag-flash.sh esp32cam path/to/firmware.bin
```

---

## Architecture

### Component Architecture

The firmware uses ESP-IDF's component model with custom components in `firmware/components/`:

| Component | Purpose | Key Files | Notes |
|-----------|---------|-----------|-------|
| `web_server` | HTTP REST API & Web UI | `web_server.c`, `web_ui.c` | ~920 LOC, serves control interface |
| `camera` | OV2640 driver | `camera.c` | ESP32-CAM only, QVGA MJPEG |
| `lcd_display` | ST7789 LCD driver | `lcd_display.c` | TTGO only, 135x240 display |
| `mqtt_service` | Telemetry publishing | `mqtt_service.c` | Optional, 5s default interval |
| `resource_guard` | Memory safety | `resource_guard.c` | Heap/stack monitoring |
| `build_info` | Git metadata | `build_info.h` (generated) | Commit hash, branch, timestamp |

### Main Application Tasks

Tasks created in `main.c` (running on Core 0):

| Task Name | Stack Size | Priority | Purpose |
|-----------|------------|----------|---------|
| `status_task` | 4096* | 2 | Collects metrics at 20Hz |
| `lcd_update_task` | 4096* | 1 | LCD refresh (TTGO only) |
| `web_server_task` | auto | default | HTTP request handling |
| `mqtt_publish_task` | 4096* | 2 | MQTT telemetry at 5s interval |

*TTGO uses reduced stacks: status=2560, lcd=3072, mqtt=3072

### Configuration Generation Pipeline

```
rover_config.yaml  ─┐
                    ├─► generate_config.py ─► config_generated.h ─► Compilation
secrets.yaml       ─┘
```

Key defines generated:
- `WIFI_MODE_*` - WiFi mode flags
- `WIFI_AP_SSID`, `WIFI_STA_SSID` - Network names
- `ENABLE_MQTT`, `ENABLE_REST_API` - Feature flags
- `MDNS_HOSTNAME` - Network discovery name
- `CFG_WATCHDOG_TIMEOUT_MS` - Safety timeout

---

## Hardware Reference

### Memory Budgets

**ESP32-CAM** (has 4MB PSRAM):
- Free heap at startup: ~150KB
- PSRAM available for camera buffers
- OTA rollback enabled (PSRAM buffer)

**TTGO T-Display** (no PSRAM):
- Free heap at startup: ~40-60KB
- Aggressive optimization required
- OTA rollback **disabled** (IRAM constraints)

Critical TTGO optimizations:
```
HTTP max header: 512B (vs 1024B on ESP32-CAM)
HTTP max URI: 256B (vs 512B)
Max connections: 4
LWIP sockets: 8
MQTT: No SSL/WebSocket
```

### Hardware Pin Maps

**ESP32-CAM:**
```
Camera: GPIO 0,5,18-19,21-23,25-27,32,34-36,39
Flash LED: GPIO 4
JTAG: GPIO 12-15 (alternate use)
```

**TTGO T-Display:**
```
LCD: GPIO 4,5,16,18,19,23 (SPI + control)
Buttons: GPIO 0 (left), GPIO 35 (right)
Battery ADC: GPIO 34
```

---

## API Reference

### REST API Quick Reference

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/` | Web UI |
| GET | `/stream` | MJPEG video (ESP32-CAM) |
| POST | `/control` | `{"speed":-100..100, "steering":-100..100, "estop":bool}` |
| GET | `/status` | JSON system status |
| GET | `/camera` | Camera state |
| POST | `/camera` | `{"enabled":true/false}` |
| POST | `/led` | `{"on":true/false}` (ESP32-CAM) |
| POST | `/ota` | Firmware binary + password |

### Status JSON Structure

```json
{
  "target": "esp32cam|ttgo",
  "velocity": 0.0,
  "battery": 7.4,
  "camera": true,
  "rssi": -45,
  "btnL": false,
  "btnR": true,
  "diag": {
    "ssid": "NetworkName",
    "ip": "192.168.x.x",
    "wifiMode": "sta|ap|apsta",
    "channel": 1,
    "clients": 2,
    "txPower": 19,
    "freeHeap": 150000,
    "minHeap": 140000,
    "totalHeap": 295000,
    "freeInternal": 260000,
    "uptime": 3600,
    "restApi": true,
    "mqttEnabled": true,
    "mqttConnected": false,
    "localTime": "14:30:45",
    "ntpSynced": true,
    "buildVersion": "2.0+34",
    "buildFingerprint": "abc1234",
    "buildTime": "2026-01-25T13:34:02Z",
    "buildBranch": "develop",
    "buildDirty": false
  }
}
```

**Note:** The `clients` field is only present when `wifiMode` is "ap" or "apsta" (Access Point mode). In "sta" (Station) mode, clients count is not relevant and omitted from the response.

---

## Testing

### Test Organization

```
test/
├── src/
│   ├── test_config.c              # 15 tests - YAML config validation
│   ├── test_diag_state_machine.c  # 12 tests - Diagnostic mode FSM
│   ├── test_resource_guard.c      # 13 tests - Memory safety (target only)
│   └── test_runner.c              # Test harness
├── integration/                    # pytest hardware tests
├── Makefile                        # Host-based test build
└── CMakeLists.txt                  # ESP32 target tests
```

### Running Tests

```bash
# Host tests (no hardware needed)
cd test && make test

# Coverage report
cd test && make coverage-html

# Integration tests (requires device)
export DEVICE_IP=esp32-rover.local
export OTA_PASSWORD=your_password
pytest test/integration/ -v
```

---

## Development Patterns

### Common Development Tasks

**Adding a new configuration option:**
1. Add to `config/rover_config.yaml`
2. Update `scripts/generate_config.py` to generate define
3. Regenerate: `source ./firmware/esp-idf/export.sh && python scripts/generate_config.py`
4. Use `#ifdef CONFIG_OPTION` in C code

**Adding a new component:**
1. Create `firmware/components/mycomp/`
2. Add `CMakeLists.txt` with `idf_component_register()`
3. Add `include/mycomp.h` for public API
4. Add to dependencies in `firmware/main/CMakeLists.txt`

**Debugging memory issues:**
1. Check heap: `esp_get_free_heap_size()`
2. Check internal DRAM: `heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)`
3. Check stack watermarks: `uxTaskGetStackHighWaterMark(NULL)`
4. Use resource_guard component thresholds

### Key Implementation Patterns

**Safe task creation (target-specific stack sizes):**
```c
#ifdef CONFIG_TARGET_TTGO
#define STATUS_TASK_STACK 2560
#else
#define STATUS_TASK_STACK 4096
#endif
```

**Feature toggle pattern:**
```c
#if ENABLE_MQTT
    mqtt_service_init();
#endif
```

**Target-conditional code:**
```c
#ifdef CONFIG_TARGET_ESP32CAM
    camera_init();
#elif defined(CONFIG_TARGET_TTGO)
    lcd_display_init();
#endif
```

---

## Troubleshooting

### Quick Reference

| Issue | Cause | Solution |
|-------|-------|----------|
| Build fails after target switch | Old sdkconfig | `./scripts/build.sh <target> fullclean` |
| OTA timeout | Rate limiting | Normal for ESP32-CAM (50 KB/s limit) |
| TTGO heap exhaustion | No PSRAM | Check stack sizes, reduce HTTP buffers |
| Camera not initializing | Wrong pins or PSRAM | Verify sdkconfig.defaults.esp32cam |
| WiFi not connecting | Wrong mode/creds | Check rover_config.yaml, secrets.yaml |
| mDNS not working | Firewall/router | Use IP address directly |

### File Locations

```
Main entry point:      firmware/main/main.c
Configuration header:  firmware/main/config.h
Generated config:      firmware/main/config_generated.h
Web server:            firmware/components/web_server/
Camera driver:         firmware/components/camera/
LCD driver:            firmware/components/lcd_display/
Build script:          scripts/build.sh
OTA script:            scripts/ota.sh
Config generator:      scripts/generate_config.py
Main config:           config/rover_config.yaml
Secrets:               config/secrets.yaml
Unit tests:            test/src/
Documentation:         docs/
Binaries archive:      binaries/esp32cam/, binaries/ttgo/
```

---

## Version Information

- **Current Version**: Check `CHANGELOG.md` or run build
- **ESP-IDF Version**: v5.2.2 (embedded in `firmware/esp-idf/`)
- **Version in binary**: Accessible via build_info component
