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

# OTA flash to ESP32-CAM (via mDNS)
curl -X POST -H "X-OTA-Password: rover1234" --data-binary @firmware/build/esp32-rover.bin http://esp32-rover.local/ota

# OTA flash to TTGO (via mDNS)
curl -X POST -H "X-OTA-Password: rover1234" --data-binary @firmware/build/esp32-rover.bin http://ttgo-rover.local/ota

# OTA flash via direct IP
curl -X POST -H "X-OTA-Password: rover1234" --data-binary @firmware/build/esp32-rover.bin http://<IP>/ota
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
