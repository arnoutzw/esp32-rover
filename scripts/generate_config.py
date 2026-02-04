#!/usr/bin/env python3
"""
Generate config_generated.h from rover_config.yaml and secrets.yaml

This script reads the YAML configuration and secrets, then generates C 
preprocessor definitions for compile-time configuration of the rover firmware.

Usage:
    python generate_config.py [config.yaml] [output.h]

Defaults:
    config.yaml = rover_config.yaml
    output.h = main/config_generated.h

Secrets are loaded from secrets.yaml in the same directory as config.yaml.
"""

import sys
import os

# Try to import yaml, provide helpful error if not available
try:
    import yaml
except ImportError:
    print("Error: PyYAML is required. Install with: pip install pyyaml")
    sys.exit(1)


def load_secrets(config_dir: str) -> dict:
    """Load secrets from secrets.yaml if it exists."""
    secrets_path = os.path.join(config_dir, "secrets.yaml")
    if os.path.exists(secrets_path):
        try:
            with open(secrets_path, "r") as f:
                return yaml.safe_load(f) or {}
        except yaml.YAMLError as e:
            print(f"Warning: Invalid YAML in {secrets_path}: {e}")
            return {}
    else:
        print(f"Warning: secrets.yaml not found at {secrets_path}")
        print("         Copy secrets.yaml.example to secrets.yaml and fill in your values.")
        return {}


def generate_header(config: dict, secrets: dict, target: str) -> str:
    """Generate C header content from configuration and secrets dictionaries."""
    lines = [
        "// =============================================================================",
        "// AUTO-GENERATED FILE - DO NOT EDIT MANUALLY",
        "// Generated from rover_config.yaml and secrets.yaml by generate_config.py",
        "// =============================================================================",
        "#pragma once",
        "",
    ]

    # Target is passed in from ROVER_TARGET environment variable (set by build.sh)
    # Don't define ROVER_TARGET_* here - CMake does that based on build command
    lines.append(f"// Config generated for target: {target}")
    lines.append("// NOTE: ROVER_TARGET_* is defined by CMake, not here")
    lines.append("")

    # WiFi configuration
    wifi = config.get("wifi", {})
    wifi_mode = wifi.get("mode", "ap_only").lower()

    lines.append("// WiFi Mode Configuration")
    if wifi_mode == "ap_only":
        lines.append("#define WIFI_MODE_AP_ONLY 1")
        lines.append("#define WIFI_MODE_STA_ONLY 0")
        lines.append("#define WIFI_MODE_STA_FIRST 0")
    elif wifi_mode == "sta_only":
        lines.append("#define WIFI_MODE_AP_ONLY 0")
        lines.append("#define WIFI_MODE_STA_ONLY 1")
        lines.append("#define WIFI_MODE_STA_FIRST 0")
    else:  # sta_first
        lines.append("#define WIFI_MODE_AP_ONLY 0")
        lines.append("#define WIFI_MODE_STA_ONLY 0")
        lines.append("#define WIFI_MODE_STA_FIRST 1")
    lines.append("")

    # AP settings (password from secrets)
    ap = wifi.get("ap", {})
    lines.append("// WiFi AP Settings")
    lines.append(f'#define WIFI_AP_SSID "{ap.get("ssid", "ESP32-Rover")}"')
    lines.append(f'#define WIFI_AP_PASSWORD "{secrets.get("wifi_ap_password", "rover1234")}"')
    lines.append(f'#define WIFI_AP_CHANNEL {ap.get("channel", 1)}')
    lines.append(f'#define WIFI_AP_MAX_CONN {ap.get("max_connections", 4)}')
    lines.append("")

    # STA settings (SSID and password from secrets)
    sta = wifi.get("sta", {})
    lines.append("// WiFi STA Settings")
    lines.append(f'#define WIFI_STA_SSID "{secrets.get("wifi_sta_ssid", "")}"')
    lines.append(f'#define WIFI_STA_PASSWORD "{secrets.get("wifi_sta_password", "")}"')
    lines.append(f'#define WIFI_STA_CONNECT_TIMEOUT_S {sta.get("connect_timeout", 10)}')
    lines.append("")

    # REST API configuration
    rest_api = config.get("rest_api", {})
    lines.append("// REST API Configuration")
    lines.append(f'#define ENABLE_REST_API {1 if rest_api.get("enabled", True) else 0}')
    lines.append(f'#define REST_API_CACHE_INTERVAL_MS {rest_api.get("cache_interval_ms", 1000)}')
    lines.append("")

    # MQTT configuration (username/password from secrets)
    mqtt = config.get("mqtt", {})
    broker = mqtt.get("broker", {})
    lines.append("// MQTT Configuration")
    lines.append(f'#define ENABLE_MQTT {1 if mqtt.get("enabled", False) else 0}')
    lines.append(f'#define MQTT_BROKER_HOST "{broker.get("host", "")}"')
    lines.append(f'#define MQTT_BROKER_PORT {broker.get("port", 1883)}')
    lines.append(f'#define MQTT_USERNAME "{secrets.get("mqtt_username", "")}"')
    lines.append(f'#define MQTT_PASSWORD "{secrets.get("mqtt_password", "")}"')
    lines.append(f'#define MQTT_CLIENT_ID "{mqtt.get("client_id", "esp32-rover")}"')
    lines.append(f'#define MQTT_TOPIC_PREFIX "{mqtt.get("topic_prefix", "esp32-rover")}"')
    lines.append(f'#define MQTT_PUBLISH_INTERVAL_MS {mqtt.get("publish_interval_ms", 5000)}')
    lines.append(f'#define MQTT_QOS {mqtt.get("qos", 0)}')
    lines.append("")

    # Motor configuration
    motor = config.get("motor", {})
    pid = motor.get("pid", {})
    lines.append("// Motor Configuration")
    lines.append(f'#define CFG_MOTOR_POLE_PAIRS {motor.get("pole_pairs", 7)}')
    lines.append(f'#define CFG_MOTOR_VOLTAGE_LIMIT {motor.get("voltage_limit", 6.0)}f')
    lines.append(f'#define CFG_MOTOR_VELOCITY_LIMIT {motor.get("velocity_limit", 20.0)}f')
    lines.append(f'#define CFG_MOTOR_PWM_FREQ {motor.get("pwm_frequency", 20000)}')
    lines.append(f'#define CFG_MOTOR_PID_P {pid.get("p", 0.2)}f')
    lines.append(f'#define CFG_MOTOR_PID_I {pid.get("i", 2.0)}f')
    lines.append(f'#define CFG_MOTOR_PID_D {pid.get("d", 0.0)}f')
    lines.append(f'#define CFG_MOTOR_PID_RAMP {pid.get("ramp", 1000.0)}f')
    lines.append(f'#define CFG_MOTOR_LPF_TF {motor.get("lpf_time_constant", 0.01)}f')
    lines.append("")

    # Servo configuration
    servo = config.get("servo", {})
    lines.append("// Servo Configuration")
    lines.append(f'#define CFG_SERVO_MIN_PULSE_US {servo.get("min_pulse_us", 500)}')
    lines.append(f'#define CFG_SERVO_MAX_PULSE_US {servo.get("max_pulse_us", 2500)}')
    lines.append(f'#define CFG_SERVO_CENTER_PULSE_US {servo.get("center_pulse_us", 1500)}')
    lines.append(f'#define CFG_SERVO_MAX_ANGLE {servo.get("max_angle", 45)}')
    lines.append(f'#define CFG_SERVO_TRIM {servo.get("trim", 0)}')
    lines.append("")

    # Control parameters
    control = config.get("control", {})
    lines.append("// Control Parameters")
    lines.append(f'#define CFG_CONTROL_LOOP_FREQ {control.get("loop_frequency_hz", 100)}')
    lines.append(f'#define CFG_WATCHDOG_TIMEOUT_MS {control.get("watchdog_timeout_ms", 500)}')
    lines.append(f'#define CFG_MAX_SPEED {control.get("max_speed_percent", 100)}')
    lines.append(f'#define CFG_SPEED_RAMP_RATE {control.get("speed_ramp_rate", 5)}')
    lines.append("")

    # Safety features
    safety = config.get("safety", {})
    lines.append("// Safety Features")
    lines.append(f'#define CFG_ENABLE_WATCHDOG {1 if safety.get("watchdog_enabled", True) else 0}')
    lines.append(f'#define CFG_ENABLE_ESTOP {1 if safety.get("estop_enabled", True) else 0}')
    lines.append("")

    # mDNS configuration (target-specific hostname)
    mdns = config.get("mdns", {})
    lines.append("// mDNS Configuration")
    if target == "esp32cam":
        mdns_hostname = mdns.get("hostname_esp32cam", "esp32-rover")
    else:
        mdns_hostname = mdns.get("hostname_ttgo", "ttgo-rover")
    lines.append(f'#define MDNS_HOSTNAME "{mdns_hostname}"')
    lines.append(f'#define MDNS_INSTANCE_NAME "{mdns.get("instance_name", "ESP32 Rover Control")}"')
    lines.append("")

    # OTA configuration (password from secrets, hostname from mDNS)
    ota = config.get("ota", {})
    lines.append("// OTA Configuration")
    lines.append(f'#define ENABLE_OTA {1 if ota.get("enabled", True) else 0}')
    lines.append(f'#define OTA_HOSTNAME MDNS_HOSTNAME')  # Use same hostname as mDNS
    lines.append(f'#define OTA_PASSWORD "{secrets.get("ota_password", "rover1234")}"')
    lines.append("")

    # Task watchdog configuration (REQ-37)
    task_wdt = config.get("task_watchdog", {})
    lines.append("// Task Watchdog Configuration (REQ-37)")
    lines.append(f'#define ENABLE_TASK_WATCHDOG {1 if task_wdt.get("enabled", True) else 0}')
    lines.append(f'#define TASK_WDT_TIMEOUT_SEC {task_wdt.get("timeout_sec", 30)}')
    lines.append(f'#define TASK_WDT_PANIC_ON_TIMEOUT {1 if task_wdt.get("panic_on_timeout", True) else 0}')
    lines.append("")

    # Debug options
    debug = config.get("debug", {})
    lines.append("// Debug Options")
    lines.append(f'#define CFG_DEBUG_MOTOR {1 if debug.get("motor", False) else 0}')
    lines.append(f'#define CFG_DEBUG_ENCODER {1 if debug.get("encoder", False) else 0}')
    lines.append(f'#define CFG_DEBUG_SERVO {1 if debug.get("servo", False) else 0}')
    lines.append(f'#define CFG_DEBUG_CAMERA {1 if debug.get("camera", False) else 0}')
    lines.append(f'#define CFG_DEBUG_WEBSERVER {1 if debug.get("webserver", False) else 0}')
    lines.append(f'#define CFG_DEBUG_MQTT {1 if debug.get("mqtt", False) else 0}')
    lines.append("")

    return "\n".join(lines)


def main():
    # Parse arguments
    config_file = sys.argv[1] if len(sys.argv) > 1 else "../config/rover_config.yaml"
    output_file = sys.argv[2] if len(sys.argv) > 2 else "../firmware/main/config_generated.h"

    # Resolve paths relative to script location
    script_dir = os.path.dirname(os.path.abspath(__file__))
    config_path = os.path.join(script_dir, config_file) if not os.path.isabs(config_file) else config_file
    output_path = os.path.join(script_dir, output_file) if not os.path.isabs(output_file) else output_file

    # Read configuration
    try:
        with open(config_path, "r") as f:
            config = yaml.safe_load(f)
    except FileNotFoundError:
        print(f"Error: Configuration file not found: {config_path}")
        sys.exit(1)
    except yaml.YAMLError as e:
        print(f"Error: Invalid YAML in {config_path}: {e}")
        sys.exit(1)

    # Load secrets
    config_dir = os.path.dirname(config_path)
    secrets = load_secrets(config_dir)

    # Get target from environment variable (set by build.sh)
    target = os.environ.get("ROVER_TARGET", "").lower()
    if not target:
        print("Error: ROVER_TARGET environment variable not set.")
        print("This script should be called from the build system, or you can set it manually:")
        print("  export ROVER_TARGET=esp32cam  # or ttgo or xiao_esp32s3")
        print("  python generate_config.py")
        sys.exit(1)

    if target not in ("esp32cam", "ttgo", "xiao_esp32s3"):
        print(f"Error: Invalid ROVER_TARGET '{target}'. Must be 'esp32cam', 'ttgo', or 'xiao_esp32s3'.")
        sys.exit(1)

    # Generate header
    header_content = generate_header(config, secrets, target)

    # Write output
    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    with open(output_path, "w") as f:
        f.write(header_content)

    print(f"Generated {output_path} from {config_path}")


if __name__ == "__main__":
    main()
