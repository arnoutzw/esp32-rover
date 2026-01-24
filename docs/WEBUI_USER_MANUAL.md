# ESP32 Rover - Web UI User Manual

This manual explains how to use the web-based control interface for the ESP32 Rover.

## Table of Contents

- [Supported Hardware](#supported-hardware)
- [Building the Firmware](#building-the-firmware)
- [Getting Connected](#getting-connected)
- [Interface Overview](#interface-overview)
- [Control Elements](#control-elements)
- [Hardware Button Indicators](#hardware-button-indicators)
- [Diagnostic Mode](#diagnostic-mode-ttgo-t-display-only)
- [Understanding the Display](#understanding-the-display)
- [Tips for Best Performance](#tips-for-best-performance)
- [Troubleshooting](#troubleshooting)

---

## Supported Hardware

The ESP32 Rover firmware supports two hardware configurations:

| Build Target | Board | Camera | Description |
|--------------|-------|--------|-------------|
| **ESP32-CAM** | AI-Thinker ESP32-CAM | Yes | Full features with live video streaming |
| **TTGO T-Display** | LilyGO TTGO T-Display | No | Motor/servo control only |

### Feature Comparison

| Feature | ESP32-CAM | TTGO T-Display |
|---------|-----------|----------------|
| Live Video | Yes | No (shows "Camera Error") |
| Motor Control | Yes | Yes |
| Servo Steering | Yes | Yes |
| Telemetry | Yes | Yes |
| Web Interface | Yes | Yes |
| LCD Display | No | Yes (built-in ST7789) |
| Hardware Buttons | No | Yes (L/R indicators) |

**Note**: When using TTGO T-Display, the camera panel will show "Camera Error" permanently - this is expected behavior since the board has no camera hardware. However, it has a built-in LCD display and two hardware buttons that are shown in the telemetry section.

---

## Building the Firmware

This project is **self-contained** with ESP-IDF v5.2.2 embedded. No external ESP-IDF installation required.

### First-Time Setup (run once)

```bash
# Install ESP-IDF toolchain (~1GB download)
./setup.sh
```

### Using the Build Script (Recommended)

```bash
# Build for ESP32-CAM (with camera)
./build.sh esp32cam

# Build for TTGO T-Display (no camera)
./build.sh ttgo

# Build and flash to device
./build.sh esp32cam flash

# Build and flash to specific port
./build.sh ttgo flash -p /dev/cu.usbserial-0001

# Open serial monitor
./build.sh ttgo monitor
```

### Manual Build

If you prefer to use idf.py directly:

```bash
# Source the embedded ESP-IDF
source esp-idf/export.sh

# For ESP32-CAM
cp sdkconfig.defaults.esp32cam sdkconfig.defaults
rm -f sdkconfig  # Remove old config
ROVER_TARGET=esp32cam idf.py build flash monitor

# For TTGO T-Display
cp sdkconfig.defaults.ttgo sdkconfig.defaults
rm -f sdkconfig  # Remove old config
ROVER_TARGET=ttgo idf.py build flash monitor
```

### Switching Targets

When switching between build targets, perform a full clean:

```bash
./build.sh ttgo fullclean
./build.sh esp32cam build
```

---

## Getting Connected

### Step 1: Power On the Rover

When powered on, the ESP32 creates a WiFi access point. You'll see this in the serial monitor:

```
WiFi AP started. SSID: ESP32-Rover, Password: rover1234
Connect to WiFi 'ESP32-Rover' and open http://192.168.4.1
```

### Step 2: Connect to WiFi

1. On your phone or computer, open WiFi settings
2. Find and connect to **"ESP32-Rover"**
3. Enter password: **rover1234**
4. Wait for connection (you may see "No Internet" warning - this is normal)

### Step 3: Open the Control Interface

1. Open a web browser
2. Navigate to: **http://192.168.4.1**
3. The control interface will load

---

## Interface Overview

The web interface is divided into several sections:

```
┌─────────────────────────────────────────────────────────────┐
│  ┌─────────────────────────────────────────────────────┐   │
│  │ [1] HEADER BAR                                       │   │
│  │     ESP32-CAM Rover          Connected ●             │   │
│  └─────────────────────────────────────────────────────┘   │
│                                                             │
│  ┌─────────────────────┐  ┌───────────────────────────┐   │
│  │                     │  │ [3] JOYSTICK              │   │
│  │ [2] CAMERA VIEW     │  │                           │   │
│  │                     │  │      ┌─────┐              │   │
│  │   (Live Stream)     │  │      │  ●  │              │   │
│  │                     │  │      └─────┘              │   │
│  │                     │  │                           │   │
│  └─────────────────────┘  ├───────────────────────────┤   │
│                           │ [4] MAX SPEED   [5] TRIM   │   │
│                           │ ────●──────     ────●────  │   │
│                           ├───────────────────────────┤   │
│                           │ [6] EMERGENCY STOP        │   │
│                           │ ┌───────────────────────┐ │   │
│                           │ │   EMERGENCY STOP      │ │   │
│                           │ └───────────────────────┘ │   │
│                           ├───────────────────────────┤   │
│                           │ [7] TELEMETRY             │   │
│                           │ Speed: 0%   Steering: 0°  │   │
│                           │ Velocity: 0 rad/s         │   │
│                           │ Battery: 7.4V             │   │
│                           │ Hardware Buttons: [L] [R] │   │
│                           └───────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

---

## Control Elements

### [1] Header Bar

```
┌─────────────────────────────────────────────────────────────┐
│  ESP32-CAM Rover                    Disconnected  ○         │
└─────────────────────────────────────────────────────────────┘
```

**Title**: Shows the device name

**Connection Status**:
- **Green dot (●) + "Connected"**: Communication with rover is active
- **Red dot (○) + "Disconnected"**: No communication - check WiFi connection

---

### [2] Camera View

```
┌─────────────────────────────────────────────────────────────┐
│                                                             │
│                    [ Camera Loading... ]                    │
│                                                             │
│                         - or -                              │
│                                                             │
│                    [ Camera Error ]                         │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

**Live Video Stream**: When camera is enabled and connected, shows real-time MJPEG video from the rover.

**Status Messages**:
- **"Camera Loading..."**: Stream is initializing
- **"Camera Error"**: Camera unavailable (automatically retries every 2 seconds)
- If using TTGO T-Display (no camera), this panel will show "Camera Error" permanently - this is expected

---

### [3] Virtual Joystick

```
        ┌─────────────────────┐
        │         ↑           │
        │    FORWARD          │
        │                     │
        │  ← LEFT  ●  RIGHT → │
        │                     │
        │    REVERSE          │
        │         ↓           │
        └─────────────────────┘
```

**How to Use**:

1. **Touch/Click** anywhere on the joystick area
2. **Drag** to control:
   - **Up**: Move forward (positive speed)
   - **Down**: Move reverse (negative speed)
   - **Left**: Steer left (negative steering)
   - **Right**: Steer right (positive steering)
3. **Release** to stop (joystick returns to center)

**Behavior**:
- The farther from center, the more speed/steering applied
- Diagonal movements combine speed and steering
- Values range from -100% to +100%

**Mobile Users**: The joystick area prevents page scrolling to avoid accidental movements while controlling.

---

### [4] Max Speed Slider

```
MAX SPEED
├──────────●──────────┤  50%
0%                   100%
```

**Purpose**: Limits the maximum speed sent to the motor.

**How it Works**:
- Joystick value is multiplied by this percentage
- At 50%: Full joystick forward = 50% motor speed
- At 100%: Full joystick forward = 100% motor speed

**Recommended Settings**:
| Environment | Max Speed |
|-------------|-----------|
| Indoor testing | 20-30% |
| Open area | 50-70% |
| Experienced user | 80-100% |

**Safety Note**: Start with low speed until you're comfortable with the controls!

---

### [5] Trim Slider

```
TRIM
├────────────●────────────┤  0
-20                      +20
```

**Purpose**: Corrects steering drift when driving straight.

**When to Use**:
- If the rover pulls left when you want to go straight → Add positive trim (+)
- If the rover pulls right when you want to go straight → Add negative trim (-)

**How it Works**:
- Trim value is **added** to the joystick steering value
- Example: Joystick at center (0°) + Trim at +5 = Steering output of +5°

**Adjustment Procedure**:
1. Set speed to low (~20%)
2. Try to drive straight
3. If rover drifts, adjust trim in opposite direction
4. Repeat until rover drives straight with joystick centered

**Note**: Trim settings are not saved - they reset when you refresh the page.

---

### [6] Emergency Stop Button

```
┌─────────────────────────────────────────┐
│           EMERGENCY STOP                │
│              (Red)                      │
└─────────────────────────────────────────┘
```

**Purpose**: Immediately stops all motor movement.

**When to Use**:
- Rover heading toward obstacle
- Loss of control
- Any unsafe situation

**What Happens When Pressed**:
1. Speed immediately set to 0
2. Steering set to 0
3. Emergency stop flag sent to rover
4. Motor is disabled briefly

**Important**: The rover will resume normal operation after you move the joystick again. This is not a persistent lock.

---

### [7] Telemetry Display

```
┌─────────────────────────────────────────┐
│  Speed          │  Steering             │
│  45%            │  -23°                 │
├─────────────────┼───────────────────────┤
│  Velocity       │  Battery              │
│  3.2 rad/s      │  7.4V                 │
├─────────────────┴───────────────────────┤
│  Hardware Buttons                       │
│  [L]  [R]                               │
└─────────────────────────────────────────┘
```

**Speed**: Current joystick speed value (-100% to +100%)
- Negative = Reverse
- Positive = Forward

**Steering**: Current joystick steering value + trim (-100° to +100°)
- Negative = Left
- Positive = Right

**Velocity**: Actual motor velocity in radians per second
- Read from the encoder
- Shows real wheel speed, not commanded speed
- Useful for verifying motor is responding

**Battery**: Battery voltage reading (TTGO T-Display reads internal single-cell Li-ion)
- Full charge: 4.2V
- Nominal: 3.7V
- Low battery warning: Below 3.4V
- Critical: Below 3.0V (stop using immediately!)

**Hardware Buttons** (TTGO T-Display only): Shows the state of the two physical buttons on the front of the TTGO T-Display board.
- **[L]** - Left button (GPIO 0) - Lights up blue when pressed
- **[R]** - Right button (GPIO 35) - Lights up blue when pressed
- Gray background = not pressed
- Blue background with glow = pressed

**Note**: On ESP32-CAM builds, the hardware button indicators will always show as not pressed since that board doesn't have these buttons.

---

## Hardware Button Indicators

The TTGO T-Display board has two physical buttons on the front panel that can be monitored via the web interface.

### Button Layout on TTGO T-Display

```
┌────────────────────────────┐
│     TTGO T-Display         │
│  ┌──────────────────────┐  │
│  │                      │  │
│  │      LCD Screen      │  │
│  │                      │  │
│  └──────────────────────┘  │
│                            │
│   [L]              [R]     │
│  GPIO 0          GPIO 35   │
└────────────────────────────┘
```

### Web UI Button Indicators

```
Hardware Buttons
┌─────┐  ┌─────┐
│  L  │  │  R  │
└─────┘  └─────┘
  ↑         ↑
 Gray      Gray
(not pressed)

Hardware Buttons
┌─────┐  ┌─────┐
│  L  │  │  R  │
└─────┘  └─────┘
  ↑
 Blue
 Glow
(pressed)
```

### Visual States

| State | Background | Text | Border | Effect |
|-------|------------|------|--------|--------|
| Not Pressed | Dark gray (#2d2d44) | Gray (#666) | Gray (#444) | None |
| Pressed | Blue (#3282b8) | White | Blue | Blue glow shadow |

### Use Cases

The hardware buttons can be used for:

1. **Testing connectivity** - Press buttons to verify WebSocket/HTTP communication
2. **Physical feedback** - Confirm the rover is responding to web commands
3. **Diagnostic Mode** - Hold both buttons for 3 seconds to enter diagnostic screen

### Technical Details

- **Update Rate**: Button states are polled at 20 Hz (every 50ms) for responsive feedback
- **Debouncing**: Hardware buttons use internal pull-up resistors
- **Active State**: Buttons are active LOW (pressed = GPIO reads 0)
- **LCD Optimization**: Button indicators only redraw when state changes to minimize latency

---

## Diagnostic Mode (TTGO T-Display Only)

The TTGO T-Display has a built-in diagnostic screen that shows detailed system and WiFi information.

### Entering Diagnostic Mode

1. **Hold both front buttons (L + R) simultaneously**
2. **Keep holding for 3 seconds**
3. The screen will switch to the diagnostic display

### Exiting Diagnostic Mode

- **Release both buttons** to return to the normal rover status display

### Diagnostic Information Displayed

```
┌─────────────────────────────────┐
│      DIAGNOSTICS                │
├─────────────────────────────────┤
│  -- WiFi --                     │
│  SSID: ESP32-Rover              │
│  Chan: 1      TX: 20dBm         │
│  Clients: 1                     │
│  IP: 192.168.4.1                │
│  MAC: C4:4F:33:6A:4E:61         │
├─────────────────────────────────┤
│  -- System --                   │
│  CPU: 240 MHz                   │
│  Heap: 156 KB                   │
│  Min: 142 KB                    │
│  Batt: 3.85V                    │
│  Up: 00:15:32                   │
├─────────────────────────────────┤
│  Release to exit                │
└─────────────────────────────────┘
```

### WiFi Section

| Field | Description |
|-------|-------------|
| **SSID** | Access point name |
| **Chan** | WiFi channel (1-13) |
| **TX** | Transmit power in dBm |
| **Clients** | Number of connected stations |
| **IP** | Device IP address |
| **MAC** | Device MAC address |

### System Section

| Field | Description |
|-------|-------------|
| **CPU** | CPU frequency (typically 240 MHz) |
| **Heap** | Current free heap memory |
| **Min** | Minimum free heap since boot (helps detect memory leaks) |
| **Batt** | Battery voltage (color-coded: green > 3.7V, yellow > 3.4V, red < 3.4V) |
| **Up** | Uptime since last reboot (HH:MM:SS) |

### Use Cases for Diagnostic Mode

1. **Troubleshooting WiFi issues** - Check channel, TX power, and connected clients
2. **Memory monitoring** - Monitor heap usage for memory leaks
3. **Battery health** - Check precise battery voltage
4. **System verification** - Confirm CPU frequency and uptime

---

## Understanding the Display

### Speed vs Velocity

| Term | What it Shows | Source |
|------|---------------|--------|
| **Speed** | Commanded speed (%) | Joystick input |
| **Velocity** | Actual motor speed (rad/s) | Encoder feedback |

If Speed is high but Velocity is 0:
- Motor may be stuck
- Encoder not working
- Motor driver issue

---

## Tips for Best Performance

### General Operation

1. **Always start with low speed** - Get familiar with controls first
2. **Keep the rover in sight** - WiFi range is limited (~20-30m open area)
3. **Watch the connection indicator** - Stop if it turns red
4. **Use smooth joystick movements** - Avoid jerky inputs

### Mobile Device Tips

1. **Lock screen rotation** - Prevents accidental rotation while controlling
2. **Turn off auto-sleep** - Keep screen on during operation
3. **Use airplane mode + WiFi** - Prevents phone from switching to mobile data
4. **Close other apps** - Ensures best browser performance

### Best Practices

1. **Pre-flight check**:
   - Battery charged?
   - Wheels free to move?
   - Connection stable?

2. **Test in safe area first**:
   - Start indoors or in enclosed space
   - Verify controls work as expected

3. **Have an exit strategy**:
   - Know where Emergency Stop button is
   - Keep phone charged
   - Stay within WiFi range

---

## Troubleshooting

### "Disconnected" Status

**Symptoms**: Red dot, commands not working

**Solutions**:
1. Check WiFi connection on your device
2. Refresh the web page
3. Move closer to the rover
4. Power cycle the rover

---

### Joystick Not Responding

**Symptoms**: Dragging joystick has no effect

**Solutions**:
1. Refresh the page
2. Check for "Connected" status
3. Try a different browser (Chrome recommended)
4. On mobile: Make sure you're touching inside the joystick circle

---

### Camera Shows "Error"

**Symptoms**: Camera placeholder shows error message

**Possible Causes**:
1. Camera disabled in config (`DISABLE_CAMERA 1`)
2. Camera hardware not connected
3. PSRAM not available (ESP32-CAM requires PSRAM)

**Note**: If using TTGO T-Display, camera is disabled by design.

---

### Rover Not Moving

**Symptoms**: Joystick works, telemetry shows speed, but rover doesn't move

**Check**:
1. Velocity telemetry - Is it changing?
2. Motor enable status - Check serial output
3. Battery voltage - Is it sufficient?
4. Motor connections - Verify wiring

**Debug via Serial**:
```bash
idf.py -p /dev/cu.usbserial-XXXX monitor
```

Look for motor/encoder error messages.

---

### Steering Reversed

**Symptoms**: Left on joystick = rover goes right

**Solution**: Adjust in `config.h`:
```c
#define STEERING_MAX_ANGLE  -45  // Negative to reverse
```

Or swap servo wires.

---

### Motor Direction Reversed

**Symptoms**: Forward on joystick = rover goes backward

**Solution**: In `config.h`, change motor direction:
```c
.direction = MOTOR_DIR_CCW,  // Try CCW instead of CW
```

---

### High Latency / Lag

**Symptoms**: Commands take long to respond

**Causes**:
1. WiFi interference
2. Browser performance issues
3. Too far from rover

**Solutions**:
1. Move closer to rover
2. Change WiFi channel in `config.h`
3. Close other browser tabs
4. Use a faster device

---

## Quick Reference Card

| Control | Action |
|---------|--------|
| Joystick Up | Forward |
| Joystick Down | Reverse |
| Joystick Left | Steer Left |
| Joystick Right | Steer Right |
| Release Joystick | Stop |
| Max Speed Slider | Limit top speed |
| Trim Slider | Correct drift |
| Emergency Stop | Immediate halt |

---

## WiFi Settings

| Setting | Default Value |
|---------|---------------|
| Network Name (SSID) | ESP32-Rover |
| Password | rover1234 |
| IP Address | 192.168.4.1 |
| Web Port | 80 |

To change these, edit `main/config.h` and reflash.

---

## Need Help?

1. Check the [Troubleshooting](#troubleshooting) section above
2. Monitor serial output for error messages
3. Verify hardware connections
4. Check GitHub issues for known problems

