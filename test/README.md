# ESP32 Rover Unit Tests

This directory contains unit tests for the ESP32 Rover firmware. Tests are designed to run on the host machine (not on the ESP32) for faster iteration.

## Test Structure

- `test_config.c` - Tests for REQ-01 (Configuration YAML parsing)
- `test_diag_state_machine.c` - Tests for REQ-06 (Diagnostic mode state machine)
- `test_runner.c` - Main test runner

## Running Tests

```bash
cd test
make test
```

## Requirements Coverage

| Requirement | Test File | Description |
|-------------|-----------|-------------|
| REQ-01 | test_config.c | Configuration YAML generates correct C defines |
| REQ-02 | test_config.c | WiFi mode flags are mutually exclusive |
| REQ-03 | test_config.c | REST API enable/disable flag |
| REQ-04 | test_config.c | MQTT enable/disable flag and settings |
| REQ-05 | test_config.c | Service status fields exist in config |
| REQ-06 | test_diag_state_machine.c | Diagnostic mode entry/exit state machine |

## Adding New Tests

1. Create a new `test_*.c` file
2. Include Unity framework: `#include "unity.h"`
3. Add test functions with `void test_*` naming
4. Register tests in `test_runner.c`
