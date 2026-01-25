"""
Pytest configuration and fixtures for ESP32 Rover integration tests.
"""

import os
import pytest

# Default configuration
DEFAULT_DEVICE_IP = "esp32-rover.local"
DEFAULT_TIMEOUT = 5


def pytest_addoption(parser):
    """Add custom command line options."""
    parser.addoption(
        "--device-ip",
        action="store",
        default=os.environ.get("DEVICE_IP", DEFAULT_DEVICE_IP),
        help="Device IP address or hostname"
    )
    parser.addoption(
        "--device-timeout",
        action="store",
        default=os.environ.get("DEVICE_TIMEOUT", DEFAULT_TIMEOUT),
        type=int,
        help="Request timeout in seconds"
    )


@pytest.fixture
def device_ip(request):
    """Get device IP address from command line or environment."""
    return request.config.getoption("--device-ip")


@pytest.fixture
def device_timeout(request):
    """Get request timeout from command line or environment."""
    return request.config.getoption("--device-timeout")


@pytest.fixture
def device_url(device_ip):
    """Get full device URL."""
    return f"http://{device_ip}"


@pytest.fixture
def ota_password():
    """Get OTA password from environment."""
    password = os.environ.get("OTA_PASSWORD")
    if not password:
        pytest.skip("OTA_PASSWORD environment variable not set")
    return password


def pytest_configure(config):
    """Register custom markers."""
    config.addinivalue_line(
        "markers", "slow: marks tests as slow (deselect with '-m \"not slow\"')"
    )
    config.addinivalue_line(
        "markers", "ota: marks tests that require OTA password"
    )
    config.addinivalue_line(
        "markers", "destructive: marks tests that may affect device state"
    )
