# Plan: Embedded Coding Standards for ESP32 Rover Firmware

## Overview

Add missing embedded coding standards to CLAUDE.md (Phase 1) and then update the codebase to comply with those standards (Phase 2). Standards are prioritized: Critical → High → Medium.

## Phase 1: Update CLAUDE.md with Coding Standards

Add a new **"Embedded Coding Standards"** section after the existing "Development Rules" section.

### Critical Priority Rules

1. **Error Handling Policy**
   - Never use `ESP_ERROR_CHECK()` for non-critical initialization (crashes entire device)
   - Always check return values from `esp_*` functions
   - Use `if (ret != ESP_OK) { ESP_LOGE(); return ret; }` pattern
   - Log all errors with context using `esp_err_to_name()`

2. **Memory Management Rules**
   - Always check malloc/pvPortMalloc return for NULL before use
   - Pair every allocation with deallocation in error paths
   - Prefer static allocation for fixed-size buffers
   - Document memory ownership in function comments

3. **Input Validation Rules**
   - Validate all HTTP parameters (bounds, type, length)
   - Validate config values at startup
   - Sanitize strings before use in format functions
   - Check array indices before access

4. **Watchdog Usage Policy**
   - Document watchdog timeout in config (currently 5000ms)
   - Ensure long-running loops call `vTaskDelay()` or yield
   - Log warning if approaching watchdog timeout

5. **Interrupt/ISR Rules**
   - All ISR-modified variables MUST be `volatile`
   - Keep ISR code minimal (set flag, notify task)
   - Use `IRAM_ATTR` for all ISR functions
   - Never call blocking functions from ISR

### High Priority Rules

6. **Naming Conventions** (document existing practice)
   - Static variables: `s_variable_name`
   - Constants: `UPPER_SNAKE_CASE`
   - Functions: `module_verb_noun()` (e.g., `wifi_start_ap()`)
   - Types: `name_t` suffix (e.g., `rover_status_t`)
   - Handlers: `*_handler` suffix

7. **Function Size/Complexity Limits**
   - Maximum 100 lines per function
   - Extract state machines to separate functions
   - Single responsibility per function

8. **Return Value Checking**
   - All `esp_err_t` returns must be checked
   - Document functions where return is intentionally ignored with `(void)` cast

9. **Magic Number Prohibition**
   - All numeric literals must be named constants
   - Hardware-specific values in config headers
   - Add constants for: battery thresholds, LCD offsets, timing values

10. **Comment Requirements**
    - File header with purpose and author
    - Function header for non-trivial functions
    - Explain "why" not "what" in inline comments

### Medium Priority Rules

11. **Global Variable Policy**
    - Minimize global state; group related variables into structs
    - Document scope and thread-safety requirements
    - Use mutexes for shared state across tasks

12. **Assertion Usage**
    - Use `configASSERT()` for debug invariants
    - Never assert on recoverable conditions
    - Document assertion rationale

13. **Logging Levels Policy**
    - ERROR: Failures that affect functionality
    - WARN: Recoverable issues, degraded operation
    - INFO: State changes, startup/shutdown
    - DEBUG: Detailed tracing (disabled in release)

14. **Task Priority Policy**
    - Document priority rationale in code comments
    - Higher priority = more time-critical
    - Avoid priority inversion (use mutexes correctly)

15. **Compiler Warning Policy**
    - Treat all warnings as errors (`-Werror`)
    - Fix warnings, don't suppress them
    - Document any necessary suppressions

---

## Phase 2: Update Codebase to Comply with Standards

### Step 2.1: Critical Fixes (ISR Safety)

**Files:** `firmware/main/main.c`

Add `volatile` keyword to ISR-modified variables (lines 68-69):
```c
static volatile bool s_button_left_pressed = false;
static volatile bool s_button_right_pressed = false;
```

Also `s_ping_success` (line 486) used in ping callback.

### Step 2.2: Critical Fixes (Memory Safety)

**Files:** `firmware/main/main.c`, `firmware/components/web_server/web_server.c`

- Add NULL check after `pvPortMalloc()` at line 742 in main.c
- Verify malloc checks in web_server.c (lines 272, 764)
- Fix memory leak in `stream_handler()` error path

### Step 2.3: Critical Fixes (Error Handling)

**Files:** `firmware/main/main.c`

Replace `ESP_ERROR_CHECK()` with checked returns for non-critical init:
- Lines 274-275: `esp_netif_init()` in AP mode
- Lines 281-282: NVS init
- Lines 304-306: WiFi init
- Keep `ESP_ERROR_CHECK()` only for truly fatal errors

### Step 2.4: High Priority (Magic Numbers)

**Files:** `firmware/main/config.h` (or new constants file)

Add named constants:
```c
// Battery thresholds
#define BATTERY_VOLTAGE_EMPTY_V     3.0f
#define BATTERY_VOLTAGE_FULL_V      4.2f

// LCD display offsets (ST7789 specific)
#define LCD_COL_OFFSET              52
#define LCD_ROW_OFFSET              40

// Ping configuration
#define PING_PACKET_COUNT           3
#define PING_INTERVAL_MS            1000
#define PING_TIMEOUT_MS             2000

// PWM configuration
#define LCD_BACKLIGHT_PWM_FREQ_HZ   5000
#define LCD_BACKLIGHT_DUTY_MAX      255

// SPI configuration
#define LCD_SPI_CLOCK_HZ            (40 * 1000 * 1000)
```

### Step 2.5: High Priority (Function Refactoring)

**Files:** `firmware/main/main.c`, `firmware/components/web_server/web_server.c`

1. Extract state machines from `lcd_update_task()` (217 lines → ~80 lines):
   - `handle_diagnostic_mode_input()`
   - `handle_sleep_mode_input()`

2. Extract JSON builder from `status_handler()` (159 lines → ~60 lines):
   - `build_status_json()` helper function

### Step 2.6: Medium Priority (Mutex Protection)

**Files:** `firmware/main/main.c`

Add mutex protection for `s_battery_voltage_filtered`:
- Create or reuse existing mutex
- Take mutex before write in `read_battery_voltage()`
- Take mutex before read in `status_update_task()` and `lcd_update_task()`

### Step 2.7: Medium Priority (Logging)

**Files:** Multiple

- Add rate limiting for repetitive logs (stream pause, WiFi events)
- Ensure consistent log level usage per policy

---

## Files to Modify

| File | Phase | Changes |
|------|-------|---------|
| `CLAUDE.md` | 1 | Add Embedded Coding Standards section |
| `firmware/main/main.c` | 2 | ISR volatile, error handling, magic numbers, refactoring |
| `firmware/main/config.h` | 2 | Add named constants |
| `firmware/components/web_server/web_server.c` | 2 | Memory safety, JSON builder extraction |
| `firmware/components/lcd_display/lcd_display.c` | 2 | Magic numbers for battery thresholds |

---

## Verification

### After Phase 1 (Rules)
- Review CLAUDE.md for completeness
- Ensure rules are clear and actionable

### After Phase 2 (Code)
1. Build both targets:
   ```bash
   ./scripts/build.sh ttgo
   ./scripts/build.sh esp32cam
   ```

2. Run unit tests:
   ```bash
   cd test && make test
   ```

3. Flash and verify:
   ```bash
   ./scripts/ota.sh ttgo 192.168.2.75
   ```
   - Check diagnostics screen displays correctly
   - Verify button handling works
   - Check REST API status endpoint

4. Static analysis (if available):
   ```bash
   cppcheck firmware/main/main.c firmware/components/
   ```
