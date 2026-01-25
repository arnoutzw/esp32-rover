# Root Cause Analysis Report

**Version:** 2.0.1
**Commit:** e4b11fc
**Date:** 2026-01-25
**Investigator:** Claude AI Assistant

---

## Executive Summary

Three bugs were reported for TTGO T-Display version 2.0.1. Investigation has identified the root causes for all three issues:

| Bug | Severity | Root Cause | Files Affected |
|-----|----------|------------|----------------|
| Connected clients count incorrect | Medium | Uninitialized struct before WiFi API call | main.c |
| Log streaming stuck disconnected | High | Position tracking bug + static keepalive counter | log_buffer.c, web_server.c, web_ui.c |
| Both buttons not showing pressed | Medium | Independent dirty tracking + no synchronization | lcd_display.c |

---

## Bug 1: LCD Diagnostics Shows Incorrect Connected Clients Count

### Symptom
The LCD diagnostics screen displays incorrect number of connected HTTP clients. The displayed value does not correspond to the actual number of web UI connections.

### Root Cause
**Uninitialized `wifi_sta_list_t` structure before calling ESP-IDF WiFi API.**

The WiFi station list structure is declared on the stack but not zero-initialized. If `esp_wifi_ap_get_sta_list()` fails or partially populates the structure, the `num` field contains garbage data from the stack.

### Code Location

**File:** `firmware/main/main.c`

**Location 1 - Status Update Task (Lines 827-830):**
```c
// Connected clients
wifi_sta_list_t sta_list;                    // BUG: Uninitialized!
if (esp_wifi_ap_get_sta_list(&sta_list) == ESP_OK) {
    status.connected_clients = sta_list.num;
}
```

**Location 2 - get_connected_station_count() (Lines 1016-1021):**
```c
static uint8_t get_connected_station_count(void)
{
    wifi_sta_list_t sta_list;                // BUG: Uninitialized!
    if (esp_wifi_ap_get_sta_list(&sta_list) == ESP_OK) {
        return sta_list.num;
    }
    return 0;
}
```

### Impact
- LCD diagnostics screen shows random/garbage client count
- Web REST API `/status` endpoint returns incorrect `clients` field
- MQTT telemetry publishes incorrect client counts (if enabled)

### Data Flow
```
esp_wifi_ap_get_sta_list()
    → sta_list.num (potentially garbage)
    → status.connected_clients / get_connected_station_count()
    → web_server_update_status() / lcd_wifi_diag_t.connected_stations
    → /status JSON / LCD diagnostic screen
```

---

## Bug 2: System Logs Stuck Disconnected/Reconnecting

### Symptom
The system logs panel in the web UI is stuck showing "Disconnected - reconnecting..." and no serial logs are displayed. The status toggles every 3 seconds but logs never appear.

### Root Cause
**Three contributing issues identified:**

#### Issue 2.1: Position Tracking Bug (PRIMARY)
**File:** `firmware/components/log_buffer/log_buffer.c` (Lines 463-576)

The `log_buffer_read_next()` function only updates the position pointer when a matching log entry is found. When filtering by log level (e.g., only showing INFO+), entries that don't match the filter are skipped but the position is NOT advanced.

```c
// Position only updates when entry matches filter
if (i < skip_count) {
    continue;  // Skip entries, but position NOT updated here!
}
*position = oldest_seq + i + 1;  // Only updates when entry matches filter
```

**Result:** If there are no logs matching the filter (e.g., all logs are DEBUG level but UI filters for INFO+), the position never advances. The function returns `false` repeatedly, the stream appears stuck, and eventually the client reconnects in an infinite loop.

#### Issue 2.2: Static Keepalive Counter (SECONDARY)
**File:** `firmware/components/web_server/web_server.c` (Lines 636-645)

```c
static int keepalive_counter = 0;  // BUG: Static shared across all clients!
if (++keepalive_counter >= 30) {
    keepalive_counter = 0;
    res = httpd_resp_send_chunk(req, ": keepalive\n\n", 13);
    // ...
}
```

The `keepalive_counter` is declared `static`, meaning it persists across multiple client connections and is shared between concurrent connections. This causes erratic keepalive timing instead of consistent 3-second intervals per client.

#### Issue 2.3: Fragile Client Error Handling
**File:** `firmware/components/web_server/web_ui.c` (Lines 1092-1098)

```javascript
logEventSource.onerror = function() {
    document.getElementById('log-status').textContent = 'Disconnected - reconnecting...';
    logEventSource.close();
    logEventSource = null;
    setTimeout(startLogStream, 3000);
};
```

The `onerror` handler treats ANY error as a full disconnection requiring reconnection. Minor network hiccups, keepalive issues, or browser timeouts all trigger the same 3-second reconnection cycle, making the UI appear "stuck" in reconnection state.

### Impact
- No logs visible in web UI
- Constant "Disconnected - reconnecting..." message
- 3-second polling cycle consuming resources
- Users cannot debug issues via web UI

### Data Flow
```
ESP_LOG* macros
    → log_vprintf_hook()
    → ring buffer (write_seq increments)
    → log_buffer_read_next() [BUG: position stuck]
    → log_stream_task() (loops returning false)
    → keepalive sent [BUG: static counter]
    → EventSource error [BUG: triggers reconnect]
    → UI shows "Disconnected - reconnecting..."
```

---

## Bug 3: Both Buttons Pressed Shows Only One

### Symptom
When both buttons (L and R) on TTGO T-Display are pressed simultaneously, the LCD screen only shows one button as pressed. Both buttons should highlight when both are held down.

### Root Cause
**Independent dirty tracking without synchronization between left and right button states.**

The LCD display uses "dirty tracking" to optimize updates - it only redraws a button when its state changes from the previous frame. Each button is tracked independently with separate state variables.

**File:** `firmware/components/lcd_display/lcd_display.c` (Lines 470-492)

```c
// Left button - only redraw if state changed
if ((int8_t)status->button_left != s_prev_button_left) {
    // ... redraw left button ...
    s_prev_button_left = (int8_t)status->button_left;
}

// Right button - only redraw if state changed
if ((int8_t)status->button_right != s_prev_button_right) {
    // ... redraw right button ...
    s_prev_button_right = (int8_t)status->button_right;
}
```

### Timing Race Condition

1. User presses both buttons simultaneously
2. GPIO interrupt fires and reads both button states
3. LCD task (60Hz, every 16ms) may read states mid-update
4. First frame: Left button detected pressed, Right button not yet seen
5. `s_prev_button_left` updated to `true`
6. Second frame: Both buttons now read as pressed
7. Left button: `true != s_prev_button_left (true)` → SKIP redraw (no change)
8. Right button: `true != s_prev_button_right (false)` → Redraw right
9. **Result:** Only right button appears pressed

Additionally, the button ISR (`main.c` lines 72-77) updates both volatile globals without synchronization, allowing the LCD task to read inconsistent state during the update.

### Impact
- Confusing user experience when pressing both buttons
- Diagnostic mode entry (3-second both-button hold) may appear inconsistent
- Button state display unreliable for simultaneous presses

### Data Flow
```
GPIO interrupt (both pins)
    → s_button_left_pressed, s_button_right_pressed (volatile globals)
    → read_button_left/right()
    → status.button_left/right
    → lcd_display_update()
    → [BUG: independent dirty checks]
    → Only one button redraws
```

---

## Affected Files Summary

| File | Bug(s) | Lines |
|------|--------|-------|
| `firmware/main/main.c` | 1, 3 | 72-77, 812-816, 827-830, 1016-1021 |
| `firmware/components/log_buffer/log_buffer.c` | 2 | 463-576 |
| `firmware/components/web_server/web_server.c` | 2 | 636-645 |
| `firmware/components/web_server/web_ui.c` | 2 | 1092-1098 |
| `firmware/components/lcd_display/lcd_display.c` | 3 | 61-62, 470-492 |

---

## Risk Assessment

| Bug | User Impact | Data Integrity | Security | Priority |
|-----|-------------|----------------|----------|----------|
| 1 (Clients count) | Low | Medium | None | P2 |
| 2 (Log streaming) | High | None | None | P1 |
| 3 (Button display) | Medium | None | None | P2 |

**Recommended Fix Order:** Bug 2 → Bug 1 → Bug 3

Bug 2 has the highest user impact as it completely blocks the ability to view logs in the web UI, which is critical for debugging and monitoring.

---

## Appendix: Reproduction Steps

### Bug 1: Connected Clients
1. Connect TTGO to power
2. Connect phone/laptop to WiFi AP
3. Open web UI in browser
4. Hold both buttons for 3 seconds to enter diagnostic mode
5. Observe "Clients:" value - may show incorrect number (0, 255, or random)

### Bug 2: Log Streaming
1. Open web UI on any browser
2. Scroll down to expand Diagnostics panel
3. Observe "Logs" section
4. Status shows "Disconnected - reconnecting..." indefinitely
5. No log entries appear in the log viewer

### Bug 3: Button Display
1. On TTGO main status screen
2. Press and hold BOTH L and R buttons simultaneously
3. Observe button indicators in header
4. Only one button (typically R) shows as highlighted
5. Release and repeat - behavior may vary based on timing
