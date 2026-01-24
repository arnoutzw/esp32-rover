# ESP32 WiFi Rover Firmware

A WiFi-controlled rover using ESP32 with SimpleFOC BLDC motor control and servo steering.

## Features

- **WiFi Control**: Access Point mode with web-based control interface
- **Live Video Stream**: MJPEG stream from OV2640 camera (ESP32-CAM only)
- **BLDC Motor Control**: SimpleFOC-style velocity control with AS5600 encoder
- **Servo Steering**: MG90S servo for front axle Ackermann steering
- **Virtual Joystick**: Touch-friendly web UI with joystick control
- **Safety Features**: Command timeout watchdog and emergency stop
- **Self-Contained**: ESP-IDF v5.2.2 embedded in project

## Supported Hardware

| Target | Board | Camera | Description |
|--------|-------|--------|-------------|
| `esp32cam` | AI-Thinker ESP32-CAM | Yes | Full features with live video |
| `ttgo` | LilyGO TTGO T-Display | No | Motor/servo control only |

### Common Hardware

- SimpleFOC Mini or compatible BLDC driver
- BLDC gimbal motor (2204/2208 or similar)
- AS5600 magnetic encoder
- MG90S servo
- 2S LiPo battery (7.4V)
- 5V BEC for servo power

## Quick Start

### 1. First-Time Setup (run once)

```bash
# Install ESP-IDF tools (downloads ~1GB of toolchain)
./setup.sh
```

### 2. Build and Flash

```bash
# Build for TTGO T-Display (no camera)
./build.sh ttgo

# Build for ESP32-CAM (with camera)
./build.sh esp32cam

# Build and flash
./build.sh ttgo flash

# Build, flash, and open monitor
./build.sh esp32cam flash monitor

# Flash to specific port
./build.sh ttgo flash -p /dev/cu.usbserial-0001
```

### 3. Connect and Control

1. Power on the rover
2. Connect to WiFi: **ESP32-Rover** (password: **rover1234**)
3. Open http://192.168.4.1 in a web browser
4. Use the virtual joystick to drive!

## Project Structure

```
esp32-rover-firmware/
├── main/
│   ├── main.c              # Application entry point
│   └── config.h            # Hardware configuration
├── components/             # Reusable ESP-IDF components
│   ├── as5600/             # Magnetic encoder driver
│   ├── bldc_motor/         # BLDC motor controller
│   ├── servo_control/      # Servo driver
│   ├── camera/             # Camera module
│   └── web_server/         # HTTP server with UI
├── esp-idf/                # Embedded ESP-IDF v5.2.2
├── docs/
│   ├── API_REFERENCE.md    # Component API documentation
│   └── WEBUI_USER_MANUAL.md # Web UI user guide
├── build.sh                # Build script
├── setup.sh                # First-time setup script
├── sdkconfig.defaults.esp32cam
└── sdkconfig.defaults.ttgo
```

## Pin Configuration

### TTGO T-Display

| GPIO | Function |
|------|----------|
| 25 | Motor IN1 |
| 26 | Motor IN2 |
| 27 | Motor IN3 |
| 33 | Motor EN |
| 21 | I2C SDA (encoder) |
| 22 | I2C SCL (encoder) |
| 32 | Servo PWM |

### ESP32-CAM

| GPIO | Function |
|------|----------|
| 12 | Motor IN1 |
| 13 | Motor IN2 |
| 14 | Motor IN3 |
| 15 | Motor EN |
| 14 | I2C SDA (encoder) |
| 15 | I2C SCL (encoder) |
| 2 | Servo PWM |
| Many | Camera (see config.h) |

## Configuration

Edit `main/config.h` to customize:

- WiFi SSID and password
- Motor parameters (pole pairs, voltage limit, PID gains)
- Servo pulse widths and steering range
- Control loop frequency

## Build Commands Reference

```bash
# Available commands
./build.sh <target> [command]

# Targets: esp32cam, ttgo
# Commands: build, flash, monitor, clean, fullclean, menuconfig

# Examples
./build.sh ttgo build          # Build only
./build.sh esp32cam flash      # Build and flash
./build.sh ttgo monitor        # Open serial monitor
./build.sh esp32cam fullclean  # Clean everything
```

## Web API

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/` | GET | Web control interface |
| `/stream` | GET | MJPEG video stream |
| `/control` | POST | Send control commands |
| `/status` | GET | Get rover status |

### Control Command Format

```json
{
  "speed": -100 to 100,
  "steering": -100 to 100,
  "estop": true/false
}
```

## Architecture

```
Core 0: Camera capture, Web server, WiFi
Core 1: Motor control loop (100Hz)
```

## Documentation

- [API Reference](docs/API_REFERENCE.md) - Component APIs and configuration
- [Web UI User Manual](docs/WEBUI_USER_MANUAL.md) - Control interface guide

## Troubleshooting

- **Motor not moving**: Check encoder magnet position, verify I2C connection
- **Camera not working**: Ensure you're using ESP32-CAM build with PSRAM
- **WiFi connection issues**: Move closer to rover, check for interference
- **Servo jitter**: Ensure adequate 5V power supply
- **Build fails**: Run `./setup.sh` first to install tools

## License

MIT License
