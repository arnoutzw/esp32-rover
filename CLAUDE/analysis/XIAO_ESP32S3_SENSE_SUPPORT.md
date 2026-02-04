# XIAO ESP32S3 Sense Support Implementation Analysis

## Overview

This document outlines the necessary changes to add **XIAO ESP32S3 Sense** as a third build target to the ESP32 Rover firmware. The XIAO ESP32S3 Sense is a compact development board with built-in camera, making it a natural addition to the existing ESP32-CAM and TTGO T-Display targets.

**Status**: Analysis Complete
**Date**: 2026-02-04

## Hardware Comparison

| Feature | ESP32-CAM | TTGO T-Display | XIAO ESP32S3 Sense |
|---------|-----------|-----------------|-------------------|
| **Chip** | ESP32-D0WDQ6 | ESP32 | ESP32S3 |
| **Core** | Dual-core (Xtensa) | Dual-core (Xtensa) | Dual-core (Xtensa) |
| **Flash** | 4MB | 4MB | 8MB (QSPI) |
| **PSRAM** | 4MB (required) | None | 8MB (QSPI) |
| **Camera** | OV2640 | None | OV2640 (built-in) |
| **LCD** | None | ST7789 135x240 | None |
| **Buttons** | None | GPIO 0, 35 | 3x programmable buttons |
| **Battery** | N/A | Yes (ADC GPIO 34) | No built-in battery |
| **USB** | Micro | Micro | USB-C |
| **Form Factor** | Large (54x40mm) | Medium (52x26mm) | Tiny (21x17.8mm) |

## Key Differences for Implementation

### 1. **ESP32S3 Specific Features**
- **USB-JTAG built-in**: Can debug/flash via USB without FTDI adapter
- **QSPI Flash & PSRAM**: Faster, more memory available
- **2x SPI ports** available (SPI0 reserved, SPI1/SPI2 available)
- **Built-in OV2640 camera** on standard pins (similar to ESP32-CAM but different GPIO mapping)
- **3x Programmable buttons** (GPIO 0, 1, 2)
- **Small form factor**: Limited available GPIO vs ESP32-CAM

### 2. **Camera Integration**
XIAO ESP32S3 Sense has a built-in OV2640 camera with fixed pinout:
- Uses SPI0 (reserved by ESP-IDF for FLASH)
- DVPD0-DVPD7: GPIO 11, 9, 8, 10, 12, 18, 17, 16
- HSYNC: GPIO 15
- VSYNC: GPIO 6
- PCLK: GPIO 13
- XCLK: GPIO 7
- SIOD (SDA): GPIO 40
- SIOC (SCL): GPIO 41
- No power-down control (always on)
- No reset pin control

### 3. **Build Target Name**
Following naming convention:
- `esp32cam` - AI-Thinker ESP32-CAM
- `ttgo` - TTGO T-Display
- `xiao_esp32s3` - Seeed Studio XIAO ESP32S3 Sense

### 4. **Memory Constraints**
- **Flash**: 8MB (sufficient for larger app with more features)
- **PSRAM**: 8MB (double ESP32-CAM, enables more aggressive buffering)
- **Internal RAM**: Similar to ESP32
- **Strategy**: Can allocate more to camera frame buffers and telemetry

## Files to Create/Modify

### New Files (3)

#### 1. `firmware/sdkconfig.defaults.xiao_esp32s3`
**Purpose**: ESP-IDF configuration for XIAO ESP32S3 Sense

Key differences from ESP32-CAM:
- `CONFIG_IDF_TARGET="esp32s3"` (instead of "esp32")
- `CONFIG_ESPTOOLPY_FLASHSIZE_8MB=y` (8MB flash vs 4MB)
- `CONFIG_ESP32S3_PSRAM_SUPPORT=y` (PSRAM for ESP32S3)
- `CONFIG_SPIRAM=y` with optimized settings
- Camera enabled (similar to ESP32-CAM)
- No LCD or button support (like ESP32-CAM)

**Template based on**: `sdkconfig.defaults.esp32cam` with ESP32S3 adaptations

---

#### 2. `firmware/main/config_xiao_esp32s3.h`
**Alternative**: Embed XIAO-specific pins directly in `config.h` with `#ifdef ROVER_TARGET_XIAO_ESP32S3`

**Pin Configuration for XIAO ESP32S3 Sense**:
```c
#ifdef ROVER_TARGET_XIAO_ESP32S3
// Camera pins (OV2640 built-in, fixed pinout)
#define CAM_PIN_PWDN        -1              // Not controllable
#define CAM_PIN_RESET       -1              // Not controllable
#define CAM_PIN_XCLK        GPIO_NUM_7
#define CAM_PIN_SIOD        GPIO_NUM_40     // SDA
#define CAM_PIN_SIOC        GPIO_NUM_41     // SCL
#define CAM_PIN_D7          GPIO_NUM_11
#define CAM_PIN_D6          GPIO_NUM_9
#define CAM_PIN_D5          GPIO_NUM_8
#define CAM_PIN_D4          GPIO_NUM_10
#define CAM_PIN_D3          GPIO_NUM_12
#define CAM_PIN_D2          GPIO_NUM_18
#define CAM_PIN_D1          GPIO_NUM_17
#define CAM_PIN_D0          GPIO_NUM_16
#define CAM_PIN_VSYNC       GPIO_NUM_6
#define CAM_PIN_HREF        GPIO_NUM_15
#define CAM_PIN_PCLK        GPIO_NUM_13

#define CAM_XCLK_FREQ       20000000        // 20MHz
#define CAM_FRAME_SIZE      FRAMESIZE_QVGA  // 320x240
#define CAM_JPEG_QUALITY    12

// Buttons (optional - 3x programmable buttons)
#define BUTTON_A_PIN        GPIO_NUM_0
#define BUTTON_B_PIN        GPIO_NUM_1
#define BUTTON_C_PIN        GPIO_NUM_2

// No LCD, no battery ADC
#define ENABLE_LCD_DISPLAY  0
#define ENABLE_BATTERY_ADC  0
#define ENABLE_DEEP_SLEEP   0               // No accessible sleep mechanism

#endif
```

---

#### 3. `CLAUDE/analysis/XIAO_ESP32S3_IMPLEMENTATION_PLAN.md`
**Purpose**: Step-by-step implementation guide (this document serves as analysis)

### Modified Files (4)

#### 1. `firmware/main/config.h`
**Changes**:
- Add XIAO ESP32S3 target validation
- Add `#ifdef ROVER_TARGET_XIAO_ESP32S3` section with camera & button pins
- Update comment for "ROVER_TARGET_XIAO_ESP32S3"

**Diff Overview**:
```c
// Line 10: Update comment
// - Define ROVER_TARGET_XIAO_ESP32S3 for Seeed XIAO ESP32S3 Sense (with camera)

// Line 20-27: Update target validation
#if !defined(ROVER_TARGET_ESP32CAM) && !defined(ROVER_TARGET_TTGO) && !defined(ROVER_TARGET_XIAO_ESP32S3)
    #define ROVER_TARGET_TTGO 1
#endif

#if (defined(ROVER_TARGET_ESP32CAM) + defined(ROVER_TARGET_TTGO) + defined(ROVER_TARGET_XIAO_ESP32S3)) > 1
    #error "Only one target can be defined"
#endif

// Line 30: Update camera disable logic
#ifdef ROVER_TARGET_ESP32CAM
    #define DISABLE_CAMERA 0
#elif defined(ROVER_TARGET_XIAO_ESP32S3)
    #define DISABLE_CAMERA 0  // Camera enabled
#else
    #define DISABLE_CAMERA 1
#endif

// After line 164: Add new XIAO_ESP32S3 section with pins
#elif defined(ROVER_TARGET_XIAO_ESP32S3)
// ... pin definitions as above ...
```

---

#### 2. `firmware/main/CMakeLists.txt`
**Changes**:
- Add XIAO target validation in `if()` statement
- Add camera component for XIAO target
- Define `ROVER_TARGET_XIAO_ESP32S3=1` for XIAO builds

**Diff Overview**:
```cmake
# Line 33-40: Update target validation
if(ROVER_TARGET STREQUAL "esp32cam")
    list(APPEND COMPONENT_REQUIRES camera)
    message(STATUS "Building for ESP32-CAM (camera enabled)")
elseif(ROVER_TARGET STREQUAL "ttgo")
    message(STATUS "Building for TTGO T-Display (no camera)")
elseif(ROVER_TARGET STREQUAL "xiao_esp32s3")
    list(APPEND COMPONENT_REQUIRES camera)
    message(STATUS "Building for XIAO ESP32S3 Sense (camera enabled)")
else()
    message(FATAL_ERROR "Invalid ROVER_TARGET: ${ROVER_TARGET}. Must be 'esp32cam', 'ttgo', or 'xiao_esp32s3'")
endif()

# Line 51-55: Update compile definitions
if(ROVER_TARGET STREQUAL "esp32cam")
    target_compile_definitions(${COMPONENT_LIB} PUBLIC ROVER_TARGET_ESP32CAM=1)
elseif(ROVER_TARGET STREQUAL "ttgo")
    target_compile_definitions(${COMPONENT_LIB} PUBLIC ROVER_TARGET_TTGO=1)
elseif(ROVER_TARGET STREQUAL "xiao_esp32s3")
    target_compile_definitions(${COMPONENT_LIB} PUBLIC ROVER_TARGET_XIAO_ESP32S3=1)
endif()
```

---

#### 3. `scripts/build.sh`
**Changes**:
- Add `xiao_esp32s3` target in `setup_target()` function
- Update usage/help text to list all three targets
- Validate `sdkconfig.defaults.xiao_esp32s3` exists

**Diff Overview**:
```bash
# Lines 32-34: Update usage
echo "  esp32cam      Build for ESP32-CAM AI-Thinker (with camera)"
echo "  ttgo          Build for TTGO T-Display (no camera)"
echo "  xiao_esp32s3  Build for XIAO ESP32S3 Sense (with camera)"

# Lines 56-75: Add to setup_target()
case $target in
    esp32cam)
        echo -e "${BLUE}Setting up for ESP32-CAM (with camera)${NC}"
        SDKCONFIG_DEFAULTS="sdkconfig.defaults.esp32cam"
        TARGET_DEFINE="ROVER_TARGET_ESP32CAM"
        ;;
    ttgo)
        echo -e "${BLUE}Setting up for TTGO T-Display (no camera)${NC}"
        SDKCONFIG_DEFAULTS="sdkconfig.defaults.ttgo"
        TARGET_DEFINE="ROVER_TARGET_TTGO"
        ;;
    xiao_esp32s3)
        echo -e "${BLUE}Setting up for XIAO ESP32S3 Sense (with camera)${NC}"
        SDKCONFIG_DEFAULTS="sdkconfig.defaults.xiao_esp32s3"
        TARGET_DEFINE="ROVER_TARGET_XIAO_ESP32S3"
        ;;
    *)
        echo -e "${RED}Error: Unknown target '$target'${NC}"
        print_usage
        exit 1
        ;;
esac
```

---

#### 4. `README.md`
**Changes**:
- Add XIAO ESP32S3 Sense to supported hardware table
- Add XIAO-specific instructions in Quick Start
- Update pin configuration section
- Add XIAO hardware notes

**Diff Overview**:
```markdown
## Supported Hardware

| Target | Board | Camera | LCD | Buttons | Description |
|--------|-------|--------|-----|---------|-------------|
| `esp32cam` | AI-Thinker ESP32-CAM | Yes | No | No | Full features with live video |
| `ttgo` | LilyGO TTGO T-Display | No | Yes | Yes | LCD status display + buttons |
| `xiao_esp32s3` | Seeed XIAO ESP32S3 Sense | Yes | No | Yes | Compact with camera + programmable buttons |

### Quick Start

# Build for XIAO ESP32S3 Sense (with camera)
./scripts/build.sh xiao_esp32s3

# Build and flash
./scripts/build.sh xiao_esp32s3 flash

# Build, flash, and open monitor
./scripts/build.sh xiao_esp32s3 flash monitor

### XIAO ESP32S3 Sense Pin Configuration

| GPIO | Function | Notes |
|------|----------|-------|
| 7 | Camera XCLK | Camera clock |
| 6 | Camera VSYNC | Camera sync |
| 13 | Camera PCLK | Camera pixel clock |
| 15 | Camera HREF | Horizontal reference |
| 8-12, 16-18 | Camera D0-D7 | Camera data pins |
| 40 | Camera SDA | I2C data (also GPIO40) |
| 41 | Camera SCL | I2C clock (also GPIO41) |
| 0 | Button A | Programmable button |
| 1 | Button B | Programmable button |
| 2 | Button C | Programmable button |

**Available GPIO for future expansion**: 3, 4, 5, 14, 19, 20, 21

### Notes

- **No Power-Down Control**: Camera power is always on (cannot be disabled to save power)
- **USB-C Connection**: Use USB-C cable for flashing and serial monitoring
- **Compact Form Factor**: Minimal GPIO overhead for motor control expansion
- **8MB Flash & PSRAM**: Double the memory of TTGO, supports larger buffering
- **OTA Flashing**: Uses same rate limiting as ESP32-CAM (50KB/s for stability)
```

---

## Implementation Checklist

- [ ] Create `firmware/sdkconfig.defaults.xiao_esp32s3`
  - [ ] Copy from `sdkconfig.defaults.esp32cam` as base
  - [ ] Change `CONFIG_IDF_TARGET="esp32s3"`
  - [ ] Update flash size to 8MB
  - [ ] Update PSRAM configuration for ESP32S3
  - [ ] Keep camera enabled
  - [ ] Remove LCD config references
  - [ ] Remove button config references

- [ ] Modify `firmware/main/config.h`
  - [ ] Add XIAO ESP32S3 to target comment
  - [ ] Update target validation logic
  - [ ] Update camera disable conditional
  - [ ] Add XIAO ESP32S3 section with camera pins
  - [ ] Add button pin definitions for XIAO buttons
  - [ ] Disable LCD and battery ADC for XIAO

- [ ] Modify `firmware/main/CMakeLists.txt`
  - [ ] Add XIAO target to if/elseif chain
  - [ ] Add camera component for XIAO
  - [ ] Add compile definition for XIAO

- [ ] Modify `scripts/build.sh`
  - [ ] Add XIAO to usage text
  - [ ] Add XIAO case to setup_target()
  - [ ] Validate sdkconfig exists

- [ ] Update `README.md`
  - [ ] Add XIAO to hardware table
  - [ ] Add XIAO to quick start examples
  - [ ] Add XIAO pin configuration section
  - [ ] Add hardware notes for XIAO

- [ ] Test Implementation
  - [ ] Build for xiao_esp32s3 target
  - [ ] Verify no compile errors
  - [ ] Verify camera functionality
  - [ ] Test button input (if implemented)
  - [ ] Verify OTA flashing
  - [ ] Test WiFi AP/STA modes

---

## Considerations & Notes

### Camera Differences
- **XIAO camera is always powered** - cannot be disabled via GPIO
- Pin mapping is fixed and differs from ESP32-CAM
- No PWDN or RESET control - use software-based reset if needed

### Button Implementation
XIAO has 3 programmable buttons vs TTGO's 2. Options:
1. **Map all 3 buttons** - A=left, B=right, C=mode
2. **Use only 2 buttons** - Ignore button C for consistency
3. **Use button C for unique function** - e.g., camera reset trigger

### Memory Advantages
- 8MB PSRAM enables:
  - Larger frame buffers for better quality
  - More telemetry history
  - Reduced memory pressure during OTA

### Compilation Strategy
- Camera component conditionally includes XIAO support via `ROVER_TARGET_XIAO_ESP32S3` macro
- No new component creation needed
- Existing component logic extends naturally to XIAO

### Testing Strategy
1. Verify build completes without errors
2. Test camera initialization and streaming
3. Test WiFi connectivity (AP and STA modes)
4. Test OTA flashing
5. Verify button inputs work (if wired)
6. Performance profiling (frame rate, memory usage)

---

## References

- [XIAO ESP32S3 Sense Datasheet](https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/)
- [ESP32-S3 Technical Reference Manual](https://www.espressif.com/sites/default/files/documentation/esp32-s3_technical_reference_manual_en.pdf)
- OV2640 Camera Module: Fixed pinout on XIAO, no configuration needed

---

## Future Enhancements

Once XIAO support is implemented:
1. Add button event handling for XIAO (sleep mode, AP switch)
2. Implement camera power management workaround (soft reset)
3. Add XIAO-specific LED indicators via GPIO
4. Memory profiling and optimization for XIAO's larger heap
5. Performance benchmarking vs ESP32-CAM

