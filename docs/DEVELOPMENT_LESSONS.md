# ESP32 Rover - Development Lessons Learned

This document captures key bugfixes, feature implementations, and lessons learned during development of the ESP32 Rover firmware. These insights can help avoid similar issues in future ESP32/embedded projects.

---

## Table of Contents

- [Hardware Resource Conflicts](#hardware-resource-conflicts)
- [Display Optimization](#display-optimization)
- [Responsive Input Handling](#responsive-input-handling)
- [System Diagnostics](#system-diagnostics)
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

---

## Further Reading

- [ESP-IDF LEDC Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/ledc.html)
- [ESP-IDF GPIO Interrupts](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/gpio.html)
- [Heap Memory Debugging](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/heap_debug.html)
- [FreeRTOS Task Utilities](https://www.freertos.org/a00021.html)
