#!/bin/bash
# ESP32 Rover OTA Flash Script
# Automatically applies rate limiting for ESP32-CAM to prevent memory issues

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
FIRMWARE_BIN="$PROJECT_ROOT/firmware/build/esp32-rover.bin"
BINARIES_DIR="$PROJECT_ROOT/binaries"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# OTA password must be set via environment variable (security requirement)
# No default password - must be explicitly provided

print_usage() {
    echo ""
    echo "ESP32 Rover OTA Flash Script"
    echo "============================"
    echo ""
    echo "Usage: $0 <target> [host] [--binary-file <path>]"
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
    echo "Options:"
    echo "  --binary-file <path>    Flash a specific binary file from archive"
    echo "  --list-binaries         List available binaries for target"
    echo ""
    echo "Examples:"
    echo "  OTA_PASSWORD=mypass $0 esp32cam           # Flash with password"
    echo "  $0 ttgo --list-binaries                   # List available binaries"
    echo "  $0 esp32cam 192.168.2.88                  # Flash to specific IP"
    echo "  $0 esp32cam --binary-file esp32-rover_esp32cam_20240115_143022_abc1234.bin"
    echo ""
    echo "Environment variables (REQUIRED):"
    echo "  OTA_PASSWORD    OTA password for authentication (no default)"
    echo ""
    echo "Tip: Set OTA_PASSWORD in your shell config or use:"
    echo "  export OTA_PASSWORD=your_password"
    echo ""
}

if [ $# -lt 1 ]; then
    print_usage
    exit 1
fi

list_binaries() {
    local target=$1
    local target_dir="$BINARIES_DIR/$target"

    if [ ! -d "$target_dir" ]; then
        echo -e "${RED}No binaries found for target: $target${NC}"
        return 1
    fi

    echo -e "${BLUE}Available binaries for $target:${NC}"
    echo ""
    # List binaries sorted by date (newest first)
    ls -t "$target_dir"/*.bin 2>/dev/null | while read bin; do
        if [ -L "$bin" ]; then
            continue  # Skip symlinks
        fi
        local size=$(stat -f%z "$bin" 2>/dev/null || stat -c%s "$bin" 2>/dev/null)
        local size_kb=$((size / 1024))
        printf "  %-60s (%d KB)\n" "$(basename "$bin")" "$size_kb"
        # Show metadata if it exists
        local metadata="${bin%.bin}.txt"
        if [ -f "$metadata" ]; then
            cat "$metadata" | sed 's/^/    /'
        fi
        echo ""
    done
}

TARGET=$1
HOST=$2
BINARY_FILE=""

# Parse additional arguments
shift 1
while [ $# -gt 0 ]; do
    case $1 in
        --list-binaries)
            list_binaries "$TARGET"
            exit 0
            ;;
        --binary-file)
            shift
            BINARY_FILE="$1"
            shift
            ;;
        *)
            HOST="$1"
            shift
            ;;
    esac
done

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

# Check if host is reachable via ping before attempting OTA
echo -e "${BLUE}Checking connectivity to $HOST...${NC}"
if ! ping -c 1 -W 2 "$HOST" > /dev/null 2>&1; then
    echo -e "${RED}Error: Cannot reach OTA host at $HOST${NC}"
    echo ""
    echo "Possible solutions:"
    echo "  1. Check device is powered on and connected to WiFi"
    echo "  2. Verify device IP address or hostname is correct"
    echo "  3. Check network connectivity from this machine"
    echo "  4. Try specifying IP address instead of hostname"
    echo ""
    exit 1
fi
echo -e "${GREEN}Host is reachable!${NC}"
echo ""

# Determine which binary to use
if [ -n "$BINARY_FILE" ]; then
    # User specified a binary file
    if [[ "$BINARY_FILE" = /* ]]; then
        # Absolute path
        FIRMWARE_BIN="$BINARY_FILE"
    else
        # Relative path - assume it's in the target's binary archive
        FIRMWARE_BIN="$BINARIES_DIR/$TARGET/$BINARY_FILE"
    fi
else
    # Try to use latest.bin symlink, fall back to build directory
    if [ -L "$BINARIES_DIR/$TARGET/latest.bin" ]; then
        FIRMWARE_BIN="$BINARIES_DIR/$TARGET/latest.bin"
    fi
fi

# Check if firmware binary exists
if [ ! -f "$FIRMWARE_BIN" ]; then
    echo -e "${RED}Error: Firmware binary not found at $FIRMWARE_BIN${NC}"
    echo ""
    if [ -n "$BINARY_FILE" ]; then
        echo "Specified binary file does not exist."
        echo "List available binaries:"
        echo "  $0 $TARGET --list-binaries"
    else
        echo "Build the firmware first:"
        echo "  ./scripts/build.sh $TARGET"
        echo ""
        echo "Or specify a binary from the archive:"
        echo "  $0 $TARGET --list-binaries"
    fi
    echo ""
    exit 1
fi

# Validate OTA password is set (security requirement - no default password)
if [ -z "$OTA_PASSWORD" ]; then
    echo -e "${RED}Error: OTA_PASSWORD environment variable not set${NC}"
    echo ""
    echo "For security, OTA password must be explicitly provided."
    echo "Set it with: export OTA_PASSWORD=your_password"
    echo ""
    echo "Or run with: OTA_PASSWORD=your_password $0 $TARGET"
    exit 1
fi

# Get firmware size
FIRMWARE_SIZE=$(stat -f%z "$FIRMWARE_BIN" 2>/dev/null || stat -c%s "$FIRMWARE_BIN" 2>/dev/null)
FIRMWARE_SIZE_KB=$((FIRMWARE_SIZE / 1024))

# Calculate SHA256 checksum for integrity verification
FIRMWARE_CHECKSUM=$(shasum -a 256 "$FIRMWARE_BIN" 2>/dev/null | cut -d' ' -f1 || sha256sum "$FIRMWARE_BIN" 2>/dev/null | cut -d' ' -f1)

echo -e "${YELLOW}Flashing firmware to $HOST${NC}"
echo "  Binary: $(basename "$FIRMWARE_BIN")"
if [ -L "$FIRMWARE_BIN" ]; then
    echo "  (Symlink → $(readlink "$FIRMWARE_BIN"))"
fi
echo "  Size: ${FIRMWARE_SIZE_KB} KB"
echo "  SHA256: ${FIRMWARE_CHECKSUM}"
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
