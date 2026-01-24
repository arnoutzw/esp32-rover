# ESP32 Rover Firmware - Project Notes

## ESP-IDF Location

ESP-IDF is embedded within this project (not installed globally):

```bash
# Source ESP-IDF environment before running Python scripts or builds
source ./esp-idf/export.sh
```

**Full path**: `/Users/arnoutzwartbol/workspaces/obsidian/MyVault/projects/ESP32_Rover/esp32-rover-firmware/esp-idf/export.sh`

## Common Commands

```bash
# Build for ESP32-CAM
./build.sh esp32cam

# Build for TTGO T-Display
./build.sh ttgo

# Regenerate config (after changing rover_config.yaml)
source ./esp-idf/export.sh && python generate_config.py

# OTA flash to ESP32-CAM (via mDNS)
curl -X POST -H "X-OTA-Password: rover1234" --data-binary @build/esp32-rover.bin http://esp32-rover.local/ota

# OTA flash to TTGO (via mDNS)
curl -X POST -H "X-OTA-Password: rover1234" --data-binary @build/esp32-rover.bin http://ttgo-rover.local/ota

# OTA flash via direct IP
curl -X POST -H "X-OTA-Password: rover1234" --data-binary @build/esp32-rover.bin http://<IP>/ota
```

## Target Configuration

The `target` field in `rover_config.yaml` controls mDNS hostname generation:
- `target: esp32cam` → hostname `esp32-rover.local`
- `target: ttgo` → hostname `ttgo-rover.local`

The actual build target is selected via `./build.sh esp32cam` or `./build.sh ttgo`.

## Key Files

- `rover_config.yaml` - Main configuration file
- `secrets.yaml` - WiFi passwords, OTA password (not in git)
- `generate_config.py` - Generates `main/config_generated.h` from YAML
- `main/config.h` - Hardware pin definitions
- `main/config_generated.h` - Auto-generated config defines
