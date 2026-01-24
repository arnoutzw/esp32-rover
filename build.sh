#!/bin/bash
# ESP32 Rover Build Script
# Supports building for ESP32-CAM or TTGO T-Display targets
#
# This project is self-contained with ESP-IDF v5.2.2 embedded.
# Run ./setup.sh first to install the required tools.

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
IDF_PATH_LOCAL="$SCRIPT_DIR/esp-idf"
cd "$SCRIPT_DIR"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

print_usage() {
    echo ""
    echo "ESP32 Rover Build Script (Self-Contained)"
    echo "=========================================="
    echo ""
    echo "Usage: $0 <target> [command]"
    echo ""
    echo "Targets:"
    echo "  esp32cam    Build for ESP32-CAM AI-Thinker (with camera)"
    echo "  ttgo        Build for TTGO T-Display (no camera)"
    echo ""
    echo "Commands (optional):"
    echo "  build       Build the project (default)"
    echo "  flash       Build and flash to device"
    echo "  monitor     Open serial monitor"
    echo "  clean       Clean build directory"
    echo "  fullclean   Full clean (removes sdkconfig)"
    echo "  menuconfig  Open menuconfig"
    echo ""
    echo "Examples:"
    echo "  $0 esp32cam              # Build for ESP32-CAM"
    echo "  $0 ttgo flash            # Build and flash for TTGO"
    echo "  $0 esp32cam flash -p /dev/ttyUSB0  # Flash to specific port"
    echo ""
    echo "First-time setup:"
    echo "  ./setup.sh               # Install ESP-IDF tools (run once)"
    echo ""
}

setup_target() {
    local target=$1

    case $target in
        esp32cam)
            echo -e "${BLUE}Setting up for ESP32-CAM (with camera)${NC}"
            SDKCONFIG_DEFAULTS="sdkconfig.defaults.esp32cam"
            TARGET_DEFINE="ROVER_TARGET_ESP32CAM"
            ;;
        ttgo)
            echo -e "${BLUE}Setting up for TTGO T-Display (no camera)${NC}"
            SDKCONFIG_DEFAULTS="sdkconfig.defaults.ttgo"
            TARGET_DEFINE="ROVER_TARGET_TTGO"
            ;;
        *)
            echo -e "${RED}Error: Unknown target '$target'${NC}"
            print_usage
            exit 1
            ;;
    esac

    # Check if sdkconfig.defaults file exists
    if [ ! -f "$SDKCONFIG_DEFAULTS" ]; then
        echo -e "${RED}Error: $SDKCONFIG_DEFAULTS not found${NC}"
        exit 1
    fi

    # Copy target-specific defaults
    cp "$SDKCONFIG_DEFAULTS" sdkconfig.defaults
    echo -e "${GREEN}Using $SDKCONFIG_DEFAULTS${NC}"

    # Export the target define for CMake
    export ROVER_TARGET="$target"
}

run_command() {
    local cmd=$1
    shift
    local extra_args="$@"

    case $cmd in
        build)
            echo -e "${YELLOW}Building project...${NC}"
            idf.py build $extra_args
            echo -e "${GREEN}Build complete!${NC}"
            ;;
        flash)
            echo -e "${YELLOW}Building and flashing...${NC}"
            idf.py flash $extra_args
            echo -e "${GREEN}Flash complete!${NC}"
            ;;
        monitor)
            echo -e "${YELLOW}Opening serial monitor...${NC}"
            idf.py monitor $extra_args
            ;;
        clean)
            echo -e "${YELLOW}Cleaning build directory...${NC}"
            idf.py clean
            echo -e "${GREEN}Clean complete!${NC}"
            ;;
        fullclean)
            echo -e "${YELLOW}Full clean (removing sdkconfig)...${NC}"
            idf.py fullclean
            rm -f sdkconfig
            echo -e "${GREEN}Full clean complete!${NC}"
            ;;
        menuconfig)
            echo -e "${YELLOW}Opening menuconfig...${NC}"
            idf.py menuconfig
            ;;
        *)
            echo -e "${RED}Error: Unknown command '$cmd'${NC}"
            print_usage
            exit 1
            ;;
    esac
}

# Main script
if [ $# -lt 1 ]; then
    print_usage
    exit 1
fi

# Always use the embedded ESP-IDF
if [ ! -d "$IDF_PATH_LOCAL" ]; then
    echo -e "${RED}Error: Embedded ESP-IDF not found at $IDF_PATH_LOCAL${NC}"
    echo "The esp-idf directory should be part of this project."
    exit 1
fi

# Source the embedded ESP-IDF (suppress verbose output)
echo -e "${BLUE}Using embedded ESP-IDF v5.2.2${NC}"
export IDF_PATH="$IDF_PATH_LOCAL"

# Check if tools are installed
if [ ! -f "$HOME/.espressif/python_env/idf5.2_py"*/bin/python ] 2>/dev/null; then
    # Tools might not be installed, try sourcing anyway
    :
fi

# Source export.sh (this sets up PATH and other environment variables)
# Redirect output to hide the verbose ESP-IDF banner
source "$IDF_PATH_LOCAL/export.sh" > /dev/null 2>&1 || {
    echo -e "${RED}Error: Failed to source ESP-IDF environment.${NC}"
    echo ""
    echo "Have you run the setup script?"
    echo "  ./setup.sh"
    echo ""
    exit 1
}

TARGET=$1
COMMAND=${2:-build}
shift 2 2>/dev/null || shift 1 2>/dev/null || true

setup_target "$TARGET"
run_command "$COMMAND" "$@"
