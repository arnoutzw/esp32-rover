# Bug Report: when holding the right button briefly Wifi AP countdown is shown and never disappears

## Build Information

| Field | Value |
|-------|-------|
| **Version** | v2.0.3 |
| **Git Hash** | d7acd7c |
| **Reported** | 2026-01-25 23:20:20 |
| **Status** | Open |

## Description

when holding the right button briefly Wifi AP countdown is shown and never disappears however it should time out after something like 10s to not cause clutter on the screen

## Steps to Reproduce

hold the right button for 1-2s

## Investigation

**Root Cause:** When the right button is released before the 5-second threshold, the WiFi switch state transitions from `WIFI_SWITCH_ENTERING` to `WIFI_SWITCH_OFF`. However, the overlay (drawn by `lcd_display_overlay()`) was not being cleared when this transition occurred. The overlay draws a dark gray stripe with yellow text at the top of the screen, and the normal status display update doesn't redraw this area, leaving the overlay visible indefinitely.

**Fix:** Added a tracking variable `wifi_switch_overlay_shown` to detect when the overlay was displayed. When the state transitions out of `WIFI_SWITCH_ENTERING` while the overlay was shown, the display is cleared with `lcd_display_reset_state()` and `lcd_display_clear()` to remove the stale overlay.

**Files Modified:**
- `firmware/main/main.c`: Added overlay tracking and clear logic

---

*Filed using file_report.py*
