# Fix Implementation Plan

**Version:** 2.0.1
**Commit:** e4b11fc
**Date:** 2026-01-25
**RCA Reference:** [RCA_v2.0.1_e4b11fc.md](RCA_v2.0.1_e4b11fc.md)

---

## Overview

This document outlines the implementation plan for fixing the three bugs identified in the Root Cause Analysis. Fixes are ordered by priority (P1 first).

---

## Fix 1: Log Streaming Stuck Disconnected (Priority: P1)

### 1.1 Position Tracking Fix (PRIMARY)

**File:** `firmware/components/log_buffer/log_buffer.c`

**Current Code (around line 520-530):**
```c
// Position only updates when entry matches filter
if (i < skip_count) {
    continue;
}
*position = oldest_seq + i + 1;
```

**Fix:** Update position for ALL entries checked, regardless of filter match:

```c
// Always advance position as we check entries
*position = oldest_seq + i + 1;

// Skip entries that don't match filter
if (i < skip_count) {
    continue;
}
```

**Alternative approach (cleaner):** Track position advancement separately:

```c
// At the end of the function, always update position to skip checked entries
// even if no matching entry was found
*position = oldest_seq + checked_count;
return found_match;
```

**Impact:** Low risk - changes internal position tracking only, no API changes.

---

### 1.2 Static Keepalive Counter Fix (SECONDARY)

**File:** `firmware/components/web_server/web_server.c`

**Current Code (around line 636):**
```c
static int keepalive_counter = 0;  // Shared across connections!
```

**Fix:** Move counter inside the task/function scope (non-static):

```c
int keepalive_counter = 0;  // Per-connection counter
```

**Impact:** Low risk - only affects keepalive timing, improves behavior for concurrent connections.

---

### 1.3 Client Error Handling Improvement (TERTIARY)

**File:** `firmware/components/web_server/web_ui.c`

**Current Code (around line 1092-1098):**
```javascript
logEventSource.onerror = function() {
    document.getElementById('log-status').textContent = 'Disconnected - reconnecting...';
    logEventSource.close();
    logEventSource = null;
    setTimeout(startLogStream, 3000);
};
```

**Fix:** Add connection state tracking and exponential backoff:

```javascript
var logReconnectAttempts = 0;
var maxReconnectDelay = 30000;  // 30 seconds max

logEventSource.onopen = function() {
    document.getElementById('log-status').textContent = 'Connected';
    logReconnectAttempts = 0;  // Reset on successful connection
};

logEventSource.onerror = function(e) {
    if (logEventSource.readyState === EventSource.CLOSED) {
        document.getElementById('log-status').textContent = 'Disconnected - reconnecting...';
        logEventSource.close();
        logEventSource = null;

        // Exponential backoff: 1s, 2s, 4s, 8s, 16s, 30s max
        var delay = Math.min(1000 * Math.pow(2, logReconnectAttempts), maxReconnectDelay);
        logReconnectAttempts++;
        setTimeout(startLogStream, delay);
    }
};
```

**Impact:** Low risk - JavaScript changes only, improves reconnection behavior.

---

### 1.4 Testing Checklist

- [ ] Open web UI, expand diagnostics
- [ ] Set log level to "Info+"
- [ ] Verify logs appear (even if few)
- [ ] Verify status shows "Connected" not "Disconnected"
- [ ] Generate some logs (trigger actions)
- [ ] Verify new logs appear in real-time
- [ ] Disconnect WiFi briefly, verify reconnection
- [ ] Test with multiple browser tabs open simultaneously

---

## Fix 2: Incorrect Connected Clients Count (Priority: P2)

### 2.1 Initialize wifi_sta_list_t Structure

**File:** `firmware/main/main.c`

**Location 1 - Status Update Task (around line 827):**

**Current Code:**
```c
wifi_sta_list_t sta_list;
if (esp_wifi_ap_get_sta_list(&sta_list) == ESP_OK) {
    status.connected_clients = sta_list.num;
}
```

**Fix:**
```c
wifi_sta_list_t sta_list = {0};  // Zero-initialize
if (esp_wifi_ap_get_sta_list(&sta_list) == ESP_OK) {
    status.connected_clients = sta_list.num;
} else {
    status.connected_clients = 0;  // Explicit fallback
}
```

---

**Location 2 - get_connected_station_count() (around line 1016):**

**Current Code:**
```c
static uint8_t get_connected_station_count(void)
{
    wifi_sta_list_t sta_list;
    if (esp_wifi_ap_get_sta_list(&sta_list) == ESP_OK) {
        return sta_list.num;
    }
    return 0;
}
```

**Fix:**
```c
static uint8_t get_connected_station_count(void)
{
    wifi_sta_list_t sta_list = {0};  // Zero-initialize
    if (esp_wifi_ap_get_sta_list(&sta_list) == ESP_OK) {
        return sta_list.num;
    }
    return 0;
}
```

**Impact:** Minimal risk - defensive initialization, no behavior change when API succeeds.

---

### 2.2 Testing Checklist

- [ ] Enter diagnostic mode (hold both buttons 3 seconds)
- [ ] With 0 clients connected, verify "Clients: 0"
- [ ] Connect one device to WiFi AP
- [ ] Verify "Clients: 1"
- [ ] Connect second device
- [ ] Verify "Clients: 2"
- [ ] Disconnect one device
- [ ] Verify count decrements correctly
- [ ] Check REST API `/status` returns correct `clients` field

---

## Fix 3: Both Buttons Not Showing Pressed (Priority: P2)

### 3.1 Synchronized Button Redraw

**File:** `firmware/components/lcd_display/lcd_display.c`

**Current Code (around lines 470-492):**
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

**Fix:** Redraw BOTH buttons if EITHER state changed:

```c
// Check if ANY button state changed - redraw both together for consistency
bool button_state_changed = ((int8_t)status->button_left != s_prev_button_left) ||
                            ((int8_t)status->button_right != s_prev_button_right);

if (button_state_changed) {
    // Redraw left button
    uint16_t btn_l_bg = status->button_left ? COLOR_CYAN : COLOR_DARKGRAY;
    uint16_t btn_l_fg = status->button_left ? COLOR_BLACK : COLOR_LIGHTGRAY;
    lcd_fill_rect(0, y, btn_width - 1, btn_height, btn_l_bg);
    lcd_draw_string(btn_width / 2 - 6, y + 6, "L", btn_l_fg, btn_l_bg, 1);

    // Redraw right button
    uint16_t btn_r_bg = status->button_right ? COLOR_CYAN : COLOR_DARKGRAY;
    uint16_t btn_r_fg = status->button_right ? COLOR_BLACK : COLOR_LIGHTGRAY;
    lcd_fill_rect(btn_width + 1, y, btn_width - 1, btn_height, btn_r_bg);
    lcd_draw_string(btn_width + btn_width / 2 - 6, y + 6, "R", btn_r_fg, btn_r_bg, 1);

    // Update both previous states
    s_prev_button_left = (int8_t)status->button_left;
    s_prev_button_right = (int8_t)status->button_right;
}
```

**Impact:** Slightly more LCD writes when only one button changes, but ensures consistency for simultaneous presses.

---

### 3.2 Testing Checklist

- [ ] Press only left button - verify "L" highlights, "R" stays gray
- [ ] Release left button - verify "L" returns to gray
- [ ] Press only right button - verify "R" highlights, "L" stays gray
- [ ] Release right button - verify "R" returns to gray
- [ ] Press BOTH buttons simultaneously - verify BOTH highlight
- [ ] Hold both buttons - verify both stay highlighted
- [ ] Release both buttons - verify both return to gray
- [ ] Rapid press/release both - verify consistent display

---

## Implementation Order

| Order | Fix | File(s) | Estimated Time | Risk |
|-------|-----|---------|----------------|------|
| 1 | 1.1 Position tracking | log_buffer.c | 15 min | Low |
| 2 | 1.2 Static keepalive | web_server.c | 5 min | Low |
| 3 | 1.3 Client error handling | web_ui.c | 15 min | Low |
| 4 | 2.1 Initialize sta_list | main.c | 10 min | Low |
| 5 | 3.1 Synchronized button redraw | lcd_display.c | 15 min | Low |

**Total Estimated Time:** ~1 hour

---

## Post-Fix Verification

After implementing all fixes:

1. **Build for TTGO:**
   ```bash
   ./scripts/build.sh ttgo
   ```

2. **Flash via OTA:**
   ```bash
   OTA_PASSWORD=<password> ./scripts/ota.sh ttgo
   ```

3. **Run full test suite** using checklists above

4. **Verify no regressions:**
   - Main status screen displays correctly
   - Camera toggle works (ESP32-CAM)
   - WiFi connection stable
   - mDNS resolution works
   - Diagnostic mode entry/exit works

---

## Version Recommendation

After fixes are verified, create a patch release:

```bash
./scripts/release.sh patch "Fix TTGO bugs: log streaming, client count, button display"
```

This will create version **v2.0.2**.

---

## Files Changed Summary

| File | Changes |
|------|---------|
| `firmware/components/log_buffer/log_buffer.c` | Fix position tracking in `log_buffer_read_next()` |
| `firmware/components/web_server/web_server.c` | Remove `static` from `keepalive_counter` |
| `firmware/components/web_server/web_ui.c` | Add exponential backoff and `onopen` handler |
| `firmware/main/main.c` | Initialize `wifi_sta_list_t` to zero (2 locations) |
| `firmware/components/lcd_display/lcd_display.c` | Synchronized button redraw logic |
