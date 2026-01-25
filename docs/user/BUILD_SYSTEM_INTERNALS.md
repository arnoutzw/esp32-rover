# ESP32 Rover Firmware - Build System Internals

This document provides a technical deep-dive into the build system internals, including CMake configuration, sdkconfig files, partition tables, component architecture, and the configuration generation pipeline.

---

## Table of Contents

1. [Build System Overview](#build-system-overview)
2. [CMake Configuration](#cmake-configuration)
   - [Project CMakeLists.txt](#project-cmakeliststxt)
   - [Main Component CMakeLists.txt](#main-component-cmakeliststxt)
   - [Custom Components](#custom-components)
3. [SDK Configuration (sdkconfig)](#sdk-configuration-sdkconfig)
   - [Target-Specific Defaults](#target-specific-defaults)
   - [ESP32-CAM Configuration](#esp32-cam-configuration)
   - [TTGO Configuration](#ttgo-configuration)
   - [Configuration Inheritance](#configuration-inheritance)
4. [Partition Table](#partition-table)
5. [Configuration Generation Pipeline](#configuration-generation-pipeline)
   - [YAML Configuration Files](#yaml-configuration-files)
   - [Config Generator Script](#config-generator-script)
   - [Generated Header](#generated-header)
6. [Build Info Generation](#build-info-generation)
7. [Unit Test System](#unit-test-system)
8. [Component Dependencies](#component-dependencies)
9. [Memory Considerations](#memory-considerations)
10. [Build Process Flow](#build-process-flow)

---

## Build System Overview

The ESP32 Rover uses ESP-IDF's CMake-based build system with custom extensions:

```
Build Pipeline:

┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐
│ rover_config.yaml│────▶│ generate_config │────▶│config_generated.h│
│ secrets.yaml    │     │      .py        │     │                 │
└─────────────────┘     └─────────────────┘     └─────────────────┘
                                                         │
┌─────────────────┐     ┌─────────────────┐              │
│ sdkconfig.      │────▶│    sdkconfig    │              │
│ defaults.*      │     │   (generated)   │              │
└─────────────────┘     └─────────────────┘              │
                                 │                       │
                                 ▼                       ▼
                        ┌─────────────────────────────────┐
                        │         CMake / idf.py          │
                        │         Build System            │
                        └─────────────────────────────────┘
                                         │
                        ┌────────────────┼────────────────┐
                        ▼                ▼                ▼
              ┌──────────────┐  ┌──────────────┐  ┌──────────────┐
              │ build_info.h │  │   .elf file  │  │  .bin file   │
              │  (generated) │  │              │  │              │
              └──────────────┘  └──────────────┘  └──────────────┘
```

---

## CMake Configuration

### Project CMakeLists.txt

**File:** `firmware/CMakeLists.txt`

```cmake
cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS components)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32-rover)
```

**Key elements:**
- `EXTRA_COMPONENT_DIRS` adds the `components/` directory to the component search path
- `$ENV{IDF_PATH}` references the embedded ESP-IDF
- Project name `esp32-rover` becomes the output binary name

### Main Component CMakeLists.txt

**File:** `firmware/main/CMakeLists.txt`

This is the most complex CMakeLists.txt as it handles target selection:

```cmake
# Target selection via environment variable
if(DEFINED ENV{ROVER_TARGET})
    set(ROVER_TARGET $ENV{ROVER_TARGET})
endif()

# Default to TTGO if not specified
if(NOT DEFINED ROVER_TARGET OR ROVER_TARGET STREQUAL "")
    set(ROVER_TARGET "ttgo")
endif()

# Base requirements for all targets
set(COMPONENT_REQUIRES
    driver
    esp_wifi
    esp_http_server
    nvs_flash
    esp_psram
    esp_adc
    esp_system
    web_server
    lcd_display
    mqtt_service
    lwip
    app_update
    resource_guard
    build_info
)

# Conditional camera component
if(ROVER_TARGET STREQUAL "esp32cam")
    list(APPEND COMPONENT_REQUIRES camera)
    message(STATUS "Building for ESP32-CAM (camera enabled)")
elseif(ROVER_TARGET STREQUAL "ttgo")
    message(STATUS "Building for TTGO T-Display (no camera)")
else()
    message(FATAL_ERROR "Invalid ROVER_TARGET: ${ROVER_TARGET}")
endif()

# Register component
idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES ${COMPONENT_REQUIRES}
)

# Target-specific compile definitions
if(ROVER_TARGET STREQUAL "esp32cam")
    target_compile_definitions(${COMPONENT_LIB} PUBLIC ROVER_TARGET_ESP32CAM=1)
elseif(ROVER_TARGET STREQUAL "ttgo")
    target_compile_definitions(${COMPONENT_LIB} PUBLIC ROVER_TARGET_TTGO=1)
endif()

# Optional JTAG debug mode
if(JTAG_DEBUG)
    target_compile_definitions(${COMPONENT_LIB} PUBLIC ENABLE_JTAG_DEBUG=1)
    message(STATUS "JTAG debug mode ENABLED - GPIO 12-15 available")
endif()
```

**Target selection mechanism:**
1. `ROVER_TARGET` environment variable is set by `build.sh`
2. CMake reads it and conditionally includes/excludes components
3. Compile definitions (`ROVER_TARGET_ESP32CAM` or `ROVER_TARGET_TTGO`) are added
4. Code uses `#if defined(ROVER_TARGET_ESP32CAM)` for target-specific sections

### Custom Components

Each component in `firmware/components/` has its own CMakeLists.txt:

**lcd_display/CMakeLists.txt:**
```cmake
idf_component_register(
    SRCS "lcd_display.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_lcd
    PRIV_REQUIRES build_info main
)
```

**camera/CMakeLists.txt:**
```cmake
idf_component_register(
    SRCS "camera.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_psram espressif__esp32-camera
)
```

**build_info/CMakeLists.txt:**
```cmake
idf_component_register(
    INCLUDE_DIRS "include"
)

# Generate build_info.h at build time
set(PROJECT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../..")
set(BUILD_INFO_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/include/build_info.h")

add_custom_command(
    OUTPUT "${BUILD_INFO_HEADER}"
    COMMAND ${PROJECT_ROOT}/scripts/generate_build_info.sh "${BUILD_INFO_HEADER}"
    WORKING_DIRECTORY ${PROJECT_ROOT}
    COMMENT "Generating build_info.h"
    VERBATIM
)
add_custom_target(generate_build_info ALL DEPENDS "${BUILD_INFO_HEADER}")
```

**Key CMake functions:**
- `idf_component_register()` - Registers a component with ESP-IDF
- `SRCS` - Source files to compile
- `INCLUDE_DIRS` - Public include directories
- `REQUIRES` - Public dependencies (propagated to dependents)
- `PRIV_REQUIRES` - Private dependencies (not propagated)

---

## SDK Configuration (sdkconfig)

### Target-Specific Defaults

The project maintains separate sdkconfig defaults for each target:

| File | Target | Description |
|------|--------|-------------|
| `sdkconfig.defaults.esp32cam` | ESP32-CAM | PSRAM enabled, camera support, larger buffers |
| `sdkconfig.defaults.ttgo` | TTGO | No PSRAM, IRAM optimizations, smaller buffers |
| `sdkconfig.defaults` | Generated | Copy of active target's defaults (not in git) |
| `sdkconfig` | Generated | Full configuration with all defaults applied |

### ESP32-CAM Configuration

**File:** `firmware/sdkconfig.defaults.esp32cam`

```kconfig
# Target ESP32
CONFIG_IDF_TARGET="esp32"

# Flash size (4MB)
CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"

# PSRAM support - required for camera buffer
CONFIG_ESP32_SPIRAM_SUPPORT=y
CONFIG_SPIRAM=y
CONFIG_SPIRAM_USE_MALLOC=y
CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=16384
CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP=y
CONFIG_SPIRAM_MALLOC_RESERVE_INTERNAL=32768
CONFIG_SPIRAM_MODE_QUAD=y
CONFIG_SPIRAM_TYPE_AUTO=y
CONFIG_SPIRAM_SPEED_80M=y

# Camera support
CONFIG_CAMERA_CORE0=y

# WiFi
CONFIG_ESP_WIFI_SOFTAP_SUPPORT=y

# HTTP Server - larger buffers for MJPEG streaming
CONFIG_HTTPD_MAX_REQ_HDR_LEN=1024
CONFIG_HTTPD_MAX_URI_LEN=512

# FreeRTOS
CONFIG_FREERTOS_HZ=1000
CONFIG_FREERTOS_USE_TRACE_FACILITY=y

# LWIP - more sockets for streaming
CONFIG_LWIP_MAX_SOCKETS=16

# Log level
CONFIG_LOG_DEFAULT_LEVEL_INFO=y

# Disable WiFi NVS
CONFIG_ESP_WIFI_NVS_ENABLED=n

# Task Watchdog - manual init
CONFIG_ESP_TASK_WDT_INIT=n

# OTA Rollback Support - enabled (has PSRAM for bootloader)
CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y
```

### TTGO Configuration

**File:** `firmware/sdkconfig.defaults.ttgo`

```kconfig
# Target ESP32
CONFIG_IDF_TARGET="esp32"

# Flash size (4MB)
CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"

# PSRAM support - DISABLED (TTGO has no PSRAM)
CONFIG_ESP32_SPIRAM_SUPPORT=n
CONFIG_SPIRAM=n

# Camera support - disabled
# CONFIG_CAMERA_CORE0 is not set

# WiFi
CONFIG_ESP_WIFI_SOFTAP_SUPPORT=y

# HTTP Server - smaller buffers to save RAM
CONFIG_HTTPD_MAX_REQ_HDR_LEN=512
CONFIG_HTTPD_MAX_URI_LEN=256

# FreeRTOS
CONFIG_FREERTOS_HZ=1000
CONFIG_FREERTOS_USE_TRACE_FACILITY=y

# LWIP - fewer sockets
CONFIG_LWIP_MAX_SOCKETS=8

# Log level
CONFIG_LOG_DEFAULT_LEVEL_INFO=y

# Disable WiFi NVS
CONFIG_ESP_WIFI_NVS_ENABLED=n

# Task Watchdog - manual init
CONFIG_ESP_TASK_WDT_INIT=n

# OTA Rollback Support - DISABLED (IRAM constraints)
# CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE is not set

# MQTT - smaller buffers
CONFIG_MQTT_USE_CUSTOM_CONFIG=y
CONFIG_MQTT_BUFFER_SIZE=1024
CONFIG_MQTT_TASK_STACK_SIZE=4096
CONFIG_MQTT_TRANSPORT_SSL=n
CONFIG_MQTT_TRANSPORT_WEBSOCKET=n

# IRAM optimization - move code to flash
CONFIG_FREERTOS_PLACE_FUNCTIONS_INTO_FLASH=y
CONFIG_FREERTOS_PLACE_SNAPSHOT_FUNS_INTO_FLASH=y
CONFIG_RINGBUF_PLACE_FUNCTIONS_INTO_FLASH=y
CONFIG_RINGBUF_PLACE_ISR_FUNCTIONS_INTO_FLASH=y
CONFIG_HAL_DEFAULT_ASSERTION_LEVEL=1
CONFIG_COMPILER_OPTIMIZATION_SIZE=y
CONFIG_SPI_FLASH_ROM_DRIVER_PATCH=y
CONFIG_ESP_EVENT_POST_FROM_ISR=n
CONFIG_LWIP_IRAM_OPTIMIZATION=n
CONFIG_ESP_WIFI_IRAM_OPT=n
CONFIG_ESP_WIFI_RX_IRAM_OPT=n
CONFIG_SPI_MASTER_ISR_IN_IRAM=n
CONFIG_SPI_SLAVE_ISR_IN_IRAM=n
CONFIG_GPTIMER_ISR_HANDLER_IN_IRAM=n
CONFIG_HEAP_PLACE_FUNCTION_INTO_FLASH=y
```

### Configuration Inheritance

When you run `./scripts/build.sh ttgo`:

1. `build.sh` copies `sdkconfig.defaults.ttgo` → `sdkconfig.defaults`
2. ESP-IDF's cmake reads `sdkconfig.defaults`
3. Missing options are filled from ESP-IDF defaults
4. Full `sdkconfig` is generated
5. If target changes, `sdkconfig` is deleted and regenerated

**Important:** `sdkconfig` and `sdkconfig.defaults` are generated files and should not be manually edited. Always edit the target-specific `sdkconfig.defaults.*` files.

---

## Partition Table

**File:** `firmware/partitions.csv`

```csv
# Name,   Type, SubType, Offset,  Size, Flags
nvs,      data, nvs,     0x9000,  0x4000,
otadata,  data, ota,     0xd000,  0x2000,
phy_init, data, phy,     0xf000,  0x1000,
ota_0,    app,  ota_0,   0x10000, 0x180000,
ota_1,    app,  ota_1,   0x190000,0x180000,
storage,  data, spiffs,  0x310000,0xF0000,
```

**Partition layout (4MB flash):**

| Partition | Offset | Size | Purpose |
|-----------|--------|------|---------|
| nvs | 0x9000 | 16 KB | Non-volatile storage for settings |
| otadata | 0xD000 | 8 KB | OTA boot partition selection |
| phy_init | 0xF000 | 4 KB | PHY calibration data |
| ota_0 | 0x10000 | 1.5 MB | Primary application slot |
| ota_1 | 0x190000 | 1.5 MB | Secondary application slot (OTA updates) |
| storage | 0x310000 | 960 KB | SPIFFS filesystem (unused currently) |

**Visual layout:**
```
0x000000 ┌────────────────────┐
         │ Bootloader (28KB)  │
0x009000 ├────────────────────┤
         │ NVS (16KB)         │
0x00D000 ├────────────────────┤
         │ OTA Data (8KB)     │
0x00F000 ├────────────────────┤
         │ PHY Init (4KB)     │
0x010000 ├────────────────────┤
         │                    │
         │ OTA_0 (1.5MB)      │  ← Active app runs here
         │ Application        │
         │                    │
0x190000 ├────────────────────┤
         │                    │
         │ OTA_1 (1.5MB)      │  ← OTA writes new app here
         │ Application        │
         │                    │
0x310000 ├────────────────────┤
         │ Storage (960KB)    │
         │ SPIFFS             │
0x400000 └────────────────────┘
```

**OTA update flow:**
1. Current app runs from `ota_0`
2. OTA writes new firmware to `ota_1`
3. `otadata` is updated to boot from `ota_1`
4. Device reboots into new firmware
5. Next OTA writes to `ota_0`, and so on

---

## Configuration Generation Pipeline

### YAML Configuration Files

**File:** `config/rover_config.yaml`

```yaml
wifi:
  mode: sta_first  # sta_first, ap_only, sta_only
  sta:
    connect_timeout: 10
  ap:
    ssid: "TTGO-Rover"
    channel: 1
    max_connections: 4

rest_api:
  enabled: true
  cache_interval_ms: 1000

mqtt:
  enabled: false
  broker:
    host: ""
    port: 1883
  client_id: "esp32-rover"
  topic_prefix: "esp32-rover"
  publish_interval_ms: 5000
  qos: 0

mdns:
  hostname_esp32cam: "esp32-rover"
  hostname_ttgo: "ttgo-rover"
  instance_name: "ESP32 Rover Control"

ota:
  enabled: true

task_watchdog:
  enabled: true
  timeout_sec: 30
  panic_on_timeout: true

debug:
  motor: false
  encoder: false
  servo: false
  camera: false
  webserver: false
  mqtt: false
```

**File:** `config/secrets.yaml` (gitignored)

```yaml
wifi_ap_password: "your_ap_password"
wifi_sta_ssid: "YourHomeWiFi"
wifi_sta_password: "your_wifi_password"
mqtt_username: ""
mqtt_password: ""
ota_password: "your_ota_password"
```

### Config Generator Script

**File:** `scripts/generate_config.py`

The script performs these steps:

1. **Load YAML files:**
   ```python
   with open(config_path, "r") as f:
       config = yaml.safe_load(f)
   secrets = load_secrets(config_dir)
   ```

2. **Get target from environment:**
   ```python
   target = os.environ.get("ROVER_TARGET", "").lower()
   ```

3. **Generate C preprocessor definitions:**
   ```python
   lines.append(f'#define WIFI_AP_SSID "{ap.get("ssid", "ESP32-Rover")}"')
   lines.append(f'#define WIFI_AP_PASSWORD "{secrets.get("wifi_ap_password", "")}"')
   ```

4. **Handle target-specific values:**
   ```python
   if target == "esp32cam":
       mdns_hostname = mdns.get("hostname_esp32cam", "esp32-rover")
   else:
       mdns_hostname = mdns.get("hostname_ttgo", "ttgo-rover")
   ```

### Generated Header

**File:** `firmware/main/config_generated.h` (generated, not in git)

```c
// AUTO-GENERATED FILE - DO NOT EDIT MANUALLY
// Generated from rover_config.yaml and secrets.yaml
#pragma once

// Config generated for target: ttgo

// WiFi Mode Configuration
#define WIFI_MODE_AP_ONLY 0
#define WIFI_MODE_STA_ONLY 0
#define WIFI_MODE_STA_FIRST 1

// WiFi AP Settings
#define WIFI_AP_SSID "TTGO-Rover"
#define WIFI_AP_PASSWORD "your_ap_password"
#define WIFI_AP_CHANNEL 1
#define WIFI_AP_MAX_CONN 4

// WiFi STA Settings
#define WIFI_STA_SSID "YourHomeWiFi"
#define WIFI_STA_PASSWORD "your_wifi_password"
#define WIFI_STA_CONNECT_TIMEOUT_S 10

// REST API Configuration
#define ENABLE_REST_API 1
#define REST_API_CACHE_INTERVAL_MS 1000

// MQTT Configuration
#define ENABLE_MQTT 0
#define MQTT_BROKER_HOST ""
#define MQTT_BROKER_PORT 1883
// ... more definitions ...

// mDNS Configuration
#define MDNS_HOSTNAME "ttgo-rover"
#define MDNS_INSTANCE_NAME "ESP32 Rover Control"

// OTA Configuration
#define ENABLE_OTA 1
#define OTA_HOSTNAME MDNS_HOSTNAME
#define OTA_PASSWORD "your_ota_password"

// Task Watchdog Configuration
#define ENABLE_TASK_WATCHDOG 1
#define TASK_WDT_TIMEOUT_SEC 30
#define TASK_WDT_PANIC_ON_TIMEOUT 1

// Debug Options
#define CFG_DEBUG_MOTOR 0
#define CFG_DEBUG_WEBSERVER 0
// ...
```

---

## Build Info Generation

**File:** `scripts/generate_build_info.sh`

This script runs at build time (via CMake custom command) and generates version information:

```bash
# Get git information
GIT_HASH=$(git rev-parse --short HEAD 2>/dev/null || echo "unknown")
GIT_HASH_FULL=$(git rev-parse HEAD 2>/dev/null || echo "unknown")
GIT_BRANCH=$(git rev-parse --abbrev-ref HEAD 2>/dev/null || echo "unknown")

# Check if dirty
if git diff-index --quiet HEAD -- 2>/dev/null; then
    GIT_DIRTY="false"
else
    GIT_DIRTY="true"
fi

# Build timestamp
BUILD_TIME=$(date -u +"%Y-%m-%dT%H:%M:%SZ")
BUILD_TIMESTAMP=$(date +%s)

# Version from git tags
GIT_VERSION=$(git describe --tags --abbrev=0 2>/dev/null | sed 's/^v//' || echo "dev")
VERSION_AHEAD=$(git describe --tags 2>/dev/null | sed 's/^v//' | grep -o '\-[0-9]*\-' | tr -d '-' || echo "")
if [ -n "$VERSION_AHEAD" ] && [ "$VERSION_AHEAD" != "0" ]; then
    BUILD_VERSION="${GIT_VERSION}+${VERSION_AHEAD}"
else
    BUILD_VERSION="$GIT_VERSION"
fi
```

**Generated header:** `firmware/components/build_info/include/build_info.h`

```c
#ifndef BUILD_INFO_H
#define BUILD_INFO_H

// Git information
#define BUILD_GIT_HASH       "77b4f74"
#define BUILD_GIT_HASH_FULL  "77b4f74abc123def456..."
#define BUILD_GIT_BRANCH     "develop"
#define BUILD_GIT_DIRTY      false

// Build timestamp
#define BUILD_TIME           "2026-01-25T21:58:41Z"
#define BUILD_TIMESTAMP      1737842321UL

// ESP-IDF version
#define BUILD_IDF_VERSION    "v5.2.2"

// Build fingerprint
#define BUILD_FINGERPRINT    "77b4f74"

// Project version
#define BUILD_VERSION        "2.0.3+6"

#endif
```

**IMPORTANT:** This file is generated at build time. Incremental builds may not regenerate it if only non-header source files changed. This is why **clean builds are mandatory** before flashing - to ensure the correct git hash is embedded.

---

## Unit Test System

**Directory:** `test/`

The unit tests run on the host machine (not on ESP32) using the Unity test framework.

### Makefile

**File:** `test/Makefile`

```makefile
CC = gcc
CFLAGS = -Wall -Wextra -g -I. -I../firmware/main
LDFLAGS =

# Coverage flags
COVERAGE_CFLAGS = -fprofile-arcs -ftest-coverage
COVERAGE_LDFLAGS = --coverage

# Source files
SRCS = src/test_runner.c src/test_config.c src/test_diag_state_machine.c
OBJS = $(SRCS:.c=.o)
TARGET = test_runner

# Build test runner
$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

# Run tests
.PHONY: test
test: $(TARGET)
	./$(TARGET)

# Coverage targets
.PHONY: coverage
coverage: coverage-build
	./$(TARGET)
	gcov -r src/*.c

.PHONY: coverage-html
coverage-html: coverage
	lcov --capture --directory . --output-file coverage.lcov
	genhtml coverage.lcov --output-directory coverage_report
```

### Test Structure

```
test/
├── Makefile              # Host-based test build
├── unity.h               # Unity test framework header
├── src/
│   ├── test_runner.c     # Test main entry point
│   ├── test_config.c     # Configuration tests
│   └── test_diag_state_machine.c  # State machine tests
└── integration/          # Integration test scripts
```

### Running Tests

```bash
# Run all tests
cd test && make test

# Run with coverage
cd test && make coverage

# Generate HTML coverage report
cd test && make coverage-html
```

Tests are automatically run before every build by `build.sh`. Build fails if tests fail.

---

## Component Dependencies

```
Component Dependency Graph:

                    ┌─────────┐
                    │  main   │
                    └────┬────┘
                         │
         ┌───────────────┼───────────────┐
         │               │               │
         ▼               ▼               ▼
   ┌───────────┐   ┌───────────┐   ┌───────────┐
   │web_server │   │lcd_display│   │   camera  │
   └─────┬─────┘   └─────┬─────┘   └─────┬─────┘
         │               │               │
         │               │               │
         ▼               ▼               ▼
   ┌───────────┐   ┌───────────┐   ┌───────────┐
   │   cJSON   │   │  esp_lcd  │   │esp32-camera│
   └───────────┘   └───────────┘   │  (managed) │
                                   └───────────┘
         │               │
         └───────┬───────┘
                 │
                 ▼
          ┌───────────┐
          │build_info │
          └───────────┘
```

**Component descriptions:**

| Component | Location | Purpose | Dependencies |
|-----------|----------|---------|--------------|
| main | firmware/main/ | Application entry | All custom components |
| web_server | components/web_server/ | HTTP server, REST API | cJSON, mdns |
| lcd_display | components/lcd_display/ | ST7789 LCD driver | esp_lcd, build_info |
| camera | components/camera/ | OV2640 camera wrapper | espressif__esp32-camera |
| mqtt_service | components/mqtt_service/ | MQTT telemetry | mqtt |
| resource_guard | components/resource_guard/ | Memory monitoring | heap |
| build_info | components/build_info/ | Version info | None (header only) |

---

## Memory Considerations

### ESP32-CAM (with PSRAM)

- **Internal RAM:** ~320 KB available
- **PSRAM:** 4 MB (external)
- **Camera buffer:** Allocated in PSRAM
- **WiFi/LWIP buffers:** Can use PSRAM
- **Stack sizes:** Standard (4-8 KB per task)

### TTGO (no PSRAM)

- **Internal RAM:** ~320 KB available (all there is)
- **IRAM:** ~128 KB (code cache)
- **Stack sizes:** Reduced (2-4 KB per task)
- **Buffer sizes:** Reduced

**TTGO Memory Optimizations:**

1. **Move code to flash:**
   ```kconfig
   CONFIG_FREERTOS_PLACE_FUNCTIONS_INTO_FLASH=y
   CONFIG_HEAP_PLACE_FUNCTION_INTO_FLASH=y
   ```

2. **Reduce buffer sizes:**
   ```kconfig
   CONFIG_HTTPD_MAX_REQ_HDR_LEN=512  # vs 1024 on ESP32-CAM
   CONFIG_LWIP_MAX_SOCKETS=8         # vs 16 on ESP32-CAM
   CONFIG_MQTT_BUFFER_SIZE=1024      # Custom smaller buffer
   ```

3. **Disable unused features:**
   ```kconfig
   CONFIG_MQTT_TRANSPORT_SSL=n
   CONFIG_MQTT_TRANSPORT_WEBSOCKET=n
   CONFIG_ESP_WIFI_IRAM_OPT=n
   ```

---

## Build Process Flow

```
┌─────────────────────────────────────────────────────────────────┐
│                    ./scripts/build.sh ttgo                      │
└─────────────────────────────────────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────┐
│ 1. Check clean git state (uncommitted/unpushed changes)         │
└─────────────────────────────────────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────┐
│ 2. Source ESP-IDF environment (export.sh)                       │
└─────────────────────────────────────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────┐
│ 3. Setup target:                                                │
│    - Copy sdkconfig.defaults.ttgo → sdkconfig.defaults          │
│    - Set ROVER_TARGET=ttgo environment variable                 │
│    - Run generate_config.py → config_generated.h                │
└─────────────────────────────────────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────┐
│ 4. Check if target switched (clean build if so)                 │
└─────────────────────────────────────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────┐
│ 5. Run unit tests (cd test && make test)                        │
│    - Abort build if tests fail                                  │
└─────────────────────────────────────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────┐
│ 6. Run idf.py build:                                            │
│    a. CMake configuration                                       │
│    b. Generate build_info.h (custom command)                    │
│    c. Compile all components                                    │
│    d. Link into esp32-rover.elf                                 │
│    e. Convert to esp32-rover.bin                                │
└─────────────────────────────────────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────┐
│ 7. Archive binary:                                              │
│    - Copy to binaries/ttgo/esp32-rover_ttgo_<date>_<hash>.bin   │
│    - Generate .sha256 checksum file                             │
│    - Generate .txt metadata file                                │
│    - Update latest.bin symlink                                  │
└─────────────────────────────────────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────┐
│ 8. Display build metrics (duration, binary size)                │
│    - Log to build_metrics.csv                                   │
└─────────────────────────────────────────────────────────────────┘
```

---

## Key Files Reference

| File | Purpose | Generated |
|------|---------|-----------|
| `firmware/CMakeLists.txt` | Project-level CMake | No |
| `firmware/main/CMakeLists.txt` | Main component with target logic | No |
| `firmware/sdkconfig.defaults.ttgo` | TTGO SDK config | No |
| `firmware/sdkconfig.defaults.esp32cam` | ESP32-CAM SDK config | No |
| `firmware/sdkconfig.defaults` | Active target's defaults | Yes |
| `firmware/sdkconfig` | Full SDK configuration | Yes |
| `firmware/partitions.csv` | Flash partition layout | No |
| `config/rover_config.yaml` | Application configuration | No |
| `config/secrets.yaml` | Sensitive values (gitignored) | No |
| `firmware/main/config_generated.h` | Generated C config | Yes |
| `firmware/components/build_info/include/build_info.h` | Git/build info | Yes |
| `scripts/build.sh` | Main build orchestrator | No |
| `scripts/generate_config.py` | YAML → C header | No |
| `scripts/generate_build_info.sh` | Git → build_info.h | No |

---

*Last updated: 2026-01-25*
