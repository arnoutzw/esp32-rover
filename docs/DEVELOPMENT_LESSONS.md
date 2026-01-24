# ESP32 Rover - Development Lessons Learned

This document captures key bugfixes, feature implementations, and lessons learned during development of the ESP32 Rover firmware. These insights can help avoid similar issues in future ESP32/embedded projects.

---

## Table of Contents

- [Hardware Resource Conflicts](#hardware-resource-conflicts)
- [Display Optimization](#display-optimization)
- [Responsive Input Handling](#responsive-input-handling)
- [System Diagnostics](#system-diagnostics)
- [Configuration System](#configuration-system)
- [WiFi Connectivity](#wifi-connectivity)
- [Summary of Best Practices](#summary-of-best-practices)

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

## Summary of Best Practices

### 1. Hardware Resource Management
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

---

## Commits Reference

| Commit | Description |
|--------|-------------|
| `cb75159` | Add diagnostic mode with WiFi and system info |
| `15ce905` | Add memory stats and task count to diagnostic screen |
| `17db3d6` | Fix LCD backlight turning off on emergency stop |
| `4a68d55` | Improve button responsiveness with GPIO interrupts |
| `e583ef9` | Increase LCD refresh rate to 60Hz and boost SPI speed |

---

## Further Reading

- [ESP-IDF LEDC Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/ledc.html)
- [ESP-IDF GPIO Interrupts](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/gpio.html)
- [Heap Memory Debugging](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/heap_debug.html)
- [FreeRTOS Task Utilities](https://www.freertos.org/a00021.html)
