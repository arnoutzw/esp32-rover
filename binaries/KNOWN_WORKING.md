# Known Working Firmware Builds

This file tracks firmware builds that have been verified to work correctly on hardware.

## TTGO T-Display

### e4b11fc - Last Known Working Build (2026-01-25)

**Status**: ✓ Verified working on hardware

**Commit**: e4b11fc (2026-01-25 13:52:25 UTC)

**Binary**: `ttgo/esp32-rover_ttgo_20260125_135255_e4b11fc.bin`

**SHA256**: `5cc3b871525a09449ff443b32f981941629a72d1207e1176a17cf469d4f4a606`

**Verification**:
- HTTP REST API responding correctly
- Device stable, no boot failures
- Free heap: ~191KB
- Uptime verified over multiple power cycles

**Features**:
- HTTP server with 4 max open sockets (safe with LWIP_MAX_SOCKETS=8)
- WiFi AP mode working
- NTP time sync working
- REST API endpoints functional
- MQTT enabled (not connected in test environment)

**Notes**:
- This build was created before the clean git state enforcement was implemented
- All subsequent builds will have clean git state enforcement
- This binary is preserved as a known-good baseline for the TTGO T-Display target

---

## ESP32-CAM

No verified working builds archived yet.
