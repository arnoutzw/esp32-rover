# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- GitHub Actions CI/CD pipeline for automated testing and builds
- Code coverage measurement for unit tests
- Static analysis with shellcheck and cppcheck
- Release automation script (`scripts/release.sh`)
- SHA256 checksum verification for OTA updates
- Build metrics tracking (size, duration)

### Changed
- OTA password now required via environment variable (no default)
- Binary archive includes SHA256 checksums

### Security
- Removed hardcoded default OTA password

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
