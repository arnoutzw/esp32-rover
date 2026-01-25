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

### Plan Management

**All implementation plans MUST be stored in `CLAUDE/plans/` directory.**

When creating plans for features, refactoring, or multi-step implementations:

1. **Create a plan file** in `CLAUDE/plans/` with a descriptive name (e.g., `wifi-retry-implementation.md`, `memory-optimization-plan.md`)

2. **Plan file structure:**
   ```markdown
   # Plan: [Descriptive Title]

   ## Overview
   Brief description of what this plan accomplishes.

   ## Priority & Rationale
   - **Priority**: Critical / High / Medium / Low
   - **Why this priority**: [Argumentation for the priority level]
   - **Dependencies**: [Any prerequisites or blocking items]

   ## Implementation Steps
   Ordered list of concrete steps with clear acceptance criteria.

   ## Files to Modify
   Table of files and the changes needed.

   ## Verification
   How to verify the implementation is complete and correct.
   ```

3. **Priority levels with argumentation:**
   - **Critical**: Safety issues, data loss risks, or complete feature breakage. *Must include justification.*
   - **High**: Significant functionality impact or blocking other work. *Explain what is blocked.*
   - **Medium**: Improvements that enhance quality but aren't urgent. *Describe the benefit.*
   - **Low**: Nice-to-have enhancements. *Note why it can wait.*

4. **Always prioritize existing plans**: Before starting new work, check `CLAUDE/plans/` for incomplete plans and prioritize them based on their documented priority and rationale.

5. **Update plan status**: Mark plans as complete or archive them when finished.

**Why this matters:**
- Plans persist across sessions and context resets
- Prioritization with argumentation ensures informed decision-making
- Clear documentation prevents duplicate work
- Progress can be tracked and resumed

### Git Branching Strategy

**All development work happens on the `develop` branch. NEVER commit directly to `main`.**

**Strict Rules:**
- Work on `develop` branch for ALL changes (code, docs, config, everything)
- Commit frequently to `develop` with descriptive messages
- **NEVER commit directly to `main`** - main only receives merges from develop
- **NEVER checkout main to make changes** - only for merging/tagging releases
- Only merge to `main` and create a version tag when the user explicitly requests a release

**CRITICAL: Always Push Immediately After Every Commit**

Every `git commit` MUST be immediately followed by `git push`. This applies to:
- All code changes (firmware, scripts, tests)
- All documentation updates (markdown files, comments)
- All configuration changes
- Commits on any branch (develop, feature branches)

**Workflow:**
```bash
# Normal development - ALWAYS commit AND push together
git checkout develop
# ... make changes ...
git add -A && git commit -m "Description of changes" && git push origin develop

# When user requests a release to main:
git checkout main
git merge develop --no-ff -m "Release vX.X.X"
git tag -a vX.X.X -m "Release description"
git push origin main --tags  # Push commits AND tags immediately
git checkout develop
git push origin develop  # Ensure develop is also pushed
```

**Why never commit to main directly:**
- `main` should only contain tagged releases
- Direct commits to main cause divergence between branches
- Merging main back to develop creates confusing history
- All changes must be tested on develop first

**Why push immediately:**
- Changes are synced to remote immediately
- CI/CD pipelines are triggered without delay
- Team members have access to latest code
- Release workflows execute automatically
- No risk of losing work if local machine fails
- Documentation updates are immediately visible to all users

### Commit Before Build Rule

**NEVER attempt a build with uncommitted changes. Always commit first.**

Before running ANY build command (`./scripts/build.sh`, `idf.py build`, or similar):
1. Stage all changes: `git add -A`
2. Commit with a descriptive message: `git commit -m "Description"`
3. Push to origin: `git push origin develop`
4. Only then run the build command

**This ensures:**
- The git commit hash embedded in the firmware matches the actual source code
- Build fingerprint verification works correctly after flashing
- No "dirty" builds that cannot be reproduced from git history
- Every flashed firmware can be traced to an exact commit

**Workflow example:**
```bash
# After making code changes, ALWAYS do this sequence:
git add -A
git commit -m "Add WiFi retry logic for robust STA connection"
git push origin develop
./scripts/build.sh ttgo
```

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

### Build and Flash Verification Rule

**ALWAYS use `./scripts/build.sh` for builds, never `idf.py build` directly.**

The build script:
- Archives the binary with git metadata (commit hash, timestamp)
- Updates `latest.bin` symlink used by OTA script
- Enforces clean git state
- Generates `config_generated.h` automatically

**After every OTA flash, verify the firmware fingerprint matches:**

```bash
# Get the expected commit hash from the build
git rev-parse --short HEAD  # e.g., c184d71

# Verify the device is running the correct firmware
curl -s http://<device-ip>/status | jq '.diag.buildFingerprint'
# Should output: "c184d71"
```

**If fingerprints don't match:**
1. You may have flashed an old archived binary (check `binaries/<target>/latest.bin` symlink)
2. The device may have rolled back to a previous OTA partition
3. Rebuild with `./scripts/build.sh <target>` to create a fresh archived binary

### Bug Documentation

**For every investigated bug with a confirmed fix, add a new entry to `docs/implementation/DEVELOPMENT_LESSONS.md`.**

Each entry should include:
1. **Symptom**: What the user observed (error message, behavior)
2. **Root Cause**: The underlying technical issue
3. **Investigation Process**: How the bug was diagnosed
4. **Solution**: The fix applied
5. **Lesson Learned**: Key takeaway for future development

This ensures knowledge is preserved and similar issues can be avoided or quickly diagnosed in the future.

### Bug Report Analysis

**For every analyzed bug report, create both an RCA (Root Cause Analysis) and a FIX_PLAN document.**

Bug reports are stored in `docs/bugreport/v{VERSION}/{HASH}/` where:
- `{VERSION}` is the firmware version (e.g., `v2.0.1`)
- `{HASH}` is the 7-character git commit hash (e.g., `e4b11fc`)

**Required documents for each analyzed bug:**

1. **RCA (Root Cause Analysis)** - `RCA_v{VERSION}_{HASH}.md`
   ```markdown
   # Root Cause Analysis: [Bug Title]

   ## Summary
   Brief description of the bug and its impact.

   ## Symptoms
   - What the user observed
   - Error messages, logs, or unexpected behavior

   ## Environment
   - Firmware version and build hash
   - Target hardware (TTGO/ESP32-CAM)
   - Configuration settings relevant to the bug

   ## Investigation
   Step-by-step analysis of how the root cause was identified.

   ## Root Cause
   Technical explanation of why the bug occurred.

   ## Contributing Factors
   Any secondary issues that enabled or worsened the bug.

   ## Impact Assessment
   - Severity: Critical / High / Medium / Low
   - Affected functionality
   - Risk of recurrence
   ```

2. **FIX_PLAN** - `FIX_PLAN_v{VERSION}_{HASH}.md`
   ```markdown
   # Fix Plan: [Bug Title]

   ## Overview
   Brief description of the fix approach.

   ## Related RCA
   Link to the RCA document.

   ## Proposed Fix
   Detailed description of the code changes.

   ## Files to Modify
   | File | Change |
   |------|--------|
   | path/to/file.c | Description of change |

   ## Implementation Steps
   1. Step one
   2. Step two

   ## Testing Plan
   How to verify the fix works.

   ## Rollback Plan
   How to revert if the fix causes issues.
   ```

**Workflow:**
1. User reports a bug or issue is discovered
2. Create the version/hash folder in `docs/bugreport/`
3. Create initial bug report document
4. Analyze and create `RCA_*.md`
5. Design fix and create `FIX_PLAN_*.md`
6. Implement fix following the plan
7. Update RCA with "Resolution" section after fix is verified

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

## Embedded Coding Standards

These rules are based on MISRA C, BARR-C, CERT C, and IEC 62443 standards, adapted for ESP-IDF and FreeRTOS embedded development.

### Critical Priority

#### 1. Error Handling Policy

**Never use `ESP_ERROR_CHECK()` for non-critical initialization** - it crashes the entire device on failure.

```c
// BAD - crashes device if wifi init fails
ESP_ERROR_CHECK(esp_wifi_init(&cfg));

// GOOD - handles error gracefully
esp_err_t ret = esp_wifi_init(&cfg);
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "WiFi init failed: %s", esp_err_to_name(ret));
    return ret;
}
```

**Rules:**
- Use `ESP_ERROR_CHECK()` only for truly fatal errors during startup (e.g., NVS init failure)
- Always check return values from `esp_*` functions
- Log all errors with context using `esp_err_to_name()`
- Propagate errors up the call stack - don't silently continue

#### 2. Memory Management Rules

**Always check allocation returns for NULL before use.**

```c
// BAD - no NULL check
TaskStatus_t *task_array = pvPortMalloc(num_tasks * sizeof(TaskStatus_t));
task_array[0].xHandle = NULL;  // Crash if allocation failed

// GOOD - check before use
TaskStatus_t *task_array = pvPortMalloc(num_tasks * sizeof(TaskStatus_t));
if (task_array == NULL) {
    ESP_LOGE(TAG, "Failed to allocate task array");
    return ESP_ERR_NO_MEM;
}
```

**Rules:**
- Check `malloc()`, `pvPortMalloc()`, `heap_caps_malloc()` returns for NULL
- Pair every allocation with deallocation in error paths (no leaks)
- Prefer static allocation for fixed-size buffers
- Document memory ownership in function comments
- Use `heap_caps_malloc(MALLOC_CAP_INTERNAL)` for DMA buffers

#### 3. Input Validation Rules

**Validate all external inputs at system boundaries.**

```c
// BAD - trusts input blindly
int speed = atoi(req->uri_query["speed"]);
motor_set_speed(speed);

// GOOD - validate bounds
int speed = atoi(speed_str);
if (speed < -100 || speed > 100) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Speed must be -100..100");
    return ESP_FAIL;
}
```

**Rules:**
- Validate all HTTP parameters (bounds, type, length)
- Validate config values at startup
- Check array indices before access
- Sanitize strings before use in format functions (prevent format string attacks)

#### 4. Watchdog Usage Policy

**Ensure all long-running operations yield to the watchdog.**

```c
// BAD - blocks watchdog for too long
while (processing) {
    process_chunk();  // May take > 5 seconds
}

// GOOD - yield periodically
while (processing) {
    process_chunk();
    vTaskDelay(pdMS_TO_TICKS(10));  // Yields to scheduler, feeds watchdog
}
```

**Rules:**
- Watchdog timeout is configured at `CFG_WATCHDOG_TIMEOUT_MS` (default 5000ms)
- All loops must call `vTaskDelay()` or `taskYIELD()` within timeout period
- Log warning if operation approaches watchdog timeout
- Never disable watchdog in production code

#### 5. Interrupt/ISR Rules

**All ISR-modified variables MUST be `volatile`.**

```c
// BAD - compiler may optimize away reads
static bool s_button_pressed = false;

// GOOD - ensures memory is re-read each time
static volatile bool s_button_pressed = false;
```

**Rules:**
- Use `volatile` for all variables modified in ISR and read elsewhere
- Use `IRAM_ATTR` for all ISR functions (keeps them in fast RAM)
- Keep ISR code minimal: set flag, notify task, return
- Never call blocking functions (`vTaskDelay`, `xSemaphoreTake`) from ISR
- Use `FromISR` variants: `xTaskNotifyFromISR()`, `xQueueSendFromISR()`

### High Priority

#### 6. Naming Conventions

**Follow these naming patterns consistently:**

| Element | Convention | Example |
|---------|------------|---------|
| Static/file-scope variables | `s_` prefix | `s_battery_voltage` |
| Global variables | `g_` prefix (avoid globals) | `g_system_state` |
| Constants/defines | `UPPER_SNAKE_CASE` | `BATTERY_VOLTAGE_MIN` |
| Functions | `module_verb_noun()` | `wifi_start_ap()` |
| Types | `name_t` suffix | `rover_status_t` |
| Handlers/callbacks | `*_handler` or `*_cb` suffix | `button_isr_handler` |
| Boolean variables | `is_`, `has_`, `can_` prefix | `is_connected` |

#### 7. Function Size/Complexity Limits

**Keep functions small and focused.**

**Rules:**
- Maximum 100 lines of code per function
- Maximum 4 levels of nesting
- Single responsibility per function
- Extract state machines to separate functions
- If a function needs a comment block explaining sections, split it

```c
// BAD - 200+ line function with multiple responsibilities
void lcd_update_task(void *param) {
    // 50 lines of diagnostic mode handling
    // 50 lines of sleep mode handling
    // 100 lines of display update
}

// GOOD - extracted into focused functions
static void handle_diagnostic_mode(void) { /* 30 lines */ }
static void handle_sleep_mode(void) { /* 30 lines */ }
static void update_display(void) { /* 40 lines */ }

void lcd_update_task(void *param) {
    handle_diagnostic_mode();
    handle_sleep_mode();
    update_display();
}
```

#### 8. Return Value Checking

**All `esp_err_t` returns must be checked.**

```c
// BAD - ignoring return value
esp_wifi_get_mode(&wifi_mode);

// GOOD - explicit check
if (esp_wifi_get_mode(&wifi_mode) != ESP_OK) {
    ESP_LOGW(TAG, "Failed to get WiFi mode");
}

// GOOD - intentionally ignored (documented)
(void)esp_wifi_get_mode(&wifi_mode);  // Best-effort, non-critical
```

#### 9. Magic Number Prohibition

**All numeric literals must be named constants.**

```c
// BAD - magic numbers scattered in code
if (voltage < 3.0f) { /* low battery */ }
spi_clock = 40 * 1000 * 1000;
vTaskDelay(5000 / portTICK_PERIOD_MS);

// GOOD - named constants
#define BATTERY_VOLTAGE_EMPTY_V    3.0f
#define BATTERY_VOLTAGE_FULL_V     4.2f
#define LCD_SPI_CLOCK_HZ           (40 * 1000 * 1000)
#define PING_TIMEOUT_MS            5000
```

**Standard constants to define:**
- Battery thresholds: `BATTERY_VOLTAGE_EMPTY_V`, `BATTERY_VOLTAGE_FULL_V`
- Display offsets: `LCD_COL_OFFSET`, `LCD_ROW_OFFSET`
- Timing values: `*_TIMEOUT_MS`, `*_INTERVAL_MS`
- Hardware values: `*_CLOCK_HZ`, `*_DUTY_MAX`

#### 10. Comment Requirements

**Document the "why", not the "what".**

```c
// BAD - states the obvious
i++;  // Increment i

// GOOD - explains non-obvious reasoning
// Use 52-pixel offset because ST7789 has 240x320 panel but we use 135x240 window
#define LCD_COL_OFFSET 52
```

**Rules:**
- File header with purpose (not author/date - git tracks that)
- Function header for non-trivial functions (params, return, side effects)
- Inline comments only for non-obvious logic
- TODO comments must include issue/ticket reference

### Medium Priority

#### 11. Global Variable Policy

**Minimize global state; protect shared data.**

```c
// BAD - unprotected global
float g_battery_voltage;  // Read by multiple tasks

// GOOD - protected by mutex, grouped in struct
typedef struct {
    float battery_voltage;
    SemaphoreHandle_t mutex;
} battery_state_t;
static battery_state_t s_battery = {0};
```

**Rules:**
- Prefer static file-scope over global scope
- Group related variables into structs
- Document thread-safety requirements
- Use mutexes for data shared across tasks

#### 12. Assertion Usage

**Use assertions for debug invariants only.**

```c
// GOOD - debug invariant check
configASSERT(task_handle != NULL);  // Programming error if NULL

// BAD - asserting on runtime condition
configASSERT(esp_wifi_connect() == ESP_OK);  // Don't assert on external failure
```

**Rules:**
- Use `configASSERT()` for catching programming errors
- Never assert on recoverable runtime conditions
- Assertions may be compiled out in release builds

#### 13. Logging Levels Policy

**Use appropriate log levels consistently.**

| Level | Use For | Example |
|-------|---------|---------|
| ERROR | Failures affecting functionality | `ESP_LOGE(TAG, "OTA failed: %s", err)` |
| WARN | Recoverable issues, degraded operation | `ESP_LOGW(TAG, "mDNS init failed, continuing")` |
| INFO | State changes, startup/shutdown | `ESP_LOGI(TAG, "WiFi connected to %s", ssid)` |
| DEBUG | Detailed tracing (disabled in release) | `ESP_LOGD(TAG, "Received command: speed=%d", speed)` |

**Rules:**
- Don't log in tight loops (use rate limiting)
- Include relevant context in log messages
- Use `#if CFG_DEBUG_*` for verbose debug logging

#### 14. Task Priority Policy

**Document priority rationale; avoid inversion.**

```c
// Document why this priority was chosen
#define MOTOR_TASK_PRIORITY    (tskIDLE_PRIORITY + 3)  // High: safety-critical timing
#define STATUS_TASK_PRIORITY   (tskIDLE_PRIORITY + 2)  // Medium: telemetry updates
#define LCD_TASK_PRIORITY      (tskIDLE_PRIORITY + 1)  // Low: display refresh
```

**Rules:**
- Higher priority = more time-critical
- Safety tasks (motor control) get highest priority
- Use priority inheritance mutexes to prevent inversion
- Document priority rationale in code

#### 15. Compiler Warning Policy

**Treat all warnings as errors.**

Build with `-Werror` enabled (default in this project). Never suppress warnings without documented justification.

```c
// BAD - suppressing without reason
#pragma GCC diagnostic ignored "-Wunused-variable"

// GOOD - if suppression is truly needed, document why
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"
// ESP-IDF's httpd_req_to_sockfd returns int but we need to cast to socklen_t
// for setsockopt call - alignment is guaranteed by httpd implementation
socklen_t len = (socklen_t)httpd_req_to_sockfd(req);
#pragma GCC diagnostic pop
```

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
| TTGO heap exhaustion | No PSRAM | Check stack sizes, reduce HTTP buffers |
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
