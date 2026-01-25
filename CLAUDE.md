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
