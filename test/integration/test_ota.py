"""
Integration tests for ESP32 Rover OTA (Over-The-Air) update functionality.

These tests verify OTA endpoint behavior without actually performing updates.
Full OTA update tests are marked as destructive and should be run manually.
"""

import pytest
import requests


class TestOTAEndpoint:
    """Tests for the OTA update endpoint."""

    def test_ota_endpoint_exists(self, device_url, device_timeout):
        """Test that OTA endpoint responds (even without auth)."""
        # OPTIONS or HEAD request to check endpoint exists
        try:
            response = requests.options(f"{device_url}/ota", timeout=device_timeout)
            # Endpoint exists but may return various status codes
            assert response.status_code in [200, 401, 403, 405]
        except requests.exceptions.RequestException:
            pytest.fail("OTA endpoint not reachable")

    def test_ota_rejects_get_request(self, device_url, device_timeout):
        """Test that OTA endpoint rejects GET requests."""
        response = requests.get(f"{device_url}/ota", timeout=device_timeout)
        # Should reject non-POST requests
        assert response.status_code in [405, 400, 404]

    @pytest.mark.ota
    def test_ota_requires_authentication(self, device_url, device_timeout):
        """Test that OTA endpoint requires password header."""
        # Send POST without password
        response = requests.post(
            f"{device_url}/ota",
            data=b"fake_firmware_data",
            timeout=device_timeout
        )
        # Should reject without authentication
        assert response.status_code in [401, 403]

    @pytest.mark.ota
    def test_ota_rejects_wrong_password(self, device_url, device_timeout):
        """Test that OTA endpoint rejects wrong password."""
        response = requests.post(
            f"{device_url}/ota",
            data=b"fake_firmware_data",
            headers={"X-OTA-Password": "wrong_password_12345"},
            timeout=device_timeout
        )
        # Should reject wrong password
        assert response.status_code in [401, 403]

    @pytest.mark.ota
    def test_ota_accepts_correct_password_header(self, device_url, device_timeout, ota_password):
        """Test that OTA endpoint accepts correct password (without actual update)."""
        # Send minimal invalid firmware data - should fail after auth but pass auth
        response = requests.post(
            f"{device_url}/ota",
            data=b"not_valid_firmware",
            headers={"X-OTA-Password": ota_password},
            timeout=device_timeout
        )
        # Should pass authentication but fail on invalid firmware
        # 400 = bad firmware, 200 = unexpected success, 401/403 = auth failed
        assert response.status_code != 401, "Authentication failed with correct password"
        assert response.status_code != 403, "Access forbidden with correct password"


class TestOTASafety:
    """Safety-related OTA tests."""

    @pytest.mark.ota
    @pytest.mark.destructive
    @pytest.mark.skip(reason="Destructive test - run manually")
    def test_ota_full_update_cycle(self, device_url, device_timeout, ota_password):
        """
        Test full OTA update cycle.

        WARNING: This test will actually update the firmware!
        Only run this manually with a test binary.

        To run:
            pytest test/integration/test_ota.py::TestOTASafety::test_ota_full_update_cycle -v --rundestructive
        """
        import os

        firmware_path = os.environ.get("TEST_FIRMWARE_PATH")
        if not firmware_path:
            pytest.skip("TEST_FIRMWARE_PATH not set")

        with open(firmware_path, 'rb') as f:
            firmware_data = f.read()

        response = requests.post(
            f"{device_url}/ota",
            data=firmware_data,
            headers={"X-OTA-Password": ota_password},
            timeout=120  # OTA can take a while
        )

        assert response.status_code == 200, f"OTA update failed: {response.text}"

    def test_ota_rejects_empty_payload(self, device_url, device_timeout, ota_password):
        """Test that OTA endpoint rejects empty firmware data."""
        response = requests.post(
            f"{device_url}/ota",
            data=b"",
            headers={"X-OTA-Password": ota_password},
            timeout=device_timeout
        )
        # Should reject empty payload
        assert response.status_code in [400, 411]  # Bad request or Length required
