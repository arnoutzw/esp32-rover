#!/bin/bash
# ESP32 Rover OTA Flash Script
# Automatically applies rate limiting for ESP32-CAM to prevent memory issues

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
FIRMWARE_BIN="$PROJECT_ROOT/firmware/build/esp32-rover.bin"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Default OTA password (can be overridden via environment variable)
OTA_PASSWORD="${OTA_PASSWORD:-rover1234}"

print_usage() {
    echo ""
    echo "ESP32 Rover OTA Flash Script"
    echo "============================"
    echo ""
    echo "Usage: $0 <target> [host]"
    echo ""
    echo "Targets:"
    echo "  esp32cam    Flash to ESP32-CAM (rate-limited for stability)"
    echo "  ttgo        Flash to TTGO T-Display"
    echo ""
    echo "Host (optional):"
    echo "  IP address or hostname. Defaults to mDNS name based on target:"
    echo "    esp32cam → esp32-rover.local"
    echo "    ttgo     → ttgo-rover.local"
    echo ""
    echo "Examples:"
    echo "  $0 esp32cam                    # Flash to esp32-rover.local"
    echo "  $0 ttgo                        # Flash to ttgo-rover.local"
    echo "  $0 esp32cam 192.168.2.88       # Flash to specific IP"
    echo "  $0 ttgo 192.168.2.75           # Flash to specific IP"
    echo ""
    echo "Environment variables:"
    echo "  OTA_PASSWORD    OTA password (default: rover1234)"
    echo ""
}

if [ $# -lt 1 ]; then
    print_usage
    exit 1
fi

TARGET=$1
HOST=$2

# Set defaults based on target
case $TARGET in
    esp32cam)
        DEFAULT_HOST="esp32-rover.local"
        # ESP32-CAM needs rate limiting due to memory constraints during OTA
        RATE_LIMIT="--limit-rate 50k"
        echo -e "${BLUE}Target: ESP32-CAM (with rate limiting for stability)${NC}"
        ;;
    ttgo)
        DEFAULT_HOST="ttgo-rover.local"
        RATE_LIMIT=""
        echo -e "${BLUE}Target: TTGO T-Display${NC}"
        ;;
    *)
        echo -e "${RED}Error: Unknown target '$TARGET'${NC}"
        print_usage
        exit 1
        ;;
esac

# Use provided host or default
HOST="${HOST:-$DEFAULT_HOST}"

# Check if firmware binary exists
if [ ! -f "$FIRMWARE_BIN" ]; then
    echo -e "${RED}Error: Firmware binary not found at $FIRMWARE_BIN${NC}"
    echo ""
    echo "Build the firmware first:"
    echo "  ./scripts/build.sh $TARGET"
    echo ""
    exit 1
fi

# Get firmware size
FIRMWARE_SIZE=$(stat -f%z "$FIRMWARE_BIN" 2>/dev/null || stat -c%s "$FIRMWARE_BIN" 2>/dev/null)
FIRMWARE_SIZE_KB=$((FIRMWARE_SIZE / 1024))

echo -e "${YELLOW}Flashing firmware to $HOST${NC}"
echo "  Binary: $FIRMWARE_BIN"
echo "  Size: ${FIRMWARE_SIZE_KB} KB"
if [ -n "$RATE_LIMIT" ]; then
    echo "  Rate limit: 50 KB/s (ESP32-CAM memory protection)"
fi
echo ""

# Perform OTA flash
curl -X POST \
    -H "X-OTA-Password: $OTA_PASSWORD" \
    $RATE_LIMIT \
    --progress-bar \
    --data-binary @"$FIRMWARE_BIN" \
    "http://$HOST/ota"

echo ""
echo -e "${GREEN}OTA flash complete!${NC}"
