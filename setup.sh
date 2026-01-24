#!/bin/bash
# ESP32 Rover - First-Time Setup Script
# Installs ESP-IDF tools for the embedded ESP-IDF v5.2.2
#
# This project includes ESP-IDF as a submodule/embedded directory.
# Run this script once to install the required toolchain.

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
IDF_PATH="$SCRIPT_DIR/esp-idf"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo ""
echo -e "${BLUE}╔════════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║           ESP32 Rover - First-Time Setup                   ║${NC}"
echo -e "${BLUE}╚════════════════════════════════════════════════════════════╝${NC}"
echo ""

# Check if ESP-IDF exists
if [ ! -d "$IDF_PATH" ]; then
    echo -e "${RED}Error: ESP-IDF not found at $IDF_PATH${NC}"
    echo ""
    echo "Expected ESP-IDF to be embedded in the project."
    echo "Please ensure the esp-idf directory exists."
    exit 1
fi

# Check ESP-IDF version
if [ -d "$IDF_PATH/.git" ]; then
    cd "$IDF_PATH"
    IDF_VERSION=$(git describe --tags 2>/dev/null || echo "unknown")
    echo -e "${GREEN}Found ESP-IDF: $IDF_VERSION${NC}"
    cd "$SCRIPT_DIR"
else
    echo -e "${YELLOW}ESP-IDF found (version unknown - no .git directory)${NC}"
fi

echo ""
echo -e "${YELLOW}Installing ESP-IDF tools...${NC}"
echo "This may take several minutes on first run."
echo ""

# Run the ESP-IDF install script for ESP32 target only
cd "$IDF_PATH"
./install.sh esp32

echo ""
echo -e "${GREEN}╔════════════════════════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║                    Setup Complete!                         ║${NC}"
echo -e "${GREEN}╚════════════════════════════════════════════════════════════╝${NC}"
echo ""
echo "To use the build tools, you have two options:"
echo ""
echo -e "${BLUE}Option 1: Use the build script (recommended)${NC}"
echo "  The build script automatically sets up the environment:"
echo "    ./build.sh ttgo build"
echo "    ./build.sh esp32cam flash"
echo ""
echo -e "${BLUE}Option 2: Manual environment setup${NC}"
echo "  Source the export script in your shell:"
echo "    source esp-idf/export.sh"
echo "  Then use idf.py directly:"
echo "    idf.py build"
echo ""
echo -e "${YELLOW}Note: The export command must be run in each new terminal session${NC}"
echo -e "${YELLOW}      if you want to use idf.py directly.${NC}"
echo ""
