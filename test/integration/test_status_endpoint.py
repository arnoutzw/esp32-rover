"""
Integration tests for the ESP32 Rover status endpoint.

These tests verify that the device's /status endpoint returns
correct and complete diagnostic information.
"""

import pytest
import requests


class TestStatusEndpoint:
    """Tests for the /status REST API endpoint."""

    def test_status_endpoint_reachable(self, device_url, device_timeout):
        """Test that status endpoint responds with 200 OK."""
        response = requests.get(f"{device_url}/status", timeout=device_timeout)
        assert response.status_code == 200

    def test_status_returns_json(self, device_url, device_timeout):
        """Test that status endpoint returns valid JSON."""
        response = requests.get(f"{device_url}/status", timeout=device_timeout)
        assert response.headers.get('content-type', '').startswith('application/json')
        data = response.json()  # Will raise if not valid JSON
        assert isinstance(data, dict)

    def test_status_contains_target(self, device_url, device_timeout):
        """Test that status contains target type."""
        response = requests.get(f"{device_url}/status", timeout=device_timeout)
        data = response.json()
        assert 'target' in data
        assert data['target'] in ['esp32cam', 'ttgo']

    def test_status_contains_diagnostics(self, device_url, device_timeout):
        """Test that status contains diagnostic data."""
        response = requests.get(f"{device_url}/status", timeout=device_timeout)
        data = response.json()
        assert 'diag' in data
        diag = data['diag']

        # Check for expected diagnostic fields
        expected_fields = [
            'ssid', 'ip', 'rssi', 'buildFingerprint',
            'buildTime', 'buildBranch', 'buildDirty'
        ]
        for field in expected_fields:
            assert field in diag, f"Missing diagnostic field: {field}"

    def test_build_fingerprint_format(self, device_url, device_timeout):
        """Test that build fingerprint is a valid git short hash."""
        response = requests.get(f"{device_url}/status", timeout=device_timeout)
        data = response.json()
        fingerprint = data['diag']['buildFingerprint']

        # Git short hash is typically 7-8 hex characters
        assert len(fingerprint) >= 7, "Build fingerprint too short"
        assert all(c in '0123456789abcdef' for c in fingerprint.lower()), \
            "Build fingerprint should be hexadecimal"

    def test_wifi_rssi_valid(self, device_url, device_timeout):
        """Test that WiFi RSSI is within valid range."""
        response = requests.get(f"{device_url}/status", timeout=device_timeout)
        data = response.json()
        rssi = data.get('rssi') or data['diag'].get('rssi')

        # RSSI typically ranges from -100 to 0 dBm
        assert isinstance(rssi, int), "RSSI should be an integer"
        assert -100 <= rssi <= 0, f"RSSI {rssi} out of valid range"

    def test_battery_voltage_present(self, device_url, device_timeout):
        """Test that battery voltage is reported."""
        response = requests.get(f"{device_url}/status", timeout=device_timeout)
        data = response.json()

        assert 'battery' in data
        battery = data['battery']
        assert isinstance(battery, (int, float)), "Battery should be numeric"
        # ESP32 ADC reads 0-3.3V, typical LiPo is 3.0-4.2V
        assert 0 <= battery <= 5.0, f"Battery voltage {battery} seems invalid"

    @pytest.mark.slow
    def test_response_time(self, device_url, device_timeout):
        """Test that status endpoint responds quickly."""
        import time

        start = time.time()
        response = requests.get(f"{device_url}/status", timeout=device_timeout)
        elapsed = time.time() - start

        assert response.status_code == 200
        assert elapsed < 1.0, f"Response time {elapsed:.2f}s is too slow"


class TestStatusEndpointTargetSpecific:
    """Target-specific status tests."""

    def test_camera_status_on_esp32cam(self, device_url, device_timeout):
        """Test that ESP32-CAM reports camera status."""
        response = requests.get(f"{device_url}/status", timeout=device_timeout)
        data = response.json()

        if data['target'] == 'esp32cam':
            assert 'camera' in data
            assert isinstance(data['camera'], bool)

    def test_buttons_on_ttgo(self, device_url, device_timeout):
        """Test that TTGO reports button states."""
        response = requests.get(f"{device_url}/status", timeout=device_timeout)
        data = response.json()

        if data['target'] == 'ttgo':
            assert 'btnL' in data
            assert 'btnR' in data
            assert isinstance(data['btnL'], bool)
            assert isinstance(data['btnR'], bool)
