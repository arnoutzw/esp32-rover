# ESP-PROG JTAG Debugging Guide for ESP32-CAM

This guide explains how to connect an ESP-PROG debugger to an ESP32-CAM module for hardware debugging using JTAG.

## Overview

The ESP-PROG is Espressif's official debugging tool that provides:
- **JTAG debugging** via OpenOCD for hardware breakpoints, stepping, and memory inspection
- **Serial programming** via USB-to-UART for flashing firmware

On the ESP32-CAM, the JTAG pins (GPIO 12-15) are normally used for motor control. The firmware's **JTAG Debug Mode** (REQ-38) disables motor initialization to free these pins for debugging.

## Hardware Requirements

| Item | Description |
|------|-------------|
| ESP-PROG | Espressif's debugging board (or compatible FTDI FT2232-based adapter) |
| ESP32-CAM | AI-Thinker ESP32-CAM module |
| Jumper wires | 6x female-to-female dupont wires |
| USB cables | 2x micro-USB (one for ESP-PROG, one for ESP32-CAM power/serial) |

## Pin Connections

### JTAG Interface

Connect the ESP-PROG JTAG header to the ESP32-CAM:

| ESP-PROG JTAG Pin | Signal | ESP32-CAM Pin | Notes |
|-------------------|--------|---------------|-------|
| 1 | VDD | 3.3V | Reference voltage only (do not power ESP32-CAM from here) |
| 2 | TMS | GPIO 14 | Test Mode Select |
| 3 | GND | GND | Ground (required) |
| 4 | TCK | GPIO 13 | Test Clock |
| 5 | GND | - | Optional additional ground |
| 6 | TDO | GPIO 15 | Test Data Out |
| 7 | GND | - | Optional additional ground |
| 8 | TDI | GPIO 12 | Test Data In |
| 9 | GND | - | Optional additional ground |
| 10 | NC | - | Not connected |

### ESP-PROG JTAG Header Pinout

```
Looking at the ESP-PROG board with USB connector facing away:

JTAG Header (2x5 pins):
┌─────────────────────┐
│  2   4   6   8  10  │
│ TMS TCK TDO TDI NC  │
│                     │
│  1   3   5   7   9  │
│ VDD GND GND GND GND │
└─────────────────────┘
```
![[Espressif-ESP-Prog-Debug-Probe-Pinout-CIRCUITSTATE-Electronics-1-1-1920x1344.png]]
### Minimum Required Connections

For JTAG to work, you need at minimum:

```
ESP-PROG          ESP32-CAM
─────────         ─────────
TMS (pin 2)  ───► GPIO 14
TCK (pin 4)  ───► GPIO 13
TDO (pin 6)  ───► GPIO 15
TDI (pin 8)  ───► GPIO 12
GND (pin 3)  ───► GND
```

**Important**: Always connect at least one GND wire. Signal integrity suffers without proper grounding.

## ESP32-CAM Pin Location
![[Pasted image 20260124235207.png]]
```
ESP32-CAM Module (top view, camera connector at top):

              ┌──────────────────┐
              │    [CAMERA]      │
              │                  │
    GPIO 12 ──┤ ●              ● ├── 5V
    GPIO 13 ──┤ ●              ● ├── GND
    GPIO 15 ──┤ ●              ● ├── GPIO 14
    GPIO 14 ──┤ ●              ● ├── GPIO 2
        ... ──┤ ●              ● ├── ...
              │                  │
              │   [ESP32 chip]   │
              │                  │
              └──────────────────┘
```

**Note**: Pin locations may vary by ESP32-CAM variant. Always verify with your board's pinout diagram.

## Wiring Diagram

```
┌─────────────────┐                    ┌─────────────────┐
│    ESP-PROG     │                    │   ESP32-CAM     │
│                 │                    │                 │
│  JTAG Header    │                    │                 │
│  ┌─────────┐    │                    │                 │
│  │ TMS ────┼────┼────────────────────┼─► GPIO 14      │
│  │ TCK ────┼────┼────────────────────┼─► GPIO 13      │
│  │ TDO ────┼────┼────────────────────┼─► GPIO 15      │
│  │ TDI ────┼────┼────────────────────┼─► GPIO 12      │
│  │ GND ────┼────┼────────────────────┼─► GND          │
│  └─────────┘    │                    │                 │
│                 │                    │                 │
│  PROG Header    │                    │                 │
│  ┌─────────┐    │     (Optional)     │                 │
│  │ TXD ────┼────┼─ ─ ─ ─ ─ ─ ─ ─ ─ ─┼─► U0R (GPIO 3) │
│  │ RXD ────┼────┼─ ─ ─ ─ ─ ─ ─ ─ ─ ─┼─► U0T (GPIO 1) │
│  │ GND ────┼────┼─ ─ ─ ─ ─ ─ ─ ─ ─ ─┼─► GND          │
│  └─────────┘    │                    │                 │
│                 │                    │                 │
│     [USB]       │                    │     [USB]       │
└────────┬────────┘                    └────────┬────────┘
         │                                      │
         ▼                                      ▼
    To Computer                          Power/Serial
    (OpenOCD)                            (optional)
```

## GPIO 12 Boot Consideration

**Critical**: GPIO 12 determines the flash voltage at boot:
- **LOW (0V)**: 3.3V flash operation (correct for most ESP32-CAM modules)
- **HIGH (3.3V)**: 1.8V flash operation (will cause boot failure)

The ESP-PROG's TDI line may have a pull-up. If the ESP32-CAM fails to boot with JTAG connected:

1. Add a **10kΩ pull-down resistor** between GPIO 12 and GND on the ESP32-CAM
2. Or configure efuses to ignore GPIO 12 strapping (irreversible):
   ```bash
   espefuse.py set_flash_voltage 3.3V
   ```

## Building Firmware for JTAG Debug

Build the firmware with JTAG debug mode enabled:

```bash
# Navigate to firmware directory
cd esp32-rover-firmware

# Build with JTAG debug mode (disables motor control)
JTAG_DEBUG=1 ROVER_TARGET=esp32cam idf.py build

# Flash via USB-serial (not JTAG)
idf.py -p /dev/cu.usbserial-XXXX flash
```

You should see this message during build:
```
-- JTAG debug mode ENABLED - motor control DISABLED (GPIO 12-15 available for JTAG)
```

And at runtime in the serial log:
```
W (xxx) MAIN: ==============================================
W (xxx) MAIN: JTAG DEBUG MODE - Motor control DISABLED
W (xxx) MAIN: GPIO 12-15 available for JTAG debugging
W (xxx) MAIN: ==============================================
```

## Starting OpenOCD

### macOS / Linux

```bash
# Start OpenOCD with ESP32 configuration
openocd -f interface/ftdi/esp32_devkitj_v1.cfg -f target/esp32.cfg
```

If using ESP-IDF's bundled OpenOCD:
```bash
# Use the openocd from ESP-IDF tools
~/.espressif/tools/openocd-esp32/*/openocd-esp32/bin/openocd \
    -f interface/ftdi/esp32_devkitj_v1.cfg \
    -f target/esp32.cfg
```

### Expected Output

```
Open On-Chip Debugger v0.11.0-esp32-xxx
Licensed under GNU GPL v2
Info : Listening on port 6666 for tcl connections
Info : Listening on port 4444 for telnet connections
Info : clock speed 20000 kHz
Info : JTAG tap: esp32.cpu0 tap/device found: 0x120034e5
Info : JTAG tap: esp32.cpu1 tap/device found: 0x120034e5
Info : esp32.cpu0: Target halted, PC=0x400D1234
Info : Listening on port 3333 for gdb connections
```

## Connecting GDB

### Using ESP-IDF's GDB

```bash
# Start GDB with the ELF file
xtensa-esp32-elf-gdb build/esp32-rover.elf

# In GDB, connect to OpenOCD
(gdb) target remote :3333

# Reset and halt at entry point
(gdb) mon reset halt

# Set a breakpoint
(gdb) break app_main

# Continue execution
(gdb) continue
```

### Common GDB Commands

| Command | Description |
|---------|-------------|
| `target remote :3333` | Connect to OpenOCD |
| `mon reset halt` | Reset and halt CPU |
| `continue` or `c` | Continue execution |
| `break <function>` | Set breakpoint at function |
| `break file.c:123` | Set breakpoint at line |
| `step` or `s` | Step into |
| `next` or `n` | Step over |
| `print <var>` | Print variable value |
| `info registers` | Show CPU registers |
| `backtrace` or `bt` | Show call stack |
| `mon halt` | Halt execution |

## Using VS Code with ESP-IDF Extension

1. Install the [ESP-IDF Extension](https://marketplace.visualstudio.com/items?itemName=espressif.esp-idf-extension) for VS Code

2. Create `.vscode/launch.json`:
```json
{
    "version": "0.2.0",
    "configurations": [
        {
            "name": "ESP32 JTAG Debug",
            "type": "espidf",
            "request": "launch",
            "mode": "auto",
            "initGdbCommands": [
                "set remote hardware-watchpoint-limit 2",
                "mon reset halt",
                "flushregs"
            ]
        }
    ]
}
```

3. Press F5 to start debugging

## Troubleshooting

### OpenOCD: "Error: JTAG scan chain interrogation failed"

**Causes**:
- Incorrect wiring
- Missing ground connection
- ESP32-CAM not powered
- GPIO 12 boot failure

**Solutions**:
1. Verify all JTAG wire connections
2. Ensure GND is connected
3. Check ESP32-CAM has power (LED should light briefly on boot)
4. Add pull-down resistor to GPIO 12

### OpenOCD: "Error: esp32.cpu0: xtensa_resume: DSR.StatusClear unexpected"

**Cause**: OpenOCD version mismatch

**Solution**: Use the OpenOCD version bundled with ESP-IDF:
```bash
~/.espressif/tools/openocd-esp32/*/openocd-esp32/bin/openocd ...
```

### GDB: "Remote 'g' packet reply is too long"

**Cause**: GDB/OpenOCD protocol mismatch

**Solution**: Use ESP-IDF's GDB:
```bash
xtensa-esp32-elf-gdb build/esp32-rover.elf
```

### ESP32-CAM boots to download mode with JTAG connected

**Cause**: GPIO 0 (XCLK) pulled low or floating

**Solution**: Ensure GPIO 0 has a pull-up resistor (10kΩ to 3.3V)

### Debugging works but ESP32-CAM won't boot standalone

**Cause**: GPIO 12 strapping affected by JTAG adapter

**Solution**: Disconnect JTAG for normal operation, or burn efuse for fixed 3.3V flash voltage

## Safety Notes

1. **Never connect motor power while JTAG is connected** - Motor driver outputs could damage the ESP-PROG
2. **Use appropriate voltage levels** - ESP-PROG operates at 3.3V, same as ESP32
3. **Disconnect JTAG for production builds** - JTAG debug mode disables motor control
4. **Back up your firmware** before debugging sessions that modify flash

## References

- [ESP-PROG User Guide](https://docs.espressif.com/projects/espressif-esp-iot-solution/en/latest/hw-reference/ESP-Prog_guide.html)
- [ESP32 JTAG Debugging](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/jtag-debugging/index.html)
- [OpenOCD Documentation](https://openocd.org/doc/html/index.html)
- [ESP-IDF Debugging Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/jtag-debugging/tips-and-quirks.html)
