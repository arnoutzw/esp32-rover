#!/bin/bash
#
# ESP32 Rover REST API Demo Script
#
# This script demonstrates how to interact with the rover's REST API
# using curl commands.
#
# Usage:
#   ./demo_api.sh [ROVER_IP]
#
# If no IP is provided, defaults to 192.168.4.1 (AP mode default)
#

set -e

# Default rover IP (AP mode)
ROVER_IP="${1:-192.168.4.1}"
BASE_URL="http://${ROVER_IP}"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

echo -e "${CYAN}================================${NC}"
echo -e "${CYAN}  ESP32 Rover REST API Demo${NC}"
echo -e "${CYAN}================================${NC}"
echo ""
echo -e "Rover IP: ${GREEN}${ROVER_IP}${NC}"
echo ""

# Check if jq is available for pretty printing
if command -v jq &> /dev/null; then
    JQ_CMD="jq"
else
    JQ_CMD="cat"
    echo -e "${YELLOW}Note: Install 'jq' for pretty-printed JSON output${NC}"
    echo ""
fi

# Function to make API calls
api_call() {
    local endpoint="$1"
    local description="$2"

    echo -e "${YELLOW}>>> ${description}${NC}"
    echo -e "    GET ${BASE_URL}${endpoint}"
    echo ""

    response=$(curl -s -w "\n%{http_code}" "${BASE_URL}${endpoint}" 2>/dev/null) || {
        echo -e "${RED}Error: Could not connect to rover at ${ROVER_IP}${NC}"
        echo "Make sure:"
        echo "  1. You are connected to the rover's WiFi network"
        echo "  2. The rover is powered on and running"
        echo "  3. The IP address is correct"
        exit 1
    }

    http_code=$(echo "$response" | tail -n1)
    body=$(echo "$response" | sed '$d')

    if [ "$http_code" == "200" ]; then
        echo -e "${GREEN}Response (HTTP $http_code):${NC}"
        echo "$body" | $JQ_CMD
    else
        echo -e "${RED}Error (HTTP $http_code):${NC}"
        echo "$body"
    fi
    echo ""
}

# Demo 1: Get rover status
echo -e "${CYAN}--- Demo 1: Get Rover Status ---${NC}"
api_call "/status" "Fetching rover status and diagnostics"

# Demo 2: Parse specific fields (if jq is available)
if command -v jq &> /dev/null; then
    echo -e "${CYAN}--- Demo 2: Parse Specific Fields ---${NC}"

    echo -e "${YELLOW}>>> Extracting key metrics${NC}"
    echo ""

    status=$(curl -s "${BASE_URL}/status")

    echo -e "Battery Voltage:  ${GREEN}$(echo "$status" | jq -r '.battery')V${NC}"
    echo -e "Motor Velocity:   ${GREEN}$(echo "$status" | jq -r '.velocity') rad/s${NC}"
    echo -e "Steering Angle:   ${GREEN}$(echo "$status" | jq -r '.steering')°${NC}"
    echo -e "WiFi SSID:        ${GREEN}$(echo "$status" | jq -r '.diag.ssid')${NC}"
    echo -e "IP Address:       ${GREEN}$(echo "$status" | jq -r '.diag.ip')${NC}"
    echo -e "Free Heap:        ${GREEN}$(echo "$status" | jq -r '.diag.freeHeap') bytes${NC}"
    echo -e "Uptime:           ${GREEN}$(echo "$status" | jq -r '.diag.uptime') seconds${NC}"
    echo -e "Tasks (total):    ${GREEN}$(echo "$status" | jq -r '.diag.tasks')${NC}"
    echo -e "Tasks Core 0:     ${GREEN}$(echo "$status" | jq -r '.diag.tasksCore0')${NC}"
    echo -e "Tasks Core 1:     ${GREEN}$(echo "$status" | jq -r '.diag.tasksCore1')${NC}"
    echo -e "REST API:         ${GREEN}$(echo "$status" | jq -r '.diag.restApi')${NC}"
    echo -e "MQTT Connected:   ${GREEN}$(echo "$status" | jq -r '.diag.mqttConnected')${NC}"
    echo ""
fi

# Demo 3: Continuous monitoring
echo -e "${CYAN}--- Demo 3: Continuous Monitoring (5 samples) ---${NC}"
echo -e "${YELLOW}>>> Polling status every second${NC}"
echo ""

for i in {1..5}; do
    status=$(curl -s "${BASE_URL}/status" 2>/dev/null) || break

    if command -v jq &> /dev/null; then
        battery=$(echo "$status" | jq -r '.battery')
        velocity=$(echo "$status" | jq -r '.velocity')
        heap=$(echo "$status" | jq -r '.diag.freeHeap')
        echo -e "[$i/5] Battery: ${GREEN}${battery}V${NC} | Velocity: ${GREEN}${velocity}${NC} rad/s | Heap: ${GREEN}${heap}${NC} bytes"
    else
        echo "[$i/5] $(echo "$status" | head -c 80)..."
    fi

    [ $i -lt 5 ] && sleep 1
done

echo ""
echo -e "${CYAN}================================${NC}"
echo -e "${GREEN}Demo complete!${NC}"
echo ""
echo "Available endpoints:"
echo "  GET /status  - Full rover status and diagnostics (JSON)"
echo "  GET /        - Web UI (HTML)"
echo ""
echo "Example curl commands:"
echo "  curl http://${ROVER_IP}/status"
echo "  curl http://${ROVER_IP}/status | jq '.battery'"
echo "  curl http://${ROVER_IP}/status | jq '.diag'"
echo ""
