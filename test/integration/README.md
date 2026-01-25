# ESP32 Rover Integration Tests

These tests verify the behavior of the actual ESP32 device over the network.
They require a physical device running the firmware.

## Prerequisites

1. Python 3.8+ with pip
2. ESP32 device running the rover firmware
3. Device accessible on the network

## Setup

```bash
# Install dependencies
pip install -r requirements.txt

# Set environment variables
export DEVICE_IP="esp32-rover.local"  # or IP address
export OTA_PASSWORD="your_ota_password"
```

## Running Tests

```bash
# Run all integration tests
pytest test/integration/ -v

# Run specific test file
pytest test/integration/test_status_endpoint.py -v

# Run with device IP override
DEVICE_IP=192.168.1.100 pytest test/integration/ -v

# Run with timeout override
DEVICE_TIMEOUT=10 pytest test/integration/ -v
```

## Test Categories

### Network Tests (`test_network.py`)
- Device reachability
- mDNS resolution
- Response time benchmarks

### Status Endpoint Tests (`test_status_endpoint.py`)
- JSON response validation
- Build fingerprint verification
- Diagnostic data completeness

### OTA Tests (`test_ota.py`)
- OTA endpoint accessibility
- Authentication verification
- (Manual) Full OTA update cycle

## Adding New Tests

1. Create a new test file: `test_<feature>.py`
2. Import the common fixtures from `conftest.py`
3. Use the `device_url` fixture for device address
4. Mark slow tests with `@pytest.mark.slow`
5. Mark tests requiring OTA password with `@pytest.mark.ota`

## Example Test

```python
import pytest
import requests

def test_device_responds(device_url):
    """Test that device responds to HTTP request."""
    response = requests.get(f"{device_url}/status", timeout=5)
    assert response.status_code == 200

def test_build_fingerprint_present(device_url):
    """Test that build fingerprint is in status response."""
    response = requests.get(f"{device_url}/status", timeout=5)
    data = response.json()
    assert 'diag' in data
    assert 'buildFingerprint' in data['diag']
```

## CI Integration

These tests are NOT run in the standard CI pipeline because they require
physical hardware. They should be run manually during development or as
part of a hardware-in-the-loop (HIL) test setup.

To run in CI with a test device:

```yaml
integration-test:
  runs-on: self-hosted  # Requires runner with device access
  steps:
    - name: Run integration tests
      env:
        DEVICE_IP: ${{ secrets.TEST_DEVICE_IP }}
        OTA_PASSWORD: ${{ secrets.OTA_PASSWORD }}
      run: pytest test/integration/ -v
```
