#!/bin/bash
# Build openocd-esp32 for JTAG flashing support
#
# Prerequisites (macOS):
#   brew install automake autoconf libtool pkg-config libusb libftdi texinfo
#
# Prerequisites (Linux):
#   sudo apt-get install automake autoconf libtool pkg-config libusb-1.0-0-dev libftdi1-dev texinfo
#
# Usage:
#   ./scripts/build-openocd.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
OPENOCD_DIR="$PROJECT_ROOT/tools/openocd-esp32"

echo "=== Building openocd-esp32 ==="
echo "Directory: $OPENOCD_DIR"

cd "$OPENOCD_DIR"

# Initialize submodules if needed
if [ ! -f "jimtcl/configure.ac" ]; then
    echo "Initializing submodules..."
    git submodule update --init --recursive
fi

# Initialize libjaylink configure
if [ ! -f "src/jtag/drivers/libjaylink/configure" ]; then
    echo "Generating libjaylink configure..."
    cd src/jtag/drivers/libjaylink
    autoreconf -fiv
    cd "$OPENOCD_DIR"
fi

# Bootstrap if needed
if [ ! -f "configure" ]; then
    echo "Running bootstrap..."
    ./bootstrap
fi

# Configure
echo "Configuring..."
./configure --enable-ftdi --disable-werror --enable-internal-jimtcl --enable-internal-libjaylink

# Build
echo "Building..."
if [[ "$OSTYPE" == "darwin"* ]]; then
    make -j$(sysctl -n hw.ncpu)
else
    make -j$(nproc)
fi

echo ""
echo "=== Build complete ==="
echo "OpenOCD binary: $OPENOCD_DIR/src/openocd"
echo ""
echo "To flash firmware via JTAG:"
echo "  $OPENOCD_DIR/src/openocd \\"
echo "    -s $OPENOCD_DIR/tcl \\"
echo "    -f interface/ftdi/esp_ftdi.cfg \\"
echo "    -f target/esp32.cfg \\"
echo "    -c \"program_esp binaries/esp32cam/latest.bin 0x0 verify reset exit\""
