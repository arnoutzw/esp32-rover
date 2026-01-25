# Root Cause Analysis: TTGO Button Display - Both Buttons Not Showing Simultaneously

## Summary

When pressing both hardware buttons on the TTGO T-Display simultaneously, only one button indicator lights up on the LCD at a time, not both as expected.

## Investigation

### Code Analysis

**Button Reading (main.c:63-111)**

The button implementation uses GPIO interrupts with a shared ISR handler:

```c
static void IRAM_ATTR button_isr_handler(void* arg)
{
    // Read current button states directly (active LOW)
    s_button_left_pressed = (gpio_get_level(BUTTON_LEFT_PIN) == 0);
    s_button_right_pressed = (gpio_get_level(BUTTON_RIGHT_PIN) == 0);
}
```

Both buttons share the same ISR, which reads both button states on ANY button edge event.

**Button Configuration:**
- Left button: GPIO 0 (active LOW, internal pull-up)
- Right button: GPIO 35 (active LOW, internal pull-up)
- Interrupt trigger: `GPIO_INTR_ANYEDGE` (both press and release)

**Display Update (lcd_display.c:468-490)**

Each button's display is updated independently when its state changes. There's no logic preventing both from being displayed as pressed.

### Root Cause Identified

**The ISR-based button reading has a race condition when buttons are pressed simultaneously:**

1. **Interrupt-driven reading**: Button states are ONLY updated when an interrupt fires (edge event)

2. **Sequential interrupt processing**: When you press both buttons "simultaneously," they don't actually trigger at the exact same microsecond. One fires first.

3. **Race condition scenario**:
   - User presses both buttons at nearly the same time
   - Button A's edge triggers the ISR first
   - ISR reads: Button A = pressed, Button B = NOT YET pressed (still bouncing/traveling)
   - Button B's edge triggers ISR ~10-50ms later
   - ISR reads: Button A = pressed, Button B = pressed
   - **BUT** the LCD update cycle at 60Hz (~16ms) may catch the intermediate state

4. **GPIO 0 is special**: GPIO 0 is used for ESP32 boot mode selection and has external components (typically a capacitor for strapping). This can cause slower edge transitions compared to GPIO 35.

5. **No debouncing**: There's no software debounce, making the system sensitive to contact bounce timing.

### Why This Happens

Human button presses are never truly simultaneous - there's typically 10-100ms between the two contacts closing. The interrupt-based design captures the first button press, but the second button may still be in transition when the first ISR fires.

## Evidence

- Both buttons are on separate GPIOs (GPIO 0 and GPIO 35)
- The ISR reads both buttons on every edge event
- No debounce delay is implemented
- LCD updates at 60Hz (every ~16ms)
- The 50ms status update task polls button state

## Impact

- **Severity**: Low - Cosmetic issue, doesn't affect functionality
- **Scope**: LCD display visual feedback only
- **Workaround**: Press buttons with slight delay, or hold both and the display will eventually update correctly

## Recommendations

See FIX_PLAN_v2.0.2_97b1285.md for implementation details.

**Option 1 (Recommended)**: Add polling-based button reading in the LCD update task instead of relying solely on ISR for display purposes.

**Option 2**: Add a small debounce delay in the ISR to allow both buttons to settle before reading.

**Option 3**: Read button states directly (poll) instead of using ISR-cached values for display.
