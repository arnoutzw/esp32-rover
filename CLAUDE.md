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

# Regenerate config manually (normally done automatically by build.sh)
# Requires ROVER_TARGET environment variable to be set
source ./firmware/esp-idf/export.sh && ROVER_TARGET=esp32cam python scripts/generate_config.py

# OTA flash to ESP32-CAM (auto rate-limited for stability)
./scripts/ota.sh esp32cam

# OTA flash to TTGO
./scripts/ota.sh ttgo

# OTA flash to specific IP
./scripts/ota.sh esp32cam 192.168.2.88
./scripts/ota.sh ttgo 192.168.2.75
```

## JTAG Flashing via ESP-PROG

**Flash firmware directly via JTAG using openocd-esp32.**

This method bypasses UART and flashes directly to the ESP32's SPI flash via JTAG, which is useful when:
- UART is unavailable or unreliable
- GPIO0 boot mode selection is difficult
- You need faster, more reliable flashing

### Prerequisites

openocd-esp32 is included as a git submodule in `tools/openocd-esp32`. Build it with:

```bash
# Install build dependencies (macOS)
brew install automake autoconf libtool pkg-config libusb libftdi texinfo

# Install build dependencies (Linux)
sudo apt-get install automake autoconf libtool pkg-config libusb-1.0-0-dev libftdi1-dev texinfo

# Build openocd-esp32
./scripts/build-openocd.sh
```

### ESP-PROG JTAG Wiring

Connect ESP-PROG to ESP32-CAM:

| ESP-PROG | ESP32-CAM |
|----------|-----------|
| TDI      | GPIO12    |
| TCK      | GPIO13    |
| TMS      | GPIO14    |
| TDO      | GPIO15    |
| GND      | GND       |
| 3V3      | 3V3       |

### Flash Command

```bash
# Flash latest binary for target
./scripts/jtag-flash.sh esp32cam
./scripts/jtag-flash.sh ttgo

# Flash specific binary
./scripts/jtag-flash.sh esp32cam path/to/firmware.bin
```

Or manually:

```bash
./tools/openocd-esp32/src/openocd \
  -s ./tools/openocd-esp32/tcl \
  -f interface/ftdi/esp_ftdi.cfg \
  -f target/esp32.cfg \
  -c "program_esp binaries/esp32cam/latest.bin 0x0 verify reset exit"
```

### Example Output

```
** Programming Started **
Info : Flash mapping 0: 0x10020 -> 0x3f400020, 281 KB
Info : Flash mapping 1: 0x60020 -> 0x400d0020, 836 KB
Info : Auto-detected flash bank 'esp32.cpu0.flash' size 4096 KB
Info : PROF: Erased 1241088 bytes in 4871.83 ms
Info : PROF: Wrote 1241088 bytes in 4086.12 ms (data transfer time included)
** Programming Finished in 10149 ms **
** Verify Started **
Info : PROF: Flash verified in 642.407 ms
** Verify OK **
** Resetting Target **
```

| Step | Status |
|------|--------|
| Flash detected | 4096 KB |
| Erased | ~1.2 MB in ~4.8s |
| Programmed | ~1.2 MB in ~4.1s |
| Verified | OK |
| Total time | ~10 seconds |

## Target Configuration

The build target is determined entirely by the build command - there is no `target` field in `rover_config.yaml`:
- `./scripts/build.sh esp32cam` → hostname `esp32-rover.local`
- `./scripts/build.sh ttgo` → hostname `ttgo-rover.local`

The build script automatically regenerates `config_generated.h` with the correct target-specific settings (like mDNS hostname) before each build.

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

**CRITICAL: Always Push Immediately After Every Commit**

Every `git commit` MUST be immediately followed by `git push`. This applies to:
- All code changes (firmware, scripts, tests)
- All documentation updates (markdown files, comments)
- All configuration changes
- Commits on any branch (develop, main, feature branches)

**Workflow:**
```bash
# Normal development - ALWAYS commit AND push together
git checkout develop
# ... make changes ...
git add -A && git commit -m "Description of changes" && git push

# When user requests a release to main:
git checkout main
git merge develop
git tag -a v1.X.X -m "Release description"
git push origin main --tags  # Push commits AND tags immediately
git checkout develop
```

**Why this matters:**
- Changes are synced to remote immediately
- CI/CD pipelines are triggered without delay
- Team members have access to latest code
- Release workflows execute automatically
- No risk of losing work if local machine fails
- Documentation updates are immediately visible to all users

### Clean Build Rule

**All builds and flashes are automatically enforced to use clean git state.**

The build script (`./scripts/build.sh`) now automatically:
1. Checks for uncommitted changes before building
2. Verifies all commits are pushed to origin
3. Blocks the build if the git state is dirty

**If you attempt to build with uncommitted or unpushed changes, you will see:**
```
Error: You have uncommitted changes

Uncommitted changes:
 M firmware/components/web_server/web_server.c

Please commit your changes before building:
  git add -A
  git commit -m "Your commit message"
  git push origin develop
```

**This enforcement ensures:**
- Every build is traceable to a specific commit that exists in the remote repository
- Build fingerprint in firmware always matches a known, pushed state
- No accidental "works on my machine" issues from uncommitted changes
- Firmware versions can be reliably reproduced from git history
- No more "dirty" builds with untraceable modifications

### Bug Documentation

**For every investigated bug with a confirmed fix, add a new entry to `docs/implementation/DEVELOPMENT_LESSONS.md`.**

Each entry should include:
1. **Symptom**: What the user observed (error message, behavior)
2. **Root Cause**: The underlying technical issue
3. **Investigation Process**: How the bug was diagnosed
4. **Solution**: The fix applied
5. **Lesson Learned**: Key takeaway for future development

This ensures knowledge is preserved and similar issues can be avoided or quickly diagnosed in the future.

### Documentation Updates

**When making changes to firmware features, APIs, or configuration, update the relevant documentation.**

Required updates for different change types:

| Change Type | Update Required |
|-------------|-----------------|
| New feature or API change | `CHANGELOG.md` (under [Unreleased]), this file's Technical Deep Dive section |
| Configuration change | `CHANGELOG.md`, `config/rover_config.yaml` comments, this file |
| Bug fix | `CHANGELOG.md`, `docs/implementation/DEVELOPMENT_LESSONS.md` |
| Status JSON change | `CHANGELOG.md`, Status JSON Structure section in this file |
| New endpoint | REST API Quick Reference section in this file |
| Build/tooling change | `CHANGELOG.md`, Common Commands section in this file |

**Documentation files to keep synchronized:**
- `CHANGELOG.md` - All user-visible changes
- `CLAUDE.md` - Technical reference for AI assistants and developers
- `docs/implementation/DEVELOPMENT_LESSONS.md` - Bug investigations and lessons learned
- `config/rover_config.yaml` - Inline comments for configuration options

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
  "velocity": 0.0,
  "battery": 7.4,
  "camera": true,
  "rssi": -45,
  "btnL": false,
  "btnR": true,
  "diag": {
    "ssid": "NetworkName",
    "ip": "192.168.x.x",
    "wifiMode": "sta|ap|apsta",
    "channel": 1,
    "clients": 2,
    "txPower": 19,
    "freeHeap": 150000,
    "minHeap": 140000,
    "totalHeap": 295000,
    "freeInternal": 260000,
    "uptime": 3600,
    "restApi": true,
    "mqttEnabled": true,
    "mqttConnected": false,
    "localTime": "14:30:45",
    "ntpSynced": true,
    "buildVersion": "2.0+34",
    "buildFingerprint": "abc1234",
    "buildTime": "2026-01-25T13:34:02Z",
    "buildBranch": "develop",
    "buildDirty": false
  }
}
```

**Note:** The `clients` field is only present when `wifiMode` is "ap" or "apsta" (Access Point mode). In "sta" (Station) mode, clients count is not relevant and omitted from the response.

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
