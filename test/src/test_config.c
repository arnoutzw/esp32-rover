/**
 * @file test_config.c
 * @brief Unit tests for configuration system (REQ-01 through REQ-05)
 *
 * Tests verify that the generated config_generated.h contains correct values
 * for all compile-time configuration options.
 */

#include "unity.h"

/* Include the generated config to test its values */
#include "../main/config_generated.h"

/* ==========================================================================
 * REQ-01: Configuration YAML
 * Verify that configuration defines are generated from YAML
 * ========================================================================== */

void test_req01_target_defined(void)
{
    /* At least one target should be defined */
#if defined(ROVER_TARGET_TTGO) || defined(ROVER_TARGET_ESP32CAM)
    TEST_ASSERT_TRUE(1);
#else
    TEST_ASSERT_TRUE(0);  /* No target defined */
#endif
}

void test_req01_wifi_ap_settings_exist(void)
{
    /* Verify WiFi AP settings are defined */
    TEST_ASSERT_NOT_NULL(WIFI_AP_SSID);
    TEST_ASSERT_NOT_NULL(WIFI_AP_PASSWORD);
    TEST_ASSERT_TRUE(WIFI_AP_CHANNEL >= 1 && WIFI_AP_CHANNEL <= 14);
    TEST_ASSERT_TRUE(WIFI_AP_MAX_CONN >= 1 && WIFI_AP_MAX_CONN <= 10);
}

void test_req01_motor_config_exists(void)
{
    /* Verify motor configuration values exist and are reasonable */
    TEST_ASSERT_TRUE(CFG_MOTOR_POLE_PAIRS > 0);
    TEST_ASSERT_TRUE(CFG_MOTOR_VOLTAGE_LIMIT > 0.0f);
    TEST_ASSERT_TRUE(CFG_MOTOR_VELOCITY_LIMIT > 0.0f);
    TEST_ASSERT_TRUE(CFG_MOTOR_PWM_FREQ > 0);
}

void test_req01_servo_config_exists(void)
{
    /* Verify servo configuration values exist and are reasonable */
    TEST_ASSERT_TRUE(CFG_SERVO_MIN_PULSE_US > 0);
    TEST_ASSERT_TRUE(CFG_SERVO_MAX_PULSE_US > CFG_SERVO_MIN_PULSE_US);
    TEST_ASSERT_TRUE(CFG_SERVO_CENTER_PULSE_US >= CFG_SERVO_MIN_PULSE_US);
    TEST_ASSERT_TRUE(CFG_SERVO_CENTER_PULSE_US <= CFG_SERVO_MAX_PULSE_US);
    TEST_ASSERT_TRUE(CFG_SERVO_MAX_ANGLE > 0 && CFG_SERVO_MAX_ANGLE <= 90);
}

void test_req01_control_params_exist(void)
{
    /* Verify control parameters exist and are reasonable */
    TEST_ASSERT_TRUE(CFG_CONTROL_LOOP_FREQ > 0);
    TEST_ASSERT_TRUE(CFG_WATCHDOG_TIMEOUT_MS > 0);
    TEST_ASSERT_TRUE(CFG_MAX_SPEED > 0 && CFG_MAX_SPEED <= 100);
}

/* ==========================================================================
 * REQ-02: WiFi STA-First Mode
 * Verify WiFi mode configuration is correct and mutually exclusive
 * ========================================================================== */

void test_req02_wifi_mode_defined(void)
{
    /* At least one WiFi mode should be set to 1 */
    int mode_count = 0;
#if WIFI_MODE_AP_ONLY
    mode_count++;
#endif
#if WIFI_MODE_STA_ONLY
    mode_count++;
#endif
#if WIFI_MODE_STA_FIRST
    mode_count++;
#endif
    TEST_ASSERT_EQUAL_INT(1, mode_count);
}

void test_req02_sta_settings_exist(void)
{
    /* Verify STA settings are defined */
    TEST_ASSERT_NOT_NULL(WIFI_STA_SSID);
    TEST_ASSERT_NOT_NULL(WIFI_STA_PASSWORD);
    TEST_ASSERT_TRUE(WIFI_STA_CONNECT_TIMEOUT_S > 0);
}

void test_req02_sta_timeout_reasonable(void)
{
    /* Timeout should be at least 10 seconds and not more than 120 */
    TEST_ASSERT_TRUE(WIFI_STA_CONNECT_TIMEOUT_S >= 10);
    TEST_ASSERT_TRUE(WIFI_STA_CONNECT_TIMEOUT_S <= 120);
}

/* ==========================================================================
 * REQ-03: HTTP REST API
 * Verify REST API configuration
 * ========================================================================== */

void test_req03_rest_api_flag_defined(void)
{
    /* ENABLE_REST_API should be defined as 0 or 1 */
#if defined(ENABLE_REST_API)
    TEST_ASSERT_TRUE(ENABLE_REST_API == 0 || ENABLE_REST_API == 1);
#else
    TEST_ASSERT_TRUE(0);  /* ENABLE_REST_API not defined */
#endif
}

void test_req03_rest_api_cache_interval(void)
{
    /* If REST API is enabled, cache interval should be defined and reasonable */
#if ENABLE_REST_API
    TEST_ASSERT_TRUE(REST_API_CACHE_INTERVAL_MS >= 100);
    TEST_ASSERT_TRUE(REST_API_CACHE_INTERVAL_MS <= 10000);
#else
    TEST_ASSERT_TRUE(1);  /* Skip if REST API disabled */
#endif
}

/* ==========================================================================
 * REQ-04: MQTT Telemetry Service
 * Verify MQTT configuration
 * ========================================================================== */

void test_req04_mqtt_flag_defined(void)
{
    /* ENABLE_MQTT should be defined as 0 or 1 */
#if defined(ENABLE_MQTT)
    TEST_ASSERT_TRUE(ENABLE_MQTT == 0 || ENABLE_MQTT == 1);
#else
    TEST_ASSERT_TRUE(0);  /* ENABLE_MQTT not defined */
#endif
}

void test_req04_mqtt_broker_settings(void)
{
    /* If MQTT is enabled, broker settings should be defined */
#if ENABLE_MQTT
    TEST_ASSERT_NOT_NULL(MQTT_BROKER_HOST);
    TEST_ASSERT_TRUE(MQTT_BROKER_PORT > 0 && MQTT_BROKER_PORT <= 65535);
    TEST_ASSERT_NOT_NULL(MQTT_CLIENT_ID);
    TEST_ASSERT_NOT_NULL(MQTT_TOPIC_PREFIX);
#else
    TEST_ASSERT_TRUE(1);  /* Skip if MQTT disabled */
#endif
}

void test_req04_mqtt_publish_interval(void)
{
    /* If MQTT is enabled, publish interval should be reasonable */
#if ENABLE_MQTT
    TEST_ASSERT_TRUE(MQTT_PUBLISH_INTERVAL_MS >= 1000);
    TEST_ASSERT_TRUE(MQTT_PUBLISH_INTERVAL_MS <= 60000);
#else
    TEST_ASSERT_TRUE(1);  /* Skip if MQTT disabled */
#endif
}

void test_req04_mqtt_qos_valid(void)
{
    /* If MQTT is enabled, QoS should be 0, 1, or 2 */
#if ENABLE_MQTT
    TEST_ASSERT_TRUE(MQTT_QOS >= 0 && MQTT_QOS <= 2);
#else
    TEST_ASSERT_TRUE(1);  /* Skip if MQTT disabled */
#endif
}

/* ==========================================================================
 * REQ-05: Service Status on Diagnostic Screens
 * These are implicitly tested through REQ-03 and REQ-04 flag existence
 * The actual display logic is tested via the diagnostic state machine
 * ========================================================================== */

void test_req05_service_status_flags_available(void)
{
    /* Both REST API and MQTT flags should be defined for status display */
#if defined(ENABLE_REST_API) && defined(ENABLE_MQTT)
    TEST_ASSERT_TRUE(1);
#else
    TEST_ASSERT_TRUE(0);  /* Missing service status flags */
#endif
}

/* ==========================================================================
 * Test Runner for Config Tests
 * ========================================================================== */

int run_config_tests(void)
{
    printf("\n=== Configuration Tests (REQ-01 to REQ-05) ===\n");

    UNITY_BEGIN();

    /* REQ-01 tests */
    printf("\nREQ-01: Configuration YAML\n");
    RUN_TEST(test_req01_target_defined);
    RUN_TEST(test_req01_wifi_ap_settings_exist);
    RUN_TEST(test_req01_motor_config_exists);
    RUN_TEST(test_req01_servo_config_exists);
    RUN_TEST(test_req01_control_params_exist);

    /* REQ-02 tests */
    printf("\nREQ-02: WiFi STA-First Mode\n");
    RUN_TEST(test_req02_wifi_mode_defined);
    RUN_TEST(test_req02_sta_settings_exist);
    RUN_TEST(test_req02_sta_timeout_reasonable);

    /* REQ-03 tests */
    printf("\nREQ-03: HTTP REST API\n");
    RUN_TEST(test_req03_rest_api_flag_defined);
    RUN_TEST(test_req03_rest_api_cache_interval);

    /* REQ-04 tests */
    printf("\nREQ-04: MQTT Telemetry Service\n");
    RUN_TEST(test_req04_mqtt_flag_defined);
    RUN_TEST(test_req04_mqtt_broker_settings);
    RUN_TEST(test_req04_mqtt_publish_interval);
    RUN_TEST(test_req04_mqtt_qos_valid);

    /* REQ-05 tests */
    printf("\nREQ-05: Service Status Flags\n");
    RUN_TEST(test_req05_service_status_flags_available);

    UNITY_END();
}
