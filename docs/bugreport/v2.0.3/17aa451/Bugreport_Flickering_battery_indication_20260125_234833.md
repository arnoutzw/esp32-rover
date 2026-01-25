# Bug Report: Flickering battery indication

## Build Information

| Field | Value |
|-------|-------|
| **Version** | v2.0.3 |
| **Git Hash** | 17aa451 |
| **Reported** | 2026-01-25 23:48:33 |
| **Status** | Fixed (5074ad7) |

## Description

Periodic flickering of battery indicator

## Investigation

### Root Cause

Two issues combined to cause the flickering:

1. **Too-sensitive voltage comparison**: The battery section was checking for changes at 0.01V precision (`bat_x100 = voltage * 100`). Even with the EMA low-pass filter (alpha=0.02), small ADC noise could cause oscillation between adjacent values like 3.61V and 3.62V, triggering continuous redraws.

2. **Flicker-prone bar redraw**: When the battery bar was updated, the code first cleared the entire bar to dark gray, then filled in the new percentage. This clear-then-fill approach created a visible flash on each update.

**Location**: `firmware/components/lcd_display/lcd_display.c:560-591`

### Fix

1. **Reduced comparison precision** from 0.01V to 0.05V by using `bat_x20 = voltage * 20` instead of `bat_x100 = voltage * 100`. This provides sufficient hysteresis to prevent oscillation.

2. **Flicker-free bar redraw**: Changed the bar drawing to update filled and empty portions separately, without first clearing the entire bar:
   ```c
   lcd_fill_rect(96, y + 1, new_bar_width, 8, bat_color);           // Filled part
   lcd_fill_rect(96 + new_bar_width, y + 1, 34 - new_bar_width, 8, COLOR_DARKGRAY);  // Empty part
   ```

3. **Bar outline on init**: Draw the bar background once during initialization instead of on every update.

**Commit:** 5074ad7

---

*Filed using file_report_web.py*
