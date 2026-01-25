# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [2.0.1] - 2026-01-25

### Added
- GitHub Actions CI/CD pipeline for automated testing and builds
- Code coverage measurement for unit tests
- Static analysis with shellcheck and cppcheck
- Release automation script (`scripts/release.sh`)
- SHA256 checksum verification for OTA updates
- Build metrics tracking (size, duration)
- **Memory Optimizations (REQ-42)**:
  - Log buffer size reduced from 32KB to 16KB for both TTGO and ESP32-CAM targets
  - Target-specific task stack sizes based on measured high-water mark profiling
  - TTGO task stacks optimized: STATUS (2560B), LCD (3072B), MQTT (3072B)
  - ESP32-CAM standard stacks: STATUS (4096B), LCD (4096B), MQTT (4096B)
  - Optional log buffer feature toggle via `ENABLE_LOG_BUFFER` compiler flag
  - HTTP server buffer and socket count reduction for TTGO (headers, URI, max connections)
  - MQTT transport optimization for TTGO (disabled SSL and WebSocket transports)
  - IRAM optimization with function placement into Flash storage
  - Total memory savings: ~22KB on TTGO (from 94% → lower DRAM utilization)
- **LCD Diagnostics Screen Improvements**:
  - Added clear "Up:" label to uptime counter (was unlabeled)
  - Moved uptime to separate line to prevent text overlap with battery voltage
  - Improved readability of system information display
- **OTA Script Enhancements**:
  - Pre-flight connectivity check via ping before OTA flash attempt
  - Device reachability validation with 2-second timeout
  - Helpful error messages with troubleshooting steps when device unreachable
  - Prevents wasted time waiting for flash on unreachable devices

### Changed
- OTA password now required via environment variable (no default)
- Binary archive includes SHA256 checksums
- LCD diagnostics layout: battery and uptime now on separate lines
- OTA script exits early if target device is unreachable (fail-fast pattern)
- Log buffer now unified at 16KB for both targets (was target-specific: 16KB TTGO / 32KB ESP32-CAM)

### Security
- Removed hardcoded default OTA password
- Pre-flight connectivity validation prevents resource waste on unreachable devices

### Performance
- Reduced DRAM usage on TTGO by ~22KB total
- Task stack allocation now matches actual measured usage patterns
- Optional log buffer allows further 16KB savings when disabled

## [2.0.0] - 2024-XX-XX

### Added
- Binary archive management system with metadata
- Clean target switching detection
- Unit test requirement before builds
- Build fingerprint embedding (git commit, branch, dirty state)
- Task watchdog configuration
- mDNS hostname selection based on target

### Changed
- Major repository restructure
- Embedded ESP-IDF v5.2.2 in project
- Improved build script with automatic target detection

## [1.7] - Previous Release

### Added
- TTGO T-Display support
- Diagnostic screens
- Web UI improvements

## [1.6] - Previous Release

### Added
- OTA update support
- Rate limiting for ESP32-CAM

## [1.0] - Initial Release

### Added
- Initial ESP32-CAM support
- WiFi AP mode
- Basic motor control
- Web server for control interface
