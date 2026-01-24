# ESP32 WiFi Rover Firmware

A WiFi-controlled rover using ESP32 with SimpleFOC BLDC motor control and servo steering.

<p align="center">
  <img src="docs/images/rover_top_view.svg" alt="Rover Top View" width="400"/>
</p>

## Features

- **WiFi Control**: Access Point mode with web-based control interface
- **Live Video Stream**: MJPEG stream from OV2640 camera (ESP32-CAM only)
- **BLDC Motor Control**: SimpleFOC-style velocity control with AS5600 encoder
- **Servo Steering**: MG90S servo for front axle Ackermann steering
- **Virtual Joystick**: Touch-friendly web UI with joystick control
- **LCD Display**: ST7789 135x240 on-device status display (TTGO only)
- **Hardware Buttons**: Left/Right button input with WebUI indicators (TTGO only)
- **Safety Features**: Command timeout watchdog and emergency stop
- **Self-Contained**: ESP-IDF v5.2.2 embedded in project

## Supported Hardware

| Target | Board | Camera | LCD | Buttons | Description |
|--------|-------|--------|-----|---------|-------------|
| `esp32cam` | AI-Thinker ESP32-CAM | Yes | No | No | Full features with live video |
| `ttgo` | LilyGO TTGO T-Display | No | Yes | Yes | LCD status display + buttons |

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
# Build for TTGO T-Display (with LCD + buttons)
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
│   ├── camera/             # Camera module (ESP32-CAM)
│   ├── lcd_display/        # ST7789 LCD driver (TTGO)
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

| GPIO | Function | Notes |
|------|----------|-------|
| 25 | Motor IN1 | BLDC phase A |
| 26 | Motor IN2 | BLDC phase B |
| 27 | Motor IN3 | BLDC phase C |
| 33 | Motor EN | Motor enable |
| 21 | I2C SDA | AS5600 encoder |
| 22 | I2C SCL | AS5600 encoder |
| 32 | Servo PWM | Steering servo |
| 18 | LCD SCLK | ST7789 SPI clock |
| 19 | LCD MOSI | ST7789 SPI data |
| 16 | LCD DC | ST7789 data/command |
| 5 | LCD CS | ST7789 chip select |
| 23 | LCD RST | ST7789 reset |
| 4 | LCD BL | Backlight PWM |
| 0 | Button L | Left button (active LOW) |
| 35 | Button R | Right button (active LOW) |

### ESP32-CAM

| GPIO | Function | Notes |
|------|----------|-------|
| 12 | Motor IN1 | Boot-sensitive (keep LOW) |
| 13 | Motor IN2 | BLDC phase B |
| 14 | Motor IN3 | BLDC phase C |
| 15 | Motor EN | Motor enable |
| 14 | I2C SDA | AS5600 encoder |
| 15 | I2C SCL | AS5600 encoder |
| 2 | Servo PWM | Has onboard LED |
| Many | Camera | See config.h for full pinout |

## LCD Display (TTGO only)

The TTGO T-Display features a built-in 135x240 ST7789 LCD that shows a condensed status UI:

```
┌─────────────────────────────┐
│ CONNECTED          [L] [R]  │  <- Header + button indicators
├─────────────────────────────┤
│ SPEED        +45%           │
│ ▓▓▓▓▓▓▓▓▓▓▓▓░░░░░░░░░░░░░░ │  <- Bi-directional bar
│ STEER        +12°           │
│ ▓▓▓▓▓▓▓▓▓▓▓▓░░░░░░░░░░░░░░ │
│ VEL          3.2 r/s        │
│ BAT          7.4V ████████  │
├─────────────────────────────┤
│ ESP32-Rover                 │
│ 192.168.4.1                 │
└─────────────────────────────┘
```

- **Header**: Connection status (green=connected, red=waiting)
- **Button indicators**: L/R boxes light up when pressed
- **Speed bar**: Bi-directional progress bar (-100% to +100%)
- **Steering bar**: Shows current steering angle
- **Velocity**: Actual motor speed from encoder
- **Battery**: Voltage with color-coded bar

## Hardware Buttons (TTGO only)

The TTGO T-Display has two front buttons that are exposed through:

1. **LCD Display**: L/R indicators in the header bar
2. **Web UI**: Button indicators in the telemetry section
3. **Status API**: `btnL` and `btnR` fields in `/status` response

Buttons are active LOW with internal pull-ups enabled.

## Configuration

Edit `main/config.h` to customize:

- WiFi SSID and password
- Motor parameters (pole pairs, voltage limit, PID gains)
- Servo pulse widths and steering range
- Control loop frequency
- LCD and button enable/disable

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
| `/stream` | GET | MJPEG video stream (ESP32-CAM) |
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

### Status Response Format

```json
{
  "velocity": 5.2,
  "battery": 7.4,
  "steering": 15.0,
  "motor": true,
  "camera": false,
  "rssi": -45,
  "btnL": false,
  "btnR": true
}
```

## Architecture

```
Core 0: WiFi, Web server, Camera capture, LCD updates, Status updates
Core 1: Motor control loop (100Hz)
```

### Task Allocation

| Task | Core | Priority | Frequency |
|------|------|----------|-----------|
| Motor control | 1 | 5 | 100 Hz |
| Status update | 0 | 2 | 5 Hz |
| LCD update | 0 | 1 | 10 Hz |
| Web server | 0 | - | Event-driven |

## Documentation

- [API Reference](docs/API_REFERENCE.md) - Component APIs and configuration
- [Web UI User Manual](docs/WEBUI_USER_MANUAL.md) - Control interface guide

## Troubleshooting

- **Motor not moving**: Check encoder magnet position, verify I2C connection
- **Camera not working**: Ensure you're using ESP32-CAM build with PSRAM
- **LCD not displaying**: Check SPI connections, verify `ENABLE_LCD_DISPLAY=1`
- **Buttons not responding**: Check GPIO 0/35, verify `ENABLE_BUTTONS=1`
- **WiFi connection issues**: Move closer to rover, check for interference
- **Servo jitter**: Ensure adequate 5V power supply
- **Build fails**: Run `./setup.sh` first to install tools
- **SPI clock error**: Fixed in v1.1 - uses 26MHz for non-IOMUX pin compatibility

## Changelog

### v1.1 (Latest)
- Added LCD display component for TTGO T-Display (ST7789 135x240)
- Added hardware button support with WebUI indicators
- Fixed SPI clock speed for non-IOMUX pins (26MHz max)
- Updated status API with button states

### v1.0
- Initial release with multi-target support
- SimpleFOC-style BLDC motor control
- AS5600 magnetic encoder integration
- Servo steering control
- Web-based control interface
- ESP32-CAM live video streaming

## License

MIT License
