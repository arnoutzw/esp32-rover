# ESP32 Rover Unit Tests

This directory contains unit tests for the ESP32 Rover firmware. There are two types of tests:

1. **Host-based tests** - Run on your development machine for fast iteration
2. **Target-based tests** - Run on the ESP32 for hardware-specific testing

## Test Structure

| File | Target | Description |
|------|--------|-------------|
| `test_config.c` | Host | Configuration system tests (REQ-01 to REQ-05) |
| `test_diag_state_machine.c` | Host | Diagnostic mode state machine (REQ-19) |
| `test_resource_guard.c` | ESP32 | Resource consumption guards (REQ-36) |
| `test_runner.c` | Host | Main test runner for host-based tests |
| `unity.h` | Both | Minimal Unity test framework |

## Running Host-Based Tests

Host-based tests verify configuration values and state machine logic without needing hardware.

```bash
cd test
make test
```

Expected output:
```
╔════════════════════════════════════════════════════════════╗
║         ESP32 Rover Firmware - Unit Test Suite             ║
╚════════════════════════════════════════════════════════════╝

=== Configuration Tests (REQ-01 to REQ-05) ===
...
=== Diagnostic State Machine Tests (REQ-19) ===
...

════════════════════════════════════════════════════════════
✓ All test suites passed!
════════════════════════════════════════════════════════════
```

## Running Target-Based Tests (ESP32)

Resource guard tests require running on actual ESP32 hardware.

```bash
# Build and flash test application
cd test
idf.py build
idf.py -p /dev/cu.usbserial-XXXX flash monitor
```

## Requirements Coverage

### Configuration Tests (`test_config.c`)

| Requirement | Tests | Description |
|-------------|-------|-------------|
| REQ-01 | 5 | Configuration YAML generates correct C defines |
| REQ-02 | 3 | WiFi mode flags are mutually exclusive and STA settings exist |
| REQ-03 | 2 | REST API enable/disable flag and cache interval |
| REQ-04 | 4 | MQTT enable/disable flag, broker settings, publish interval, QoS |
| REQ-05 | 1 | Service status flags available for diagnostic display |

### Diagnostic State Machine Tests (`test_diag_state_machine.c`)

| Requirement | Tests | Description |
|-------------|-------|-------------|
| REQ-19 | 12 | Diagnostic mode entry/exit state machine |

Tests cover:
- Initial state is OFF
- Single button doesn't enter diagnostic mode
- Both buttons held for 3s enters diagnostic mode
- Early release cancels entry
- Short press any button exits diagnostic mode
- Full entry/exit cycle
- Re-entry after exit

### Resource Guard Tests (`test_resource_guard.c`)

| Requirement | Tests | Description |
|-------------|-------|-------------|
| REQ-36 | 13 | Resource consumption guards and runtime checks |

Tests cover:
- Heap memory above minimum threshold
- Internal DRAM above minimum threshold
- Heap watermark (min free since boot) safe
- Heap fragmentation acceptable
- `resource_guard_can_alloc()` prediction accuracy
- Allocation cycle memory leak detection
- Task stack watermark safety
- Deep recursion stack usage
- Task count within limits
- Task create/delete cycle leak detection
- Combined resource check passes
- System stability after memory pressure
- Log buffer memory budget (REQ-31)
- Camera stream stack budget (REQ-34)

## Test Summary

| Category | Host Tests | Target Tests | Total |
|----------|------------|--------------|-------|
| Configuration | 15 | - | 15 |
| Diagnostic Mode | 12 | - | 12 |
| Resource Guards | - | 13 | 13 |
| **Total** | **27** | **13** | **40** |

## Adding New Tests

### Host-Based Test

1. Create a new `test_*.c` file
2. Include Unity framework: `#include "unity.h"`
3. Add test functions with `void test_*` naming convention
4. Create a runner function: `int run_*_tests(void)`
5. Register in `test_runner.c`:
   ```c
   extern int run_your_tests(void);
   // In main():
   total_failures += run_your_tests();
   ```

### Target-Based Test (ESP32)

1. Add test functions to `test_resource_guard.c` or create new file
2. Use Unity's `TEST_CASE()` macro with tags:
   ```c
   TEST_CASE("My test description", "[category][subcategory]")
   {
       // Test code
   }
   ```
3. Tests are automatically discovered and run by `unity_run_all_tests()`

## Related Documentation

- [software_requirements.md](../docs/software_requirements.md) - Full requirements specification
- [ESP-IDF Unit Testing](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/unit-tests.html)
- [Unity Test Framework](https://github.com/ThrowTheSwitch/Unity)
