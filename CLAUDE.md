# ESP32 Rover Firmware - Project Notes

## Project Structure

```
esp32-rover-firmware/
├── firmware/           # Firmware source code
│   ├── main/           # Main application
│   ├── components/     # Custom ESP-IDF components
│   └── esp-idf/        # ESP-IDF SDK (submodule)
├── scripts/            # Build and utility scripts
├── config/             # Configuration files
├── docs/
│   ├── requirements/   # Requirements specifications
│   ├── implementation/ # Developer documentation
│   └── user/           # User guides
└── test/               # Unit tests
```

## ESP-IDF Location

ESP-IDF is embedded within this project (not installed globally):

```bash
# Source ESP-IDF environment before running Python scripts or builds
source ./firmware/esp-idf/export.sh
```

## Common Commands

```bash
# Build for ESP32-CAM
./scripts/build.sh esp32cam

# Build for TTGO T-Display
./scripts/build.sh ttgo

# Regenerate config (after changing config/rover_config.yaml)
source ./firmware/esp-idf/export.sh && python scripts/generate_config.py

# OTA flash to ESP32-CAM (auto rate-limited for stability)
./scripts/ota.sh esp32cam

# OTA flash to TTGO
./scripts/ota.sh ttgo

# OTA flash to specific IP
./scripts/ota.sh esp32cam 192.168.2.88
./scripts/ota.sh ttgo 192.168.2.75
```

## Target Configuration

The `target` field in `config/rover_config.yaml` controls mDNS hostname generation:
- `target: esp32cam` → hostname `esp32-rover.local`
- `target: ttgo` → hostname `ttgo-rover.local`

The actual build target is selected via `./scripts/build.sh esp32cam` or `./scripts/build.sh ttgo`.

## Key Files

- `config/rover_config.yaml` - Main configuration file
- `config/secrets.yaml` - WiFi passwords, OTA password (not in git)
- `scripts/generate_config.py` - Generates `firmware/main/config_generated.h` from YAML
- `firmware/main/config.h` - Hardware pin definitions
- `firmware/main/config_generated.h` - Auto-generated config defines

## Development Rules

### Git Branching Strategy

**All development work happens on the `develop` branch.**

- Work on `develop` branch for all changes
- Commit frequently to `develop` with descriptive messages
- Only merge to `main` and create a version tag when the user explicitly requests it
- Never commit directly to `main` unless instructed

**Workflow:**
```bash
# Normal development
git checkout develop
# ... make changes ...
git add -A && git commit -m "Description of changes"
git push origin develop

# When user requests a release to main:
git checkout main
git merge develop
git tag -a v1.X.X -m "Release description"
git push origin main --tags
git checkout develop
```

### Clean Build Rule

**Never build with a dirty working directory.**

Before running any build command (`./scripts/build.sh`):
1. Check if there are uncommitted changes (`git status`)
2. If dirty, commit all changes to `develop` with a descriptive message
3. Push to origin
4. Only then proceed with the build

This ensures:
- Every build is traceable to a specific commit
- Build fingerprint in firmware matches a known state
- No accidental "works on my machine" issues from uncommitted changes

### Bug Documentation

**For every investigated bug with a confirmed fix, add a new entry to `docs/implementation/DEVELOPMENT_LESSONS.md`.**

Each entry should include:
1. **Symptom**: What the user observed (error message, behavior)
2. **Root Cause**: The underlying technical issue
3. **Investigation Process**: How the bug was diagnosed
4. **Solution**: The fix applied
5. **Lesson Learned**: Key takeaway for future development

This ensures knowledge is preserved and similar issues can be avoided or quickly diagnosed in the future.

### Binary Archive Management

**Automatically archive all built binaries and manage target switching cleanly.**

Built binaries are automatically archived in the `binaries/` directory with the following structure:
```
binaries/
├── esp32cam/          # ESP32-CAM binaries
│   ├── latest.bin → esp32-rover_esp32cam_YYYYMMDD_HHMMSS_commit.bin
│   ├── esp32-rover_esp32cam_YYYYMMDD_HHMMSS_commit.bin
│   ├── esp32-rover_esp32cam_YYYYMMDD_HHMMSS_commit.txt  # Metadata
│   └── ... (other builds)
└── ttgo/              # TTGO binaries
    └── (same structure as esp32cam)
```

**Key Features:**
1. **Automatic Archiving**: Every build automatically archives the binary with timestamp, commit hash, and branch info
2. **Latest Symlink**: `latest.bin` always points to the most recent build for that target
3. **Metadata**: Each binary has a `.txt` metadata file with build info
4. **Clean Target Switching**: When switching targets (e.g., esp32cam → ttgo), a clean build is automatically performed to prevent configuration conflicts

**Using Built Binaries:**

Flash the latest build (default):
```bash
./scripts/ota.sh esp32cam                    # Uses latest.bin symlink
```

List available binaries:
```bash
./scripts/ota.sh esp32cam --list-binaries
```

Flash a specific archived binary:
```bash
./scripts/ota.sh esp32cam --binary-file esp32-rover_esp32cam_20240115_143022_abc1234.bin
./scripts/ota.sh esp32cam --binary-file /absolute/path/to/binary.bin  # Absolute paths also work
```

Flash to specific IP with archived binary:
```bash
./scripts/ota.sh esp32cam 192.168.2.88 --binary-file esp32-rover_esp32cam_20240115_143022_abc1234.bin
```

**Target Switching Behavior:**

When you run `./scripts/build.sh` with a different target than the previous build:
1. The build system detects the target change
2. Automatically performs `idf.py clean` to remove old build artifacts
3. Removes the old `sdkconfig` to force regeneration with new target settings
4. Builds cleanly for the new target

This prevents configuration conflicts that could occur when switching between esp32cam and ttgo builds.

### CI/CD Pipeline

**Automated testing and builds via GitHub Actions.**

The project uses GitHub Actions for continuous integration:
- **Lint job**: Runs shellcheck on scripts and cppcheck on C code
- **Test job**: Runs unit tests on every push
- **Coverage job**: Generates code coverage reports
- **Build jobs**: Builds firmware for both targets (esp32cam and ttgo)

Workflow files:
- `.github/workflows/ci.yml` - Main CI pipeline (push/PR)
- `.github/workflows/release.yml` - Automated releases on tags

### Release Management

**Use the release script to create tagged releases.**

```bash
# Create a patch release (bug fixes)
./scripts/release.sh patch "Fixed OTA stability issue"

# Create a minor release (new features)
./scripts/release.sh minor "Added MQTT telemetry support"

# Create a major release (breaking changes)
./scripts/release.sh major "Complete architecture rewrite"

# Dry run to preview changes
./scripts/release.sh patch --dry-run
```

The release script:
1. Calculates the new version number
2. Generates changelog from commits
3. Builds both targets
4. Creates release artifacts in `releases/vX.X.X/`
5. Merges develop to main and creates a git tag
6. Pushes to origin

### Code Quality

**Static analysis and linting are available via lint.sh.**

```bash
# Run all linting
./scripts/lint.sh

# Shell scripts only
./scripts/lint.sh --shell-only

# C code only
./scripts/lint.sh --c-only

# Strict mode (treat warnings as errors)
./scripts/lint.sh --strict
```

Requirements:
- `shellcheck` - Install via `brew install shellcheck` or `apt install shellcheck`
- `cppcheck` - Install via `brew install cppcheck` or `apt install cppcheck`

### Code Coverage

**Unit tests support code coverage measurement.**

```bash
cd test

# Run tests with coverage
make coverage

# Generate HTML coverage report
make coverage-html
# Open test/coverage_report/index.html

# Clean coverage data
make clean
```

Requirements for HTML reports:
- `lcov` - Install via `brew install lcov` or `apt install lcov`

### OTA Security

**OTA password must be set via environment variable.**

The OTA flash script requires an explicit password - no default password is used:

```bash
# Set password and flash
export OTA_PASSWORD=your_secure_password
./scripts/ota.sh esp32cam

# Or inline
OTA_PASSWORD=your_password ./scripts/ota.sh esp32cam
```

Binary checksums are displayed before flashing and stored in archive metadata for verification.

### OTA Rollback Support

**Automatic rollback on boot failure is enabled for ESP32-CAM only.**

ESP32-CAM has `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y` set, which means:
- If a new firmware fails to boot properly, the device automatically rolls back to the previous working firmware
- The firmware must call `esp_ota_mark_app_valid_cancel_rollback()` after successful initialization to confirm it's working
- This prevents bricking the device with a bad OTA update

**Note:** TTGO T-Display does not have OTA rollback enabled due to IRAM memory constraints (no PSRAM). Extra caution should be taken when OTA flashing TTGO devices.

### Integration Tests

**Hardware integration tests are available for testing real devices.**

```bash
# Install test dependencies
pip install -r test/integration/requirements.txt

# Run integration tests against a device
export DEVICE_IP=esp32-rover.local
export OTA_PASSWORD=your_password
pytest test/integration/ -v
```

These tests verify:
- Device reachability and response times
- Status endpoint JSON responses
- OTA endpoint authentication
- Target-specific features (camera on ESP32-CAM, buttons on TTGO)

### Build Metrics

**Build duration and binary size are tracked automatically.**

After each build, metrics are:
- Displayed in the console
- Logged to `build_metrics.csv` for trend analysis

Format: `date,target,duration_s,size_bytes,commit`

---

## Technical Deep Dive (AI Assistant Reference)

### Component Architecture

The firmware uses ESP-IDF's component model with 8 custom components in `firmware/components/`:

| Component | Purpose | Key Files | Notes |
|-----------|---------|-----------|-------|
| `web_server` | HTTP REST API & Web UI | `web_server.c`, `web_ui.c` | ~920 LOC, serves control interface |
| `camera` | OV2640 driver | `camera.c` | ESP32-CAM only, QVGA MJPEG |
| `lcd_display` | ST7789 LCD driver | `lcd_display.c` | TTGO only, 135x240 display |
| `mqtt_service` | Telemetry publishing | `mqtt_service.c` | Optional, 5s default interval |
| `log_buffer` | Circular log capture | `log_buffer.c` | 16KB ring buffer, vprintf hook |
| `resource_guard` | Memory safety | `resource_guard.c` | Heap/stack monitoring |
| `build_info` | Git metadata | `build_info.h` (generated) | Commit hash, branch, timestamp |

### Main Application Tasks (Core 0)

```c
// Tasks created in main.c
Task Name          Stack   Priority  Purpose
─────────────────────────────────────────────────────
status_task        4096*   2         Collects metrics at 20Hz
lcd_update_task    4096*   1         LCD refresh (TTGO only)
web_server_task    auto    default   HTTP request handling
mqtt_publish_task  4096*   2         MQTT telemetry at 5s interval

* TTGO uses reduced stacks: status=2560, lcd=3072, mqtt=3072
```

### Configuration Generation Pipeline

```
rover_config.yaml  ─┐
                    ├─► generate_config.py ─► config_generated.h ─► Compilation
secrets.yaml       ─┘
```

Key defines generated:
- `WIFI_MODE_*` - WiFi mode flags
- `WIFI_AP_SSID`, `WIFI_STA_SSID` - Network names
- `ENABLE_MQTT`, `ENABLE_REST_API` - Feature flags
- `MDNS_HOSTNAME` - Network discovery name
- `CFG_WATCHDOG_TIMEOUT_MS` - Safety timeout

### Memory Budgets

**ESP32-CAM** (has 4MB PSRAM):
- Free heap at startup: ~150KB
- PSRAM available for camera buffers
- OTA rollback enabled (PSRAM buffer)

**TTGO T-Display** (no PSRAM):
- Free heap at startup: ~40-60KB
- Aggressive optimization required
- OTA rollback **disabled** (IRAM constraints)

Critical TTGO optimizations:
```
Log buffer: 16KB (disable with ENABLE_LOG_BUFFER=0 saves 16KB)
HTTP max header: 512B (vs 1024B on ESP32-CAM)
HTTP max URI: 256B (vs 512B)
Max connections: 4
LWIP sockets: 8
MQTT: No SSL/WebSocket
```

### Hardware Pin Maps

**ESP32-CAM:**
```
Camera: GPIO 0,5,18-19,21-23,25-27,32,34-36,39
Flash LED: GPIO 4
JTAG: GPIO 12-15 (alternate use)
```

**TTGO T-Display:**
```
LCD: GPIO 4,5,16,18,19,23 (SPI + control)
Buttons: GPIO 0 (left), GPIO 35 (right)
Battery ADC: GPIO 34
```

### REST API Quick Reference

```
GET  /              Web UI
GET  /stream        MJPEG video (ESP32-CAM)
POST /control       {"speed":-100..100, "steering":-100..100, "estop":bool}
GET  /status        JSON system status
GET  /camera        Camera state
POST /camera?enabled=true/false
GET  /logs          All logs as text
GET  /logs/stream   SSE log stream
DELETE /logs        Clear buffer
POST /ota           Firmware binary + password
```

### Status JSON Structure

```json
{
  "target": "esp32cam|ttgo",
  "battery": 7.4,
  "camera": true,
  "rssi": -45,
  "btnL": false,
  "btnR": true,
  "diag": {
    "ssid": "...",
    "ip": "192.168.x.x",
    "mac": "AA:BB:CC:DD:EE:FF",
    "freeHeap": 150000,
    "uptime": 3600,
    "tasksCore0": 8,
    "tasksCore1": 4,
    "localTime": "14:30:45",
    "ntpSynced": true,
    "internet": true,
    "restApi": true,
    "mqttEnabled": true,
    "mqttConnected": false
  }
}
```

### Test Organization

```
test/
├── src/
│   ├── test_config.c              # 15 tests - YAML config validation
│   ├── test_diag_state_machine.c  # 12 tests - Diagnostic mode FSM
│   ├── test_resource_guard.c      # 13 tests - Memory safety (target only)
│   └── test_runner.c              # Test harness
├── integration/                    # pytest hardware tests
├── Makefile                        # Host-based test build
└── CMakeLists.txt                  # ESP32 target tests
```

Run tests:
```bash
cd test && make test           # Host tests (no hardware)
cd test && make coverage-html  # Coverage report
```

### Common Development Tasks

**Adding a new configuration option:**
1. Add to `config/rover_config.yaml`
2. Update `scripts/generate_config.py` to generate define
3. Regenerate: `source ./firmware/esp-idf/export.sh && python scripts/generate_config.py`
4. Use `#ifdef CONFIG_OPTION` in C code

**Adding a new component:**
1. Create `firmware/components/mycomp/`
2. Add `CMakeLists.txt` with `idf_component_register()`
3. Add `include/mycomp.h` for public API
4. Add to dependencies in `firmware/main/CMakeLists.txt`

**Debugging memory issues:**
1. Check heap: `esp_get_free_heap_size()`
2. Check internal DRAM: `heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)`
3. Check stack watermarks: `uxTaskGetStackHighWaterMark(NULL)`
4. Use resource_guard component thresholds

### Key Implementation Patterns

**Safe task creation:**
```c
#ifdef CONFIG_TARGET_TTGO
#define STATUS_TASK_STACK 2560
#else
#define STATUS_TASK_STACK 4096
#endif
```

**Feature toggle pattern:**
```c
#if ENABLE_MQTT
    mqtt_service_init();
#endif
```

**Target-conditional code:**
```c
#ifdef CONFIG_TARGET_ESP32CAM
    camera_init();
#elif defined(CONFIG_TARGET_TTGO)
    lcd_display_init();
#endif
```

### Version Information

- **Current Version**: Check `CHANGELOG.md` or run build
- **ESP-IDF Version**: v5.2.2 (embedded in `firmware/esp-idf/`)
- **Version in binary**: Accessible via build_info component

### Troubleshooting Quick Reference

| Issue | Cause | Solution |
|-------|-------|----------|
| Build fails after target switch | Old sdkconfig | `./scripts/build.sh <target> fullclean` |
| OTA timeout | Rate limiting | Normal for ESP32-CAM (50 KB/s limit) |
| TTGO heap exhaustion | No PSRAM | Disable log buffer, check stack sizes |
| Camera not initializing | Wrong pins or PSRAM | Verify sdkconfig.defaults.esp32cam |
| WiFi not connecting | Wrong mode/creds | Check rover_config.yaml, secrets.yaml |
| mDNS not working | Firewall/router | Use IP address directly |

### File Locations Quick Reference

```
Main entry point:      firmware/main/main.c
Configuration header:  firmware/main/config.h
Generated config:      firmware/main/config_generated.h
Web server:            firmware/components/web_server/
Camera driver:         firmware/components/camera/
LCD driver:            firmware/components/lcd_display/
Build script:          scripts/build.sh
OTA script:            scripts/ota.sh
Config generator:      scripts/generate_config.py
Main config:           config/rover_config.yaml
Secrets:               config/secrets.yaml
Unit tests:            test/src/
Documentation:         docs/
Binaries archive:      binaries/esp32cam/, binaries/ttgo/
```
