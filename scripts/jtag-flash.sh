#!/bin/bash
# Flash ESP32 firmware via JTAG using ESP-PROG
#
# Usage:
#   ./scripts/jtag-flash.sh esp32cam              # Flash latest esp32cam binary
#   ./scripts/jtag-flash.sh ttgo                  # Flash latest ttgo binary
#   ./scripts/jtag-flash.sh esp32cam firmware.bin # Flash specific binary
#
# Prerequisites:
#   1. Build openocd-esp32: ./scripts/build-openocd.sh
#   2. Connect ESP-PROG JTAG to ESP32:
#      TDI -> GPIO12, TCK -> GPIO13, TMS -> GPIO14, TDO -> GPIO15, GND, 3V3

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
OPENOCD_DIR="$PROJECT_ROOT/tools/openocd-esp32"
OPENOCD="$OPENOCD_DIR/src/openocd"

# Check if openocd is built
if [ ! -x "$OPENOCD" ]; then
    echo "Error: OpenOCD not found at $OPENOCD"
    echo "Please run: ./scripts/build-openocd.sh"
    exit 1
fi

# Parse arguments
TARGET="${1:-esp32cam}"
BINARY="$2"

# Validate target
if [[ "$TARGET" != "esp32cam" && "$TARGET" != "ttgo" ]]; then
    echo "Error: Invalid target '$TARGET'. Use 'esp32cam' or 'ttgo'."
    exit 1
fi

# Determine binary path
if [ -z "$BINARY" ]; then
    BINARY="$PROJECT_ROOT/binaries/$TARGET/latest.bin"
fi

# Make relative paths absolute
if [[ "$BINARY" != /* ]]; then
    BINARY="$PROJECT_ROOT/$BINARY"
fi

# Check binary exists
if [ ! -f "$BINARY" ]; then
    echo "Error: Binary not found: $BINARY"
    exit 1
fi

# Show binary info
BINARY_SIZE=$(stat -f%z "$BINARY" 2>/dev/null || stat -c%s "$BINARY" 2>/dev/null)
echo "=== JTAG Flash ==="
echo "Target: $TARGET"
echo "Binary: $BINARY"
echo "Size: $((BINARY_SIZE / 1024)) KB"
echo ""

# Select target config
if [[ "$TARGET" == "esp32cam" || "$TARGET" == "ttgo" ]]; then
    TARGET_CFG="target/esp32.cfg"
else
    echo "Error: Unknown target type"
    exit 1
fi

# Flash via JTAG
echo "Flashing via JTAG..."
"$OPENOCD" \
    -s "$OPENOCD_DIR/tcl" \
    -f interface/ftdi/esp_ftdi.cfg \
    -f "$TARGET_CFG" \
    -c "program_esp $BINARY 0x0 verify reset exit"

echo ""
echo "=== Flash complete ==="
