# Fix Plan: TTGO Button Display - Both Buttons Not Showing Simultaneously

## Overview

Fix the race condition where pressing both buttons simultaneously doesn't show both as pressed on the LCD display.

## Priority & Rationale

- **Priority**: Low
- **Why**: Cosmetic issue only - button functionality works correctly, only visual feedback is affected
- **Dependencies**: None

## Recommended Solution

**Hybrid approach: Keep ISR for responsiveness, but poll GPIO directly for display**

The ISR is valuable for capturing button events quickly (e.g., for diagnostic mode entry), but for display purposes, polling the actual GPIO state is more reliable.

## Implementation Steps

### Step 1: Add direct GPIO reading functions

Modify `main.c` to add functions that read GPIO directly (not cached ISR values):

```c
// Read button state directly from GPIO (not ISR-cached)
static bool read_button_left_direct(void)
{
    return s_buttons_initialized && (gpio_get_level(BUTTON_LEFT_PIN) == 0);
}

static bool read_button_right_direct(void)
{
    return s_buttons_initialized && (gpio_get_level(BUTTON_RIGHT_PIN) == 0);
}
```

### Step 2: Use direct reading for LCD display

In `lcd_update_task()`, use direct GPIO reading for the LCD status update:

```c
// In lcd_update_task, around lines 1258-1259
// Change from:
btn_left = read_button_left();
btn_right = read_button_right();

// To:
btn_left = read_button_left_direct();
btn_right = read_button_right_direct();
```

### Step 3: Keep ISR for state machine logic (optional optimization)

The diagnostic mode and sleep mode state machines can continue using the ISR-cached values for edge detection, or switch to direct reading for consistency.

## Alternative Solutions

### Alternative A: Add debounce delay in ISR

Add a small delay before reading both buttons:

```c
static void IRAM_ATTR button_isr_handler(void* arg)
{
    // Small delay to let both buttons settle (not ideal for ISR)
    ets_delay_us(5000);  // 5ms delay
    s_button_left_pressed = (gpio_get_level(BUTTON_LEFT_PIN) == 0);
    s_button_right_pressed = (gpio_get_level(BUTTON_RIGHT_PIN) == 0);
}
```

**Drawback**: Delays in ISR are generally bad practice, blocks other interrupts.

### Alternative B: Use FreeRTOS task notification from ISR

Have the ISR notify a task that then reads button states after a short delay:

```c
static void IRAM_ATTR button_isr_handler(void* arg)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xTaskNotifyFromISR(button_task_handle, 0, eNoAction, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void button_task(void *arg)
{
    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        vTaskDelay(pdMS_TO_TICKS(10));  // 10ms debounce
        s_button_left_pressed = (gpio_get_level(BUTTON_LEFT_PIN) == 0);
        s_button_right_pressed = (gpio_get_level(BUTTON_RIGHT_PIN) == 0);
    }
}
```

**Drawback**: More complex, adds a task, may introduce latency for state machine logic.

## Files to Modify

| File | Changes |
|------|---------|
| `firmware/main/main.c` | Add `read_button_left_direct()` and `read_button_right_direct()` functions; update LCD task to use direct reading |

## Verification

1. Build TTGO target: `./scripts/build.sh ttgo`
2. Flash to device: `./scripts/ota.sh ttgo`
3. Test: Press both buttons simultaneously, verify both indicators light up on LCD
4. Test: Verify diagnostic mode still works (hold both buttons 3 seconds)
5. Test: Verify sleep mode still works (hold left button 5 seconds)

## Estimated Effort

- Code changes: ~15 minutes
- Testing: ~10 minutes
- Total: ~25 minutes

## Notes

- The state machines (diagnostic mode, sleep mode) use edge detection and timing, so they are less affected by this race condition
- The `/status` REST endpoint also reports button states - this will also benefit from direct GPIO reading
- Consider updating `status_update_task` to also use direct reading for consistency
