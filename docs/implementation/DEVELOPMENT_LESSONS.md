# ESP32 Rover - Development Lessons Learned

This document captures key bugfixes, feature implementations, and lessons learned during development of the ESP32 Rover firmware. These insights can help avoid similar issues in future ESP32/embedded projects.

---

## Table of Contents

- [ESP32 Memory Architecture](#esp32-memory-architecture)
- [Hardware Resource Conflicts](#hardware-resource-conflicts)
- [Display Optimization](#display-optimization)
- [Responsive Input Handling](#responsive-input-handling)
- [System Diagnostics](#system-diagnostics)
- [Configuration System](#configuration-system)
- [WiFi Connectivity](#wifi-connectivity)
- [Web Server Concurrency](#web-server-concurrency)
- [Web UI and JavaScript Integration](#web-ui-and-javascript-integration)
- [Summary of Best Practices](#summary-of-best-practices)

---

## ESP32 Memory Architecture

Understanding ESP32's memory architecture is critical for embedded development. Unlike desktop systems with gigabytes of unified RAM, the ESP32 has multiple memory types with different characteristics, speeds, and constraints.

### Memory Types Overview

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        ESP32 Memory Map                                  │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  ┌───────────────────────────────────────────────────────────────────┐  │
│  │ IRAM (Instruction RAM) - ~128KB                                   │  │
│  │ • Time-critical code (ISRs, WiFi, performance-sensitive)          │  │
│  │ • Single-cycle access from CPU                                    │  │
│  │ • VERY LIMITED - overflow is common problem                       │  │
│  └───────────────────────────────────────────────────────────────────┘  │
│                                                                          │
│  ┌───────────────────────────────────────────────────────────────────┐  │
│  │ DRAM (Data RAM) - ~328KB                                          │  │
│  │ • Variables, heap allocations, stack                              │  │
│  │ • Single-cycle access from CPU                                    │  │
│  │ • Shared with IRAM in a flexible 520KB internal SRAM pool         │  │
│  └───────────────────────────────────────────────────────────────────┘  │
│                                                                          │
│  ┌───────────────────────────────────────────────────────────────────┐  │
│  │ Flash - 4MB+ (external SPI)                                       │  │
│  │ • Application code, constants, assets                             │  │
│  │ • Cache-backed (32KB cache)                                       │  │
│  │ • ~10x slower than IRAM, but virtually unlimited                  │  │
│  └───────────────────────────────────────────────────────────────────┘  │
│                                                                          │
│  ┌───────────────────────────────────────────────────────────────────┐  │
│  │ PSRAM (SPI RAM) - 4-8MB (optional, external)                      │  │
│  │ • Extended heap for large buffers                                 │  │
│  │ • ~10x slower than internal RAM                                   │  │
│  │ • Not available on all modules (TTGO T-Display has none)          │  │
│  └───────────────────────────────────────────────────────────────────┘  │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

### IRAM (Instruction RAM) - The Critical Resource

**What is IRAM?**
IRAM is a ~128KB region of internal SRAM dedicated to instructions that require single-cycle execution. This includes:

- **Interrupt Service Routines (ISRs)** - Functions marked with `IRAM_ATTR`
- **Time-critical code** - WiFi PHY, Bluetooth, FreeRTOS core
- **Code accessed during flash operations** - Can't execute from flash while writing to it

**Why IRAM Overflows Happen**:
```
Component               Typical IRAM Usage
─────────────────────  ────────────────────
FreeRTOS kernel         ~30-40KB
WiFi subsystem          ~40-50KB (with optimizations)
Bluetooth (if enabled)  ~40KB
Application ISRs        Variable
ESP-IDF drivers         Variable
─────────────────────  ────────────────────
TOTAL available         ~128KB
```

When the linker can't fit everything into IRAM, you get:
```
Error: IRAM0 segment data does not fit.
Overflow detected: 6484 bytes
```

**How to Diagnose IRAM Usage**:
```bash
# After building, check memory usage
idf.py size

# Detailed breakdown by component
idf.py size-components

# Map file shows exact symbol placement
cat build/esp32-rover.map | grep -A5 "\.iram0"
```

### Issue: IRAM Overflow on TTGO T-Display Build

**Symptom**: Build failed with:
```
IRAM0 segment data does not fit.
Region `iram0_0_seg' overflowed by 6484 bytes
```

**Root Cause**: The TTGO T-Display configuration was using default SDK settings that placed too much code in IRAM, including:
- WiFi IRAM optimizations (not needed at AP-mode speeds)
- FreeRTOS functions in IRAM (only needed for ISR-heavy applications)
- SPI ISR handlers in IRAM (not needed when not using SPI from ISRs)
- Ring buffer functions in IRAM (rarely needed)

**Investigation Process**:
1. Stashed all changes to test if it was new code causing overflow
2. Found overflow was **pre-existing** (6440 bytes without new code)
3. Concluded: default ESP-IDF settings are IRAM-heavy

**Solution**: Added IRAM optimization options to `sdkconfig.defaults.ttgo`:

```ini
# IRAM optimization - move non-ISR code to flash to free up IRAM

# Move FreeRTOS functions to flash (safe if you don't call them from ISRs)
CONFIG_FREERTOS_PLACE_FUNCTIONS_INTO_FLASH=y
CONFIG_FREERTOS_PLACE_SNAPSHOT_FUNS_INTO_FLASH=y

# Move ring buffer functions to flash
CONFIG_RINGBUF_PLACE_FUNCTIONS_INTO_FLASH=y
CONFIG_RINGBUF_PLACE_ISR_FUNCTIONS_INTO_FLASH=y

# Reduce assertion overhead
CONFIG_HAL_DEFAULT_ASSERTION_LEVEL=1

# Optimize for size instead of speed
CONFIG_COMPILER_OPTIMIZATION_SIZE=y

# Use flash ROM driver patch (saves IRAM)
CONFIG_SPI_FLASH_ROM_DRIVER_PATCH=y

# Disable IRAM-hungry options
CONFIG_ESP_EVENT_POST_FROM_ISR=n      # No event posting from ISRs
CONFIG_LWIP_IRAM_OPTIMIZATION=n       # TCP/IP doesn't need IRAM speed
CONFIG_ESP_WIFI_IRAM_OPT=n            # WiFi code can run from flash
CONFIG_ESP_WIFI_RX_IRAM_OPT=n         # WiFi RX path from flash
CONFIG_SPI_MASTER_ISR_IN_IRAM=n       # SPI master ISR not needed in IRAM
CONFIG_SPI_SLAVE_ISR_IN_IRAM=n        # SPI slave ISR not needed in IRAM
CONFIG_GPTIMER_ISR_HANDLER_IN_IRAM=n  # GP timer ISRs not needed

# Move heap functions to flash
CONFIG_HEAP_PLACE_FUNCTION_INTO_FLASH=y
```

**Result**: Build succeeded with 35% flash partition usage (vs. 100%+ IRAM overflow).

**Lesson Learned**:
- **Default ESP-IDF settings prioritize speed over IRAM conservation** - Fine for WROVER modules with more IRAM budget, problematic for constrained builds
- **Most IRAM optimizations aren't needed** - Unless you're calling those functions from ISRs or need sub-microsecond timing
- **Create board-specific sdkconfig.defaults** - Different modules have different constraints
- **Test IRAM usage early** - Add features incrementally and watch memory usage

### IRAM Optimization Decision Matrix

| Config Option | Keep in IRAM When... | Move to Flash When... |
|--------------|----------------------|------------------------|
| `ESP_WIFI_IRAM_OPT` | High-throughput WiFi needed | AP mode or light STA use |
| `ESP_WIFI_RX_IRAM_OPT` | Critical WiFi latency | Normal web server use |
| `FREERTOS_PLACE_FUNCTIONS_INTO_FLASH` | Calling FreeRTOS from ISRs | Normal task usage |
| `SPI_MASTER_ISR_IN_IRAM` | High-speed SPI from ISRs | Normal SPI peripherals |
| `LWIP_IRAM_OPTIMIZATION` | Network latency critical | Normal HTTP/MQTT use |
| `ESP_EVENT_POST_FROM_ISR` | Posting events from ISRs | Event posting from tasks only |

### DRAM (Data RAM) - Heap and Stack

**What is DRAM?**
DRAM is the internal SRAM used for:
- Heap allocations (`malloc()`, `calloc()`)
- Task stacks
- Global/static variables
- DMA buffers (must be in DRAM, not PSRAM)

**Typical DRAM Budget**:
```
Total internal SRAM:    ~520KB (shared between IRAM and DRAM)
Less IRAM usage:        ~128KB typical
Less static allocations: ~80KB (varies by app)
Less task stacks:       ~50KB (varies by task count)
─────────────────────────────────────────
Free heap at runtime:   ~100-200KB typical
```

**Key APIs**:
```c
// Check current free heap
size_t free = esp_get_free_heap_size();

// Check lowest heap since boot (watermark for leak detection)
size_t min_free = esp_get_minimum_free_heap_size();

// Check internal DRAM specifically
size_t internal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);

// Allocate from internal DRAM specifically (not PSRAM)
void *buf = heap_caps_malloc(size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
```

**When Internal DRAM is Required**:
- DMA buffers (LCD, camera, SPI)
- WiFi buffers
- Anything accessed at high frequency
- Interrupt-accessed data

### PSRAM (SPI RAM) - Extended Memory

**What is PSRAM?**
PSRAM is external SPI-connected RAM available on some ESP32 modules:
- ESP32-WROVER: 4MB PSRAM
- ESP32-CAM: 4MB PSRAM
- ESP32-S3-WROOM-2: 2-8MB PSRAM
- **TTGO T-Display: NO PSRAM**

**PSRAM Characteristics**:
```
Speed:          ~10x slower than internal SRAM
Access:         Through SPI cache, not direct
DMA:            NOT supported (most DMA peripherals)
Use case:       Large buffers, image data, non-time-critical storage
```

**Using PSRAM**:
```c
// Enable in sdkconfig
CONFIG_ESP32_SPIRAM_SUPPORT=y
CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=4096  // Use internal for <4KB

// Allocate explicitly from PSRAM
void *large_buf = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);

// Or let malloc() use PSRAM automatically (if configured)
void *buf = malloc(large_size);  // May come from PSRAM
```

**PSRAM vs Internal RAM Trade-offs**:

| Factor | Internal DRAM | PSRAM |
|--------|--------------|-------|
| Speed | Fast (single-cycle) | Slow (~10x slower) |
| Size | ~200KB free typical | 4-8MB |
| DMA | Supported | Not supported |
| Cache | Not cached | Cached (can cause contention) |
| Power | Lower | Higher (SPI activity) |

### Flash Memory - Code and Constants

**What Goes in Flash?**
- Application code (when not in IRAM)
- String constants, lookup tables
- Assets (HTML, images)
- OTA partitions
- NVS storage

**Flash Access Considerations**:
```
Speed:          Cached (~32KB cache), ~10x slower on cache miss
Execution:      Code can execute from flash via cache
Writing:        Blocks CPU during flash writes (use IRAM for ISRs)
Endurance:      ~100,000 write cycles per sector
```

**Code Placement Attributes**:
```c
// Force function into IRAM (fast, limited space)
void IRAM_ATTR critical_isr(void) { ... }

// Force into flash (slower, unlimited space) - rarely needed
void NOINLINE_ATTR large_function(void) { ... }

// Force data into DRAM (not flash)
static DRAM_ATTR uint8_t dma_buffer[1024];
```

### Memory Debugging Commands

```bash
# Build and check memory usage
idf.py size
# Output:
# Total sizes:
#  Used static DRAM:   78344 bytes ( 102392 remain)
#  Used static IRAM:  122316 bytes (   8756 remain)  ← Watch this!
#       Used Flash: 1021342 bytes

# Detailed per-component breakdown
idf.py size-components
# Shows which component uses how much IRAM/DRAM/Flash

# At runtime, check heap
ESP_LOGI(TAG, "Free heap: %lu, Min: %lu",
         esp_get_free_heap_size(),
         esp_get_minimum_free_heap_size());
```

### Memory Best Practices Summary

1. **Monitor IRAM during development** - Check `idf.py size` after adding features
2. **Use IRAM sparingly** - Only for true ISRs and time-critical code
3. **Create board-specific configs** - WROVER can afford more IRAM usage than basic ESP32
4. **Place large buffers in PSRAM** - When available, use for camera frames, audio buffers
5. **Track heap watermark** - `esp_get_minimum_free_heap_size()` catches leaks
6. **Use DRAM for DMA** - External PSRAM doesn't work with most DMA peripherals
7. **Profile before optimizing** - Don't assume; measure with `idf.py size-components`

---

## Hardware Resource Conflicts

### Issue: LCD Backlight Turning Off on Emergency Stop

**Symptom**: When the emergency stop button was pressed in the web UI, the LCD screen would turn off unexpectedly, even though e-stop should only zero the motor speed.

**Root Cause**: LEDC (PWM) channel conflict between the BLDC motor driver and LCD backlight.

```
Component          | LEDC Channels Used
-------------------|-------------------
BLDC Motor Phase A | LEDC_CHANNEL_0
BLDC Motor Phase B | LEDC_CHANNEL_1  <-- CONFLICT
BLDC Motor Phase C | LEDC_CHANNEL_2
Servo              | LEDC_CHANNEL_3
Camera XCLK        | LEDC_CHANNEL_4
LCD Backlight      | LEDC_CHANNEL_1  <-- CONFLICT (was)
```

When `bldc_motor_emergency_stop()` called `set_pwm_duty(handle, 0, 0, 0)`, it set `LEDC_CHANNEL_1` to 0, which also controlled the backlight.

**Solution**: Changed LCD backlight to use `LEDC_CHANNEL_5`:

```c
// lcd_display.c - Before (buggy)
.channel = LEDC_CHANNEL_1,

// lcd_display.c - After (fixed)
.channel = LEDC_CHANNEL_5,  // Avoid conflict with BLDC motor (0,1,2)
```

**Lesson Learned**:
- **Always document hardware resource allocation** in a central location
- Create a resource map showing which GPIO pins, LEDC channels, timers, I2C ports, and SPI buses are used by each component
- Check for conflicts before adding new peripherals
- Consider creating a `hardware_resources.h` that defines all allocations

### Recommended Resource Allocation Table

Add this to your project documentation:

```
LEDC Channels (ESP32 has 8 low-speed, 8 high-speed):
  Channel 0: BLDC Motor Phase A
  Channel 1: BLDC Motor Phase B
  Channel 2: BLDC Motor Phase C
  Channel 3: Servo PWM
  Channel 4: Camera XCLK (ESP32-CAM only)
  Channel 5: LCD Backlight (TTGO only)
  Channel 6: (available)
  Channel 7: (available)

LEDC Timers:
  Timer 0: BLDC Motor (20kHz)
  Timer 1: LCD Backlight (5kHz), Servo (50Hz uses own timer config)
```

---

## Display Optimization

### Issue: Flickering Uptime Display

**Symptom**: The uptime counter (HH:MM:SS) on the LCD appeared to flicker/jump because the entire string was redrawn every refresh cycle (20Hz).

**Root Cause**: Redrawing unchanged pixels wastes SPI bandwidth and causes visible flicker, especially on characters that don't change frequently (hours, minutes).

**Solution**: Implement character-level dirty tracking:

```c
static char s_prev_uptime_str[9] = "";  // Track previous state

// Only redraw characters that changed
for (int i = 0; i < 8; i++) {
    if (s_prev_uptime_str[i] != uptime_str[i]) {
        lcd_draw_char(uptime_x + i * char_width, uptime_y, uptime_str[i],
                     COLOR_GREEN, COLOR_DARKGRAY, 1);
    }
}
memcpy(s_prev_uptime_str, uptime_str, 9);
```

**Performance Impact**:
- Before: 8 characters redrawn per frame (20Hz = 160 char draws/sec)
- After: ~1-2 characters redrawn per frame (20-40 char draws/sec)
- Result: 75-85% reduction in SPI traffic for uptime display

**Lesson Learned**:
- **Track previous state for all dynamic display elements**
- Only send pixel data to the display when it actually changes
- This applies to: text, icons, progress bars, status indicators
- Consider double-buffering for complex animations

### Applied to Other Elements

The same pattern was applied to:
- Button indicators (only redraw on state change)
- WiFi info section (draw once, never redraw)
- Status header bar (redraw only on connection state change)

### Improvement: 60Hz Display Refresh Rate

**Request**: The display looked sluggish at 20Hz refresh rate; user requested 60Hz for smoother updates.

**Changes Made**:

1. **Increased LCD task frequency** - Reduced delay from 50ms to 16ms:
```c
// Before
vTaskDelay(pdMS_TO_TICKS(50));  // ~20Hz

// After
vTaskDelay(pdMS_TO_TICKS(16));  // ~60Hz
```

2. **Boosted SPI clock speed** - Increased from 26MHz to 40MHz:
```c
// Before
.clock_speed_hz = 26 * 1000 * 1000,  // 26 MHz

// After
.clock_speed_hz = 40 * 1000 * 1000,  // 40 MHz (ST7789 supports up to 80MHz)
```

3. **Enabled SPI half-duplex mode** - Allows faster transfers since display is write-only:
```c
.flags = SPI_DEVICE_NO_DUMMY | SPI_DEVICE_HALFDUPLEX,
```

**Why This Works**:
- ST7789 display controller supports up to 80MHz SPI clock
- Half-duplex mode eliminates read overhead (display is write-only)
- Dirty tracking (implemented earlier) ensures we don't waste bandwidth on unchanged pixels
- Combined with dirty tracking, 60Hz is achievable without overwhelming the SPI bus

**Lesson Learned**:
- **Know your hardware limits** - ST7789 supports 80MHz, so 40MHz is conservative
- **Use half-duplex when possible** - Write-only peripherals don't need full-duplex overhead
- **Higher refresh + dirty tracking = best of both worlds** - Smooth updates without wasted bandwidth
- **Test incrementally** - Going from 26MHz to 40MHz (not 80MHz) is safer

---

## Responsive Input Handling

### Issue: Slow Button Response in Web UI

**Symptom**: Pressing physical buttons on the TTGO T-Display showed a noticeable delay (up to 700ms) before the web UI updated.

**Root Cause**: Multiple layers of polling with slow intervals:

```
Layer                  | Original Interval | Contribution
-----------------------|-------------------|-------------
GPIO polling           | 200ms (5Hz)       | Up to 200ms delay
Status task update     | 200ms (5Hz)       | Up to 200ms delay
Web UI status fetch    | 500ms (2Hz)       | Up to 500ms delay
-----------------------|-------------------|-------------
Total worst-case       |                   | ~700ms latency
```

**Solution**: Three-part fix:

1. **GPIO Interrupts** - Capture button state changes immediately:
```c
static volatile bool s_button_left_pressed = false;
static volatile bool s_button_right_pressed = false;

static void IRAM_ATTR button_isr_handler(void* arg)
{
    s_button_left_pressed = (gpio_get_level(BUTTON_LEFT_PIN) == 0);
    s_button_right_pressed = (gpio_get_level(BUTTON_RIGHT_PIN) == 0);
}

// Configure with ANYEDGE to catch both press and release
.intr_type = GPIO_INTR_ANYEDGE,
```

2. **Faster Status Updates** - Increase server-side polling:
```c
// Before
vTaskDelay(pdMS_TO_TICKS(200));  // 5Hz

// After
vTaskDelay(pdMS_TO_TICKS(50));   // 20Hz
```

3. **Faster Web Polling** - Reduce client-side interval:
```javascript
// Before
const STATUS_INTERVAL = 500;  // 2Hz

// After
const STATUS_INTERVAL = 100;  // 10Hz
```

**Result**:
```
Layer                  | New Interval | Contribution
-----------------------|--------------|-------------
GPIO interrupt         | 0ms          | Instant capture
Status task update     | 50ms (20Hz)  | Up to 50ms delay
Web UI status fetch    | 100ms (10Hz) | Up to 100ms delay
-----------------------|--------------|-------------
Total worst-case       |              | ~150ms latency
```

**Lesson Learned**:
- **Use interrupts for user input** - Polling misses fast events and adds latency
- **Consider the full latency chain** - Every layer adds delay
- **Balance responsiveness vs. CPU load** - 20Hz polling is fine for status, but don't go crazy
- **Mark ISR variables as `volatile`** - Prevents compiler optimization issues
- **Use `IRAM_ATTR`** - ISR code must be in IRAM for reliable execution

### Issue: Slow Button Response on LCD Display

**Symptom**: After fixing the web UI button latency, the LCD display still showed slow button response. Web UI was fast, but the physical display lagged.

**Root Cause**: The `lcd_display_update()` function was redrawing almost everything on every frame, even when nothing changed. Button indicators were drawn *last*, after all other elements:

```
Update order (before):    Time spent before buttons
------------------------  -------------------------
1. Header bar             ~2ms (redraw every frame)
2. E-STOP indicator       ~1ms (redraw every frame)
3. Speed text + bar       ~4ms (redraw every frame)
4. Steering text + bar    ~4ms (redraw every frame)
5. Velocity text          ~2ms (redraw every frame)
6. Battery text + bar     ~3ms (redraw every frame)
7. Button indicators      <-- Finally! (~16ms total delay)
```

Even at 60Hz (16ms frame time), the SPI transactions for all those redraws blocked before reaching the button update.

**Solution**: Two-part fix:

1. **Draw buttons FIRST** - Prioritize the most latency-sensitive element:
```c
esp_err_t lcd_display_update(const lcd_rover_status_t *status)
{
    // BUTTON INDICATORS - Draw FIRST for lowest latency!
    if ((int8_t)status->button_left != s_prev_button_left) {
        // Redraw left button...
    }
    if ((int8_t)status->button_right != s_prev_button_right) {
        // Redraw right button...
    }

    // Then draw everything else...
}
```

2. **Add dirty tracking to ALL elements** - Only redraw when values change:
```c
// State tracking variables
static int8_t s_prev_connected = -1;
static int8_t s_prev_estop = -1;
static int16_t s_prev_speed = INT16_MIN;
static int16_t s_prev_steer = INT16_MIN;
static int16_t s_prev_velocity_x10 = INT16_MIN;
static int16_t s_prev_battery_x100 = INT16_MIN;

// Only redraw when value actually changes
if (status->speed_percent != s_prev_speed) {
    // Redraw speed...
    s_prev_speed = status->speed_percent;
}
```

**Result**:
```
Update order (after):     Time spent before buttons
------------------------  -------------------------
1. Button indicators      ~0ms (drawn first!)
2. Header bar             ~0ms (skip if unchanged)
3. E-STOP indicator       ~0ms (skip if unchanged)
4. Speed (only if changed) ~0ms typical
5. Steering (only if changed) ~0ms typical
6. Velocity (only if changed) ~0ms typical
7. Battery (only if changed) ~0ms typical
```

**Performance Impact**:
- Before: ~16ms delay before button update (blocking SPI redraws)
- After: ~0ms delay (buttons drawn first, no blocking)
- Worst case (all values changed): Still faster due to dirty tracking

**Lesson Learned**:
- **Draw latency-critical elements first** - Order matters for perceived responsiveness
- **Apply dirty tracking to ALL dynamic elements** - Not just the ones that obviously flicker
- **SPI transactions block** - Long display updates delay everything that comes after
- **Use sentinel values** (like `INT16_MIN`) - Guarantees first-draw detection

---

## System Diagnostics

### Feature: Diagnostic Mode with Memory Monitoring

**Implementation**: Hold both buttons for 3 seconds to enter a diagnostic screen showing:

- WiFi info (SSID, channel, TX power, connected clients, IP, MAC)
- Memory stats (heap usage with visual bar, internal RAM, watermark)
- System info (CPU frequency, task count, battery, uptime)

**Key APIs Used**:

```c
// Memory monitoring
esp_get_free_heap_size()           // Current free heap
esp_get_minimum_free_heap_size()   // Lowest point since boot (watermark)
heap_caps_get_total_size(MALLOC_CAP_DEFAULT)    // Total heap
heap_caps_get_free_size(MALLOC_CAP_INTERNAL)    // Free internal SRAM
heap_caps_get_free_size(MALLOC_CAP_SPIRAM)      // Free PSRAM

// Task monitoring
uxTaskGetNumberOfTasks()           // Count of FreeRTOS tasks

// WiFi monitoring
esp_wifi_ap_get_sta_list(&sta_list) // Connected stations
esp_wifi_get_max_tx_power(&power)   // TX power (in 0.25dBm units)
```

**Memory Watermark Insight**: The `esp_get_minimum_free_heap_size()` function returns the lowest free heap value since boot. This is invaluable for detecting memory leaks:
- If watermark keeps decreasing over time → memory leak
- If watermark stabilizes → memory usage is bounded

**Lesson Learned**:
- **Build diagnostics into firmware from the start** - Invaluable for field debugging
- **Memory watermark is your friend** - Catches leaks before they crash
- **Physical button access to diagnostics** - Works even when WiFi/web UI fails
- **Show heap percentage with color coding** - Quick visual health check

### Feature: Web UI Diagnostics Panel

**Goal**: Mirror the LCD diagnostic screen in the web UI so users can access the same information remotely.

**Implementation**:

1. **Extended `rover_status_t` struct** to include diagnostic fields:
```c
typedef struct {
    // ... existing fields ...

    // Diagnostic data (mirrors LCD diagnostics)
    const char* wifi_ssid;          // AP SSID
    const char* wifi_ip;            // IP address
    const char* mac_addr;           // MAC address
    uint8_t wifi_channel;           // WiFi channel
    uint8_t connected_clients;      // Number of connected stations
    int8_t wifi_tx_power;           // TX power in dBm
    uint32_t free_heap;             // Free heap memory
    uint32_t min_free_heap;         // Minimum free heap since boot
    uint32_t total_heap;            // Total heap memory
    uint32_t free_internal;         // Free internal RAM
    uint32_t uptime_secs;           // Uptime in seconds
    float cpu_freq_mhz;             // CPU frequency
    uint8_t task_count;             // Number of running tasks
} rover_status_t;
```

2. **Extended JSON `/status` endpoint** with nested `diag` object:
```json
{
  "velocity": 0.0,
  "battery": 3.85,
  "diag": {
    "ssid": "ESP32-Rover",
    "ip": "192.168.4.1",
    "mac": "C4:4F:33:6A:4E:61",
    "channel": 1,
    "clients": 1,
    "txPower": 20,
    "freeHeap": 140000,
    "minHeap": 120000,
    "totalHeap": 290000,
    "freeInternal": 140000,
    "uptime": 932,
    "cpuFreq": 240,
    "tasks": 12
  }
}
```

3. **Collapsible UI panel** in web interface with color-coded RAM bar matching LCD.

**Lesson Learned**:
- **Reuse diagnostic data collection** - Same code populates both LCD and web UI
- **Use collapsible panels** - Keeps the main UI clean while providing detailed info on demand
- **Match visual design** - Same color coding (green/yellow/red) creates consistency

### Feature: CPU Usage Monitoring (REQ-SW-035)

**Goal**: Display CPU usage percentage alongside RAM usage in the web UI diagnostics panel.

**Implementation Challenge**: FreeRTOS provides `vTaskGetRunTimeStats()` but calculating CPU "busy" percentage requires:
1. Tracking idle task runtime over time
2. Calculating delta between measurements
3. Averaging across both cores on dual-core ESP32

**Solution**:

1. **Enable runtime stats in sdkconfig**:
```
CONFIG_FREERTOS_USE_TRACE_FACILITY=y
CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS=y
```

2. **Track idle task runtime deltas**:
```c
static uint32_t s_last_idle_runtime_core0 = 0;
static uint32_t s_last_idle_runtime_core1 = 0;
static uint32_t s_last_total_runtime = 0;

static uint8_t calculate_cpu_usage(void) {
#if configGENERATE_RUN_TIME_STATS
    TaskHandle_t idle_core0 = xTaskGetIdleTaskHandleForCore(0);
    TaskHandle_t idle_core1 = xTaskGetIdleTaskHandleForCore(1);

    TaskStatus_t task_status;
    uint32_t idle_runtime_core0 = 0, idle_runtime_core1 = 0;

    // Get current idle task runtimes
    vTaskGetInfo(idle_core0, &task_status, pdFALSE, eRunning);
    idle_runtime_core0 = task_status.ulRunTimeCounter;
    vTaskGetInfo(idle_core1, &task_status, pdFALSE, eRunning);
    idle_runtime_core1 = task_status.ulRunTimeCounter;

    // Use esp_timer for elapsed time (microseconds)
    uint32_t current_time = (uint32_t)(esp_timer_get_time() / 1000);
    uint32_t elapsed = current_time - s_last_total_runtime;

    if (elapsed > 0 && s_last_total_runtime > 0) {
        // Calculate idle deltas
        uint32_t idle_delta_0 = idle_runtime_core0 - s_last_idle_runtime_core0;
        uint32_t idle_delta_1 = idle_runtime_core1 - s_last_idle_runtime_core1;
        uint32_t total_idle = idle_delta_0 + idle_delta_1;

        // CPU usage = 100% - idle% (averaged across 2 cores)
        uint32_t total_time = elapsed * 2;  // 2 cores
        uint32_t busy_time = (total_time > total_idle) ? (total_time - total_idle) : 0;
        return (uint8_t)((busy_time * 100) / total_time);
    }

    // Save for next calculation
    s_last_idle_runtime_core0 = idle_runtime_core0;
    s_last_idle_runtime_core1 = idle_runtime_core1;
    s_last_total_runtime = current_time;

    return 0;
#else
    return 0;
#endif
}
```

3. **Add to status JSON and web UI** with matching horizontal bar.

**Key Insight**: The `ulRunTimeCounter` in FreeRTOS gives absolute runtime since boot. You must:
- Calculate deltas between measurements (not absolute values)
- Use `esp_timer_get_time()` for elapsed wall-clock time
- Account for dual-core by summing idle time from both cores
- Update values even on first call (return 0 until second call)

**Lesson Learned**:
- **sdkconfig.defaults vs sdkconfig** - Settings in defaults are only applied if sdkconfig is regenerated. Delete sdkconfig for a clean build.
- **esp_timer vs uxTaskGetSystemState** - The total runtime from `uxTaskGetSystemState` didn't match wall-clock time. `esp_timer_get_time()` gives reliable microseconds since boot.
- **Dual-core considerations** - ESP32 has two cores, each with its own idle task. Sum idle from both cores and divide by 2 for average CPU usage.

### Feature: Battery Voltage Low-Pass Filter

**Symptom**: Battery voltage on LCD/web UI flickered due to ADC noise.

**Root Cause**: Raw ADC readings fluctuate by ±50mV even with a stable battery, causing the display to constantly update and flicker.

**Solution**: Exponential Moving Average (EMA) filter:

```c
#define BATTERY_FILTER_ALPHA 0.02f  // ~10 second time constant at 20Hz
static float s_battery_voltage_filtered = 0.0f;
static bool s_battery_filter_initialized = false;

float read_battery_voltage(void) {
    // ... read raw ADC ...

    // Apply EMA low-pass filter
    if (!s_battery_filter_initialized) {
        s_battery_voltage_filtered = battery_voltage;
        s_battery_filter_initialized = true;
    } else {
        s_battery_voltage_filtered = BATTERY_FILTER_ALPHA * battery_voltage +
                                    (1.0f - BATTERY_FILTER_ALPHA) * s_battery_voltage_filtered;
    }
    return s_battery_voltage_filtered;
}
```

**Why Alpha = 0.02**:
- At 20Hz sampling, this gives a ~10 second time constant
- Smooths out ADC noise while still tracking actual battery drain
- Combined with dirty tracking (0.01V resolution), eliminates display flicker

**Lesson Learned**:
- **Filter noisy sensor data** - Don't pass raw ADC values to UI
- **Choose time constant carefully** - Too fast = flicker, too slow = unresponsive
- **Initialize filter with first reading** - Avoids startup glitch from zero

### Issue: ADC Timeout Crash During WiFi Activity

**Symptom**: Firmware would crash randomly with `ESP_ERR_TIMEOUT` in `adc_oneshot_read()`, especially when WiFi clients were connected.

**Root Cause**: On ESP32, WiFi and ADC1 share some hardware resources. During heavy WiFi activity, ADC reads can timeout. The code was using `ESP_ERROR_CHECK()` which calls `abort()` on any error:

```c
// WRONG - crashes on timeout
ESP_ERROR_CHECK(adc_oneshot_read(s_adc_handle, BATTERY_ADC_CHANNEL, &raw_value));
```

**Solution**: Handle the timeout gracefully by returning the last filtered value:

```c
// CORRECT - handle timeout gracefully
esp_err_t ret = adc_oneshot_read(s_adc_handle, BATTERY_ADC_CHANNEL, &raw_value);
if (ret != ESP_OK) {
    // ADC read can timeout during WiFi activity - return last filtered value
    return s_battery_voltage_filtered;
}
```

**Lesson Learned**:
- **Never use `ESP_ERROR_CHECK` for operations that can legitimately fail** - ADC reads during WiFi, network timeouts, etc.
- **WiFi and ADC1 conflict on ESP32** - Be prepared for ADC timeouts when WiFi is active
- **Return last known good value** - For sensor readings, stale data is better than a crash
- **Reserve `ESP_ERROR_CHECK` for init-time operations** - Where failure means the system can't function anyway

---

## Configuration System

### Feature: YAML-Based Compile-Time Configuration

**Goal**: Provide an easy way to configure firmware settings without modifying C header files.

**Implementation**: A `rover_config.yaml` file defines all compile-time options, and `generate_config.py` converts it to C preprocessor definitions.

**Example `rover_config.yaml`**:
```yaml
target: ttgo

wifi:
  mode: sta_first  # ap_only, sta_only, or sta_first
  ap:
    ssid: "ESP32-Rover"
    password: "rover1234"
    channel: 1
  sta:
    ssid: "MyHomeNetwork"
    password: "MyPassword"
    connect_timeout: 10

rest_api:
  enabled: true
  cache_interval_ms: 1000

mqtt:
  enabled: true
  broker:
    host: "192.168.1.100"
    port: 1883
  topic_prefix: "esp32-rover"
  publish_interval_ms: 5000
```

**Generated `config_generated.h`**:
```c
#define WIFI_MODE_STA_FIRST 1
#define WIFI_AP_SSID "ESP32-Rover"
#define WIFI_STA_SSID "MyHomeNetwork"
#define ENABLE_REST_API 1
#define ENABLE_MQTT 1
#define MQTT_BROKER_HOST "192.168.1.100"
// ... etc
```

**Usage**:
```bash
python generate_config.py   # Regenerates main/config_generated.h
idf.py build                # Compile with new settings
```

**Lesson Learned**:
- **Separate configuration from code** - YAML is more user-friendly than C headers
- **Auto-generate C headers** - Eliminates manual sync errors
- **Use compile-time toggles** - Features like MQTT can be completely compiled out
- **Include defaults** - Config generator should work even with minimal YAML

---

## WiFi Connectivity

### Feature: STA-First with AP Fallback

**Goal**: Connect to a home WiFi network when available, but provide a fallback AP mode for initial setup or when the network is unavailable.

**Implementation**: Three WiFi modes controlled by compile-time flags:
- `WIFI_MODE_AP_ONLY` - Always start as access point
- `WIFI_MODE_STA_ONLY` - Only try station mode, fail if unavailable
- `WIFI_MODE_STA_FIRST` - Try station first, fall back to AP on failure

**Connection Flow (STA-First)**:
```
Boot
  │
  ├─► Try STA connection
  │     │
  │     ├─► Success ──► Run in STA mode (use home network IP)
  │     │
  │     └─► Timeout (10s) ──► Switch to AP mode
  │                              │
  │                              └─► Run in AP mode (192.168.4.1)
```

**Key Code**:
```c
static esp_err_t wifi_init(void)
{
#if WIFI_MODE_STA_FIRST
    ESP_LOGI(TAG, "WiFi mode: STA-first with AP fallback");
    if (wifi_try_sta_connect()) {
        return ESP_OK;  // Connected to STA network
    }
    ESP_LOGW(TAG, "STA connection failed, falling back to AP mode");
    return wifi_switch_to_ap();
#else
    // AP-only mode
    return wifi_start_ap(false);
#endif
}
```

**Lesson Learned**:
- **Use event groups for connection waiting** - `xEventGroupWaitBits()` with timeout
- **Don't deinit WiFi when switching modes** - Just stop and reconfigure
- **Create both AP and STA netifs** - Both needed for switching modes
- **Track current mode for status reporting** - IP and SSID change dynamically

### Feature: REST API with Compile-Time Toggle

**Goal**: Provide a `/status` endpoint for diagnostic data, with ability to disable at compile time.

**Implementation**:
```c
// In web_server.c
#if ENABLE_REST_API
static esp_err_t status_handler(httpd_req_t *req) {
    // Return JSON with diagnostic data
}
#endif

esp_err_t web_server_init(...) {
#if ENABLE_REST_API
    httpd_register_uri_handler(server, &uri_status);
    ESP_LOGI(TAG, "REST API enabled (/status endpoint)");
#endif
}
```

**Lesson Learned**:
- **Use `#if` not `#ifdef`** - Allows `ENABLE_REST_API 0` to disable
- **Log when features are enabled/disabled** - Helps with debugging
- **Keep stubs when disabled** - Prevents undefined symbol errors

### Feature: MQTT Telemetry Service

**Goal**: Publish rover status to an MQTT broker for remote monitoring.

**Implementation**: Separate `mqtt_service` component with:
- Configurable broker, credentials, topic prefix
- Periodic publishing task
- JSON payload matching REST API format
- Only starts when in STA mode (requires network)

**Key Code**:
```c
// In main.c
#if defined(ENABLE_MQTT) && ENABLE_MQTT
    if (wifi_is_sta_mode()) {
        ret = init_mqtt_service();
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "MQTT init failed (continuing without MQTT)");
        }
    } else {
        ESP_LOGI(TAG, "MQTT disabled - not in STA mode");
    }
#endif
```

**Lesson Learned**:
- **MQTT requires external connectivity** - Only start when connected to a real network
- **Use stub implementations** - When MQTT disabled, provide no-op functions
- **Match REST API format** - Same JSON structure for consistency
- **Don't crash on broker unavailable** - Log warning and continue

---

## Web Server Concurrency

### Issue: MJPEG Stream Blocking Other HTTP Requests

**Symptom**: When viewing the camera stream in the web GUI, all other requests (`/status`, `/control`) stopped working. The telemetry values and button indicators never updated while the stream was active. Disconnecting from the stream immediately restored functionality.

**Root Cause**: ESP-IDF's httpd server uses a work queue model where handlers run on the httpd task. The MJPEG stream handler contained an infinite `while(true)` loop:

```c
// BLOCKING - prevents other requests from being processed
static esp_err_t stream_handler(httpd_req_t *req)
{
    while (true) {
        camera_fb_t *fb = camera_capture_frame();
        // ... send frame ...
        vTaskDelay(pdMS_TO_TICKS(66));  // ~15 FPS
    }
    return res;
}
```

The `vTaskDelay()` yields the CPU but the handler never returns, so the httpd work queue remains blocked and no other URI handlers can execute.

**Initial (Failed) Fix Attempt**: Increased `max_open_sockets`:
```c
http_config.max_open_sockets = 10;  // Default is 7
```
This allowed more connections but didn't solve the problem because httpd still only has one work queue.

**Solution**: Use ESP-IDF's async request handling to run the stream in a separate FreeRTOS task:

```c
// Async task data
typedef struct {
    httpd_req_t *req;
    int socket_fd;
} stream_task_data_t;

// Stream task runs independently of httpd work queue
static void stream_task(void *pvParameters)
{
    stream_task_data_t *data = (stream_task_data_t *)pvParameters;
    httpd_req_t *req = data->req;

    while (true) {
        camera_fb_t *fb = camera_capture_frame();
        if (!fb) break;

        esp_err_t res = httpd_resp_send_chunk(req, ...);
        if (res != ESP_OK) {
            camera_return_frame(fb);
            break;  // Client disconnected
        }
        camera_return_frame(fb);
        vTaskDelay(pdMS_TO_TICKS(66));
    }

    // CRITICAL: Complete the async request when done
    httpd_req_async_handler_complete(req);
    free(data);
    vTaskDelete(NULL);
}

// Handler returns immediately after spawning task
static esp_err_t stream_handler(httpd_req_t *req)
{
    // Start async handling - this allows httpd to process other requests
    httpd_req_t *async_req = NULL;
    esp_err_t res = httpd_req_async_handler_begin(req, &async_req);
    if (res != ESP_OK) return res;

    // Allocate task data
    stream_task_data_t *task_data = malloc(sizeof(stream_task_data_t));
    task_data->req = async_req;
    task_data->socket_fd = httpd_req_to_sockfd(req);

    // Create stream task - handler returns immediately
    xTaskCreatePinnedToCore(
        stream_task,
        "mjpeg_stream",
        4096,
        task_data,
        3,
        &stream_task_handle,
        0  // Core 0
    );

    return ESP_OK;  // Handler returns, httpd work queue unblocked!
}
```

**Key API Functions**:
- `httpd_req_async_handler_begin(req, &async_req)` - Creates a copy of the request for async use
- `httpd_req_async_handler_complete(req)` - MUST be called when async operation finishes
- `httpd_req_to_sockfd(req)` - Gets socket FD (useful for tracking which client)

**Why This Works**:
```
BEFORE (blocking):                    AFTER (async):
┌─────────────────┐                  ┌─────────────────┐
│   httpd task    │                  │   httpd task    │
├─────────────────┤                  ├─────────────────┤
│ stream_handler  │ ◄─ BLOCKED      │ stream_handler  │ ◄─ returns immediately
│   while(true)   │                  │  spawn task     │
│     ...         │                  └────────┬────────┘
│     ...         │                           │
│   never returns │                  ┌────────▼────────┐
└─────────────────┘                  │  stream_task    │ (separate FreeRTOS task)
                                     │   while(true)   │
/status blocked!                     │     ...         │
/control blocked!                    └─────────────────┘

                                     /status ✓ works!
                                     /control ✓ works!
```

**Important Considerations**:
1. **Memory cleanup** - Free task data and call `httpd_req_async_handler_complete()` when done
2. **Single stream limit** - Only allow one stream at a time (check `stream_task_handle != NULL`)
3. **Client disconnect detection** - Check `httpd_resp_send_chunk()` return value
4. **Task priority** - Stream task should be lower priority than motor control

**Lesson Learned**:
- **Async handlers are essential for long-running HTTP operations** - Streams, file downloads, etc.
- **Increasing `max_open_sockets` doesn't fix blocking handlers** - Still one work queue
- **Always call `httpd_req_async_handler_complete()`** - Memory leak otherwise
- **Use separate task for any handler that takes > 100ms** - Keep httpd responsive
- **Check for client disconnection** - Exit the loop cleanly when client closes connection

### When to Use Async Handlers

| Scenario | Use Async? | Reason |
|----------|------------|--------|
| Serve static HTML | No | Returns in milliseconds |
| Return JSON status | No | Returns in milliseconds |
| MJPEG camera stream | **Yes** | Runs indefinitely |
| Large file download | **Yes** | Takes seconds/minutes |
| WebSocket handler | **Yes** | Long-lived connection |
| POST with small body | No | Quick parsing and response |

---

## Web UI and JavaScript Integration

### Issue: Web UI Diagnostics Not Refreshing

**Symptom**: The diagnostics panel in the web UI showed static values that never updated. All metrics (uptime, memory, WiFi info) remained at their initial values. The connection status indicator worked, but telemetry values were frozen.

**Root Cause**: The `/status` JSON response was missing the `velocity` field that the JavaScript expected. When the JavaScript tried to call `.toFixed(1)` on `undefined`, it threw a `TypeError`. This exception was caught by the `catch(e)` block which silently ignored the error, preventing all DOM updates in `fetchStatus()`.

**JavaScript code expecting velocity**:
```javascript
// In fetchStatus() - line 832-833 of web_ui.c
document.getElementById('tel-velocity').textContent =
    data.velocity.toFixed(1) + ' rad/s';
```

**Original /status response (missing velocity)**:
```json
{
  "target": "esp32cam",
  "battery": 5.64,
  "camera": true,
  "diag": { ... }
}
```

**Investigation Process**:
1. Verified REST API `/status` endpoint was working via `curl` - returned valid JSON
2. Examined web_ui.c JavaScript code for `fetchStatus()` function
3. Found `setInterval(fetchStatus, STATUS_INTERVAL)` was correctly set up (100ms)
4. Noticed `fetchStatus()` had a silent `catch(e)` block
5. Compared JavaScript's expected fields against actual JSON response
6. Found `data.velocity.toFixed(1)` would fail because `velocity` was undefined

**Solution**: Added `velocity` field to the `/status` JSON response in `web_server.c`:

```c
// In status_handler() - web_server.c
snprintf(response, sizeof(response),
    "{"
    "\"target\":\"%s\","
    "\"velocity\":%.1f,"    // Added this line
    "\"battery\":%.2f,"
    ...
    target_str,
    0.0f,  // velocity - placeholder for UI compatibility
    status.battery_voltage,
    ...
```

**Lesson Learned**:
- **Silent catch blocks hide errors** - At minimum, log to console in development
- **Keep API contracts in sync** - When UI expects a field, backend must provide it
- **Test incrementally** - A single undefined field can break an entire update function
- **Defensive JavaScript** - Use optional chaining (`data.velocity?.toFixed(1)`) or default values (`(data.velocity || 0).toFixed(1)`)

**Prevention Pattern**:
```javascript
// Better error handling in fetchStatus()
async function fetchStatus() {
    try {
        const response = await fetch('/status');
        if (response.ok) {
            const data = await response.json();
            // Use defensive access patterns
            document.getElementById('tel-velocity').textContent =
                (data.velocity ?? 0).toFixed(1) + ' rad/s';
            // ... rest of updates
        }
    } catch (e) {
        console.error('Status fetch failed:', e);  // Don't silently ignore!
    }
}
```

---

## Summary of Best Practices

### 1. Memory Management
- [ ] Monitor IRAM usage with `idf.py size` after adding features
- [ ] Create board-specific sdkconfig.defaults for constrained modules
- [ ] Use PSRAM for large buffers when available (camera frames, audio)
- [ ] Mark only true ISR code with `IRAM_ATTR` - keep IRAM lean
- [ ] Disable IRAM optimizations not needed for your use case
- [ ] Track heap watermark to detect memory leaks
- [ ] Use `MALLOC_CAP_INTERNAL` for DMA buffers

### 2. Hardware Resource Management
- [ ] Create a central resource allocation document
- [ ] Check for LEDC channel conflicts before adding PWM peripherals
- [ ] Document GPIO assignments with boot restrictions noted
- [ ] Verify I2C address conflicts when adding sensors

### 2. Display Performance
- [ ] Track previous state for all dynamic elements
- [ ] Only redraw pixels that actually change
- [ ] Draw static content once, not every frame
- [ ] Consider SPI bandwidth when designing refresh rates

### 3. Input Responsiveness
- [ ] Use GPIO interrupts for buttons, not polling
- [ ] Mark ISR-accessed variables as `volatile`
- [ ] Place ISR functions in IRAM with `IRAM_ATTR`
- [ ] Audit the full latency chain from input to display

### 4. System Monitoring
- [ ] Include heap monitoring in production firmware
- [ ] Track minimum free heap (watermark) for leak detection
- [ ] Provide physical access to diagnostics (button combo)
- [ ] Log task count to detect runaway task creation

### 5. Debugging Strategies
- [ ] Add debug flags in config.h for each subsystem
- [ ] Use ESP_LOGI/W/E consistently with component tags
- [ ] Build serial monitor access into the workflow
- [ ] Create diagnostic screens accessible without network

### 6. Web Server Architecture
- [ ] Use async handlers for long-running operations (streams, downloads)
- [ ] Never block httpd work queue with infinite loops
- [ ] Always call `httpd_req_async_handler_complete()` when async operation ends
- [ ] Limit concurrent streams to prevent resource exhaustion
- [ ] Check return values of `httpd_resp_send_chunk()` to detect disconnects

---

## Commits Reference

| Commit | Description |
|--------|-------------|
| `cb75159` | Add diagnostic mode with WiFi and system info |
| `15ce905` | Add memory stats and task count to diagnostic screen |
| `17db3d6` | Fix LCD backlight turning off on emergency stop |
| `4a68d55` | Improve button responsiveness with GPIO interrupts |
| `e583ef9` | Increase LCD refresh rate to 60Hz and boost SPI speed |
| `17aa451` | Add CPU usage monitoring to web UI diagnostics (REQ-SW-035) |

---

## Further Reading

### Memory Architecture
- [ESP32 Memory Types](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/mem_alloc.html)
- [IRAM and DRAM](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/memory-types.html)
- [Support for External RAM (PSRAM)](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/external-ram.html)
- [Heap Memory Allocation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/heap_debug.html)
- [Minimizing RAM Usage](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/performance/ram-usage.html)

### Peripherals and System
- [ESP-IDF LEDC Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/ledc.html)
- [ESP-IDF GPIO Interrupts](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/gpio.html)
- [FreeRTOS Task Utilities](https://www.freertos.org/a00021.html)
